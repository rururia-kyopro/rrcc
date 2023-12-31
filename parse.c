#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <assert.h>
#include "rrcc.h"

Node *scope;
int locals_stack_size;
int max_stack_size;
Vector *globals;
int global_size;
Vector *global_string_literals;
Vector *struct_registry;
Vector *union_registry;
Vector *enum_registry;
Vector *typedef_registry;
Vector *switch_stack;
Vector *break_targets;
Vector *continue_targets;
Node *current_func;
int unnamed_struct_count = 0;

Type void_type = { VOID };
Type char_type = { CHAR, NOSIGNED };
Type unsigned_char_type = { CHAR, UNSIGNED };
Type signed_char_type = { CHAR, SIGNED };
Type signed_short_type = { SHORT, SIGNED };
Type unsigned_short_type = { SHORT, UNSIGNED };
Type signed_int_type = { INT, SIGNED };
Type unsigned_int_type = { INT, UNSIGNED };
Type signed_long_type = { LONG, SIGNED };
Type unsigned_long_type = { LONG, UNSIGNED };
Type signed_longlong_type = { LONGLONG, SIGNED };
Type unsigned_longlong_type = { LONGLONG, UNSIGNED };
Type float_type = { FLOAT, UNSIGNED };
Type double_type = { DOUBLE, UNSIGNED };
Type longdouble_type = { LONGDOUBLE, UNSIGNED };
Type complex_type = { COMPLEX, NOSIGNED };
Type bool_type = { BOOL, NOSIGNED };

char *node_kind(NodeKind kind){
    switch(kind){
        case ND_TRANS_UNIT: return "ND_TRANS_UNIT";
        case ND_ADD: return "ND_ADD";
        case ND_SUB: return "ND_SUB";
        case ND_MUL: return "ND_MUL";
        case ND_DIV: return "ND_DIV";
        case ND_MOD: return "ND_MOD";
        case ND_LSHIFT: return "ND_LSHIFT";
        case ND_RSHIFT: return "ND_RSHIFT";
        case ND_OR: return "ND_OR";
        case ND_XOR: return "ND_XOR";
        case ND_AND: return "ND_AND";
        case ND_ASSIGN: return "ND_ASSIGN";
        case ND_EQUAL: return "ND_EQUAL";
        case ND_NOT_EQUAL: return "ND_NOT_EQUAL";
        case ND_LESS: return "ND_LESS";
        case ND_LESS_OR_EQUAL: return "ND_LESS_OR_EQUAL";
        case ND_GREATER: return "ND_GREATER";
        case ND_GREATER_OR_EQUAL: return "ND_GREATER_OR_EQUAL";
        case ND_LOGICAL_OR: return "ND_LOGICAL_OR";
        case ND_LOGICAL_AND: return "ND_LOGICAL_AND";
        case ND_CAST: return "ND_CAST";
        case ND_NUM: return "ND_NUM";
        case ND_STRING_LITERAL: return "ND_STRING_LITERAL";
        case ND_COMMA_EXPR: return "ND_COMMA_EXPR";
        case ND_LVAR: return "ND_LVAR";
        case ND_GVAR: return "ND_GVAR";
        case ND_IDENT: return "ND_IDENT";
        case ND_RETURN: return "ND_RETURN";
        case ND_IF: return "ND_IF";
        case ND_SWITCH: return "ND_SWITCH";
        case ND_CASE: return "ND_CASE";
        case ND_DEFAULT: return "ND_DEFAULT";
        case ND_BREAK: return "ND_BREAK";
        case ND_CONTINUE: return "ND_CONTINUE";
        case ND_WHILE: return "ND_WHILE";
        case ND_FOR: return "ND_FOR";
        case ND_DO: return "ND_DO";
        case ND_COMPOUND: return "ND_COMPOUND";
        case ND_CALL: return "ND_CALL";
        case ND_POSTFIX_INC: return "ND_POSTFIX_INC";
        case ND_POSTFIX_DEC: return "ND_POSTFIX_DEC";
        case ND_PREFIX_INC: return "ND_PREFIX_INC";
        case ND_PREFIX_DEC: return "ND_PREFIX_DEC";
        case ND_FUNC_DEF: return "ND_FUNC_DEF";
        case ND_FUNC_DECL: return "ND_FUNC_DECL";
        case ND_SCOPE: return "ND_SCOPE";
        case ND_DECL_LIST: return "ND_DECL_LIST";
        case ND_ADDRESS_OF: return "ND_ADDRESS_OF";
        case ND_DEREF: return "ND_DEREF";
        case ND_BIT_NOT: return "ND_BIT_NOT";
        case ND_SIZEOF: return "ND_SIZEOF";
        case ND_DECL_VAR: return "ND_DECL_VAR";
        case ND_TYPE: return "ND_TYPE";
        case ND_INIT: return "ND_INIT";
        case ND_CONVERT: return "ND_CONVERT";
        case ND_GVAR_DEF: return "ND_GVAR_DEF";
        case ND_TYPE_POINTER: return "ND_TYPE_POINTER";
        case ND_TYPE_ARRAY: return "ND_TYPE_ARRAY";
        case ND_TYPE_FUNC: return "ND_TYPE_FUNC";
        case ND_TYPE_STRUCT: return "ND_TYPE_STRUCT";
        case ND_TYPE_ENUM: return "ND_TYPE_ENUM";
        case ND_TYPE_TYPEDEF: return "ND_TYPE_TYPEDEF";
        case ND_TYPE_EXTERN: return "ND_TYPE_EXTERN";
        default: assert(false);
    }
}

void error(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    fprintf(stderr, "Error: %s\n", filename);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void debug_log(char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

// エラーの起きた場所を報告するための関数
// 下のようなフォーマットでエラーメッセージを表示する
//
// foo.c:10: x = y + + 5;
//                   ^ 式ではありません
void error_at(char *loc, char *fmt, ...) {
    print_current_position(loc);

    va_list ap;
    va_start(ap, fmt);

    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void print_current_position(char *loc) {
    if(*loc == '\0')loc--;

    char *line = loc;
    while (user_input < line && line[-1] != '\n')
        line--;

    char *end = loc;
    while (*end != '\n')
        end++;

    char *last_line = line - 1;
    while (user_input < last_line && last_line[-1] != '\n')
        last_line--;

    // 見つかった行が全体の何行目なのかを調べる
    int line_num = 1;
    for (char *p = user_input; p < line; p++)
        if (*p == '\n')
            line_num++;

    fprintf(stderr, "%.*s", (int)(line - last_line), last_line);

    // 見つかった行を、ファイル名と行番号と一緒に表示
    int indent = fprintf(stderr, "%s:%d: ", filename, line_num);
    fprintf(stderr, "%.*s\n", (int)(end - line), line);

    // エラー箇所を"^"で指し示して、エラーメッセージを表示
    int pos = loc - line + indent;
    fprintf(stderr, "%*s", pos, " ");
    fprintf(stderr, "^ ");
}

Node *new_node(NodeKind kind, Node *lhs, Node *rhs){
    Node *node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    return node;
}

Node *new_node_ptr_offset(NodeKind kind, Node *lhs, Node *rhs) {
    Type *t = lhs->expr_type->ptr_to;
    // First, convert integer to long (have the same size as pointer)
    rhs = new_node_conv(rhs, &signed_long_type);

    // Scale factor also is of long type
    Node *scale_node = new_node_num(type_sizeof(t));
    scale_node->expr_type = &signed_long_type;

    // Multiply. i * size
    rhs = new_node(ND_MUL, rhs, scale_node);
    rhs->expr_type = &signed_long_type;

    lhs = new_node(ND_CONVERT, lhs, NULL);
    lhs->expr_type = type_new_ptr(&void_type);

    Node *node = new_node(kind, lhs, rhs);
    node->expr_type = calloc(1, sizeof(Type));
    node->expr_type->ty = PTR;
    node->expr_type->ptr_to = t;
    return node;
}

void conv_binop_node(Node *node) {
    if(!type_is_same(node->rhs->expr_type, node->expr_type)) {
        node->rhs = new_node_conv(node->rhs, node->expr_type);
    }
    if(!type_is_same(node->lhs->expr_type, node->expr_type)) {
        node->lhs = new_node_conv(node->lhs, node->expr_type);
    }
}

Node *new_node_add(Node *lhs, Node *rhs) {
    if(type_is_ptr_array(rhs->expr_type)) {
        if(type_is_ptr_array(lhs->expr_type)) {
            error_at(token->str, "Invalid addition for pointer/array");
        }
        Node *tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }

    if(type_is_ptr_array(lhs->expr_type)) {
        // array_or_ptr + int -> ptr. array will be implicitly converted to ptr.
        // (type*)((void *)array_or_ptr + (long)i * size)
        return new_node_ptr_offset(ND_ADD, lhs, rhs);
    }
    // int + int
    Node *node = new_node(ND_ADD, lhs, rhs);
    node->expr_type = type_arithmetic(node->rhs->expr_type, node->lhs->expr_type);
    conv_binop_node(node);
    return node;
}

Node *new_node_sub(Node *lhs, Node *rhs) {
    if(type_is_ptr_array(rhs->expr_type)) {
        if(type_is_ptr_array(lhs->expr_type)) {
            if(!type_is_same(rhs->expr_type, lhs->expr_type)) {
                error_at(token->str, "Invalid subtract: incompatible pointers");
                return NULL;
            }
            Node *node = new_node(ND_SUB, lhs, rhs);
            node->expr_type = &signed_long_type;
            node = new_node(ND_DIV, node, new_node_num(type_sizeof(rhs->expr_type->ptr_to)));
            node->rhs->expr_type = &signed_long_type;
            node->expr_type = &signed_long_type;
            return node;
        }
        error_at(token->str, "Invalid subtract integer - ptr");
        return NULL;
    }
    if(type_is_ptr_array(lhs->expr_type)) {
        return new_node_ptr_offset(ND_SUB, lhs, rhs);
    }
    // int - int
    Node *node = new_node(ND_SUB, lhs, rhs);
    node->expr_type = type_arithmetic(node->rhs->expr_type, node->lhs->expr_type);
    conv_binop_node(node);
    return node;
}

Node *new_node_binop(NodeKind kind, Node *lhs, Node *rhs){
    lhs = apply_int_promotion(lhs);
    rhs = apply_int_promotion(rhs);

    Node *node;
    Type *target_type;
    if(kind == ND_ADD) {
        return new_node_add(lhs, rhs);
    }else if(kind == ND_SUB) {
        return new_node_sub(lhs, rhs);
    }
    node = calloc(1, sizeof(Node));
    node->kind = kind;
    node->lhs = lhs;
    node->rhs = rhs;
    switch(kind) {
        case ND_MUL:
        case ND_DIV:
        case ND_MOD:
            node->expr_type = type_arithmetic(lhs->expr_type, rhs->expr_type);
            target_type = node->expr_type;
            break;
        case ND_EQUAL:
        case ND_NOT_EQUAL:
        case ND_LESS:
        case ND_LESS_OR_EQUAL:
        case ND_GREATER:
        case ND_GREATER_OR_EQUAL:
            return type_comparator(node, lhs->expr_type, rhs->expr_type);
        case ND_OR:
        case ND_XOR:
        case ND_AND:
            target_type = node->expr_type = type_bitwise(lhs->expr_type, rhs->expr_type);
            break;
        case ND_LSHIFT:
        case ND_RSHIFT:
            target_type = node->expr_type = type_shift(lhs->expr_type, rhs->expr_type);
            break;
    }
    conv_binop_node(node);
    return node;
}

Node *new_node_num_suffix(unsigned long val, NumSuffix suffix) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->val = val;
    unsigned long intmax = (1UL << 31) - 1;
    unsigned long uintmax = (1UL << 32) - 1;
    unsigned long longmax = (1UL << 63) - 1;
    if(suffix == SUF_NONE && val <= intmax) {
        node->expr_type = &signed_int_type;
    } else if((suffix == SUF_NONE || suffix == SUF_U) && val <= uintmax) {
        node->expr_type = &unsigned_int_type;
    } else if((suffix == SUF_NONE || suffix == SUF_L) && val <= longmax) {
        node->expr_type = &signed_long_type;
    } else if(suffix != SUF_ULL && suffix != SUF_LL) {
        node->expr_type = &unsigned_long_type;
    } else {
        node->expr_type = &unsigned_longlong_type;
    }
    return node;
}

Node *new_node_num(unsigned long val) {
    return new_node_num_suffix(val, SUF_NONE);
}

Node *new_node_char(int val) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_NUM;
    node->val = val;
    node->expr_type = &char_type;
    return node;
}

Node *new_node_scope(Node **scope) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_SCOPE;
    node->scope.childs = new_vector();
    node->scope.locals = new_vector();
    node->scope.current = 0;
    node->scope.parent = *scope;
    if(*scope) {
        vector_push((*scope)->scope.childs, node);
    }
    *scope = node;
    return node;
}

