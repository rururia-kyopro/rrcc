#include "vector.h"
#include <stddef.h>
#include <stdbool.h>

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);

void debug_log(char *fmt, ...);

char *read_file(char *path);
char read_escape(char **p);

int pp_main(char *file);
char *do_pp();
void init_include_pathes();
void append_include_pathes(char *p);

typedef struct Token Token;
typedef struct Node Node;
typedef struct NodeList NodeList;
typedef struct FuncDefArg FuncDefArg;
typedef struct LVar LVar;
typedef struct Type Type;
typedef struct GVar GVar;
typedef struct StringLiteral StringLiteral;
typedef struct StructMember StructMember;
typedef struct StructRegistryEntry StructRegistryEntry;
typedef struct EnumMember EnumMember;
typedef struct EnumRegistryEntry EnumRegistryEntry;
typedef struct TypedefRegistryEntry TypedefRegistryEntry;

/// Enums ///

typedef enum {
    TK_RESERVED,
    TK_VOID,
    TK_CHAR,
    TK_SHORT,
    TK_INT,
    TK_LONG,
    TK_FLOAT,
    TK_DOUBLE,
    TK_SIGNED,
    TK_UNSIGNED,
    TK_BOOL,
    TK_COMPLEX,
    TK_STRUCT,
    TK_UNION,
    TK_ENUM,
    TK_TYPEDEF,
    TK_EXTERN,
    TK_STATIC,
    TK_AUTO,
    TK_REGISTER,
    TK_CONST,
    TK_RESTRICT,
    TK_VOLATILE,
    TK_INLINE,
    TK_RETURN,
    TK_IF,
    TK_ELSE,
    TK_WHILE,
    TK_FOR,
    TK_DO,
    TK_SIZEOF,
    TK_IDENT,
    TK_NUM,
    TK_STRING_LITERAL,
    TK_EOF,
    TK_MAX
} TokenKind;

typedef enum {
    TS_NONE,
    TS_TYPEDEF,
    TS_EXTERN,
    TS_STATIC,
    TS_AUTO,
    TS_REGISTER,
} TypeStorage;

typedef enum {
    TQ_NONE,
    TQ_CONST,
    TQ_RESTRICT,
    TQ_VOLATILE,
} TypeQual;

typedef enum {
    TB_NONE,
    TB_VOID,
    TB_CHAR,
    TB_SHORT,
    TB_INT,
    TB_LONG,
    TB_LONGLONG,
    TB_FLOAT,
    TB_DOUBLE,
    TB_LONGDOUBLE,
    TB_BOOL,
    TB_COMPLEX,
    TB_STRUCT,
    TB_UNION,
    TB_ENUM,
    TB_TYPEDEF_NAME,
} TypeBasic;

/// Token ///

struct Token {
    TokenKind kind;
    Token *prev;
    Token *next;
    int val;
    char *str;
    int len;
    Vector *literal;
    int literal_len;
};

extern Token *token;

extern char *user_input;
extern char *filename;
extern int debug_parse;

void next_token();
void unget_token();
bool consume(char* op);
bool consume_kind(TokenKind kind);
void expect(char *op);
void expect_kind(TokenKind kind);
bool consume_ident(char **ident, int *ident_len);
void expect_ident(char **ident, int *ident_len);
int expect_number();
bool peek(char* op);
bool peek_kind(TokenKind kind);
bool peek_ident(char **ident, int *ident_len);
bool at_eof();
Token *tokenize(char *p);

/// Parse ///

typedef enum {
    ND_TRANS_UNIT,
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_ASSIGN,
    ND_EQUAL,
    ND_NOT_EQUAL,
    ND_LESS,
    ND_LESS_OR_EQUAL,
    ND_GREATER,
    ND_GREATER_OR_EQUAL,
    ND_NUM,
    ND_STRING_LITERAL,
    ND_LVAR,
    ND_GVAR,
    ND_IDENT,
    ND_RETURN,
    ND_IF,
    ND_WHILE,
    ND_FOR,
    ND_DO,
    ND_COMPOUND,
    ND_CALL,
    ND_FUNC_DEF,
    ND_FUNC_DECL,
    ND_DECL_LIST,
    ND_ADDRESS_OF,
    ND_DEREF,
    ND_SIZEOF,
    ND_DECL_VAR,
    ND_TYPE,
    ND_INIT,
    ND_CONVERT,
    ND_GVAR_DEF,
    ND_TYPE_POINTER,
    ND_TYPE_ARRAY,
    ND_TYPE_FUNC,
    ND_TYPE_STRUCT,
    ND_TYPE_ENUM,
    ND_TYPE_TYPEDEF,
    ND_TYPE_EXTERN,
} NodeKind;

struct NodeList {
    Node *node;
    NodeList *next;
};

struct FuncDefArg {
    char *ident;
    int ident_len;
    LVar *lvar;
    Type *type;
};

