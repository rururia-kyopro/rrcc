#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "rrcc.h"

Token *token;

char *user_input;

void next_token() {
    token = token->next;
}

void unget_token() {
    if(token->prev == NULL){
        error("Tried to unget at the beginning.");
    }
    token = token->prev;
}

static bool is_prefix(char *prefix, char *str, int len) {
    if(len < strlen(prefix)){
        len = strlen(prefix);
    }
    return strncmp(str, prefix, len) == 0;
}

bool consume(char* op){
    if(token->kind != TK_RESERVED || !is_prefix(op, token->str, token->len))
        return false;
    next_token();
    return true;
}

bool consume_kind(TokenKind kind) {
    if(token->kind != kind)
        return false;
    next_token();
    return true;
}

void expect(char *op){
    if(token->kind != TK_RESERVED || !is_prefix(op, token->str, token->len))
        error_at(token->str, "Not '%s'", op);
    next_token();
}

void expect_kind(TokenKind kind) {
    if(token->kind != kind)
        error_at(token->str, "Not token kind %d", kind);
    next_token();
}

bool consume_ident(char **ident, int *ident_len) {
    if(token->kind != TK_IDENT)
        return false;
    *ident = token->str;
    *ident_len = token->len;
    next_token();
    return true;
}

void expect_ident(char **ident, int *ident_len) {
    if(!consume_ident(ident, ident_len)) {
        error_at(token->str, "Not an identifier");
    }
}

int expect_number() {
    if(token->kind != TK_NUM)
        error_at(token->str, "Not a number");
    int val = token->val;
    next_token();
    return val;
}

bool peek(char* op) {
    return token->kind == TK_RESERVED && is_prefix(op, token->str, token->len);
}

bool peek_kind(TokenKind kind) {
    return token->kind == kind;
}

bool peek_ident(char **ident, int *ident_len) {
    if(token->kind == TK_IDENT) {
        *ident = token->str;
        *ident_len = token->len;
        return true;
    }
    return false;
}

bool at_eof() {
    return token->kind == TK_EOF;
}

static Token *new_token(TokenKind kind, Token *cur, char *str, int len){
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
    tok->prev = cur;
    cur->next = tok;
    return tok;
}

static int read_ident(char *p) {
    char *q = p;
    p++;
    while(*p == '_' || isalpha(*p) || isdigit(*p)){
        p++;
    }
    return p - q;
}

static bool is_keyword(char *keyword, char *str, int len) {
    if(strlen(keyword) > len) {
        len = strlen(keyword);
    }
    return strncmp(str, keyword, len) == 0;
}

Token *tokenize(char *p){
    Token head;
    head.prev = NULL;
    head.next = NULL;
    Token *cur = &head;

    while(*p){
        if(isspace(*p)){
            p++;
            continue;
        }

        if(strncmp(p, "//", 2) == 0) {
            p += 2;
            while(*p != '\n' && *p){
                p++;
            }
            continue;
        }

        if(strncmp(p, "/*", 2) == 0) {
            char *q = strstr(p + 2, "*/");
            if(!q) {
                error_at(p, "Comment is not closed");
            }
            p = q + 2;
            continue;
        }

        if(*p == '\'') {
            char *q = p;
            p++;
            char c;
            if(*p == '\\') {
                p++;
                c = read_escape(&p);
                p++;
            } else {
                c = *p;
                p++;
            }
            if(*p != '\''){
                error("expect ' (single quote)");
            }
            cur = new_token(TK_NUM, cur, q, p - q);
            cur->val = c;
            p++;
            continue;
        }

        if(*p == '"') {
            p++;
            char *literal = p;
            int state = 0;
            Vector *char_vec = new_vector();
            while(*p) {
                if(state == 1) {
                    char c = read_escape(&p);
                    vector_push(char_vec, (void *)(long)c);
                    p++;
                    state = 0;
                    continue;
                }
                if(*p == '"') {
                    break;
                }
                if(*p == '\\'){
                    state = 1;
                }else{
                    vector_push(char_vec, (void *)(long)*p);
                    state = 0;
                }
                p++;
            }
            int len = p - literal;
            if(*p == '"') {
                p++;
                cur = new_token(TK_STRING_LITERAL, cur, literal, len);
                cur->literal = char_vec;
                cur->literal_len = vector_size(char_vec);
                continue;
            } else {
                error_at(p, "expect \" (double quote)");
            }
        }

        if(strncmp(p, "==", 2) == 0 || strncmp(p, "!=", 2) == 0 || strncmp(p, "<=", 2) == 0 || strncmp(p, ">=", 2) == 0 || strncmp(p, "->", 2) == 0){
            cur = new_token(TK_RESERVED, cur, p, 2);
            p += 2;
            continue;
        }
        if(strchr("+-*/()><=;{},&[].", *p)){
            cur = new_token(TK_RESERVED, cur, p++, 1);
            continue;
        }
        if(*p == '_' || isalpha(*p)){
            int len = read_ident(p);
            struct Keyword { char *name; TokenKind kind; };
            const struct Keyword keywords[] = {
                { "void", TK_VOID },
                { "char", TK_CHAR },
                { "short", TK_SHORT },
                { "int", TK_INT },
                { "long", TK_LONG },
                { "float", TK_FLOAT },
                { "double", TK_DOUBLE },
                { "signed", TK_SIGNED },
                { "unsigned", TK_UNSIGNED },
                { "_Bool", TK_BOOL },
                { "_Complex", TK_COMPLEX },
                { "struct", TK_STRUCT },
                { "union", TK_UNION },
                { "enum", TK_ENUM },
                { "typedef", TK_TYPEDEF },
                { "extern", TK_EXTERN },
                { "static", TK_STATIC },
                { "auto", TK_AUTO },
                { "register", TK_REGISTER },
                { "const", TK_CONST },
                { "restrict", TK_RESTRICT },
                { "volatile", TK_VOLATILE },
                { "inline", TK_INLINE },
                { "return", TK_RETURN },
                { "if", TK_IF },
                { "else", TK_ELSE },
                { "while", TK_WHILE },
                { "for", TK_FOR },
                { "do", TK_DO },
                { "sizeof", TK_SIZEOF },
            };
            bool found = false;
            for(int i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++){
                if(is_keyword(keywords[i].name, p, len)) {
                    cur = new_token(keywords[i].kind, cur, p, len);
                    found = true;
                    break;
                }
            }
            if(!found) {
                cur = new_token(TK_IDENT, cur, p, len);
            }
            p += len;
            continue;
        }

        if(isdigit(*p)){
            cur = new_token(TK_NUM, cur, p, 1);
            cur->val = strtol(p, &p, 10);
            continue;
        }

        error_at(p, "Tokenize error");
    }

    new_token(TK_EOF, cur, p, 0);
    return head.next;
}