void end_scope(Node **scope) {
    assert(*scope);
    if(max_stack_size < locals_stack_size) {
        max_stack_size = locals_stack_size;
    }
    locals_stack_size -= (*scope)->scope.current;
    *scope = (*scope)->scope.parent;
}

Node *new_node_lvar(LVar *lvar) {
    Node *node = calloc(1, sizeof(Node));
    node->kind = ND_LVAR;
    node->lvar = lvar;
    node->expr_type = lvar->type;
    return node;
}

Node *new_node_conv(Node *node, Type *new_type) {
    node = new_node(ND_CONVERT, node, NULL);
    node->expr_type = new_type;
    return node;
}

Node *new_node_assignment(Node *lhs, Node *rhs) {
    Type *f_type = rhs->expr_type;
    Type *t_type = lhs->expr_type;
    Node *node = new_node(ND_ASSIGN, lhs, rhs);
    if(type_is_arithmetic(f_type) && type_is_arithmetic(t_type)) {
        node->rhs = new_node_conv(node->rhs, t_type);
    }else if((t_type->ty == STRUCT || t_type->ty == UNION) && type_is_same(t_type, f_type)) {
    }else if(t_type->ty == PTR && type_is_same(t_type, f_type)) {
    }else if(t_type->ty == PTR && f_type->ty == PTR && (f_type->ptr_to->ty == VOID || f_type->ptr_to->ty == VOID)) {
    }else if(t_type->ty == PTR && rhs->kind == ND_NUM && rhs->val == 0) {
        node->rhs = new_node_conv(node->rhs, t_type);
    }else if(t_type->ty == BOOL && f_type->ty == PTR) {
        node->rhs = new_node_conv(node->rhs, t_type);
    }
    node->expr_type = node->lhs->expr_type;
    return node;
}

Node *apply_int_promotion(Node *node) {
    if(type_is_int(node->expr_type)) {
        if(type_int_conv_rank(&signed_int_type) >= type_int_conv_rank(node->expr_type)) {
            if(node->expr_type->ty == INT && node->expr_type->signedness == UNSIGNED) {
                return new_node_conv(node, &unsigned_int_type);
            }
            if(node->expr_type->ty == INT && node->expr_type->signedness == SIGNED) {
                return node;
            }
            return new_node_conv(node, &signed_int_type);
        }
    }
    return node;
}

void define_builtin_one(char *funcname, Type *type) {
    GVar *gvar = calloc(1, sizeof(GVar));
    gvar->name = funcname;
    gvar->len = strlen(gvar->name);
    gvar->has_definition = true;
    gvar->type = type;
    gvar->is_builtin = true;
    vector_push(globals, gvar);
}

StructMember* create_struct_member(Type* type, char* ident, size_t offset) {
    StructMember* member = calloc(1, sizeof(StructMember));
    member->type = type;
    member->ident = ident;
    member->ident_len = strlen(ident);
    member->offset = offset;
    return member;
}

Type* create_va_list_struct() {
    Type* va_list_struct = type_new_struct("__builtin_va_list_tag", strlen("__builtin_va_list_tag"));
    va_list_struct->members = new_vector();
    va_list_struct->struct_complete = true;
    va_list_struct->struct_size = 24;

    vector_push(va_list_struct->members, create_struct_member(&unsigned_int_type, "gp_offset", 0));
    vector_push(va_list_struct->members, create_struct_member(&unsigned_int_type, "fp_offset", 4));
    vector_push(va_list_struct->members, create_struct_member(type_new_ptr(&void_type), "overflow_arg_area", 8));
    vector_push(va_list_struct->members, create_struct_member(type_new_ptr(&void_type), "reg_save_area", 16));

    return va_list_struct;
}

void define_builtins() {
    Vector *args = new_vector();
    vector_push(args, type_new_ptr(&void_type));
    vector_push(args, type_new_ptr(&void_type));
    define_builtin_one("__builtin_va_start", type_new_func(&void_type, args, false));

    args = new_vector();
    define_builtin_one("__builtin_va_end", type_new_func(&void_type, args, false));

    TypedefRegistryEntry *entry = calloc(1, sizeof(TypedefRegistryEntry));
    entry->ident = "__builtin_va_list";
    entry->ident_len = strlen(entry->ident);
    entry->type = type_new_array(create_va_list_struct(), true, 1);
    vector_push(typedef_registry, entry);
}

// translation_unit = function_definition*
Node *translation_unit() {
    globals = new_vector();
    global_size = 0;
    global_string_literals = new_vector();
    struct_registry = new_vector();
    union_registry = new_vector();
    enum_registry = new_vector();
    typedef_registry = new_vector();
    switch_stack = new_vector();
    break_targets = new_vector();
    continue_targets = new_vector();

    define_builtins();

    Node *node = new_node(ND_TRANS_UNIT, NULL, NULL);
    node->trans_unit.decl = new_vector();

    while(!at_eof()){
        //debug_log("next: %d %s\n", token->kind, token->str);
        vector_push(node->trans_unit.decl, external_declaration());
    }
    return node;
}

// TODO: function prototype?
// TODO: extern variable?
// declarator = function_definition
//            | global_variable_definition
//
Node *external_declaration() {
    return type_(true, true, false);
}

// function_definition = type ident "(" ( type ident "," )* ( type ident )? ")" stmt
Node *function_definition(TypeStorage type_storage, Node *type_node, bool is_inline) {
    Node *node = new_node(ND_FUNC_DEF, NULL, NULL);
    node->line_info = token->line_info;
    current_func = node;
    node->func_def.arg_vec = new_vector();
    node->func_def.is_inline = is_inline;
    node->func_def.type_storage = type_storage;

    Type *type = node->func_def.type = type_node->type.type;
    if(!type_find_ident(type_node, &node->func_def.ident, &node->func_def.ident_len)) {
        error_at(token->str, "Function definition must have identifier");
    }

    // ND_FUNC_DEF -> ND_SCOPE -> ND_COMPOUND
    Node *new_scope = new_node_scope(&scope);
    node->lhs = new_scope;

    // Assigns all space for local variables in function prologue.
    // It also include sub-scope introduced by compound statements.
    // max_stack_size tracks deepest stack size along with all scopes.
    // This size doesn't add up sub-scopes in the same level,
    // because they don't use variables simultaneously.
    // e.g.
    // int func() {
    //    if(...){
    //        int a;
    //    }
    //    if(...){
    //        long b;
    //    }
    // }
    // In above example, variable a and b can share same stack address because both aren't used simultaneously.
    // So, in this case max_stack_size equals max(sizeof(a), sizeof(b))
    assert(locals_stack_size == 0);
    max_stack_size = 0;

    for(int i = 0; i < vector_size(type->args); i++) {
        Node *arg_lvar_node = vector_get(type->args, i);
        Node *arg_type_node = arg_lvar_node->lhs;
        Type *arg_type = arg_type_node->type.type;
        // Array in function argument is treated as pointer.
        if(arg_type->ty == ARRAY) {
            arg_type = type_new_ptr(arg_type->ptr_to);
        }

        FuncDefArg *arg = calloc(1, sizeof(FuncDefArg));
        arg->index = i;
        arg->type = arg_type;
        if(!type_find_ident(arg_type_node, &arg->ident, &arg->ident_len)) {
            error_at(token->str, "Function argument must have identifier");
        }

        vector_push(node->func_def.arg_vec, arg);
        if(find_lvar_one(scope->scope.locals, arg->ident, arg->ident_len)) {
            error_at(token->str, "Arguments with same name are defined: %.*s", arg->ident_len, arg->ident);
        }
        arg->lvar = new_lvar(scope->scope.locals, arg->ident, arg->ident_len);
        arg->lvar->func_arg_index = i + 1;
        arg->lvar->type = arg_type;

        int size = type_sizeof(arg->lvar->type);

        scope->scope.current += size;
        locals_stack_size += size;
    }

    Node *gvar_def_node = new_node(ND_GVAR_DEF, NULL, NULL);

    GVar *gvar = find_gvar(globals, node->func_def.ident, node->func_def.ident_len);
    if(gvar != NULL) {
        //error_at(token->str, "A global variable with same name is already defined");
    }

    gvar = new_gvar(globals, node->func_def.ident, node->func_def.ident_len, type_node->type.type, true);
    gvar_def_node->gvar_def.gvar = gvar;

    new_scope->lhs = stmt();
    if(new_scope->lhs->kind != ND_SCOPE || new_scope->lhs->lhs->kind != ND_COMPOUND) {
        error_at(token->str, "Statement of function definition shall be a compound statement.");
    }
    end_scope(&scope);
    node->func_def.max_stack_size = max_stack_size;

    return node;
}

Node *variable_definition(bool is_global, Node *type_node, TypeStorage type_storage) {
    Type *type = type_node->type.type;
    Node *node = new_node(is_global ? ND_GVAR_DEF : ND_DECL_VAR, type_node, NULL);

    Node *init_expr = NULL;
    if(consume("=")) {
        if(type->ty == FUNC) {
            error_at(token->str, "Function type cannot have initializer");
        }
        if(type_storage == TS_TYPEDEF) {
            error_at(token->str, "typedef cannot have initializer");
        }
        if(type_storage == TS_EXTERN) {
            error("extern variable cannot have initializer");
        }
        init_expr = initializer(type);
        if((init_expr->kind == ND_INIT) != (type->ty == ARRAY || type->ty == STRUCT)) {
            error_at(token->str, "Initializer type does not match");
        }
    }

    if(type_storage == TS_EXTERN) {
        char *ident;
        int ident_len;
        if(!type_find_ident(node, &ident, &ident_len)) {
            error("No identifier on extern variable");
        }
        global_variable_definition(node, ident, ident_len, false);

        return new_node(ND_TYPE_EXTERN, node, NULL);
    }
    if(type_storage == TS_TYPEDEF) {
        return typedef_declaration(is_global, type_node);
    }
    if(type->ty == STRUCT && !type->struct_complete) {
        error_at(token->str, "Cannot define variable with incomplete type (struct): %.*s", type->ident_len, type->ident);
    }
    if(is_global) {
        node->gvar_def.init_expr = init_expr;
        Node *cur = node;
        // debug_log("global node.");
        bool found = false;
        for(; cur != NULL; cur = cur->lhs) {
            if(cur->kind == ND_IDENT) {
                found = true;
                global_variable_definition(node, cur->ident.ident, cur->ident.ident_len, true);
                break;
            }
        }
        if (!found && type->ty != STRUCT && type->ty != ENUM) {
            error_at(token->str, "No identifier found for global variable definition");
        }
        if(!found && (type->ty == STRUCT || type->ty == ENUM)) {
            return type_node;
        }
    }else {
        node->decl_var.init_expr = init_expr;
    }

    // Complete array size
    if(init_expr && init_expr->kind == ND_INIT) {
        Vector *vec = init_expr->init.init_expr;
        if(type->ty == ARRAY) {
            if(!type->has_array_size) {
                type->array_size = vector_size(vec);
                type->has_array_size = true;
            }
        }
    }

    return node;
}

