#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>
#include "9cc.h"

Node *code[100];

LVar *locals;
int local_count = 0;

char *node_kind(NodeKind kind){
    switch(kind){
        case ND_ADD: return "ND_ADD";
        case ND_SUB: return "ND_SUB";
        case ND_MUL: return "ND_MUL";
        case ND_DIV: return "ND_DIV";
        case ND_ASSIGN: return "ND_ASSIGN";
        case ND_EQUAL: return "ND_EQUAL";
        case ND_NOT_EQUAL: return "ND_NOT_EQUAL";
        case ND_LESS: return "ND_LESS";
        case ND_LESS_OR_EQUAL: return "ND_LESS_OR_EQUAL";
        case ND_GREATER: return "ND_GREATER";
        case ND_GREATER_OR_EQUAL: return "ND_GREATER_OR_EQUAL";
        case ND_NUM: return "ND_NUM";
        case ND_LVAR: return "ND_LVAR";
        case ND_RETURN: return "ND_RETURN";
        case ND_IF: return "ND_IF";
        case ND_WHILE: return "ND_WHILE";
        case ND_FOR: return "ND_FOR";
        case ND_DO: return "ND_DO";
        case ND_COMPOUND: return "ND_COMPOUND";
        default: assert(false);
    }
}

void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void error_at(char *loc, char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    int pos = loc - user_input;
    fprintf(stderr, "%s\n", user_input);
    fprintf(stderr, "%*s", pos, " ");
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

Node *new_node(NodeKind kind, Node *lhs, Node *rhs){
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node *new_node_num(int val) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->val = val;
    return node;
}

Node *new_node_ident(LVar *lvar) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_LVAR;
    node->lvar = lvar;
    return node;
}

// program = stmt*
void program() {
    int i = 0;
    while(!at_eof()){
        code[i++] = stmt();
    }
    code[i] = NULL;
}

// stmt    = expr ";"
//         | "if" "(" expr ")" stmt ("else" stmt)?
//         | "while" "(" expr ")" stmt
//         | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//         | "do" stmt "while" "(" expr ")" ";"
//         | "return" expr ";"
//         | "{" stmt* "}"
Node *stmt() {
    if(consume_kind(TK_IF)) {
        expect("(");
        Node *if_expr = expr();
        expect(")");
        Node *if_stmt = stmt();
        Node *else_stmt = NULL;
        if(consume_kind(TK_ELSE)){
            else_stmt = stmt();
        }
        Node *node = new_node(ND_IF, if_expr, if_stmt);
        node->else_stmt = else_stmt;
        return node;
    }else if(consume_kind(TK_WHILE)) {
        expect("(");
        Node *while_expr = expr();
        expect(")");
        Node *while_stmt = stmt();
        return new_node(ND_WHILE, while_expr, while_stmt);
    }else if(consume_kind(TK_FOR)) {
        expect("(");
        Node *for_init_expr = NULL;
        Node *for_condition_expr = NULL;
        Node *for_update_expr = NULL;
        if(!consume(";")){
            for_init_expr = expr();
            expect(";");
        }
        if(!consume(";")){
            for_condition_expr = expr();
            expect(";");
        }
        if(!consume(")")){
            for_update_expr = expr();
            expect(")");
        }
        Node *node = new_node(ND_FOR, for_init_expr, for_condition_expr);
        node->for_update_expr = for_update_expr;
        node->for_stmt = stmt();
        return node;
    }else if(consume_kind(TK_DO)) {
        Node *do_stmt = stmt();
        expect_kind(TK_WHILE);
        expect("(");
        Node *do_expr = expr();
        expect(")");
        Node *node = new_node(ND_DO, do_stmt, do_expr);
        expect(";");
        return node;
    }else if(consume_kind(TK_RETURN)) {
        Node *node = new_node(ND_RETURN, expr(), NULL);
        expect(";");
        return node;
    }else if(consume("{")) {
        Node *node = new_node(ND_COMPOUND, NULL, NULL);
        int max_compound_stmt = 100;
        node->compound_stmt_list = calloc(max_compound_stmt, sizeof(Node*));

        int i = 0;
        while(!consume("}")){
            node->compound_stmt_list[i] = stmt();
            i++;
            if(i >= max_compound_stmt){
                error("Too large compound statement. max=%d", max_compound_stmt);
            }
        }
        return node;
    }
    Node *node = expr();
    expect(";");
    return node;
}

Node *expr() {
    return assign();
}

Node *assign() {
    Node *node = equality();
    if(consume("="))
        node = new_node(ND_ASSIGN, node, assign());
    return node;
}

