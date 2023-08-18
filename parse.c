#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>
#include "9cc.h"

Node *code[100];

Vector *locals;
int locals_stack_size;
Vector *globals;
int global_size;
Vector *global_string_literals;

Type int_type = { INT, NULL };
Type char_type = { CHAR, NULL };

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
        case ND_STRING_LITERAL: return "ND_STRING_LITERAL";
        case ND_LVAR: return "ND_LVAR";
        case ND_GVAR: return "ND_GVAR";
        case ND_IDENT: return "ND_IDENT";
        case ND_RETURN: return "ND_RETURN";
        case ND_IF: return "ND_IF";
        case ND_WHILE: return "ND_WHILE";
        case ND_FOR: return "ND_FOR";
        case ND_DO: return "ND_DO";
        case ND_COMPOUND: return "ND_COMPOUND";
        case ND_CALL: return "ND_CALL";
        case ND_FUNC_DEF: return "ND_FUNC_DEF";
        case ND_ADDRESS_OF: return "ND_ADDRESS_OF";
        case ND_DEREF: return "ND_DEREF";
        case ND_SIZEOF: return "ND_SIZEOF";
        case ND_DECL_VAR: return "ND_DECL_VAR";
        case ND_TYPE: return "ND_TYPE";
        case ND_INIT: return "ND_INIT";
        case ND_GVAR_DEF: return "ND_GLOBAL_VAR";
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

// エラーの起きた場所を報告するための関数
// 下のようなフォーマットでエラーメッセージを表示する
//
// foo.c:10: x = y + + 5;
//                   ^ 式ではありません
void error_at(char *loc, char *fmt, ...) {
    if(*loc == '\0')loc--;
    // locが含まれている行の開始地点と終了地点を取得
    char *line = loc;
    while (user_input < line && line[-1] != '\n')
        line--;

    char *end = loc;
    while (*end != '\n')
        end++;

    // 見つかった行が全体の何行目なのかを調べる
    int line_num = 1;
    for (char *p = user_input; p < line; p++)
        if (*p == '\n')
            line_num++;

    // 見つかった行を、ファイル名と行番号と一緒に表示
    int indent = fprintf(stderr, "%s:%d: ", filename, line_num);
    fprintf(stderr, "%.*s\n", (int)(end - line), line);

    va_list ap;
    va_start(ap, fmt);

    // エラー箇所を"^"で指し示して、エラーメッセージを表示
    int pos = loc - line + indent;
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

Node *new_node_add(Node *lhs, Node *rhs) {
    Node *node = new_node(ND_ADD, lhs, rhs);
    if(type_is_int(node->lhs->expr_type)){
        if(type_is_int(node->rhs->expr_type)) {
            // int + int
            node->expr_type = type_arithmetic(node->rhs->expr_type, node->lhs->expr_type);
        }else {
            // int + ptr -> ptr
            node->expr_type = node->rhs->expr_type;
        }
    }else{
        if(type_is_int(node->rhs->expr_type)) {
            // ptr + int -> ptr
            node->expr_type = node->lhs->expr_type;
        }else {
            // ptr + ptr -> error
            error_at(token->str, "Add pointer to pointer");
        }
    }
    return node;
}

Node *new_node_arithmetic(NodeKind kind, Node *lhs, Node *rhs){
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    node->expr_type = type_arithmetic(lhs->expr_type, rhs->expr_type);
    return node;
}

Node *new_node_compare(NodeKind kind, Node *lhs, Node *rhs){
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    node->expr_type = type_comparator(lhs->expr_type, rhs->expr_type);
    return node;
}

Node *new_node_num(int val) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->val = val;
    node->expr_type = &int_type;
    return node;
}

Node *new_node_char(int val) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->val = val;
    node->expr_type = &char_type;
    return node;
}

Node *new_node_lvar(LVar *lvar) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_LVAR;
    node->lvar = lvar;
    node->expr_type = lvar->type;
    return node;
}

// translation_unit = function_definition*
void translation_unit() {
    globals = new_vector();
    global_size = 0;
    global_string_literals = new_vector();

    int i = 0;
    while(!at_eof()){
        code[i++] = declarator();
    }
    code[i] = NULL;
}