// global_variable_definition = type ("=" initializer)? ";"
Node *global_variable_definition(Node *node, char *ident, int ident_len, bool has_definition) {
    GVar *gvar = find_gvar(globals, ident, ident_len);
    if(gvar != NULL) {
        if(!gvar->has_definition) {
            node->gvar_def.gvar = gvar;
            return NULL;
        }
        error_at(ident, "A global variable with same name is already defined");
    }
    // debug_log("global def: %p", node->lhs->type.type);

    gvar = new_gvar(globals, ident, ident_len, node->lhs->type.type, has_definition);
    node->gvar_def.gvar = gvar;
    return node;
}

Node *local_variable_definition() {
    Node *decl_list = type_(true, false, false);
    if(decl_list->kind != ND_DECL_LIST) {
        error_at(token->str, "Function was defined in non global scope");
    }

    Node *decl_list_local = new_node(ND_DECL_LIST_LOCAL, NULL, NULL);
    decl_list_local->decl_list_local.decls = new_vector();

    for(int i = 0; i < vector_size(decl_list->decl_list.decls); i++) {
        Node *node = vector_get(decl_list->decl_list.decls, i);
        Node *type_node = node->lhs;
        char *ident;
        int ident_len;
        if(!type_find_ident(node, &ident, &ident_len)) {
            error("Local variable must have identifier");
        }

        if(find_lvar_one(scope->scope.locals, ident, ident_len) != NULL){
            error("variable with same name is already defined.");
        }
        node->decl_var.lvar = new_lvar(scope->scope.locals, ident, ident_len);
        node->decl_var.lvar->type = type_node->type.type;
        int size = type_sizeof(node->decl_var.lvar->type);
        scope->scope.current += size;
        locals_stack_size += size;

        Type *type = node->decl_var.lvar->type;
        Node *init_expr = node->decl_var.init_expr;
        if(init_expr) {
            Node *lvar_init_node = lvar_initializer_node(node->decl_var.lvar, 0, init_expr, type);
            node->rhs = lvar_init_node;
        }
        vector_push(decl_list_local->decl_list_local.decls, node);
    }
    return decl_list_local;
}

// initializer = expr
//             | "{" (( expr "," )* expr )? "}"
Node *initializer(Type *type) {
    if(consume("{")) {
        Node *node = new_node(ND_INIT, NULL, NULL);
        node->init.init_expr = new_vector();
        if(type->ty != ARRAY && type->ty != STRUCT) {
            error_at(token->str, "initializer list doesn't match to type");
        }
        if(!consume("}")) {
            int i = 0;
            while(1) {
                Type *child_type;
                if(type->ty == ARRAY){
                    if(type->has_array_size && i >= type->array_size) {
                        error_at(token->str, "Too many initializer for array");
                    }
                    child_type = type->ptr_to;
                }else {
                    if(i >= vector_size(type->members)) {
                        error_at(token->str, "Too many initializer for struct");
                    }
                    StructMember *member = vector_get(type->members, i);
                    child_type = member->type;
                }
                vector_push(node->init.init_expr, initializer(child_type));

                if(consume("}")) {
                    break;
                }
                expect(",");
                // Trailing comma
                if(consume("}")) {
                    break;
                }
                i++;
            }
        }
        return node;
    } else if(consume_kind(TK_STRING_LITERAL)) {
        unget_token();
        Node *node;
        if(type->ty == ARRAY) {
            node = new_node(ND_INIT, NULL, NULL);
            node->init.init_expr = new_vector();
            for(int i = 0; i < vector_size(token->literal); i++) {
                vector_push(node->init.init_expr, new_node_char((char)(long)vector_get(token->literal, i)));
            }
            next_token();
            vector_push(node->init.init_expr, new_node_char(0));
        }else {
            node = primary_expression();
        }
        return node;
    }
    return assignment_expression();
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
    Token *tok = token;
    TokenKind kind;
    if(consume_kind(TK_IF)) {
        expect("(");
        Node *if_expr = expression();
        if_expr->line_info = tok->line_info;
        expect(")");
        Node *if_stmt = stmt();
        Node *else_stmt = NULL;
        if(consume_kind(TK_ELSE)){
            else_stmt = stmt();
        }
        Node *node = new_node(ND_IF, if_expr, if_stmt);
        node->else_stmt = else_stmt;
        return node;
    }else if(consume_kind(TK_SWITCH)) {
        expect("(");
        Node *switch_expr = expression();
        switch_expr->line_info = tok->line_info;
        expect(")");

        Node *node = new_node(ND_SWITCH, switch_expr, NULL);
        node->switch_.cases = new_vector();
        vector_push(switch_stack, node);
        vector_push(break_targets, node);
        node->rhs = stmt();
        vector_pop(switch_stack);
        vector_pop(break_targets);
        return node;
    }else if(consume_kind(TK_CASE)) {
        if(vector_size(switch_stack) == 0) {
            error_at(token->str, "case statement can only be appeared in switch statement.");
        }
        Node *expr = expression();
        expect(":");
        expr = constant_fold(expr);
        if(expr->kind != ND_NUM) {
            error_at(token->str, "case statement must have constant expression");
        }
        Node *node = new_node(ND_CASE, stmt(), expr);
        Node *switch_node = vector_get(switch_stack, vector_size(switch_stack) - 1);
        vector_push(switch_node->switch_.cases, node);
        return node;
    }else if(consume_kind(TK_DEFAULT)) {
        if(vector_size(switch_stack) == 0) {
            error_at(token->str, "default statement can only be appeared in switch statement.");
        }
        expect(":");
        Node *node = new_node(ND_DEFAULT, stmt(), NULL);
        Node *switch_node = vector_get(switch_stack, vector_size(switch_stack) - 1);
        if(switch_node->switch_.default_stmt != NULL) {
            error_at(token->str, "Duplicate default statement");
        }
        switch_node->switch_.default_stmt = node;
        return node;
    }else if(consume_kind(TK_BREAK)) {
        if(vector_size(break_targets) == 0) {
            error_at(token->str, "break must be in a switch statement.");
        }
        Node *node = new_node(ND_BREAK, NULL, NULL);
        expect(";");
        return node;
    }else if(consume_kind(TK_CONTINUE)) {
        if(vector_size(continue_targets) == 0) {
            error_at(token->str, "break must be in a switch statement.");
        }
        Node *node = new_node(ND_CONTINUE, NULL, NULL);
        expect(";");
        return node;
    }else if(consume_kind(TK_WHILE)) {
        expect("(");
        Node *while_expr = expression();
        expect(")");
        Node *node = new_node(ND_WHILE, while_expr, NULL);
        vector_push(break_targets, node);
        vector_push(continue_targets, node);
        node->rhs = stmt();
        vector_pop(break_targets);
        vector_pop(continue_targets);
        return node;
    }else if(consume_kind(TK_FOR)) {
        expect("(");
        Node *for_init_expr = NULL;
        Node *for_condition_expr = NULL;
        Node *for_update_expr = NULL;
        TokenKind kind;

        // for statement introduce its own scope for variables declared in clause-1.
        // e.g.
        // for (int i=0; i < 10; i++){} 
        // In above example, i lives until ends of for-body.
        Node *new_scope = new_node_scope(&scope);

        if(consume_type_prefix(&kind)) {
            unget_token();
            for_init_expr = local_variable_definition();
        } else if(!consume(";")){
            for_init_expr = expression();
            expect(";");
        }
        if(!consume(";")){
            for_condition_expr = expression();
            expect(";");
        }
        if(!consume(")")){
            for_update_expr = expression();
            expect(")");
        }
        Node *node = new_node(ND_FOR, for_init_expr, for_condition_expr);
        node->for_update_expr = for_update_expr;
        vector_push(break_targets, node);
        vector_push(continue_targets, node);
        node->for_stmt = stmt();
        vector_pop(break_targets);
        vector_pop(continue_targets);

        scope->lhs = node;

        end_scope(&scope);
        node->line_info = tok->line_info;

        return new_scope;
    }else if(consume_kind(TK_DO)) {
        Node *node = new_node(ND_DO, NULL, NULL);
        vector_push(break_targets, node);
        vector_push(continue_targets, node);
        node->lhs = stmt();
        vector_pop(break_targets);
        vector_pop(continue_targets);
        expect_kind(TK_WHILE);
        expect("(");
        node->rhs = expression();
        expect(")");
        expect(";");
        return node;
    }else if(consume_kind(TK_RETURN)) {
        Node *expr = NULL;
        if(!consume(";")) {
            expr = expression();
            expect(";");
        }
        Type *type = current_func->func_def.type->ptr_to;
        if(type->ty == VOID) {
            if(expr != NULL && expr->expr_type->ty != VOID) {
                error_at(token->str, "Cannot return value on void function");
            }
        }else if(expr == NULL){
            error_at(token->str, "Specify expression on return statament on non-void function.");
        }
        Node *node = new_node(ND_RETURN, expr, NULL);
        if(expr && !type_is_same(type, node->lhs->expr_type)) {
            node->lhs = new_node_conv(node->lhs, type);
        }
        node->line_info = tok->line_info;
        return node;
    }else if(consume_type_prefix(&kind)) {
        unget_token();
        Node *node = local_variable_definition();
        node->line_info = tok->line_info;
        return node;
    }else if(consume("{")) {
        Node *node = new_node(ND_COMPOUND, NULL, NULL);
        node->line_info = tok->line_info;
        Node *new_scope = new_node_scope(&scope);
        new_scope->lhs = node;
        node->compound_stmt_list = new_vector();

        int i = 0;
        while(!consume("}")){
            vector_push(node->compound_stmt_list, stmt());
        }
        end_scope(&scope);
        return new_scope;
    }
    Node *node = expression();
    node->line_info = tok->line_info;
    expect(";");
    return node;
}

Node *expression() {
    Node *node = assignment_expression();
    while(consume(",")) {
        node = new_node(ND_COMMA_EXPR, node, assignment_expression());
        node->expr_type = node->rhs->expr_type;
    }
    return node;
}

// assignment_expression = conditional_expression ( "=" assignment_expression )?
Node *assignment_expression() {
    Node *node = conditional_expression();
    Node *rhs = NULL;
    if(consume("=")) {
        rhs = assignment_expression();
    }else if(consume("*=")) {
        rhs = new_node_binop(ND_MUL, node, assignment_expression());
    }else if(consume("/=")) {
        rhs = new_node_binop(ND_DIV, node, assignment_expression());
    }else if(consume("%=")) {
        rhs = new_node_binop(ND_MOD, node, assignment_expression());
    }else if(consume("+=")) {
        rhs = new_node_binop(ND_ADD, node, assignment_expression());
    }else if(consume("-=")) {
        rhs = new_node_binop(ND_SUB, node, assignment_expression());
    }else if(consume("<<=")) {
        rhs = new_node_binop(ND_LSHIFT, node, assignment_expression());
    }else if(consume(">>=")) {
        rhs = new_node_binop(ND_RSHIFT, node, assignment_expression());
    }else if(consume("&=")) {
        rhs = new_node_binop(ND_AND, node, assignment_expression());
    }else if(consume("^=")) {
        rhs = new_node_binop(ND_XOR, node, assignment_expression());
    }else if(consume("|=")) {
        rhs = new_node_binop(ND_OR, node, assignment_expression());
    }else {
        return node;
    }
    return new_node_assignment(node, rhs);
}

