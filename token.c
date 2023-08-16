#include <ctype.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "9cc.h"

Token *token;

char *user_input;

static bool is_prefix(char *prefix, char *str, int len) {
    if(len < strlen(prefix)){
        len = strlen(prefix);
    }
    return strncmp(str, prefix, len) == 0;
}

bool consume(char* op){
    if(token->kind != TK_RESERVED || !is_prefix(op, token->str, token->len))
        return false;
    token = token->next;
    return true;
}

bool consume_kind(TokenKind kind) {
    if(token->kind != kind)
        return false;
    token = token->next;
    return true;
}

void expect(char *op){
    if(token->kind != TK_RESERVED || !is_prefix(op, token->str, token->len))
        error_at(token->str, "Not '%s'", op);
    token = token->next;
}

void expect_kind(TokenKind kind) {
    if(token->kind != kind)
        error_at(token->str, "Not token kind %d", kind);
    token = token->next;
}

bool consume_ident(char **ident, int *ident_len) {
    if(token->kind != TK_IDENT)
        return false;
    *ident = token->str;
    *ident_len = token->len;
    token = token->next;
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
    token = token->next;
    return val;
}

bool at_eof() {
    return token->kind == TK_EOF;
}

static Token *new_token(TokenKind kind, Token *cur, char *str, int len){
    Token *tok = calloc(1, sizeof(Token));
    tok->kind = kind;
    tok->str = str;
    tok->len = len;
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
    head.next = NULL;
    Token *cur = &head;

    while(*p){
        if(isspace(*p)){
            p++;
            continue;
        }

        if(strncmp(p, "==", 2) == 0 || strncmp(p, "!=", 2) == 0 || strncmp(p, "<=", 2) == 0 || strncmp(p, ">=", 2) == 0){
            cur = new_token(TK_RESERVED, cur, p, 2);
            p += 2;
            continue;
        }
        if(strchr("+-*/()><=;{},", *p)){
            cur = new_token(TK_RESERVED, cur, p++, 1);
            continue;
        }
        if(*p == '_' || isalpha(*p)){
            int len = read_ident(p);
            if (is_keyword("return", p, len)){
                cur = new_token(TK_RETURN, cur, p, len);
            }else if (is_keyword("if", p, len)){
                cur = new_token(TK_IF, cur, p, len);
            }else if (is_keyword("else", p, len)){
                cur = new_token(TK_ELSE, cur, p, len);
            }else if (is_keyword("while", p, len)){
                cur = new_token(TK_WHILE, cur, p, len);
            }else if (is_keyword("for", p, len)){
                cur = new_token(TK_FOR, cur, p, len);
            }else if (is_keyword("do", p, len)){
                cur = new_token(TK_DO, cur, p, len);
            }else{
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
