#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "rrcc.h"

int cur_label = 0;
static const char *args_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

int stack_base = 0;
int switch_number = 0;
int current_break_target = 0;

int stack_align(int size) {
    return (size + 7) & ~7;
}

char *access_size(int size) {
    if(size == 1) return "byte ptr";
    if(size == 2) return "word ptr";
    if(size == 4) return "dword ptr";
    if(size == 8) return "qword ptr";
    return "unknown";
}

char *dump_initializer(int size, Vector *init_expr) {
    char *buf = calloc(1, size * vector_size(init_expr));
    char *p = buf;
    for(int i = 0; i < vector_size(init_expr); i++){
        Node *expr = vector_get(init_expr, i);
        if(type_is_int(expr->expr_type)) {
            long long val = expr->val;
            memcpy(p, &val, size);
            p += size;
        }
    }
    return buf;
}

// lvar->offset indicates storage size in byte which the variables above this variable occupy.
// When we use rbp - (sub offset) to access this variable, we must add the size of this variable.
int get_stack_sub_offset(LVar *lvar) {
    return lvar->offset + type_sizeof(lvar->type);
}

void gen_lvar(Node *node) {
    if(node->kind == ND_LVAR) {
        printf("  // access %.*s\n", node->lvar->len, node->lvar->name);
        printf("  mov rax, rbp\n");
        printf("  sub rax,%d\n", get_stack_sub_offset(node->lvar));
        printf("  push rax\n");
    }else if(node->kind == ND_GVAR) {
        printf("  lea rax, [rip + %.*s]\n", node->gvar.gvar->len, node->gvar.gvar->name);
        printf("  push rax\n");
    } else if(node->kind == ND_DEREF) {
        gen(node->lhs);
    }
}

void load(int size) {
    printf("  pop rax\n");
    switch(size) {
        case 1:
            printf("  mov al, %s[rax]\n", access_size(size));
            printf("  movsx rax, al\n");
            break;
        case 2:
            printf("  mov ax, %s[rax]\n", access_size(size));
            printf("  movsx rax, ax\n");
            break;
        case 4:
            printf("  mov eax, %s[rax]\n", access_size(size));
            printf("  movsx rax, eax\n");
            break;
        case 8:
            printf("  mov rax, %s[rax]\n", access_size(size));
            break;
    }
    printf("  push rax\n");
}

void store(int size) {
    printf("  pop rbx\n");
    printf("  pop rax\n");
    switch(size) {
        case 1:
            printf("  mov %s [rax], bl\n", access_size(size));
            break;
        case 2:
            printf("  mov %s [rax], bx\n", access_size(size));
            break;
        case 4:
            printf("  mov %s [rax], ebx\n", access_size(size));
            break;
        case 8:
            printf("  mov %s [rax], rbx\n", access_size(size));
            break;
    }
    printf("  push rbx\n");
}