Node *conditional_expression() {
    Node *node = logical_OR_expression();
    if(consume("?")) {
        Node *expr = expression();
        expect(":");
        Node *lhs = conditional_expression();
        node = new_node(ND_IF, node, expr);
        node->expr_type = lhs->expr_type;
        node->else_stmt = lhs;
    }
    return node;
}

// logical_OR_expression = logical_AND_expression ( "||" logical_AND_expression )*
Node *logical_OR_expression() {
    Node *node = logical_AND_expression();

    while(consume("||")) {
        Node *cond = new_node(ND_EQUAL, node, new_node_num(0));
        node = new_node(ND_IF, cond, logical_AND_expression());
        node->expr_type = &signed_int_type;
        node->else_stmt = new_node_num(1);
    }
    return node;
}

// logical_AND_expression = inclusive_OR_expression ( "&&" inclusive_OR_expression )*
Node *logical_AND_expression() {
    Node *node = inclusive_OR_expression();

    while(1) {
        if(consume("&&")) {
            node = new_node(ND_IF, node, inclusive_OR_expression());
            node->expr_type = &signed_int_type;
            node->else_stmt = new_node_num(0);
        } else {
            return node;
        }
    }
}

// inclusive_OR_expression = exclusive_OR_expression ( "|" exclusive_OR_expression )*
Node *inclusive_OR_expression() {
    Node *node = exclusive_OR_expression();

    while(1) {
        if(consume("|")) {
            node = new_node_binop(ND_OR, node, exclusive_OR_expression());
        } else {
            return node;
        }
    }
}

// exclusive_OR_expression = AND_expression ( "^" AND_expression )*
Node *exclusive_OR_expression() {
    Node *node = AND_expression();

    while(1) {
        if(consume("^")) {
            node = new_node_binop(ND_XOR, node, AND_expression());
        } else {
            return node;
        }
    }
}

// AND_expression = equality_expression ( "&" equality_expression )*
Node *AND_expression() {
    Node *node = equality_expression();

    while(1) {
        if(consume("&")) {
            node = new_node_binop(ND_AND, node, equality_expression());
        } else {
            return node;
        }
    }
}

// equality = relational ( "==" relational | "!=" relational )*
Node *equality_expression() {
    Node *node = relational_expression();

    for(;;){
        if(consume("=="))
            node = new_node_binop(ND_EQUAL, node, relational_expression());
        else if(consume("!="))
            node = new_node_binop(ND_NOT_EQUAL, node, relational_expression());
        else
            return node;
    }
}

// relational_expression = shift_expression ( "<" shift_expression | "<=" shift_expression | ">" shift_expression | ">=" shift_expression )*
Node *relational_expression() {
    Node *node = shift_expression();

    for(;;){
        if(consume("<"))
            node = new_node_binop(ND_LESS, node, shift_expression());
        else if(consume("<="))
            node = new_node_binop(ND_LESS_OR_EQUAL, node, shift_expression());
        else if(consume(">"))
            node = new_node_binop(ND_GREATER, node, shift_expression());
        else if(consume(">="))
            node = new_node_binop(ND_GREATER_OR_EQUAL, node, shift_expression());
        else
            return node;
    }
}

// shift_expression = additive_expression ( ( "<<" | ">>" ) additive_expression )*
Node *shift_expression() {
    Node *node = additive_expression();

    while(1) {
        if(consume("<<"))
            node = new_node_binop(ND_LSHIFT, node, additive_expression());
        else if(consume(">>"))
            node = new_node_binop(ND_RSHIFT, node, additive_expression());
        else
            return node;
    }
}

// add = mul ( "+" mul | "-" mul )*
Node *additive_expression() {
    Node *node = multiplicative_expression();

    for(;;){
        if(consume("+")) {
            node = new_node_binop(ND_ADD, node, multiplicative_expression());
        } else if(consume("-")) {
            node = new_node_binop(ND_SUB, node, multiplicative_expression());
        }
        else
            return node;
    }
}

// mul = unary ( "*" unary | "/" unary )*
Node *multiplicative_expression() {
    Node *node = cast_expression();

    for(;;){
        if(consume("*"))
            node = new_node_binop(ND_MUL, node, cast_expression());
        else if(consume("/"))
            node = new_node_binop(ND_DIV, node, cast_expression());
        else if(consume("%"))
            node = new_node_binop(ND_MOD, node, cast_expression());
        else
            return node;
    }
}

// cast_expression = unary_expression
//                 | ( type ) cast_expression
Node *cast_expression() {
    if(consume("(")) {
        TokenKind kind;
        if(consume_type_prefix(&kind)) {
            unget_token();
            Node *type_node = type_(false, false, true);
            consume(")");

            Node *inner = cast_expression();
            Node *node = new_node(ND_CAST, inner, NULL);
            Node *var_node = vector_get(type_node->decl_list.decls, 0);
            node->expr_type = var_node->lhs->type.type;
            node->lhs = new_node_conv(node->lhs, node->expr_type);
            return node;
        }
        unget_token();
    }
    return unary_expression();
}

// unary = "+" primary
//       | "-" primary
//       | "*" unary
//       | "&" unary
//       | "sizeof" unary
//       | "sizeof" "(" type ")"
//       | primary
Node *unary_expression() {
    if(consume("+"))
        return cast_expression();
    if(consume("-"))
        return new_node_binop(ND_SUB, new_node_num(0), cast_expression());
    if(consume("&")) {
        Node *node = new_node(ND_ADDRESS_OF, unary_expression(), NULL);
        node->expr_type = calloc(1, sizeof(Type));
        node->expr_type->ty = PTR;
        node->expr_type->ptr_to = node->lhs->expr_type;
        return node;
    }
    if(consume("*")) {
        Node *node = new_node(ND_DEREF, unary_expression(), NULL);
        if(node->lhs->expr_type->ty != PTR && node->lhs->expr_type->ty != ARRAY) {
            error_at(token->str, "Dereference non pointer type");
        }
        node->expr_type = node->lhs->expr_type->ptr_to;
        return node;
    }
    if(consume("~")) {
        Node *node = new_node(ND_BIT_NOT, cast_expression(), NULL);
        node->expr_type = node->lhs->expr_type;
        return node;
    }
    if(consume("!")) {
        Node *node = new_node(ND_EQUAL, cast_expression(), new_node_num(0));
        if(!type_is_scalar(node->lhs->expr_type)) {
            error_at(token->str, "Not operator expect scalar type operand.");
        }
        node->expr_type = node->lhs->expr_type;
        return node;
    }
    if(peek("++") || peek("--")) {
        bool plus = consume("++");
        if(!plus) {
            consume("--");
        }
        Node *node = new_node(plus ? ND_PREFIX_INC : ND_PREFIX_DEC, unary_expression(), NULL);
        node->expr_type = node->lhs->expr_type;
        // increment integer
        node->incdec.value = 1;
        // increment pointer (Adds type size)
        if(node->expr_type->ty == PTR) {
            node->incdec.value = type_sizeof(node->expr_type->ptr_to);
        }
        if(!plus) {
            node->incdec.value = -node->incdec.value;
        }
        return node;
    }
    if(consume_kind(TK_SIZEOF)) {
        bool paren = consume("(");
        if(paren) {
            TokenKind kind;
            if(consume_type_prefix(&kind)) {
                unget_token();
                Node *type_node = type_(false, false, true);
                // type is returned in list of a ND_DECL_VAR.
                Node *var_node = vector_get(type_node->decl_list.decls, 0);
                expect(")");
                return new_node_num(type_sizeof(var_node->lhs->type.type));
            } else {
                unget_token();
            }
        }
        return new_node_num(type_sizeof(unary_expression()->expr_type));
    }
    return postfix_expression();
}

// postfix_expression = "(" expr ")"
//         | primary_expression
//         | postfix_expression "(" ( (expr ",")* expr )? ")"
//         | postfix_expression "[" expression "]"
//         | postfix_expression "." ident
//         | postfix_expression "->" ident
Node *postfix_expression() {
    Node *node = primary_expression();
    while(1) {
        if(consume("(")){
            Node *call_node = new_node(ND_CALL, node, NULL);
            call_node->expr_type = node->expr_type->ptr_to;

            NodeList *arg_tail = &call_node->call_arg_list;
            if(!consume(")")){
                while(1){
                    NodeList *nodelist = calloc(1, sizeof(NodeList));
                    nodelist->node = assignment_expression();
                    arg_tail->next = nodelist;
                    arg_tail = nodelist;
                    if(!consume(",")){
                        expect(")");
                        break;
                    }
                }
            }

            if(node->kind != ND_GVAR) {
                error_at(token->str, "Function call for non-ident is not supported");
            }
            call_node->call_ident = node->gvar.gvar->name;
            call_node->call_ident_len = node->gvar.gvar->len;
            node = call_node;
        }else if(consume("[")) {
            Node *expr_node = expression();
            expect("]");

            Node *added = new_node_binop(ND_ADD, node, expr_node);

            node = new_node(ND_DEREF, added, NULL);
            node->expr_type = added->expr_type->ptr_to;
        }else if(peek(".") || peek("->")) {
            bool dot = consume(".");
            if(!dot) {
                consume("->");
            }
            char *ident;
            int ident_len;
            expect_ident(&ident, &ident_len);

            Vector *vec;
            Type *struct_type = node->expr_type;
            if(!dot) {
                if(struct_type->ty != PTR) {
                    error("Access struct member for non struct pointer variable");
                }
                struct_type = struct_type->ptr_to;
            }
            if(struct_type->ty != STRUCT) {
                error("Access struct member for non struct variable");
            }
            vec = struct_type->members;
            size_t offset = 0;
            StructMember *mem = find_struct_member(struct_type->members, ident, ident_len, &offset);
            if(!mem) {
                error_at(token->str, "No member name found: %.*s", ident_len, ident);
            }
            Node *left = NULL;
            if(dot){
                left = new_node(ND_ADDRESS_OF, node, NULL);
            }else{
                left = node;
            }
            left->expr_type = type_new_ptr(&char_type);
            Node *add = new_node_binop(ND_ADD, left, new_node_num(offset));
            node = new_node(ND_DEREF, add, NULL);
            node->expr_type = mem->type;
        }else if(peek("++") || peek("--")) {
            bool plus = consume("++");
            if(!plus) {
                consume("--");
            }
            node = new_node(plus ? ND_POSTFIX_INC : ND_POSTFIX_DEC, node, NULL);
            node->expr_type = node->lhs->expr_type;
            // increment integer
            node->incdec.value = 1;
            // increment pointer (Adds type size)
            if(node->expr_type->ty == PTR) {
                node->incdec.value = type_sizeof(node->expr_type->ptr_to);
            }
            if(!plus) {
                node->incdec.value = -node->incdec.value;
            }
        }else{
            break;
        }
    }
    return node;
}

// primary_expression = identifier
//                    | constant
//                    | string-literal
//                    | "(" expression ")"
Node *primary_expression() {
    if(consume("(")){
        Node *node = expression();
        expect(")");
        return node;
    }
    Node *node;
    char *ident;
    int ident_len;
    if(consume_ident(&ident, &ident_len)){
        if(compare_slice(ident, ident_len, "__func__")) {
            node = create_func_name_literal();
        } else {
            node = find_symbol(globals, ident, ident_len);
            if(node == NULL) {
                error_at(ident, "symbol %.*s is not defined.", ident_len, ident);
            }
        }
    }else if(consume_kind(TK_STRING_LITERAL)) {
        unget_token();
        StringLiteral *literal = calloc(1, sizeof(StringLiteral));
        literal->str = token->str;
        literal->len = token->len;
        literal->char_vec = token->literal;
        literal->vec_len = token->literal_len;
        literal->index = vector_size(global_string_literals);
        vector_push(global_string_literals, literal);
        next_token();

        node = new_node(ND_STRING_LITERAL, NULL, NULL);
        node->expr_type = calloc(1, sizeof(Type));
        node->expr_type->ptr_to = &char_type;
        node->expr_type->ty = PTR;
        node->string_literal.literal = literal;
    }else {
        Token *tk = token;
        expect_number();
        node = new_node_num_suffix(tk->val, tk->suffix);
    }
    return node;
}