// declarator = function_definition
//            | global_variable_definition
Node *declarator() {
    Node *type_prefix = type_(expect_type_keyword());
    char *ident;
    int ident_len;
    expect_ident(&ident, &ident_len);

    if(consume("(")) {
        return function_definition(type_prefix, ident, ident_len);
    } else {
        return global_variable_definition(type_prefix, ident, ident_len);
    }
}

// function_definition = type ident "(" ( type ident "," )* ( type ident )? ")" stmt
Node *function_definition(Node *type_prefix, char *ident, int ident_len) {
    Node *node = new_node(ND_FUNC_DEF, NULL, NULL);
    node->func_def_arg_vec = new_vector();
    node->func_def_lvar = new_vector();

    node->func_def_return_type = type_prefix;
    node->func_def_ident = ident;
    node->func_def_ident_len = ident_len;

    locals_stack_size = 0;

    if(!consume(")")){
        while(1){
            FuncDefArg *arg = calloc(1, sizeof(FuncDefArg));
            arg->type = type_(expect_type_keyword());
            expect_ident(&arg->ident, &arg->ident_len);
            
            vector_push(node->func_def_arg_vec, arg);
            if(find_lvar(node->func_def_lvar, arg->ident, arg->ident_len)) {
                error_at(token->str, "Arguments with same name are defined");
            }
            arg->lvar = new_lvar(node->func_def_lvar, arg->ident, arg->ident_len);
            arg->lvar->type = arg->type->type;
            locals_stack_size += type_sizeof(arg->lvar->type);
            if(!consume(",")){
                expect(")");
                break;
            }
        }
    }

    locals = node->func_def_lvar;
    node->lhs = stmt();
    if(node->lhs->kind != ND_COMPOUND) {
        error_at(token->str, "Statement of function definition shall be a compound statement.");
    }

    return node;
}

Node *variable_definition(bool is_global, Node *type_prefix) {
    Node *node = new_node(is_global ? ND_GVAR_DEF : ND_DECL_VAR, NULL, NULL);
    Type *type = type_prefix->type;

    bool empty_num = false;
    if(consume("[")) {
        Type *array_type = calloc(1, sizeof(Type));
        array_type->ty = ARRAY;
        array_type->ptr_to = type;
        array_type->array_size = 0;
        if(!consume("]")){
            int array_size = expect_number();
            array_type->array_size = array_size;
            if(array_size < 0) {
                error_at(token->str, "Negative size array");
            }
            expect("]");
        } else {
            empty_num = true;
        }
        type_prefix->type = type = array_type;
    }
    Node *init_expr = NULL;
    if(consume("=")) {
        init_expr = initializer();
        if((init_expr->kind == ND_INIT) != (type->ty == ARRAY)) {
            error_at(token->str, "Initializer type does not match");
        }
    }
    expect(";");

    if(is_global) {
        node->gvar_def.init_expr = init_expr;
    }else {
        node->decl_var.init_expr = init_expr;
    }

    if(empty_num && init_expr == NULL) {
        error_at(token->str, "Variable with empty array size must has initializer");
    }
    if(init_expr->kind == ND_INIT) {
        Vector *vec = init_expr->init.init_expr;
        if(empty_num) {
            type->array_size = vector_size(vec);
        }
        if(vector_size(vec) > type->array_size) {
            error_at(token->str, "Too many initializer for array size %d", type->array_size);
        }
        for(int i = 0; i < vector_size(vec); i++) {
            Node *elem_node = vector_get(vec, i);
            if(!type_is_same(elem_node->expr_type, type->ptr_to)) {
                error_at(token->str, "Not compatible type");
            }
        }
    }

    return node;
}

// global_variable_definition = type ident ("[" num "]")? ("=" initializer)? ";"
Node *global_variable_definition(Node *type_prefix, char *ident, int ident_len) {
    Node *node = variable_definition(true, type_prefix);

    GVar *gvar = find_gvar(globals, ident, ident_len);
    if(gvar != NULL) {
        error_at(ident, "A global variable with same name is already defined");
    }

    gvar = new_gvar(globals, ident, ident_len, type_prefix->type);
    node->gvar_def.gvar = gvar;
    return node;
}

