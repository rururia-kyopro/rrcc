#include "vector.h"
#include <stddef.h>

void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);

typedef struct Token Token;
typedef struct Node Node;
typedef struct NodeList NodeList;
typedef struct FuncDefArg FuncDefArg;
typedef struct LVar LVar;
typedef struct Type Type;

/// Token ///

// トークンの種類
typedef enum {
    TK_RESERVED,
    TK_INT,
    TK_RETURN,
    TK_IF,
    TK_ELSE,
    TK_WHILE,
    TK_FOR,
    TK_DO,
    TK_SIZEOF,
    TK_IDENT,
    TK_NUM,
    TK_EOF,
} TokenKind;

struct Token {
    TokenKind kind;
    Token *next;
    int val;
    char *str;
    int len;
};

extern Token *token;

extern char *user_input;

bool consume(char* op);
bool consume_kind(TokenKind kind);
void expect(char *op);
void expect_kind(TokenKind kind);
bool consume_ident(char **ident, int *ident_len);
void expect_ident(char **ident, int *ident_len);
int expect_number();
bool at_eof();
Token *tokenize(char *p);

/// Parse ///

typedef enum {
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
    ND_LVAR,
    ND_IDENT,
    ND_RETURN,
    ND_IF,
    ND_WHILE,
    ND_FOR,
    ND_DO,
    ND_COMPOUND,
    ND_CALL,
    ND_FUNC_DEF,
    ND_ADDRESS_OF,
    ND_DEREF,
    ND_SIZEOF,
    ND_DECL_VAR,
    ND_TYPE
} NodeKind;

struct NodeList {
    Node *node;
    NodeList *next;
};

struct FuncDefArg {
    char *ident;
    int ident_len;
    LVar *lvar;
    Node *type;
};

struct Node {
    NodeKind kind;
    Node *lhs;
    Node *rhs;
    Type *expr_type;
    union {
        int val;
        LVar *lvar;
        LVar *decl_var_lvar;
        Type *type;
        Node *else_stmt;
        struct {
            Node *for_update_expr;
            Node *for_stmt;
        };
        Node **compound_stmt_list;
        struct {
            char *call_ident;
            int call_ident_len;
            NodeList call_arg_list;
        };
        struct {
            char *func_def_ident;
            int func_def_ident_len;
            Vector *func_def_arg_vec;
            Vector *func_def_lvar;
            Node *func_def_return_type;
        };
        struct {
            char *ident;
            int ident_len;
            LVar *lvar;
        } ident;
    };
};

extern Node *code[100];

void translation_unit();
Node *function_definition();
Node *stmt();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *primary();
Node *mul();
Node *unary();
Node *type_();
Node *ident_();

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

/// Type ///

struct Type {
    enum { INT, PTR, ARRAY } ty;
    Type *ptr_to;
    size_t array_size;
};

extern Type int_type;

int type_sizeof(Type *type);
Type *type_arithmetic(Type *type_r, Type *type_l);
Type *type_comparator(Type *type_r, Type *type_l);
bool type_implicit_ptr(Type *type);

Token *tokenize(char *);
void gen(Node *);

void dumpnodes(Node *node);