Node *constant_expression() {
    return conditional_expression();
}

Node *constant_fold(Node *node) {
    if(node->kind == ND_NUM) {
        return node;
    }
    if(node->kind == ND_ADD || node->kind == ND_SUB || node->kind == ND_MUL || node->kind == ND_DIV
            || node->kind == ND_LSHIFT || node->kind == ND_RSHIFT || node->kind == ND_AND || node->kind == ND_OR || node->kind == ND_XOR
            || node->kind == ND_GREATER || node->kind == ND_GREATER_OR_EQUAL || node->kind == ND_LESS || node->kind == ND_LESS_OR_EQUAL) {
        node->lhs = constant_fold(node->lhs);
        node->rhs = constant_fold(node->rhs);
        if(node->lhs->kind == ND_NUM && node->rhs->kind == ND_NUM) {
            switch(node->kind) {
                case ND_ADD: node->val = node->lhs->val + node->rhs->val; break;
                case ND_SUB: node->val = node->lhs->val - node->rhs->val; break;
                case ND_MUL: node->val = node->lhs->val * node->rhs->val; break;
                case ND_DIV: node->val = node->lhs->val / node->rhs->val; break;
                case ND_LSHIFT: node->val = node->lhs->val << node->rhs->val; break;
                case ND_RSHIFT: node->val = node->lhs->val >> node->rhs->val; break;
                case ND_AND: node->val = (node->lhs->val & node->rhs->val); break;
                case ND_OR: node->val = (node->lhs->val | node->rhs->val); break;
                case ND_XOR: node->val = (node->lhs->val ^ node->rhs->val); break;
                case ND_GREATER: node->val = (node->lhs->val > node->rhs->val); break;
                case ND_GREATER_OR_EQUAL: node->val = (node->lhs->val >= node->rhs->val); break;
                case ND_LESS: node->val = (node->lhs->val < node->rhs->val); break;
                case ND_LESS_OR_EQUAL: node->val = (node->lhs->val <= node->rhs->val); break;
            }
            node->kind = ND_NUM;
        }
        return node;
    }
    if(node->kind == ND_CAST) {
        node->lhs = constant_fold(node->lhs);
        return node->lhs;
    }
    if(node->kind == ND_CONVERT) {
        node->lhs = constant_fold(node->lhs);
        if(node->lhs->kind == ND_NUM) {
            if(type_sizeof(node->expr_type) == 1) {
                node->lhs->val &= 0xff;
            }else if(type_sizeof(node->expr_type) == 2) {
                node->lhs->val &= 0xffff;
            }else if(type_sizeof(node->expr_type) == 4) {
                node->lhs->val &= 0xffffffff;
            }
        }
        return node->lhs;
    }
    if(node->kind == ND_IF) {
        node->lhs = constant_fold(node->lhs);
        if(node->lhs->kind == ND_NUM) {
            if(node->lhs->val) {
                node->rhs = constant_fold(node->rhs);
                return node->rhs;
            }
            node->else_stmt = constant_fold(node->else_stmt);
            return node->else_stmt;
        }
    }
    return node;
}

// Parse type specifier like followings.
// Right rows are golang-like representation of type.
// int a               int
// int *a              * int
// int **a             * * int
// int *a[]            [] * int
// int a[3][]          [3] [] int
// int (*a)[2]         * [2] int
// int *(*a)[2]        * [2] * int
//   -> read 'int' -> read '*' -> read '(...)' and memorize it on somewhere -> read '[...]' then create "[2] * int", then restore '(...)'
// int a()             func() int
// int a(int)          func(int) int
// int a(int b)        func(int) int
// int *a(int b)       func(int) * int
// int (*a[5])(int b)  [] * func(int) int
// int (*a[5])()[]()   [] * func() [] func() (an array to a function is invalid, but BNF should(?) accept it)
// int (*a())[]        func() * [] int
//
// type = ("char" | "int") type_pointer
//
// type_pointer = "*" type_pointer
//              | type_array
//
// type_array = type_ident type_array_suffix
//
// type_array_suffix = ( "[" expr? "]" | "(" function_arguments ")" ) type_array_suffix
//
// type_ident   = ident?
//              | "(" type_pointer ")"
//
// function_arguments = ( ( type_opt_ident "," )* type_opt_ident )?
//
Node *type_(bool need_ident, bool is_global, bool parse_one_type) {
    TypeStorage type_storage = TS_NONE;
    int type_qual = 0;
    TypeBasic type_basic = TB_NONE;
    bool unsigned_flag = false;
    bool signed_flag = false;
    bool inline_flag = false;
    char *ident;
    int ident_len;
    Type *base_type = NULL;
    int tk_count[TK_MAX] = {};
    while(1) {
        TokenKind kind;
        if(!consume_type_prefix(&kind)) {
            break;
        }
        if(kind == TK_STRUCT) {
            base_type = struct_declaration(true)->type.type;
        } else if(kind == TK_UNION) {
            base_type = struct_declaration(false)->type.type;
        } else if(kind == TK_ENUM) {
            base_type = enum_declaration()->type.type;
        }
        if(kind == TK_IDENT) {
            ident = token->prev->str;
            ident_len = token->prev->len;
        }
        tk_count[kind]++;
    }
    TokenKind base_types[] = {TK_VOID, TK_CHAR, TK_INT, TK_FLOAT, TK_DOUBLE, TK_COMPLEX, TK_BOOL, TK_STRUCT, TK_UNION, TK_ENUM, TK_IDENT};
    TypeBasic basic_base_types[] = {TB_VOID, TB_CHAR, TB_INT, TB_FLOAT, TB_DOUBLE, TB_COMPLEX, TB_BOOL, TB_STRUCT, TB_UNION, TB_ENUM, TB_TYPEDEF_NAME};
    int base_type_count = 0;
    for(int i = 0; i < sizeof(base_types) / sizeof(base_types[i]); i++) {
        base_type_count += tk_count[base_types[i]];
        if(tk_count[base_types[i]]) {
            type_basic = basic_base_types[i];
        }
    }
    if(base_type_count >= 2) {
        error_at(token->str, "Extra base type keyword");
    }
    if(tk_count[TK_LONG] >= 3) {
        error_at(token->str, "Extra long keyword");
    }
    if(tk_count[TK_SHORT] >= 2) {
        error_at(token->str, "Extra short keyword");
    }
    if(tk_count[TK_SHORT] && tk_count[TK_LONG]) {
        error_at(token->str, "Extra short/long keyword");
    }
    if(tk_count[TK_LONG] == 2) {
        if(type_basic == TB_NONE || type_basic == TB_INT) {
            type_basic = TB_LONGLONG;
        }else{
            error_at(token->str, "Extra long keyword");
        }
    } else if(tk_count[TK_LONG] == 1) {
        if(type_basic == TB_NONE || type_basic == TB_INT) {
            type_basic = TB_LONG;
        }else if(type_basic == TB_DOUBLE) {
            type_basic = TB_LONGDOUBLE;
        }else{
            error_at(token->str, "Extra long keyword");
        }
    } else if(tk_count[TK_SHORT] == 1) {
        if(type_basic == TB_NONE || type_basic == TB_INT) {
            type_basic = TB_SHORT;
        }else{
            error_at(token->str, "Extra short keyword");
        }
    }
    if(type_basic == TB_NONE) {
        type_basic = TB_INT;
    }

    if(tk_count[TK_UNSIGNED] && tk_count[TK_SIGNED]) {
        error_at(token->str, "Conflicting unsigned and signed");
    }

    if(tk_count[TK_CONST]) { type_qual |= 1<<TQ_CONST; }
    if(tk_count[TK_RESTRICT]) { type_qual |= 1<<TQ_RESTRICT; }
    if(tk_count[TK_VOLATILE]) { type_qual |= 1<<TQ_VOLATILE; }

    TokenKind storage_keywords[] = {TK_TYPEDEF, TK_EXTERN, TK_STATIC, TK_AUTO, TK_REGISTER};
    TypeStorage storage_types[] = {TS_TYPEDEF, TS_EXTERN, TS_STATIC, TS_AUTO, TS_REGISTER};
    int storage_type_count = 0;
    for(int i = 0; i < sizeof(storage_keywords) / sizeof(storage_keywords[i]); i++) {
        storage_type_count += tk_count[storage_keywords[i]];
        if(tk_count[storage_keywords[i]]) {
            type_storage = storage_types[i];
        }
    }
    if(storage_type_count >= 2) {
        error_at(token->str, "Conflicting storage type keyword");
    }
    signed_flag = tk_count[TK_SIGNED];
    unsigned_flag = tk_count[TK_UNSIGNED];

    if(type_basic == TB_NONE) {
        if(signed_flag) {
            type_basic = TB_INT;
        }
        if(unsigned_flag) {
            type_basic = TB_INT;
        }
    }
    switch(type_basic) {
        case TB_VOID: base_type = &void_type; break;
        case TB_CHAR:
                      if(signed_flag) {
                          base_type = &signed_char_type;
                      }else if(unsigned_flag) {
                          base_type = &unsigned_char_type;
                      }else{
                          base_type = &char_type;
                      }
                      break;
        case TB_INT:
                      if(unsigned_flag) {
                          base_type = &unsigned_int_type;
                      }else{
                          base_type = &signed_int_type;
                      }
                      break;
        case TB_SHORT:
                      if(unsigned_flag) {
                          base_type = &unsigned_short_type;
                      }else{
                          base_type = &signed_short_type;
                      }
                      break;
        case TB_LONG:
                      if(unsigned_flag) {
                          base_type = &unsigned_long_type;
                      }else{
                          base_type = &signed_long_type;
                      }
                      break;
        case TB_LONGLONG:
                      if(unsigned_flag) {
                          base_type = &unsigned_longlong_type;
                      }else{
                          base_type = &signed_longlong_type;
                      }
                      break;
        case TB_FLOAT: base_type = &float_type; break;
        case TB_DOUBLE: base_type = &double_type; break;
        case TB_LONGDOUBLE: base_type = &longdouble_type; break;
        case TB_COMPLEX: base_type = &complex_type; break;
        case TB_BOOL: base_type = &bool_type; break;
    }
    if(type_basic == TB_TYPEDEF_NAME) {
        TypedefRegistryEntry *entry;
        for(int i = 0; i < vector_size(typedef_registry); i++) {
            entry = vector_get(typedef_registry, i);
            if(compare_ident(entry->ident, entry->ident_len, ident, ident_len)) {
                break;
            }
        }
        base_type = entry->type;
    }
    if(base_type == NULL) {
        error_at(token->str, "Cannot parse type specifier");
    }

    bool is_inline = tk_count[TK_INLINE];

    Node *list_node = new_node(ND_DECL_LIST, NULL, NULL);
    list_node->decl_list.base_type = base_type;
    list_node->decl_list.decls = new_vector();
    if(consume(";")) {
        // struct definition.
        return list_node;
    }
    while(!at_eof()) {
        Type *cur = base_type;
        Node *node = new_node(ND_TYPE, type_pointer(need_ident), NULL);
        Node *node_cur = node;
        for(; node_cur; node_cur = node_cur->lhs) {
            if(node_cur->kind == ND_IDENT) {
                break;
            } else if(node_cur->kind == ND_TYPE_POINTER) {
                cur = type_new_ptr(cur);
            } else if(node_cur->kind == ND_TYPE_ARRAY) {
                cur = type_new_array(cur, node_cur->type.array.has_size, node_cur->type.array.size);
            } else if(node_cur->kind == ND_TYPE_FUNC) {
                cur = type_new_func(cur, node_cur->type.func_args.args, node_cur->type.func_args.is_vararg);
            }
            node_cur->type.type = cur;
        }
        node->type.type = cur;
        if(peek("{")) {
            if(cur->ty != FUNC) {
                error_at(token->str, "Non function type cannot have function body");
            }
            if(type_storage == TS_TYPEDEF) {
                error_at(token->str, "typedef cannot have function body");
            }
            // function definition
            return function_definition(type_storage, node, is_inline);
        }
        // declaration
        Node *var_node;
        if(node->type.type->ty == FUNC) {
            char *ident;
            int ident_len;
            if(!type_find_ident(node, &ident, &ident_len)) {
                error("No identifier on function declaration");
            }
            GVar *gvar = find_gvar(globals, ident, ident_len);
            if(gvar == NULL) {
                gvar = new_gvar(globals, ident, ident_len, node->type.type, false);
            }

            var_node = new_node(ND_FUNC_DECL, node, NULL);
        } else {
            var_node = variable_definition(is_global, node, type_storage);
        }
        vector_push(list_node->decl_list.decls, var_node);

        // If we are in function argument list, comma should be handled on upper functions.
        if(parse_one_type) {
            break;
        }
        if(consume(";")) {
            break;
        }
        expect(",");
    }
    return list_node;
}