void gen_return() {
    printf("  pop rax\n");
    printf("  mov rsp,rbp\n");
    printf("  pop rbx\n");
    printf("  pop r15\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

void gen_string_literals() {
    for(int i = 0; i < vector_size(global_string_literals); i++) {
        StringLiteral *literal = vector_get(global_string_literals, i);
        printf(".L_S_%d:\n", literal->index);
        printf("  .string \"%.*s\"\n", literal->len, literal->str);
    }
}

void gen(Node *node){
    switch(node->kind){
        case ND_NUM:
            printf("  push %d\n", node->val);
            return;
        case ND_STRING_LITERAL:
            printf("  lea rax, .L_S_%d[rip]\n", node->string_literal.literal->index);
            printf("  push rax\n");
            return;
        case ND_LVAR: // fall through
        case ND_GVAR:
            gen_lvar(node);
            if(node->expr_type->ty != ARRAY) {
                load(type_sizeof(node->expr_type));
            }
            return;
        case ND_ADDRESS_OF:
            gen_lvar(node->lhs);
            return;
        case ND_DEREF:
            gen(node->lhs);
            load(type_sizeof(node->expr_type));
            return;
        case ND_BIT_NOT:
            gen(node->lhs);
            printf("  pop rax\n");
            printf("  not rax\n");
            printf("  push rax\n");
            return;
        case ND_POSTFIX_INC:
        case ND_POSTFIX_DEC:
            gen_lvar(node->lhs);
            printf("  pop rax\n");
            printf("  mov rsi, rax\n");
            printf("  push rax\n");
            load(type_sizeof(node->expr_type));
            printf("  pop rax\n");
            printf("  mov rcx, rax\n");
            printf("  add rax, %ld\n", node->incdec.value);
            printf("  push rsi\n");
            printf("  push rax\n");
            store(type_sizeof(node->expr_type));
            printf("  pop rax\n");
            printf("  push rcx\n");
            return;
        case ND_PREFIX_INC:
        case ND_PREFIX_DEC:
            gen_lvar(node->lhs);
            printf("  pop rax\n");
            printf("  mov rsi, rax\n");
            printf("  push rax\n");
            load(type_sizeof(node->expr_type));
            printf("  pop rax\n");
            printf("  add rax, %ld\n", node->incdec.value);
            printf("  mov rcx, rax\n");
            printf("  push rsi\n");
            printf("  push rax\n");
            store(type_sizeof(node->expr_type));
            printf("  pop rax\n");
            printf("  push rcx\n");
            return;
        case ND_ASSIGN:
            gen_lvar(node->lhs);
            gen(node->rhs);

            store(type_sizeof(node->lhs->expr_type));
            return;
        case ND_RETURN:
            if(node->lhs) {
                gen(node->lhs);
            }else {
                printf("  push 0\n");
            }
            gen_return();
            return;
        case ND_IF: {
            // if statement pushes value of executed statement.
            printf("  # if cond\n");
            gen(node->lhs);
            printf("  pop rax\n");
            printf("  test rax,rax\n");
            int label = ++cur_label;
            printf("  jz .L%d\n", label);
            printf("  # if stmt\n");
            gen(node->rhs);

            printf("  # else%s\n", node->else_stmt ? "" : " empty");
            int label_skip_else = ++cur_label;
            printf("  jmp .L%d\n", label_skip_else);
            printf(".L%d:\n", label);
            if(node->else_stmt) {
                gen(node->else_stmt);
            }else{
                printf("  push 0  # dummy else statement\n");
            }
            printf(".L%d:\n", label_skip_else);
            printf("  # if end\n");
            return;
        }
        case ND_SWITCH: {
            // TODO: Check stack position for jump target?
            // condition expression
            int cur = ++switch_number;
            int break_target = ++current_break_target;
            printf("  // switch %d\n", cur);
            gen(node->lhs);
            printf("  pop rax\n");
            for(int i = 0; i < vector_size(node->switch_.cases); i++){
                Node *case_node = vector_get(node->switch_.cases, i);
                int64_t v = case_node->rhs->val;
                printf("  cmp rax, %ld\n", v);
                printf("  jz .Lswitch_%d_%ld\n", cur, v);
            }
            if(node->switch_.default_stmt) {
                printf("  jmp .Lswitch_%d_default\n", cur);
            }else {
                printf("  jmp .Lswitch_%d_end\n", cur);
            }
            gen(node->rhs);
            printf("  pop rax\n");
            printf("  .Lswitch_%d_end:\n", cur);
            printf("  .Lbreak_%d:\n", break_target);
            printf("  push rax\n");

            return;
        }
        case ND_CASE: {
            printf("  .Lswitch_%d_%d:\n", switch_number, node->rhs->val);
            gen(node->lhs);
            return;
        }
        case ND_DEFAULT: {
            printf("  .Lswitch_%d_default:\n", switch_number);
            gen(node->lhs);
            return;
        }
        case ND_BREAK: {
            printf("  jmp .Lbreak_%d\n", current_break_target);
            return;
        }
        case ND_FOR: {
            gen(node->lhs);
            printf("  pop rax\n");
            int label_for = ++cur_label;
            printf(".L%d:\n", label_for);
            gen(node->rhs);
            printf("  pop rax\n");
            printf("  test rax,rax\n");
            int label = ++cur_label;
            printf("  jz .L%d\n", label);
            gen(node->for_stmt);
            printf("  pop rax\n");
            gen(node->for_update_expr);
            printf("  pop rax\n");
            printf("  jmp .L%d\n", label_for);
            printf(".L%d:\n", label);
            printf("  push rax\n");
            return;
        }
        case ND_WHILE: {
            int label_while = ++cur_label;
            int label_while_end = ++cur_label;
            printf(".L%d:\n", label_while);
            gen(node->lhs);
            printf("  pop rax\n");
            printf("  test rax,rax\n");
            printf("  jz .L%d\n", label_while_end);
            gen(node->rhs);
            printf("  pop rax\n");
            printf("  jmp .L%d\n", label_while);
            printf(".L%d:\n", label_while_end);
            printf("  push rax\n");
            return;
        }
        case ND_DO: {
            int label_do = ++cur_label;
            printf(".L%d:\n", label_do);
            gen(node->lhs);
            printf("  pop rax\n");
            gen(node->rhs);
            printf("  pop rax\n");
            printf("  test rax,rax\n");
            printf("  jnz .L%d\n", label_do);
            printf("  push rax\n");
            return;
        }
        case ND_COMPOUND:
            for(int i = 0; i < vector_size(node->compound_stmt_list); i++){
                gen(vector_get(node->compound_stmt_list, i));
                printf("  pop rax\n");
            }
            printf("  push rax\n");
            return;
        case ND_CALL: {
            NodeList *cur = node->call_arg_list.next;
            int reg_count = sizeof(args_regs) / sizeof(args_regs[0]);
            int i;
            for(i = 0; cur; cur = cur->next, i++){
                gen(cur->node);
            }
            cur = node->call_arg_list.next;
            for(int j = i-1; j >= 0; j--){
                printf("  pop rax\n");
                if(j < reg_count){
                    printf("  mov %s,rax\n", args_regs[j]);
                }else{
                    error("call argument >= %d is not supported.", reg_count);
                }
            }
            printf("  mov r15,rsp\n");
            printf("  and rsp,~0xf\n");
            // Number of floating point argument
            printf("  mov al,0\n");
            printf("  call %.*s\n", node->call_ident_len, node->call_ident);
            printf("  mov rsp,r15\n");
            printf("  push rax\n");
            return;
        }    
        case ND_TYPE:
            // nop
            return;
        case ND_TYPE_TYPEDEF:
            // nop
            return;
        case ND_FUNC_DECL:
            // nop
            return;
        case ND_FUNC_DEF:
            printf(".text\n");
            printf(".globl %.*s\n", node->func_def.ident_len, node->func_def.ident);
            printf("%.*s:\n", node->func_def.ident_len, node->func_def.ident);
            int size = vector_size(node->func_def.arg_vec);
            printf("  push rbp\n");
            printf("  push r15\n");
            printf("  push rbx\n");
            printf("  mov rbp,rsp\n");
            printf("  sub rsp,%d\n", stack_align(node->func_def.max_stack_size));
            for(int i = 0; i < size; i++){
                FuncDefArg *arg = vector_get(node->func_def.arg_vec, i);
                printf("  lea rax, [rbp-%d]\n", get_stack_sub_offset(arg->lvar));
                printf("  push rax\n");
                printf("  push %s\n", args_regs[i]);
                store(type_sizeof(arg->type));
                printf("  pop rax\n");
            }
            gen(node->lhs);
            gen_return();
            return;
        case ND_SCOPE: {
            gen(node->lhs);
            return; }
        case ND_DECL_VAR: {
            // Dummy element
            if(node->rhs) {
                gen(node->rhs);
            } else {
                printf("  mov rax,0\n");
                printf("  push rax\n");
            }

            return;
        }
        case ND_GVAR_DEF:
            printf(".data\n");
            printf(".globl %.*s\n", node->gvar_def.gvar->len, node->gvar_def.gvar->name);
            printf("%.*s:\n", node->gvar_def.gvar->len, node->gvar_def.gvar->name);
            int zero_size = type_sizeof(node->gvar_def.gvar->type);
            if(node->gvar_def.init_expr) {
                int size = type_sizeof(node->gvar_def.gvar->type->ptr_to);
                int len = vector_size(node->gvar_def.init_expr->init.init_expr);
                char *buf = dump_initializer(size, node->gvar_def.init_expr->init.init_expr);
                for(int i = 0; i < size*len; i++){
                    printf("  .byte %d\n", (unsigned char)buf[i]);
                }
                zero_size -= size * len;
            }
            if(zero_size) {
                printf("  .zero %d\n", zero_size);
            }
            return;
        case ND_CONVERT:
            if(node->lhs->expr_type->ty == ARRAY && node->expr_type->ty == PTR) {
                gen(node->lhs);
            } else {
                error("Conversion unsupported for type %d and %d", node->lhs->expr_type->ty, node->expr_type->ty);
            }
            return;
        case ND_TYPE_EXTERN:
            // nop
            return;
        case ND_DECL_LIST:
            for(int i = 0; i < vector_size(node->decl_list.decls); i++) {
                Node *decl = vector_get(node->decl_list.decls, i);
                gen(decl);
            }
            return;
        case ND_CAST:
            gen(node->lhs);
            return;
    }
    gen(node->lhs);
    gen(node->rhs);
    printf("  pop rsi\n");
    printf("  pop rax\n");
    switch(node->kind){
        case ND_ADD:
            printf("  // add type:%d\n", node->lhs->expr_type->ty);
            if (node->lhs->expr_type->ty == PTR) {
                // ptr + int
                int size = type_sizeof(node->lhs->expr_type->ptr_to);
                printf("  mov rcx,rax\n");
                printf("  mov rax,%d\n", size);
                printf("  mul rsi\n");
                printf("  add rax,rcx\n");
            } else {
                printf("  add rax,rsi\n");
            }
            break;
        case ND_SUB:
            printf("  // sub\n");
            if (node->lhs->expr_type->ty == PTR) {
                if(node->rhs->expr_type->ty == PTR) {
                    // ptr - ptr
                    int size = type_sizeof(node->lhs->expr_type->ptr_to);
                    printf("  sub rax,rsi\n");
                    printf("  cqto\n");
                    printf("  mov rcx,%d\n", size);
                    printf("  div rcx\n");
                } else {
                    // ptr - int
                    int size = type_sizeof(node->lhs->expr_type->ptr_to);
                    printf("  mov rcx,rax\n");
                    printf("  mov rax,%d\n", size);
                    printf("  mul rsi\n");
                    printf("  sub rcx,rax\n");
                    printf("  mov rax,rcx\n");
                }
            } else {
                // int - int
                printf("  sub rax,rsi\n");
            }
            break;
        case ND_MUL:
            printf("  cqto\n");
            printf("  mul rsi\n");
            break;
        case ND_DIV:
            printf("  cqto\n");
            printf("  div rsi\n");
            break;
        case ND_MOD:
            printf("  cqto\n");
            printf("  div rsi\n");
            printf("  mov rax, rdx\n");
            break;
        case ND_EQUAL:
            printf("  cmp rax,rsi\n");
            printf("  sete al\n");
            printf("  movzb rax,al\n");
            break;
        case ND_NOT_EQUAL:
            printf("  cmp rax,rsi\n");
            printf("  setne al\n");
            printf("  movzb rax,al\n");
            break;
        case ND_LESS:
            printf("  cmp rax,rsi\n");
            printf("  setl al\n");
            printf("  movzb rax,al\n");
            break;
        case ND_LESS_OR_EQUAL:
            printf("  cmp rax,rsi\n");
            printf("  setle al\n");
            printf("  movzb rax,al\n");
            break;
        case ND_GREATER:
            printf("  cmp rax,rsi\n");
            printf("  setg al\n");
            printf("  movzb rax,al\n");
            break;
        case ND_GREATER_OR_EQUAL:
            printf("  cmp rax,rsi\n");
            printf("  setge al\n");
            printf("  movzb rax,al\n");
            break;
        case ND_OR:
            printf("  or rax,rsi\n");
            break;
        case ND_XOR:
            printf("  xor rax,rsi\n");
            break;
        case ND_AND:
            printf("  and rax,rsi\n");
            break;
        case ND_LSHIFT:
            printf("  mov rcx,rsi\n");
            printf("  shl rax,cl\n");
            break;
        case ND_RSHIFT:
            printf("  mov rcx,rsi\n");
            printf("  shr rax,cl\n");
            break;
        case ND_COMMA_EXPR:
            printf("  mov rax, rsi\n");
            break;
    }
    printf("  push rax\n");
}