// initializer = expr
//             | "{" (( expr "," )* expr )? "}"
Node *initializer() {
    if(consume("{")) {
        Node *node = new_node(ND_INIT, NULL, NULL);
        node->init.init_expr = new_vector();
        if(!consume("}")) {
            while(1) {
                vector_push(node->init.init_expr, expr());

                if(consume("}")) {
                    break;
                }
                expect(",");
            }
        }
        return node;
    } else if(consume_kind(TK_STRING_LITERAL)) {
        Node *node = new_node(ND_INIT, NULL, NULL);
        node->init.init_expr = new_vector();
        for(int i = 0; i < prev_token->len; i++) {
            vector_push(node->init.init_expr, new_node_char(prev_token->str[i]));
        }
        vector_push(node->init.init_expr, new_node_char(0));
        return node;
    }
    return expr();
}

// stmt    = expr ";"
//         | "if" "(" expr ")" stmt ("else" stmt)?
//         | "while" "(" expr ")" stmt
//         | "for" "(" expr? ";" expr? ";" expr? ")" stmt
//         | "do" stmt "while" "(" expr ")" ";"
//         | "return" expr ";"
//         | "{" stmt* "}"
//         | type ident ( "[" num "]" )? ( "=" initializer )? ";"
Node *stmt() {
    TokenKind kind;
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
    }else if(consume_type_keyword(&kind)) {
        Node *type_prefix = type_(kind);
        Node *ident_node = ident_();
        Node *node = variable_definition(false, type_prefix);

        if(find_lvar(locals, ident_node->ident.ident, ident_node->ident.ident_len) != NULL){
            error("variable with same name is already defined.");
        }
        node->decl_var.lvar = new_lvar(locals, ident_node->ident.ident, ident_node->ident.ident_len);
        node->decl_var.lvar->type = type_prefix->type;
        locals_stack_size += type_sizeof(node->decl_var.lvar->type);
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
    if(consume("=")) {
        node = new_node(ND_ASSIGN, node, assign());
        node->expr_type = node->lhs->expr_type;
    }
    return node;
}

Node *equality() {
    Node *node = relational();

    for(;;){
        if(consume("=="))
            node = new_node_compare(ND_EQUAL, node, relational());
        else if(consume("!="))
            node = new_node_compare(ND_NOT_EQUAL, node, relational());
        else
            return node;
    }
}

Node *relational() {
    Node *node = add();

    for(;;){
        if(consume("<"))
            node = new_node_compare(ND_LESS, node, add());
        else if(consume("<="))
            node = new_node_compare(ND_LESS_OR_EQUAL, node, add());
        else if(consume(">"))
            node = new_node_compare(ND_GREATER, node, add());
        else if(consume(">="))
            node = new_node_compare(ND_GREATER_OR_EQUAL, node, add());
        else
            return node;
    }
}

Node *add() {
    Node *node = mul();

    for(;;){
        if(consume("+")) {
            node = new_node_add(node, mul());
        } else if(consume("-")) {
            node = new_node(ND_SUB, node, mul());
            if(node->lhs->expr_type->ty == INT){
                if(node->rhs->expr_type->ty == INT) {
                    // int - int
                    node->expr_type = type_arithmetic(node->rhs->expr_type, node->lhs->expr_type);
                }else {
                    // int - ptr -> error
                    error_at(token->str, "Sub pointer from int");
                }
            }else{
                if(node->rhs->expr_type->ty == INT) {
                    // ptr - int -> ptr
                    node->expr_type = node->lhs->expr_type;
                }else {
                    // ptr - ptr -> int
                    node->expr_type = &int_type;
                }
            }
        }
        else
            return node;
    }
}

Node *mul() {
    Node *node = unary();

    for(;;){
        if(consume("*"))
            node = new_node_arithmetic(ND_MUL, node, unary());
        else if(consume("/"))
            node = new_node_arithmetic(ND_DIV, node, unary());
        else
            return node;
    }
}