Node *type_pointer(bool need_ident) {
    if(consume("*")) {
        while(1) {
            if(consume_kind(TK_CONST)) {
            } else if(consume_kind(TK_RESTRICT)) {
            } else if(consume_kind(TK_VOLATILE)) {
            } else {
                break;
            }
        }
        return new_node(ND_TYPE_POINTER, type_pointer(need_ident), NULL);
    }
    return type_array(need_ident);
}

Node *type_array(bool need_ident) {
    // Peek 2 tokens to check whether inner (paren) type or function arg.
    // ((, (*, (ident are sign of inner type.
    if(consume("(")) {
        char *ident;
        int ident_len;
        if(consume("(") || consume("*") || consume_ident(&ident, &ident_len)) {
            unget_token();
            unget_token();
            Node *ident_node = type_ident(need_ident);

            Vector *array_suffix_vector = new_vector();
            type_array_suffix(array_suffix_vector);
            int suffix_len = vector_size(array_suffix_vector);
            if(suffix_len) {
                Node *tail = ident_node;
                for(int i = 0; i < suffix_len; i++) {
                    Node *node = vector_get(array_suffix_vector, i);
                    node->lhs = tail;
                    tail = node;
                }
                return tail;
            }else{
                return ident_node;
            }
        }
        unget_token();
    }
    Node *ident_node = type_ident(need_ident);

    Vector *array_suffix_vector = new_vector();
    type_array_suffix(array_suffix_vector);
    int suffix_len = vector_size(array_suffix_vector);
    if(suffix_len) {
        Node *tail = ident_node;
        for(int i = 0; i < suffix_len; i++) {
            Node *node = vector_get(array_suffix_vector, i);
            node->lhs = tail;
            tail = node;
        }
        return tail;
    }else{
        return ident_node;
    }
}

void type_array_suffix(Vector *array_suffix_vector) {
    if(consume("(")) {
        bool is_vararg = false;
        Vector *args = function_arguments(&is_vararg);
        Node *func_node = new_node(ND_TYPE_FUNC, NULL, NULL);
        func_node->type.func_args.args = args;
        func_node->type.func_args.is_vararg = is_vararg;
        func_node->lhs = func_node;
        vector_push(array_suffix_vector, func_node);
        return type_array_suffix(array_suffix_vector);
    } else if(consume("[")) {
        Node *array = new_node(ND_TYPE_ARRAY, NULL, NULL);
        if(!consume("]")) {
            Node *expr_node = constant_fold(expression());
            array->rhs = expr_node;
            if(expr_node->kind == ND_NUM) {
                array->type.array.size = expr_node->val;
                array->type.array.has_size = true;
            } else {
                error_at(token->str, "Expression with non literal number in array size is not supported");
            }
            expect("]");
        }
        vector_push(array_suffix_vector, array);
        return type_array_suffix(array_suffix_vector);
    }
}

Node *type_ident(bool need_ident) {
    if(consume("(")) {
        Node *node = type_pointer(need_ident);
        expect(")");
        return node;
    }
    if(need_ident) {
        return ident_();
    } else {
        char *ident;
        int ident_len;
        if(consume_ident(&ident, &ident_len)) {
            unget_token();
            return ident_();
        }
        return NULL;
    }
}

Node *ident_() {
    Node *node = new_node(ND_IDENT, NULL, NULL);
    expect_ident(&node->ident.ident, &node->ident.ident_len);
    return node;
}

Vector *function_arguments(bool *is_vararg) {
    Vector *args = new_vector();
    *is_vararg = false;
    if(!consume(")")) {
        while(1) {
            if(consume("...")) {
                *is_vararg = true;
                expect(")");
                break;
            }
            expect_type_prefix();
            unget_token();
            Node *type = type_(false, false, true);
            if(type->kind != ND_DECL_LIST) {
                error_at(token->str, "Function type cannot be used as argument");
            }
            vector_push(args, vector_get(type->decl_list.decls, 0));

            if(consume(")")) {
                break;
            }
            expect(",");
        }
    }
    return args;
}

// struct_declaration = "struct" ident? ( "{" struct_members "}" )?
Node *struct_declaration(bool is_struct) {
    char *ident;
    int ident_len;
    // ident is optional
    if(!consume_ident(&ident, &ident_len)) {
        char buf[100];
        sprintf(buf, "__unnamed_%s_%d", is_struct ? "struct" : "union", unnamed_struct_count);
        unnamed_struct_count++;
        ident = malloc(strlen(buf)+1);
        memcpy(ident, buf, strlen(buf)+1);
        ident_len = strlen(buf);
    }

    Node *node = new_node(ND_TYPE_STRUCT, NULL, NULL);

    Vector *reg = is_struct ? struct_registry : union_registry;
    if(consume("{")) {
        StructRegistryEntry *entry = NULL;
        bool found = false;
        for(int i = 0; i < vector_size(reg); i++) {
            entry = vector_get(reg, i);
            if(compare_ident(entry->ident, entry->ident_len, ident, ident_len)) {
                if(entry->type->struct_complete) {
                    error("Struct name is already defined");
                }
                found = true;
                break;
            }
        }
        if(found) {
            // Convert incomplete struct to complete one if already exists.
            node->type.type = entry->type;
            entry->type->struct_complete = true;
        } else {
            node->type.type = type_new_struct(ident, ident_len);
        }
        node->type.type->members = struct_members(&node->type.type->struct_size, is_struct);
        if(!found) {
            entry = calloc(1, sizeof(StructRegistryEntry));
            entry->ident = ident;
            entry->ident_len = ident_len;
            entry->type = node->type.type;
            entry->type->struct_complete = true;
            vector_push(reg, entry);
        }

        expect("}");
    } else {
        bool found = false;
        StructRegistryEntry *entry;
        for(int i = 0; i < vector_size(reg); i++) {
            entry = vector_get(reg, i);
            if(compare_ident(entry->ident, entry->ident_len, ident, ident_len)) {
                found = true;
                break;
            }
        }
        if(!found) {
            entry = calloc(1, sizeof(StructRegistryEntry));
            entry->ident = ident;
            entry->ident_len = ident_len;
            node->type.type = type_new_struct(ident, ident_len);
            entry->type = node->type.type;
            entry->type->struct_complete = false;
            vector_push(reg, entry);

            //error("No struct found for specified name");
        }
        node->type.type = entry->type;
    }

    return node;
}

// struct_members = ( type_ ";" )*
Vector *struct_members(size_t *size, bool is_struct) {
    Vector *vec = new_vector();
    while(peek_type_prefix()) {
        Node *decl_list = type_(true, false, false);
        if(decl_list->kind != ND_DECL_LIST) {
            error_at(token->str, "Invalid declaration of struct member");
        }
        if(vector_size(decl_list->decl_list.decls) == 0) {
            // unnamed member
            StructMember *member = calloc(1, sizeof(StructMember));
            member->type = decl_list->decl_list.base_type;
            member->unnamed = true;
            if(is_struct) {
                member->offset = *size;
                *size += type_sizeof(member->type);
            }else{
                member->offset = 0;
                if(*size < type_sizeof(member->type)) {
                    *size = type_sizeof(member->type);
                }
            }
            vector_push(vec, member);
        }else{
            for(int i = 0; i < vector_size(decl_list->decl_list.decls); i++){
                Node *decl_node = vector_get(decl_list->decl_list.decls, i);
                Node *type_node = decl_node->lhs;
                StructMember *member = calloc(1, sizeof(StructMember));
                type_find_ident(type_node, &member->ident, &member->ident_len);
                for(int i = 0; i < vector_size(vec); i++) {
                    StructMember *member2 = vector_get(vec, i);
                    if(member->ident_len == member2->ident_len && memcmp(member->ident, member2->ident, member->ident_len) == 0) {
                        error("Struct member with same name is already defined: %.*s", member->ident_len, member->ident);
                    }
                }
                member->type = type_node->type.type;
                if(is_struct) {
                    member->offset = *size;
                    *size += type_sizeof(type_node->type.type);
                }else{
                    member->offset = 0;
                    if(*size < type_sizeof(type_node->type.type)) {
                        *size = type_sizeof(type_node->type.type);
                    }
                }
                vector_push(vec, member);
            }
        }
    }
    return vec;
}

// enum_declaration = "enum" ident ( "{" enum_members "}" )?
Node *enum_declaration() {
    char *ident;
    int ident_len;
    if(!consume_ident(&ident, &ident_len)) {
        char buf[100];
        sprintf(buf, "__unnamed_enum_%d", unnamed_struct_count);
        unnamed_struct_count++;
        ident = malloc(strlen(buf)+1);
        memcpy(ident, buf, strlen(buf)+1);
        ident_len = strlen(buf);
    }

    Node *node = new_node(ND_TYPE_ENUM, NULL, NULL);

    if(consume("{")) {
        node->type.type = type_new_enum(ident, ident_len);
        node->type.type->members = enum_members();
        for(int i = 0; i < vector_size(enum_registry); i++) {
            EnumRegistryEntry *entry = vector_get(enum_registry, i);
            if(compare_ident(entry->ident, entry->ident_len, ident, ident_len)) {
                error_at(token->str, "Enum name is already defined: %.*s", ident_len, ident);
            }
        }
        EnumRegistryEntry *entry = calloc(1, sizeof(EnumRegistryEntry));
        entry->ident = ident;
        entry->ident_len = ident_len;
        entry->type = node->type.type;
        vector_push(enum_registry, entry);

        expect("}");
    } else {
        bool found = false;
        EnumRegistryEntry *entry;
        for(int i = 0; i < vector_size(enum_registry); i++) {
            entry = vector_get(enum_registry, i);
            if(compare_ident(entry->ident, entry->ident_len, ident, ident_len)) {
                found = true;
                break;
            }
        }
        if(!found) {
            error("No enum found for specified name");
        }
        node->type.type = entry->type;
    }

    return node;
}

