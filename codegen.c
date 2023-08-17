#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "9cc.h"

int cur_label = 0;
static const char *args_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

void gen_lvar(Node *node) {
    if(node->kind == ND_LVAR) {
        printf("  mov rax, rbp\n");
        printf("  sub rax,%d\n", node->lvar->offset);
        printf("  push rax\n");
    } else if(node->kind == ND_DEREF) {
        gen(node->lhs);
    }
}

void gen(Node *node){
    switch(node->kind){
        case ND_NUM:
            printf("  push %d\n", node->val);
            return;
        case ND_LVAR:
            gen_lvar(node);
            printf("  pop rax\n");
            printf("  mov rax, [rax]\n");
            printf("  push rax\n");
            return;
        case ND_ADDRESS_OF:
            gen_lvar(node->lhs);
            return;
        case ND_DEREF:
            gen(node->lhs);
            printf("  pop rax\n");
            printf("  mov rax, [rax]\n");
            printf("  push rax\n");
            return;
        case ND_ASSIGN:
            gen_lvar(node->lhs);
            gen(node->rhs);

            printf("  pop rdi\n");
            printf("  pop rax\n");
            printf("  mov [rax], rdi\n");
            printf("  push rdi\n");
            return;
        case ND_RETURN:
            gen(node->lhs);
            printf("  pop rax\n");
            printf("  mov rsp,rbp\n");
            printf("  pop r15\n");
            printf("  pop rbp\n");
            printf("  ret\n");
            return;
        case ND_IF: {
            printf("  # if\n");
            gen(node->lhs);
            printf("  pop rax\n");
            printf("  push rax\n");
            printf("  test rax,rax\n");
            int label = ++cur_label;
            printf("  jz .L%d\n", label);
            gen(node->rhs);
            printf("  pop rax\n");
            if (node->else_stmt) {
                int label_skip_else = ++cur_label;
                printf("  jmp .L%d\n", label_skip_else);
                printf(".L%d:\n", label);
                gen(node->else_stmt);
                printf("  pop rax\n");
                printf(".L%d:\n", label_skip_else);
            }else{
                printf(".L%d:\n", label);
            }
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
            for(int i = 0; node->compound_stmt_list[i]; i++){
                gen(node->compound_stmt_list[i]);
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
            printf("  call %.*s\n", node->call_ident_len, node->call_ident);
            printf("  mov rsp,r15\n");
            printf("  push rax\n");
            return;
        }
        case ND_FUNC_DEF:
            printf(".globl %.*s\n", node->func_def_ident_len, node->func_def_ident);
            printf("%.*s:\n", node->func_def_ident_len, node->func_def_ident);
            int size = vector_size(node->func_def_arg_vec);
            printf("  push rbp\n");
            printf("  push r15\n");
            printf("  mov rbp,rsp\n");
            printf("  sub rsp,%d*8\n", lvar_count(node->func_def_lvar));
            for(int i = 0; i < size; i++){
                FuncDefArg *arg = vector_get(node->func_def_arg_vec, i);
                printf("  mov [rbp-%d], %s\n", arg->lvar->offset, args_regs[i]);
            }
            gen(node->lhs);
            printf("  pop rax\n");
            printf("  mov rsp,rbp\n");
            printf("  pop r15\n");
            printf("  pop rbp\n");
            printf("  ret\n");
            return;
        case ND_DECL_VAR:
            // Dummy element
            printf("  mov rax,0\n");
            printf("  push rax\n");
            return;
    }
    gen(node->lhs);
    gen(node->rhs);
    printf("  pop rsi\n");
    printf("  pop rax\n");
    switch(node->kind){
        case ND_ADD:
            printf("  // add has_type:%d has_type:%d\n", node->lhs->expr_type != NULL, node->rhs->expr_type != NULL);
            if (node->lhs->expr_type && node->lhs->expr_type->ty == PTR) {
                int size;
                if(node->lhs->expr_type->ptr_to->ty == INT) {
                    size = 4;
                }else{
                    size = 8;
                }
                printf("  mov rcx,rax\n");
                printf("  mov rax,%d\n", size);
                printf("  mul rsi\n");
                printf("  add rax,rcx\n");
            } else {
                printf("  add rax,rsi\n");
            }
            break;
        case ND_SUB:
            printf("  sub rax,rsi\n");
            break;
        case ND_MUL:
            printf("  cqto\n");
            printf("  mul rsi\n");
            break;
        case ND_DIV:
            printf("  cqto\n");
            printf("  div rsi\n");
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
    }
    printf("  push rax\n");
}