// unary = "+" primary
//       | "-" primary
//       | "*" unary
//       | "&" unary
//       | "sizeof" unary
//       | primary
Node *unary() {
    if(consume("+"))
        return primary();
    if(consume("-"))
        return new_node_arithmetic(ND_SUB, new_node_num(0), primary());
    if(consume("&")) {
        Node *node = new_node(ND_ADDRESS_OF, unary(), NULL);
        node->expr_type = calloc(1, sizeof(Type));
        node->expr_type->ty = PTR;
        node->expr_type->ptr_to = node->lhs->expr_type;
        return node;
    }
    if(consume("*")) {
        Node *node = new_node(ND_DEREF, unary(), NULL);
        if(node->lhs->expr_type->ty != PTR && node->lhs->expr_type->ty != ARRAY) {
            error_at(token->str, "Dereference non pointer type");
        }
        node->expr_type = node->lhs->expr_type->ptr_to;
        return node;
    }
    if(consume_kind(TK_SIZEOF))
        return new_node_num(type_sizeof(unary()->expr_type));
    return primary();
}

// primary = "(" expr ")"
//         | ident ("(" ( (expr ",")* expr )? ")")?
//         | ident "[" expr "]"
//         | num
//         | string_literal
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
            node->expr_type = &int_type;

            NodeList *arg_tail = &node->call_arg_list;
            if(!consume(")")){
                while(1){
                    NodeList *nodelist = calloc(1, sizeof(NodeList));
                    nodelist->node = expr();
                    arg_tail->next = nodelist;
                    arg_tail = nodelist;
                    if(!consume(",")){
                        expect(")");
                        break;
                    }
                }
            }

            node->call_ident = ident;
            node->call_ident_len = ident_len;
            return node;
        }else if(consume("[")) {
            Node *expr_node = expr();
            expect("]");

            Node *var_node = find_symbol(globals, locals, ident, ident_len);
            if(var_node == NULL) {
                error_at(ident, "identifier is not defined");
            }

            Node *added = new_node_add(var_node, expr_node);

            Node *node = new_node(ND_DEREF, added, NULL);
            node->expr_type = added->expr_type->ptr_to;
            return node;
        }else{
            Node *node = find_symbol(globals, locals, ident, ident_len);
            if(node == NULL) {
                error_at(ident, "identifier is not defined");
            }
            return node;
        }
    }else if(consume_kind(TK_STRING_LITERAL)) {
        StringLiteral *literal = calloc(1, sizeof(StringLiteral));
        literal->str = prev_token->str;
        literal->len = prev_token->len;
        literal->index = vector_size(global_string_literals);
        vector_push(global_string_literals, literal);

        Node *node = new_node(ND_STRING_LITERAL, NULL, NULL);
        node->expr_type = calloc(1, sizeof(Type));
        node->expr_type->ptr_to = &char_type;
        node->expr_type->ty = PTR;
        node->string_literal.literal = literal;
        return node;
    }
    return new_node_num(expect_number());
}

Node *type_(TokenKind kind) {
    Type *cur;
    if(kind == TK_CHAR) {
        cur = &char_type;
    }else{
        cur = &int_type;
    }
    while(consume("*")){
        Type *ty = calloc(1, sizeof(Type));
        ty->ptr_to = cur;
        ty->ty = PTR;
        cur = ty;
    }
    Node *node = new_node(ND_TYPE, NULL, NULL);
    node->type = cur;
    return node;
}

Node *ident_() {
    Node *node = new_node(ND_IDENT, NULL, NULL);
    expect_ident(&node->ident.ident, &node->ident.ident_len);
    return node;
}

/// LVar ///

LVar *find_lvar(Vector *locals, char *ident, int ident_len) {
    for(int i = 0; i < lvar_count(locals); i++){
        LVar *var = vector_get(locals, i);
        if(var->len == ident_len && !memcmp(ident, var->name, var->len)) {
            return var;
        }
    }
    return NULL;
}

LVar *new_lvar(Vector *locals, char *ident, int ident_len) {
    LVar *lvar = calloc(1, sizeof(LVar));
    lvar->name = ident;
    lvar->len = ident_len;
    lvar->offset = locals_stack_size;
    vector_push(locals, lvar);

    return lvar;
}

int lvar_count(Vector *locals) {
    return vector_size(locals);
}

int lvar_stack_size(Vector *locals) {
    int ret = 0;
    for(int i = 0; i < lvar_count(locals); i++){
        LVar *var = vector_get(locals, i);
        ret += type_sizeof(var->type);
    }
    return ret;
}

/// GVar ///

GVar *find_gvar(Vector *globals, char *ident, int ident_len) {
    for(int i = 0; i < vector_size(globals); i++){
        GVar *var = vector_get(globals, i);
        if(var->len == ident_len && !memcmp(ident, var->name, var->len)) {
            return var;
        }
    }
    return NULL;
}

