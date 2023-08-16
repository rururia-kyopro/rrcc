
void error(char *fmt, ...);
void error_at(char *loc, char *fmt, ...);

typedef struct Token Token;
typedef struct Node Node;
typedef struct NodeList NodeList;
typedef struct LVar LVar;

/// Token ///

// トークンの種類
typedef enum {
    TK_RESERVED,
    TK_RETURN,
    TK_IF,
    TK_ELSE,
    TK_WHILE,
    TK_FOR,
    TK_DO,
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
    ND_RETURN,
    ND_IF,
    ND_WHILE,
    ND_FOR,
    ND_DO,
    ND_COMPOUND,
    ND_CALL,
} NodeKind;

struct NodeList {
    Node *node;
    NodeList *next;
};

struct Node {
    NodeKind kind;
    Node *lhs;
    Node *rhs;
    union {
        int val;
        LVar *lvar;
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
    };
};

extern Node *code[100];

void program();
Node *stmt();
Node *expr();
Node *assign();
Node *equality();
Node *relational();
Node *add();
Node *primary();
Node *mul();
Node *unary();

/// LVar ///

struct LVar {
    LVar *next;
    char *name;
    int len;
    int offset;
};

extern LVar *locals;
extern int local_count;

LVar *find_lvar(char *ident, int ident_len);
LVar *new_lvar(char *ident, int ident_len);
int lvar_count(LVar *locals);

Token *tokenize(char *);
void gen(Node *);

void dumpnodes(Node *node);