// enum_members = ( ( ident ( "=" num )? "," )* ident ( "=" num )? )?
Vector *enum_members() {
    Vector *vec = new_vector();
    char *ident;
    int ident_len;
    int num = 0;
    while(consume_ident(&ident, &ident_len)) {
        EnumMember *member = calloc(1, sizeof(EnumMember));
        GVar *gvar = find_gvar(globals, ident, ident_len);
        if(gvar != NULL) {
            error("Name already defined");
        }

        if(consume("=")) {
            Node *node = constant_fold(constant_expression());
            if(node->kind != ND_NUM) {
                error_at(token->str, "Cannot use non-constant expression on enum.");
            }
            num = node->val;
        }

        gvar = new_gvar(globals, ident, ident_len, &signed_int_type, true);
        gvar->is_enum = true;
        gvar->enum_num = num;
        member->gvar = gvar;
        member->num = num;
        num++;
        vector_push(vec, member);
        if(!consume(",")) {
            break;
        }
    }
    return vec;
}

// typedef_declaration = "typedef" type ";"
Node *typedef_declaration(bool is_global, Node *type_node) {
    char *ident;
    int ident_len;
    type_find_ident(type_node, &ident, &ident_len);

    for(int i = 0; i < vector_size(typedef_registry); i++) {
        TypedefRegistryEntry *entry = vector_get(typedef_registry, i);
        if(compare_ident(ident, ident_len, entry->ident, entry->ident_len)) {
            error("Typedef name is already defined");
        }
    }
    TypedefRegistryEntry *entry = calloc(1, sizeof(TypedefRegistryEntry));
    entry->ident = ident;
    entry->ident_len = ident_len;
    entry->type = type_node->type.type;
    vector_push(typedef_registry, entry);

    return new_node(ND_TYPE_TYPEDEF, type_node, NULL);
}

bool consume_type_prefix(TokenKind *kind) {
    if(!peek_type_prefix())
        return false;

    *kind = token->kind;
    next_token();
    return true;
}

TokenKind expect_type_prefix() {
    TokenKind kind;
    if(!consume_type_prefix(&kind)) {
        error_at(token->str, "Not a type kind");
    }
    return kind;
}

bool peek_type_prefix() {
    switch(token->kind){
    case TK_VOID:
    case TK_CHAR:
    case TK_SHORT:
    case TK_INT:
    case TK_LONG:
    case TK_FLOAT:
    case TK_DOUBLE:
    case TK_SIGNED:
    case TK_UNSIGNED:
    case TK_BOOL:
    case TK_COMPLEX:
    case TK_STRUCT:
    case TK_UNION:
    case TK_ENUM:
    case TK_TYPEDEF:
    case TK_EXTERN:
    case TK_STATIC:
    case TK_AUTO:
    case TK_REGISTER:
    case TK_CONST:
    case TK_RESTRICT:
    case TK_VOLATILE:
    case TK_INLINE:
        return true;
    }
    char *ident;
    int ident_len;
    if(peek_ident(&ident, &ident_len)) {
        for(int i = 0; i < vector_size(typedef_registry); i++) {
            TypedefRegistryEntry *entry = vector_get(typedef_registry, i);
            if(compare_ident(entry->ident, entry->ident_len, ident, ident_len)) {
                return true;
            }
        }
    }
    return false;
}

Node *create_func_name_literal() {
    Node *node = new_node(ND_STRING_LITERAL, NULL, NULL);
    node->string_literal.literal = calloc(1, sizeof(StringLiteral));
    if(current_func == NULL) {
        error_at(token->str, "Cannot use __func__ out of function");
    }
    
    int len = current_func->func_def.ident_len;
    char *p = malloc(len + 3);
    p[0] = '"';
    memcpy(p + 1, current_func->func_def.ident, len);
    p[len + 1] = '"';
    p[len + 2] = '\0';
    node->string_literal.literal->str = p + 1;
    node->string_literal.literal->len = len;
    node->string_literal.literal->char_vec = new_vector();
    for(int i = 0; i < len; i++) {
        vector_push(node->string_literal.literal->char_vec, (void *)(long)p[i + 1]);
    }
    node->string_literal.literal->vec_len = vector_size(node->string_literal.literal->char_vec);
    node->string_literal.literal->index = vector_size(global_string_literals);
    vector_push(global_string_literals, node->string_literal.literal);

    return node;
}

/// LVar ///

LVar *find_lvar_scope(char *ident, int ident_len) {
    Node *cur_scope = scope;
    for(; cur_scope; cur_scope = cur_scope->scope.parent) {
        LVar *var = find_lvar_one(cur_scope->scope.locals, ident, ident_len);
        if(var != NULL) {
            return var;
        }
    }
    return NULL;
}

