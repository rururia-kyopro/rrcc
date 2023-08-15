

// トークンの種類
typedef enum {
    TK_RESERVED,
    TK_IDENT,
    TK_NUM,
    TK_EOF
} TokenKind;

typedef struct Token Token;

struct Token {
    TokenKind kind;
    Token *next;
    int val;
    char *str;
    int len;
};

extern Token *token;

extern char *user_input;

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
} NodeKind;

typedef struct Node Node;

struct Node {
    NodeKind kind;
    Node *lhs;
    Node *rhs;
    int val;
    char *ident;
};


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

Token *tokenize(char *);
void gen(Node *);

void dumpnodes(Node *node);