GVar *new_gvar(Vector *globals, char *ident, int ident_len, Type *type) {
    GVar *gvar = calloc(1, sizeof(GVar));
    gvar->name = ident;
    gvar->len = ident_len;
    gvar->type = type;
    vector_push(globals, gvar);

    return gvar;
}

Node *find_symbol(Vector *globals, Vector *locals, char *ident, int ident_len) {
    LVar *lvar = find_lvar(locals, ident, ident_len);
    if(lvar == NULL){
        GVar *gvar = find_gvar(globals, ident, ident_len);
        if(gvar == NULL) {
            return NULL;
        }
        Node *node = new_node(ND_GVAR, NULL, NULL);
        node->gvar.gvar = gvar;
        node->expr_type = gvar->type;
        return node;
    }
    return new_node_lvar(lvar);
}

/// Type ///

int type_sizeof(Type *type) {
    if(type->ty == ARRAY) {
        return type->array_size * type_sizeof(type->ptr_to);
    }
    if(type->ty == CHAR) {
        return 1;
    }
    if(type->ty == INT) {
        return 4;
    }
    return 8;
}

Type *type_arithmetic(Type *type_r, Type *type_l) {
    if(type_r->ty == PTR || type_r->ty == PTR){
        error_at(token->str, "Invalid arithmetic operand with ptr type");
        return NULL;
    }
    return &int_type;
}

Type *type_comparator(Type *type_r, Type *type_l) {
    if(type_r->ty == PTR && type_l->ty != PTR ||
            type_l->ty == PTR && type_r->ty != PTR){
        error_at(token->str, "Invalid comparison between ptr and non-ptr");
        return NULL;
    }
    return &int_type;
}

bool type_implicit_ptr(Type *type) {
    return type->ty == ARRAY || type->ty == PTR;
}

bool type_is_int(Type *type) {
    return type->ty == INT || type->ty == CHAR;
}

bool type_is_basic(Type *type) {
    return type->ty == INT || type->ty == CHAR;
}

bool type_is_same(Type *type_a, Type *type_b) {
    while(1) {
        if(type_is_int(type_a) && type_is_int(type_b)) {
            return true;
        }
        if(type_a->ty != type_b->ty) {
            return false;
        }
        if(type_is_basic(type_a) || type_is_basic(type_b)) {
            return false;
        }
        type_a = type_a->ptr_to;
        type_b = type_b->ptr_to;
    }
}

void dumpnodes_inner(Node *node, int level) {
    if(node == NULL) return;
    fprintf(stderr, "%*s%s\n", (level+1)*2, " ", node_kind(node->kind));

    if(node->kind == ND_LVAR){
        fprintf(stderr, "%*sname: ", (level+1)*2, " ");
        fwrite(node->lvar->name, node->lvar->len, 1, stderr);
        fprintf(stderr, "\n");
    }else if(node->kind == ND_TYPE){
        int ptr_n = 0;
        for(Type *cur = node->type; cur; cur = cur->ptr_to) {
            if(cur->ty == PTR) {
                ptr_n++;
            }else{
                fprintf(stderr, "%*s %s ", (level+1)*2, " ", "int");
                for(int i = 0; i < ptr_n; i++){
                    fprintf(stderr, "*");
                }
                fprintf(stderr, "\n");
            }
        }
    }else if(node->kind == ND_NUM){
        fprintf(stderr, "%*svalue: %d\n", (level+1)*2, " ", node->val);
    }else if(node->kind == ND_STRING_LITERAL){
        fprintf(stderr, "%*svalue: %.*s\n", (level+1)*2, " ", node->string_literal.literal->len, node->string_literal.literal->str);
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
    }else if(node->kind == ND_CALL){
        NodeList *cur = node->call_arg_list.next;
        for(; cur; cur = cur->next){
            fprintf(stderr, "%*s// call arg\n", (level+1)*2, " ");
            dumpnodes_inner(cur->node, level + 1);
        }
    }else{
        dumpnodes_inner(node->lhs, level + 1);
        dumpnodes_inner(node->rhs, level + 1);
    }
}

void dumpnodes(Node *node) {
    dumpnodes_inner(node, 0);
}