LVar *find_lvar_one(Vector *locals, char *ident, int ident_len) {
    for(int i = 0; i < vector_size(locals); i++){
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
    // Offset from rbp set by function prologue
    lvar->offset = locals_stack_size;
    vector_push(locals, lvar);

    return lvar;
}

int lvar_stack_size(Vector *locals) {
    int ret = 0;
    for(int i = 0; i < vector_size(locals); i++){
        LVar *var = vector_get(locals, i);
        ret += type_sizeof(var->type);
    }
    return ret;
}

Node *lvar_initializer_node(LVar *base_var, size_t offset, Node *init_expr, Type *type) {
    if(init_expr->kind == ND_INIT && !type_is_scalar(type)) {
        Node *init_code_node = new_node(ND_COMPOUND, NULL, NULL);
        init_code_node->compound_stmt_list = new_vector();
        int n = vector_size(init_expr->init.init_expr);
        if(type->ty == ARRAY) {
            if(type->has_array_size && n < type->array_size) {
                n = type->array_size;
            }
        }else if(type->ty == STRUCT) {
            if(n < vector_size(type->members)) {
                n = vector_size(type->members);
            }
        }else {
            error_at(token->str, "Unsupported type for initializer");
        }
        for(int i = 0; i < n; i++) {
            Node *expr = NULL;
            if(i < vector_size(init_expr->init.init_expr)) {
                expr = vector_get(init_expr->init.init_expr, i);
            } else {
                // Initialize remaining entries with dummy empty list.
                expr = new_node(ND_INIT, NULL, NULL);
                expr->init.init_expr = new_vector();
            }
            Node *lvar_node = new_node_lvar(base_var);
            Node *node;
            if(type->ty == ARRAY) {
                node = lvar_initializer_node(base_var, offset, expr, type->ptr_to);
                offset += type_sizeof(type->ptr_to);
            }else{
                StructMember *member = vector_get(type->members, i);
                node = lvar_initializer_node(base_var, offset + member->offset, expr, member->type);
            }

            vector_push(init_code_node->compound_stmt_list, node);
        }
        return init_code_node;
    }else {
        Node *expr = init_expr;
        if(expr->kind == ND_INIT) {
            if(vector_size(expr->init.init_expr)) {
                expr = vector_get(expr->init.init_expr, 0);
            }else {
                expr = new_node_num(0);
            }
        }
        Node *lvar_node = new_node_lvar(base_var);

        Node *addressof = new_node(ND_ADDRESS_OF, lvar_node, NULL);
        addressof->expr_type = type_new_ptr(&char_type);
        Node *deref_node = new_node(ND_DEREF, new_node_binop(ND_ADD, addressof, new_node_num(offset)), NULL);
        deref_node->expr_type = type;

        return new_node_assignment(deref_node, expr);
    }
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

GVar *new_gvar(Vector *globals, char *ident, int ident_len, Type *type, bool has_definition) {
    GVar *gvar = calloc(1, sizeof(GVar));
    gvar->name = ident;
    gvar->len = ident_len;
    gvar->type = type;
    gvar->has_definition = has_definition;
    vector_push(globals, gvar);

    return gvar;
}

Node *find_symbol(Vector *globals, char *ident, int ident_len) {
    LVar *lvar = find_lvar_scope(ident, ident_len);
    if(lvar == NULL){
        GVar *gvar = find_gvar(globals, ident, ident_len);
        if(gvar == NULL) {
            return NULL;
        }
        if(gvar->is_enum) {
            Node *node = new_node_num(gvar->enum_num);
            return node;
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
    if(type->ty == SHORT) {
        return 2;
    }
    if(type->ty == INT) {
        return 4;
    }
    if(type->ty == LONG) {
        return 8;
    }
    if(type->ty == LONGLONG) {
        return 8;
    }
    if(type->ty == FLOAT) {
        return 4;
    }
    if(type->ty == DOUBLE) {
        return 8;
    }
    if(type->ty == LONGDOUBLE) {
        return 16;
    }
    if(type->ty == BOOL) {
        return 1;
    }
    if(type->ty == COMPLEX) {
        error("Complex is not supported");
    }
    if(type->ty == STRUCT) {
        return type->struct_size;
    }
    if(type->ty == ENUM) {
        return 4;
    }
    return 8;
}

Type *type_arithmetic(Type *type_r, Type *type_l) {
    if(type_r->ty == PTR || type_r->ty == PTR){
        error_at(token->str, "Invalid arithmetic operand with ptr type");
        return NULL;
    }
    if(type_is_same(type_r, type_l)) {
        return type_r;
    }
    if(type_r->signedness == type_l->signedness) {
        if(type_int_conv_rank(type_r) < type_int_conv_rank(type_l)) {
            return type_l;
        }
        return type_r;
    }
    Type *uns, *sig;
    if(type_r->signedness == UNSIGNED) {
        uns = type_r;
        sig = type_l;
    }else {
        uns = type_l;
        sig = type_r;
    }
    if(type_int_conv_rank(uns) >= type_int_conv_rank(sig)) {
        return uns;
    }
    if(type_sizeof(sig) > type_sizeof(uns)) {
        return sig;
    }

    if(sig->ty == INT) {
        return &unsigned_int_type;
    }
    if(sig->ty == LONG) {
        return &unsigned_long_type;
    }
    if(sig->ty == LONGLONG) {
        return &unsigned_longlong_type;
    }
    assert(false);
}

Node *type_comparator(Node *node, Type *type_l, Type *type_r) {
    node->expr_type = &signed_int_type;
    if(type_r->ty == PTR && node->lhs->kind == ND_NUM && node->lhs->val == 0) {
        node->lhs = new_node_conv(node->lhs, type_new_ptr(&void_type));
        return node;
    }
    if(type_l->ty == PTR && node->rhs->kind == ND_NUM && node->rhs->val == 0){
        node->rhs = new_node_conv(node->rhs, type_new_ptr(&void_type));
        return node;
    }
    if(type_r->ty == PTR && type_l->ty != PTR ||
            type_l->ty == PTR && type_r->ty != PTR){
        error_at(token->str, "Invalid comparison between ptr and non-ptr");
        return NULL;
    }
    if(type_r->ty == PTR && type_l->ty == PTR) {
        if(!type_is_same(type_r, type_l) && type_r->ptr_to->ty != VOID && type_l->ptr_to->ty != VOID) {
            error_at(token->str, "Invalid comparison between incompatible ptrs");
            return NULL;
        }
    }
    if(type_is_arithmetic(type_r) && type_is_arithmetic(type_l)) {
        Type *target_type = type_arithmetic(type_r, type_l);
        if(!type_is_same(node->lhs->expr_type, target_type)) {
            node->lhs = new_node_conv(node->lhs, target_type);
        }
        if(!type_is_same(node->rhs->expr_type, target_type)) {
            node->rhs = new_node_conv(node->rhs, target_type);
        }
    }
    return node;
}

Type *type_bitwise(Type *type_r, Type *type_l) {
    if(!type_is_int(type_r) || !type_is_int(type_l)) {
        error_at(token->str, "Invalid bitwise evaluation of non integer type");
        return NULL;
    }
    return type_arithmetic(type_r, type_l);
}

Type *type_shift(Type *type_r, Type *type_l) {
    if(!type_is_int(type_r) || !type_is_int(type_l)) {
        error_at(token->str, "Invalid shift evaluation of non integer type");
        return NULL;
    }
    return type_arithmetic(type_r, type_l);
}

bool type_implicit_ptr(Type *type) {
    return type->ty == ARRAY || type->ty == PTR;
}

bool type_is_int(Type *type) {
    return type->ty == CHAR || type->ty == SHORT || type->ty == INT ||
        type->ty == LONG || type->ty == LONGLONG || type->ty == ENUM || type->ty == BOOL;
}

bool type_is_floating(Type *type) {
    return type->ty == FLOAT || type->ty == DOUBLE || type->ty == LONGDOUBLE;
}

bool type_is_basic(Type *type) {
    return type_is_int(type) || type_is_floating(type);
}

bool type_is_scalar(Type *type) {
    return type_is_basic(type) || type->ty == PTR;
}

bool type_is_ptr_array(Type *type) {
    return type->ty == PTR || type->ty == ARRAY;
}

bool type_is_same(Type *type_a, Type *type_b) {
    while(1) {
        if(type_a->ty != type_b->ty) {
            return false;
        }
        if(type_a->ty == PTR || type_a->ty == ARRAY){
            type_a = type_a->ptr_to;
            type_b = type_b->ptr_to;
            continue;
        }
        if(type_a->ty == STRUCT || type_a->ty == UNION || type_a->ty == ENUM) {
            return compare_ident(type_a->ident, type_a->ident_len, type_b->ident, type_b->ident_len);
        }
        if(type_a->ty == FUNC) {
            // TODO: Argument type check
            // Currently only check return type.
            type_a = type_a->ptr_to;
            type_b = type_b->ptr_to;
            continue;
        }
        return type_a->signedness == type_b->signedness;
    }
}

bool type_is_compatible(Type *type_a, Type *type_b) {
    if(type_is_same(type_a, type_b)) {
        return true;
    }
    if(type_is_int(type_a) && type_is_int(type_b)) {
        return true;
    }
    return false;
}

bool type_is_signed(Type *type) {
    assert(type_is_int(type));
    return type->signedness == SIGNED || (type->ty == CHAR && type->signedness == NOSIGNED);
}

bool type_is_arithmetic(Type *type) {
    return type_is_int(type) || type_is_floating(type);
}

Type *type_new_ptr(Type *type) {
    Type *ptr_type = calloc(1, sizeof(Type));
    ptr_type->ty = PTR;
    ptr_type->ptr_to = type;
    return ptr_type;
}

Type *type_new_array(Type *type, bool has_size, int size) {
    Type *array_type = calloc(1, sizeof(Type));
    array_type->ty = ARRAY;
    array_type->ptr_to = type;
    array_type->array_size = size;
    array_type->has_array_size = has_size;
    return array_type;
}

Type *type_new_func(Type *type, Vector *args, bool is_vararg) {
    Type *func_type = calloc(1, sizeof(Type));
    func_type->ty = FUNC;
    func_type->args = args;
    func_type->ptr_to = type;
    func_type->is_vararg = is_vararg;
    return func_type;
}

Type *type_new_struct(char *ident, int ident_len) {
    Type *struct_type = calloc(1, sizeof(Type));
    struct_type->ty = STRUCT;
    struct_type->ident = ident;
    struct_type->ident_len = ident_len;
    return struct_type;
}

Type *type_new_enum(char *ident, int ident_len) {
    Type *struct_type = calloc(1, sizeof(Type));
    struct_type->ty = ENUM;
    struct_type->ident = ident;
    struct_type->ident_len = ident_len;
    return struct_type;
}

bool type_find_ident(Node *node, char **ident, int *ident_len) {
    Node *cur = node;
    for(; cur != NULL; cur = cur->lhs) {
        if(cur->kind == ND_IDENT) {
            *ident = cur->ident.ident;
            *ident_len = cur->ident.ident_len;
            return true;
        }
    }
    return false;
}

int type_int_conv_rank(Type *type) {
    assert(type_is_int(type));

    switch(type->ty) {
        case BOOL: return 1;
        case CHAR: return 2;
        case SHORT: return 3;
        case INT: return 4;
        case LONG: return 5;
        case LONGLONG: return 6;
        case ENUM: return 4;
    }
    assert(false);
}

static int type_dump_inner(Type *type, Buffer *buf) {
    for(; type; type = type->ptr_to) {
        if(type->ty == PTR) {
            append_printf(buf, "* ");
        }else if(type->ty == FUNC) {
            append_printf(buf, "(");
            for(int i = 0; i < vector_size(type->args); i++) {
                Node *decl_var_node = vector_get(type->args, i);
                Node *type_node = decl_var_node->lhs;
                type_dump_inner(type_node->type.type, buf);
                if(i != vector_size(type->args) - 1) {
                    append_printf(buf, ", ");
                }
            }
            append_printf(buf, ") ");
        }else if(type->ty == ARRAY) {
            if(type->has_array_size) {
                append_printf(buf, "[%ld] ", type->array_size);
            }else{
                append_printf(buf, "[] ");
            }
        }else if(type->ty == STRUCT) {
            append_printf(buf, "struct %.*s", type->ident_len, type->ident);
        }else if(type->ty == UNION) {
            append_printf(buf, "union %.*s", type->ident_len, type->ident);
        }else if(type->ty == ENUM) {
            append_printf(buf, "enum %.*s", type->ident_len, type->ident);
        }else{
            if(type->signedness == UNSIGNED) {
                append_printf(buf, "unsigned ");
            }else if(type->signedness == SIGNED) {
                append_printf(buf, "signed ");
            }
            switch(type->ty) {
                case VOID: append_printf(buf, "void"); break;
                case CHAR: append_printf(buf, "char"); break;
                case SHORT: append_printf(buf, "short"); break;
                case INT: append_printf(buf, "int"); break;
                case LONG: append_printf(buf, "long"); break;
                case LONGLONG: append_printf(buf, "long long"); break;
                case FLOAT: append_printf(buf, "foat"); break;
                case DOUBLE: append_printf(buf, "double"); break;
                case LONGDOUBLE: append_printf(buf, "long double"); break;
                case BOOL: append_printf(buf, "_Bool"); break;
                case COMPLEX: append_printf(buf, "_Complex"); break;
                default: append_printf(buf, "unknown"); break;
            }
        }
    }
}

int type_dump(Type *type, char **out) {
    Buffer *buf = init_buffer();
    type_dump_inner(type, buf);
    *out = buf->buf;
    return buf->len;
}

StructMember *find_struct_member(Vector *member_list, char *ident, int ident_len, size_t *offset) {
    StructMember *mem;
    for(int i = 0; i < vector_size(member_list); i++) {
        mem = vector_get(member_list, i);
        if(mem->unnamed) {
            // Recursively examine unnamed member.
            size_t rec_offset = 0;
            StructMember *memr = find_struct_member(mem->type->members, ident, ident_len, &rec_offset);
            if(memr) {
                *offset = rec_offset + mem->offset;
                return memr;
            }
            continue;
        }
        if(compare_ident(ident, ident_len, mem->ident, mem->ident_len)) {
            *offset = mem->offset;
            return mem;
        }
    }
    return NULL;
}

void print_indent(int level, const char *fmt, ...) {
    for(int i = 0; i < level; i++) {
        fprintf(stderr, "  ");
    }
    va_list ap;
    va_start(ap, fmt);

    vfprintf(stderr, fmt, ap);

    va_end(ap);
}

bool compare_ident(char *ident_a, int ident_a_len, char *ident_b, int ident_b_len) {
    if(ident_a_len != ident_b_len) {
        //debug_log("size: %d %d\n", ident_a_len, ident_b_len);
        return false;
    }
    return strncmp(ident_a, ident_b, ident_a_len) == 0;
}

bool compare_slice(char *slice, int slice_len, char *null_term_str) {
    int len = strlen(null_term_str);
    if(len != slice_len) {
        return false;
    }
    return memcmp(slice, null_term_str, slice_len) == 0;
}

void dumpnodes_inner(Node *node, int level) {
    if(node == NULL) return;
    print_indent(level, "%s\n", node_kind(node->kind));
    char *out;
    type_dump(node->expr_type, &out);
    print_indent(level, " (expr_type: %s)\n", out);

    if(node->kind == ND_LVAR){
        print_indent(level, "name: ");
        fwrite(node->lvar->name, node->lvar->len, 1, stderr);
        fprintf(stderr, "\n");
    }else if(node->kind == ND_GVAR_DEF){
        print_indent(level, "ND_GVAR_DEF: %.*s\n", node->gvar_def.gvar->len, node->gvar_def.gvar->name);
    }else if(node->kind == ND_DECL_LIST){
        for(int i = 0; i < vector_size(node->decl_list.decls); i++) {
            Node *decl = vector_get(node->decl_list.decls, i);

            dumpnodes_inner(decl, level + 1);
        }
    }else if(node->kind == ND_TYPE){
        char *out;
        type_dump(node->type.type, &out);
        print_indent(level+1, "%s\n", out);
        dumpnodes_inner(node->lhs, level+1);
    }else if(node->kind == ND_IDENT){
        print_indent(level+1, "ident: %.*s\n", node->ident.ident_len, node->ident.ident);
    }else if(node->kind == ND_TYPE_ARRAY){
        if(node->type.array.has_size) {
            print_indent(level, " size: %ld\n", node->type.array.size);
        } else {
            print_indent(level, " size: unspecified\n");
        }
        dumpnodes_inner(node->lhs, level + 1);
        dumpnodes_inner(node->rhs, level + 1);
    }else if(node->kind == ND_NUM){
        print_indent(level, " value: %lu\n", node->val);
    }else if(node->kind == ND_STRING_LITERAL){
        print_indent(level, " value: \"%.*s\"\n", node->string_literal.literal->len, node->string_literal.literal->str);
    }else if(node->kind == ND_IF){
        print_indent(level, " // if condition\n");
        dumpnodes_inner(node->lhs, level + 1);
        print_indent(level, " // if stmt\n");
        dumpnodes_inner(node->rhs, level + 1);
        print_indent(level, " // else\n");
        dumpnodes_inner(node->else_stmt, level + 1);
    }else if(node->kind == ND_FOR){
        print_indent(level, " // for init\n");
        dumpnodes_inner(node->lhs, level + 1);
        print_indent(level, " // for condition\n");
        dumpnodes_inner(node->rhs, level + 1);
        print_indent(level, " // for update\n");
        dumpnodes_inner(node->for_update_expr, level + 1);
        print_indent(level, " // for body\n");
        fprintf(stderr, "%*s// for body\n", (level+1)*2, " ");
        dumpnodes_inner(node->for_stmt, level + 1);
    }else if(node->kind == ND_COMPOUND){
        for(int i = 0; i < vector_size(node->compound_stmt_list); i++){
            dumpnodes_inner(vector_get(node->compound_stmt_list, i), level + 1);
        }
    }else if(node->kind == ND_CALL){
        NodeList *cur = node->call_arg_list.next;
        for(; cur; cur = cur->next){
            print_indent(level, " // call arg\n");
            dumpnodes_inner(cur->node, level + 1);
        }
    }else if(node->kind == ND_FUNC_DEF){
        char *out;
        type_dump(node->func_def.type, &out);
        print_indent(level, " name: %.*s %s\n", node->func_def.ident_len, node->func_def.ident, out);

        dumpnodes_inner(node->lhs, level + 1);
        dumpnodes_inner(node->rhs, level + 1);
    }else if(node->kind == ND_CONVERT){
        char *out;
        type_dump(node->expr_type, &out);
        print_indent(level, " to: %s\n", out);
        dumpnodes_inner(node->lhs, level + 1);
    }else{
        dumpnodes_inner(node->lhs, level + 1);
        dumpnodes_inner(node->rhs, level + 1);
    }
}

void dumpnodes(Node *node) {
    dumpnodes_inner(node, 0);
}

char *mystrdup(char *p) {
    int len = strlen(p);
    char *q = malloc(len + 1);
    memcpy(q, p, len + 1);
    return q;
}
