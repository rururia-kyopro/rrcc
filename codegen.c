#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "9cc.h"

int cur_label = 0;

void gen_lvar(Node *node) {
    if(node->kind != ND_LVAR)
        error("lhs of assignment is not a variable");
    
    printf("  mov rax, rbp\n");
    printf("  sub rax,%d\n", node->lvar->offset);
    printf("  push rax\n");
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
            printf("  pop rbp\n");
            printf("  ret\n");
            return;
        case ND_IF: {
            gen(node->lhs);
            printf("  pop rax\n");
            printf("  test rax,rax\n");
            int label = ++cur_label;
            printf("  jz .L%d\n", label);
            gen(node->rhs);
            if (node->else_stmt) {
                int label_skip_else = ++cur_label;
                printf("  jmp .L%d\n", label_skip_else);
                printf(".L%d:\n", label);
                gen(node->else_stmt);
                printf(".L%d:\n", label_skip_else);
            }else{
                printf(".L%d:\n", label);
            }
            return;
        }
        case ND_COMPOUND:
            for(int i = 0; node->compound_stmt_list[i]; i++){
                gen(node->compound_stmt_list[i]);
                printf("  pop rax\n");
            }
            printf("  push rax\n");
            return;
    }
    gen(node->lhs);
    gen(node->rhs);
    printf("  pop rsi\n");
    printf("  pop rax\n");
    switch(node->kind){
        case ND_ADD:
            printf("  add rax,rsi\n");
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