struct Node {
    NodeKind kind;
    Node *lhs;
    Node *rhs;
    Type *expr_type;
    union {
        int val;
        LVar *lvar;
        struct {
            Vector *decl;
        } trans_unit;
        struct {
            LVar *lvar;
            Node *init_expr;
        } decl_var;
        struct {
            Type *type;
            union {
                struct {
                    Vector *args;
                } func_args;
                struct {
                    size_t size;
                    bool has_size;
                } array;
                struct {
                } struct_;
            };
        } type;
        Node *else_stmt;
        struct {
            Node *for_update_expr;
            Node *for_stmt;
        };
        Vector *compound_stmt_list;
        struct {
            char *call_ident;
            int call_ident_len;
            NodeList call_arg_list;
        };
        struct {
            char *ident;
            int ident_len;
            Vector *arg_vec;
            Vector *lvar_vec;
            Type *type;
        } func_def;
        struct {
            char *ident;
            int ident_len;
            LVar *lvar;
        } ident;
        struct {
            GVar *gvar;
            Node *init_expr;
        } gvar_def;
        struct {
            GVar *gvar;
        } gvar;
        struct {
            StringLiteral *literal;
        } string_literal;
        struct {
            Vector *init_expr;
        } init;
        struct {
            Vector *decls;
        } decl_list;
    };
};

extern Node *code[100];

Node *translation_unit();
Node *external_declaration();
Node *function_definition(TypeStorage type_storage, Node *type_node);
Node *global_variable_definition(Node *type_prefix, char *ident, int ident_len);
Node *variable_definition(bool is_global, Node *type_node, TypeStorage type_storage);
Node *initializer();
Node *stmt();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *primary();
Node *mul();
Node *unary();
Node *type_(bool need_ident, bool is_global, bool is_funcarg);
Node *type_pointer(bool need_ident);
Node *type_array(bool need_ident);
void type_array_suffix(Vector *array_suffix_vector);
Node *type_ident(bool need_ident);
Node *ident_();
Vector *function_arguments();
Node *struct_declaration();
Vector *struct_members(size_t *size);
Node *enum_declaration();
Vector *enum_members();
Node *typedef_declaration(bool is_global, Node *type_node);
bool consume_type_prefix(TokenKind *kind);
TokenKind expect_type_prefix();
bool peek_type_prefix();

/// LVar ///

struct LVar {
    char *name;
    int len;
    int offset;
    Type *type;
};

extern Vector *locals;
extern int locals_stack_size;

LVar *find_lvar(Vector *locals, char *ident, int ident_len);
LVar *new_lvar(Vector *locals, char *ident, int ident_len);
int lvar_count(Vector *locals);
int lvar_stack_size(Vector *locals);

/// GVar ///

struct GVar {
    char *name;
    int len;
    Type *type;
    bool is_enum;
    int enum_num;
};

extern Vector *globals;
extern int global_size;

GVar *find_gvar(Vector *locals, char *ident, int ident_len);
GVar *new_gvar(Vector *locals, char *ident, int ident_len, Type *type);
Node *find_symbol(Vector *globals, Vector *locals, char *ident, int ident_len);

/// String Literal ///
struct StringLiteral {
    char *str;
    int len;
    Vector *char_vec;
    int vec_len;
    int index;
};

extern Vector *global_string_literals;

/// Type ///

struct Type {
    enum { VOID, CHAR, SHORT, INT, LONG, LONGLONG, FLOAT, DOUBLE, LONGDOUBLE, BOOL, COMPLEX, PTR, ARRAY, FUNC, STRUCT, ENUM } ty;
    enum { NOSIGNED, UNSIGNED, SIGNED } signedness;
    Type *ptr_to;
    size_t array_size;
    Vector *args; // func
    bool has_array_size;
    char *ident; // enum or struct
    int ident_len; // enum or struct
    Vector *members; // enum or struct
    size_t struct_size; // struct
    bool struct_complete;
};

int type_sizeof(Type *type);
Type *type_arithmetic(Type *type_r, Type *type_l);
Type *type_comparator(Type *type_r, Type *type_l);
bool type_implicit_ptr(Type *type);
bool type_is_int(Type *type);
bool type_is_same(Type *type_a, Type *type_b);
Type *type_new_ptr(Type *type);
Type *type_new_array(Type *type, bool has_size, int size);
Type *type_new_func(Type *type, Vector *args);
Type *type_new_struct(char *ident, int ident_len);
Type *type_new_enum(char *ident, int ident_len);
bool type_find_ident(Node *node, char **ident, int *ident_len);

/// StructMember ///
struct StructMember {
    Node *node;
    int offset;
    char *ident;
    int ident_len;
};

/// Struct Registry Entry ///

struct StructRegistryEntry {
    Type *type;
    char *ident;
    int ident_len;
};

extern Vector *struct_registry;

/// EnumMember ///
struct EnumMember {
    int num;
    GVar *gvar;
};

/// Enum Registry Entry ///

struct EnumRegistryEntry {
    Type *type;
    char *ident;
    int ident_len;
};

extern Vector *enum_registry;

/// Typedef Registry Entry ///

struct TypedefRegistryEntry {
    Type *type;
    char *ident;
    int ident_len;
};

extern Vector *typedef_registry;

bool compare_ident(char *ident_a, int ident_a_len, char *ident_b, int ident_b_len);
bool compare_slice(char *slice, int slice_len, char *null_term_str);

Token *tokenize(char *);
void gen_string_literals();
void gen(Node *);

void dumpnodes(Node *node);