Node *equality() {
    Node *node = relational();

    for(;;){
        if(consume("=="))
            node = new_node(ND_EQUAL, node, relational());
        else if(consume("!="))
            node = new_node(ND_NOT_EQUAL, node, relational());
        else
            return node;
    }
}

Node *relational() {
    Node *node = add();

    for(;;){
        if(consume("<"))
            node = new_node(ND_LESS, node, add());
        else if(consume("<="))
            node = new_node(ND_LESS_OR_EQUAL, node, add());
        else if(consume(">"))
            node = new_node(ND_GREATER, node, add());
        else if(consume(">="))
            node = new_node(ND_GREATER_OR_EQUAL, node, add());
        else
            return node;
    }
}

Node *add() {
    Node *node = mul();

    for(;;){
        if(consume("+"))
            node = new_node(ND_ADD, node, mul());
        else if(consume("-"))
            node = new_node(ND_SUB, node, mul());
        else
            return node;
    }
}

Node *mul() {
    Node *node = unary();

    for(;;){
        if(consume("*"))
            node = new_node(ND_MUL, node, unary());
        else if(consume("/"))
            node = new_node(ND_DIV, node, unary());
        else
            return node;
    }
}

Node *unary() {
    if(consume("+"))
        return primary();
    if(consume("-"))
        return new_node(ND_SUB, new_node_num(0), primary());
    return primary();
}

// primary = "(" expr ")"
//         | ident ("(" ")")?
//         | num
Node *primary() {
    if(consume("(")){
        Node *node = expr();
        expect(")");
        return node;
    }
    char *ident;
    int ident_len;
    if(consume_ident(&ident, &ident_len)){
        if(consume("(")){
            Node *node = new_node(ND_CALL, NULL, NULL);

            node->call_ident = ident;
            node->call_ident_len = ident_len;
            expect(")");
            return node;
        }else{
            LVar *lvar = find_lvar(ident, ident_len);
            if(lvar == NULL){
                lvar = new_lvar(ident, ident_len);
            }
            Node *node = new_node_ident(lvar);
            return node;
        }
    }
    return new_node_num(expect_number());
}

/// LVar ///

LVar *find_lvar(char *ident, int ident_len) {
    for(LVar *var = locals; var; var = var->next) {
        if(var->len == ident_len && !memcmp(ident, var->name, var->len)) {
            return var;
        }
    }
    return NULL;
}

LVar *new_lvar(char *ident, int ident_len) {
    LVar *lvar = calloc(1, sizeof(LVar));
    lvar->name = ident;
    lvar->len = ident_len;
    local_count++;
    lvar->offset = 8*local_count;
    lvar->next = locals;
    locals = lvar;
    return lvar;
}

int lvar_count(LVar *locals) {
    return local_count;
}

void dumpnodes_inner(Node *node, int level) {
    if(node == NULL) return;
    fprintf(stderr, "%*s%s\n", (level+1)*2, " ", node_kind(node->kind));

    if(node->kind == ND_LVAR){
        fprintf(stderr, "%*sname: ", (level+1)*2, " ");
        fwrite(node->lvar->name, node->lvar->len, 1, stderr);
        fprintf(stderr, "\n");
    }else if(node->kind == ND_IF){
        fprintf(stderr, "%*s// if condition\n", (level+1)*2, " ");
        dumpnodes_inner(node->lhs, level + 1);
        fprintf(stderr, "%*s// if stmt\n", (level+1)*2, " ");
        dumpnodes_inner(node->rhs, level + 1);
        fprintf(stderr, "%*s// else\n", (level+1)*2, " ");
        dumpnodes_inner(node->else_stmt, level + 1);
    }else if(node->kind == ND_FOR){
        fprintf(stderr, "%*s// for init\n", (level+1)*2, " ");
        dumpnodes_inner(node->lhs, level + 1);
        fprintf(stderr, "%*s// for condition\n", (level+1)*2, " ");
        dumpnodes_inner(node->rhs, level + 1);
        fprintf(stderr, "%*s// for update\n", (level+1)*2, " ");
        dumpnodes_inner(node->for_update_expr, level + 1);
        fprintf(stderr, "%*s// for body\n", (level+1)*2, " ");
        dumpnodes_inner(node->for_stmt, level + 1);
    }else if(node->kind == ND_COMPOUND){
        for(int i = 0; node->compound_stmt_list[i]; i++){
            dumpnodes_inner(node->compound_stmt_list[i], level + 1);
        }
    }else{
        dumpnodes_inner(node->lhs, level + 1);
        dumpnodes_inner(node->rhs, level + 1);
    }
}

void dumpnodes(Node *node) {
    dumpnodes_inner(node, 0);
}
