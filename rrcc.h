#include "vector.h"
#include <stddef.h>
#include <stdbool.h>

void print_current_position(char *loc);
void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);

void debug_log(char *fmt, ...);

char *read_file(char *path);
char read_escape(char **p);

int pp_main(char *file);
char *do_pp();
void init_include_pathes();
void append_include_pathes(char *p);

int match_punc(char *p);

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
    TK_SWITCH,
    TK_CASE,
    TK_DEFAULT,
    TK_BREAK,
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
    enum { SUF_NONE, SUF_L, SUF_LL, SUF_U, SUF_UL, SUF_ULL } suffix;
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
    ND_MOD,
    ND_LSHIFT,
    ND_RSHIFT,
    ND_OR,
    ND_XOR,
    ND_AND,
    ND_ASSIGN,
    ND_EQUAL,
    ND_NOT_EQUAL,
    ND_LESS,
    ND_LESS_OR_EQUAL,
    ND_GREATER,
    ND_GREATER_OR_EQUAL,
    ND_LOGICAL_OR,
    ND_LOGICAL_AND,
    ND_CAST,
    ND_NUM,
    ND_STRING_LITERAL,
    ND_COMMA_EXPR,
    ND_LVAR,
    ND_GVAR,
    ND_IDENT,
    ND_RETURN,
    ND_IF,
    ND_SWITCH,
    ND_CASE,
    ND_DEFAULT,
    ND_BREAK,
    ND_WHILE,
    ND_FOR,
    ND_DO,
    ND_COMPOUND,
    ND_CALL,
    ND_POSTFIX_INC,
    ND_POSTFIX_DEC,
    ND_PREFIX_INC,
    ND_PREFIX_DEC,
    ND_FUNC_DEF,
    ND_FUNC_DECL,
    ND_SCOPE,
    ND_DECL_LIST,
    ND_ADDRESS_OF,
    ND_DEREF,
    ND_BIT_NOT,
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
                    bool is_vararg;
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
            Type *type;
            int max_stack_size;
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
            Type *base_type;
            Vector *decls;
        } decl_list;
        struct {
            int current;
            Node *parent;
            Vector *childs;
            Vector *locals;
        } scope;
        struct {
            size_t value;
        } incdec;
        struct {
            Vector *cases;
            Node *default_stmt;
        } switch_;
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
Node *expression();
Node *assignment_expression();
Node *conditional_expression();
Node *constant_expression();
Node *logical_OR_expression();
Node *logical_AND_expression();
Node *inclusive_OR_expression();
Node *exclusive_OR_expression();
Node *AND_expression();
Node *equality_expression();
Node *relational_expression();
Node *shift_expression();
Node *additive_expression();
Node *multiplicative_expression();
Node *cast_expression();
Node *unary_expression();
Node *postfix_expression();
Node *primary_expression();
Node *type_(bool need_ident, bool is_global, bool parse_one_type);
Node *type_pointer(bool need_ident);
Node *type_array(bool need_ident);
void type_array_suffix(Vector *array_suffix_vector);
Node *type_ident(bool need_ident);
Node *ident_();
Vector *function_arguments(bool *is_vararg);
Node *struct_declaration(bool is_struct);
Vector *struct_members(size_t *size, bool is_struct);
Node *enum_declaration();
Vector *enum_members();
Node *typedef_declaration(bool is_global, Node *type_node);
bool consume_type_prefix(TokenKind *kind);
TokenKind expect_type_prefix();
bool peek_type_prefix();
Node *constant_fold(Node *node);

/// LVar ///

struct LVar {
    char *name;
    int len;
    int offset;
    Type *type;
};

extern int locals_stack_size;

LVar *find_lvar_scope(char *ident, int ident_len);
LVar *find_lvar_one(Vector *locals, char *ident, int ident_len);
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
Node *find_symbol(Vector *globals, char *ident, int ident_len);

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
    enum { VOID, CHAR, SHORT, INT, LONG, LONGLONG, FLOAT, DOUBLE, LONGDOUBLE, BOOL, COMPLEX, PTR, ARRAY, FUNC, STRUCT, UNION, ENUM } ty;
    enum { NOSIGNED, UNSIGNED, SIGNED } signedness;
    Type *ptr_to;
    size_t array_size;
    Vector *args; // func
    bool has_array_size;
    char *ident; // enum or struct or union
    int ident_len; // enum or struct or union
    Vector *members; // enum or struct or union
    size_t struct_size; // struct or union
    bool struct_complete; // struct or union
};

int type_sizeof(Type *type);
Type *type_arithmetic(Type *type_r, Type *type_l);
Type *type_comparator(Type *type_r, Type *type_l);
Type *type_logical(Type *type_r, Type *type_l);
Type *type_bitwise(Type *type_r, Type *type_l);
Type *type_shift(Type *type_r, Type *type_l);
bool type_implicit_ptr(Type *type);
bool type_is_int(Type *type);
bool type_is_floating(Type *type);
bool type_is_basic(Type *type);
bool type_is_scalar(Type *type);
bool type_is_same(Type *type_a, Type *type_b);
Type *type_new_ptr(Type *type);
Type *type_new_array(Type *type, bool has_size, int size);
Type *type_new_func(Type *type, Vector *args);
Type *type_new_struct(char *ident, int ident_len);
Type *type_new_enum(char *ident, int ident_len);
bool type_find_ident(Node *node, char **ident, int *ident_len);
StructMember *find_struct_member(Vector *member_list, char *ident, int ident_len, size_t *offset);

/// StructMember ///
struct StructMember {
    Type *type;
    int offset;
    char *ident;
    int ident_len;
    bool unnamed;
};

/// Struct Registry Entry ///

struct StructRegistryEntry {
    bool is_struct; // true: struct, false: union
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

char *mystrdup(char *p);
