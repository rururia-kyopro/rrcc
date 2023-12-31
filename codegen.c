#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include "rrcc.h"

int cur_label = 0;
static const int args_reg_len = 6;
static const char *args_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};

int stack_base = 0;
int switch_number = 0;
int current_break_target = 0;
Vector *break_target_vec;
int current_continue_target = 0;
Vector *continue_target_vec;
int reserverd_stack_size = 0;
Vector *file_no_vec;

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

void gen_initexpr(Type *type, Node *init_expr_node) {
    int zero_size = type_sizeof(type);
    if(init_expr_node) {
        if(type_is_scalar(type)) {
            int size = type_sizeof(type);
            if(init_expr_node->kind == ND_NUM) {
                int64_t val = init_expr_node->val;
                char *buf = (char *)&val;
                for(int i = 0; i < size; i++){
                    printf("  .byte %d\n", (unsigned char)buf[i]);
                }
            }else if(init_expr_node->kind == ND_STRING_LITERAL) {
                printf("  .quad .L_S_%d\n", init_expr_node->string_literal.literal->index);
            }
            zero_size -= size;
        }else if(type->ty == STRUCT) {
            if(init_expr_node->kind != ND_INIT) {
                error("Initializer type mismatch.");
            }
            Vector *init_expr = init_expr_node->init.init_expr;
            for(int i = 0; i < vector_size(init_expr); i++) {
                Node *node = vector_get(init_expr, i);
                StructMember *member = vector_get(type->members, i);
                int s = type_sizeof(member->type);

                gen_initexpr(member->type, node);
                zero_size -= s;
            }
        }else {
            Vector *init_expr = init_expr_node->init.init_expr;
            int len = vector_size(init_expr);
            for(int i = 0; i < len; i++) {
                Node *node = vector_get(init_expr, i);

                gen_initexpr(type->ptr_to, node);
            }
            int s = type_sizeof(type->ptr_to);
            zero_size -= s * len;
        }
    }
    if(zero_size) {
        printf("  .zero %d\n", zero_size);
    }
}

// lvar->offset indicates storage size in byte which the variables above this variable occupy.
// When we use rbp - (sub offset) to access this variable, we must add the size of this variable.
int get_stack_sub_offset(LVar *lvar) {
    return lvar->offset + type_sizeof(lvar->type) + reserverd_stack_size;
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

void gen_lowering_rax(int to_size) {
    if(to_size == 1) {
        printf("  and rax, 0xff\n");
    }else if(to_size == 2) {
        printf("  and rax, 0xffff\n");
    }else if(to_size == 4) {
        printf("  mov eax, eax\n");
    }else{
        // nop
    }
}

void gen_return() {
    printf("  pop rax\n");
    printf("  mov rsp,rbp\n");
    printf("  pop rbx\n");
    printf("  pop r15\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

static void gen_string_literals() {
    printf(".data\n");
    for(int i = 0; i < vector_size(global_string_literals); i++) {
        StringLiteral *literal = vector_get(global_string_literals, i);
        printf(".L_S_%d:\n", literal->index);
        printf("  .string \"%.*s\"\n", literal->len, literal->str);
    }
}

void gen_builtin_call(Node *node) {
    GVar *gvar = node->lhs->gvar.gvar;
    if(strcmp(gvar->name, "__builtin_va_start") == 0) {
        printf("  // __builtin_va_start\n");
        gen(node->call_arg_list.next->node);
        Node *arg2 = node->call_arg_list.next->next->node;
        int gp_offset = 0, fp_offset = 0;
        gp_offset = (arg2->lvar->func_arg_index - 1) * 8 + 8;
        printf("  mov dword ptr[rax], %d\n", gp_offset);
        printf("  mov dword ptr[rax+4], %d\n", fp_offset);
        printf("  lea rcx, [rbp+%d]\n", 4 * 8);
        printf("  mov qword ptr[rax+8], rcx\n"); // overflow_arg_area
        printf("  lea rcx, [rbp-%d]\n", 6 * 8);
        printf("  mov qword ptr[rax+16], rcx\n"); // reg_save_area
        printf("  push rax\n");
    }else if(strcmp(gvar->name, "__builtin_va_end") == 0) {
        printf("  push rax\n");
    }else {
        error("Unknown builtin call %s\n", gvar->name);
    }
}

void gen(Node *node){
    if(node->line_info) {
        bool found = false;
        int file_no = 0;
        for(int i = 0; i < vector_size(file_no_vec); i++) {
            LineInfo *info = vector_get(file_no_vec, i);
            if(compare_ident(info->filename, info->filename_len, node->line_info->filename, node->line_info->filename_len)) {
                found = true;
                file_no = i;
                break;
            }
        }
        if(!found) {
            file_no = vector_size(file_no_vec);
            vector_push(file_no_vec, node->line_info);
            printf("  .file %d \"%.*s\"\n", file_no + 1, node->line_info->filename_len, node->line_info->filename);
        }
        printf("  .loc %d %d\n", file_no + 1, node->line_info->line_number);
        printf("  // %.*s:%d\n", node->line_info->filename_len, node->line_info->filename, node->line_info->line_number);
    }
    switch(node->kind){
        case ND_NUM:
            printf("  mov rax, %lu\n", node->val);
            printf("  push rax\n");
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
            vector_push(break_target_vec, (void*)(long)break_target);
            printf("  // switch %d\n", cur);
            gen(node->lhs);
            printf("  pop rax\n");
            for(int i = 0; i < vector_size(node->switch_.cases); i++){
                Node *case_node = vector_get(node->switch_.cases, i);
                int64_t v = case_node->rhs->val;
                printf("  cmp rax, %lu\n", v);
                printf("  jz .Lswitch_%d_%lu\n", cur, v);
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

            vector_pop(break_target_vec);

            return;
        }
        case ND_CASE: {
            printf("  .Lswitch_%d_%lu:\n", switch_number, node->rhs->val);
            gen(node->lhs);
            return;
        }
        case ND_DEFAULT: {
            printf("  .Lswitch_%d_default:\n", switch_number);
            gen(node->lhs);
            return;
        }
        case ND_BREAK: {
            printf("  jmp .Lbreak_%d\n", (int)(long)vector_last(break_target_vec));
            return;
        }
        case ND_CONTINUE: {
            printf("  jmp .Lcontinue_%d\n", (int)(long)vector_last(continue_target_vec));
            return;
        }
        case ND_FOR: {
            int break_target = ++current_break_target;
            vector_push(break_target_vec, (void*)(long)break_target);
            int continue_targets = ++current_continue_target;
            vector_push(continue_target_vec, (void*)(long)continue_targets);
            // clause-1
            if(node->lhs) {
                gen(node->lhs);
                printf("  pop rax\n");
            }
            int label_for = ++cur_label;
            printf(".L%d:\n", label_for);
            // condition
            if(node->rhs) {
                gen(node->rhs);
                printf("  pop rax\n");
            } else {
                printf("  mov rax, 1\n");
            }
            printf("  test rax,rax\n");
            int label = ++cur_label;
            printf("  jz .L%d\n", label);
            // body
            gen(node->for_stmt);
            printf("  pop rax\n");
            // update expression
            printf("  .Lcontinue_%d:\n", continue_targets);
            if(node->for_update_expr) {
                gen(node->for_update_expr);
                printf("  pop rax\n");
            }
            printf("  jmp .L%d\n", label_for);
            printf("  .Lbreak_%d:\n", break_target);
            printf(".L%d:\n", label);
            printf("  push rax\n");

            vector_pop(break_target_vec);
            vector_pop(continue_target_vec);
            return;
        }
        case ND_WHILE: {
            int break_target = ++current_break_target;
            vector_push(break_target_vec, (void*)(long)break_target);
            int continue_targets = ++current_continue_target;
            vector_push(continue_target_vec, (void*)(long)continue_targets);
            int label_while = ++cur_label;
            int label_while_end = ++cur_label;

            printf(".L%d:\n", label_while);
            printf(".Lcontinue_%d:\n", continue_targets);
            gen(node->lhs);
            printf("  pop rax\n");
            printf("  test rax,rax\n");
            printf("  jz .L%d\n", label_while_end);
            gen(node->rhs);
            printf("  pop rax\n");
            printf("  jmp .L%d\n", label_while);
            printf("  .Lbreak_%d:\n", break_target);
            printf(".L%d:\n", label_while_end);
            printf("  push rax\n");

            vector_pop(break_target_vec);
            vector_pop(continue_target_vec);
            return;
        }
        case ND_DO: {
            int break_target = ++current_break_target;
            vector_push(break_target_vec, (void*)(long)break_target);
            int continue_targets = ++current_continue_target;
            vector_push(continue_target_vec, (void*)(long)continue_targets);
            int label_do = ++cur_label;
            printf(".L%d:\n", label_do);
            printf(".Lcontinue_%d:\n", continue_targets);
            gen(node->lhs);
            printf("  pop rax\n");
            gen(node->rhs);
            printf("  pop rax\n");
            printf("  test rax,rax\n");
            printf("  jnz .L%d\n", label_do);
            printf("  .Lbreak_%d:\n", break_target);
            printf("  push rax\n");

            vector_pop(break_target_vec);
            vector_pop(continue_target_vec);
            return;
        }
        case ND_COMPOUND:
            printf("  // compound %d\n", vector_size(node->compound_stmt_list));
            for(int i = 0; i < vector_size(node->compound_stmt_list); i++){
                gen(vector_get(node->compound_stmt_list, i));
                printf("  pop rax\n");
            }
            printf("  push rax\n");
            return;
        case ND_CALL: {
            if(node->lhs && node->lhs->kind == ND_GVAR && node->lhs->gvar.gvar->is_builtin) {
                gen_builtin_call(node);
                return;
            }
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
            if(node->func_def.type_storage != TS_STATIC) {
                printf(".globl %.*s\n", node->func_def.ident_len, node->func_def.ident);
            }
            printf("%.*s:\n", node->func_def.ident_len, node->func_def.ident);
            int size = vector_size(node->func_def.arg_vec);
            printf("  push rbp\n");
            printf("  push r15\n");
            printf("  push rbx\n");
            printf("  mov rbp,rsp\n");

            /// stack layout: (from upper address to lower address)
            /// ...
            /// arg8
            /// arg7
            /// return address
            /// saved rbp
            /// saved r15
            /// saved rbx <- rbp points here
            /// saved arguments for va_list (only used in var arg)
            /// saved arguments (To use arg as normal local variable)
            /// local var1
            /// local var2
            /// ...
            /// stack machine
            ///
            reserverd_stack_size = 0;
            if(node->func_def.type->is_vararg) {
                reserverd_stack_size = args_reg_len * 8;
            }
            printf("  // allocate stack\n");
            printf("  sub rsp,%d\n", stack_align(node->func_def.max_stack_size + reserverd_stack_size));
            for(int i = 0; i < size; i++){
                FuncDefArg *arg = vector_get(node->func_def.arg_vec, i);
                printf("  // save argument %d: %.*s\n", i, arg->lvar->len, arg->lvar->name);
                int size = type_sizeof(arg->lvar->type);
                printf("  lea rax, [rbp-%d]\n", get_stack_sub_offset(arg->lvar));
                printf("  push rax\n");
                printf("  push %s\n", args_regs[i]);
                char *out;
                type_dump(arg->type, &out);
                printf("  // type: %s\n", out);
                store(type_sizeof(arg->type));
                printf("  pop rax\n");
            }
            if(node->func_def.type->is_vararg) {
                for(int i = 0; i < args_reg_len; i++) {
                    printf("  // save argument %d for va_list\n", i);
                    printf("  lea rax, [rbp-%d]\n", (args_reg_len - 1 - i) * 8 + 8);
                    printf("  push rax\n");
                    printf("  push %s\n", args_regs[i]);
                    store(8);
                    printf("  pop rax\n");
                }
            }
            gen(node->lhs);
            gen_return();
            return;
        case ND_SCOPE: {
            printf("  // scope\n");
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
            Type *type = node->gvar_def.gvar->type;
            gen_initexpr(type, node->gvar_def.init_expr);
            return;
        case ND_CONVERT:
            if(node->lhs->expr_type->ty == ARRAY && node->expr_type->ty == PTR) {
                gen(node->lhs);
            } else {
                gen(node->lhs);
                char *out_from, *out_to;
                type_dump(node->lhs->expr_type, &out_from);
                type_dump(node->expr_type, &out_to);
                int from_size = type_sizeof(node->lhs->expr_type);
                int to_size = type_sizeof(node->expr_type);
                printf("  // convert from %s to %s\n", out_from, out_to);
                printf("  pop rax\n");
                if(from_size > to_size) {
                    // lowering size
                    gen_lowering_rax(to_size);
                }else if(from_size < to_size) {
                    // enlarge size
                    if(type_is_signed(node->lhs->expr_type)) {
                        // signed to signed
                        // (signed char)-16 -> (signed short)-16
                        // or, signed to unsigned
                        // (signed char)-16 -> (unsigned short)65520
                        // Whether converting to signed or unsigned, bit representations are the same.
                        if(from_size == 1) {
                            printf("  movsx rax, al\n");
                        } else if(from_size == 2) {
                            printf("  movsx rax, ax\n");
                        } else if(from_size == 4) {
                            printf("  movsx rax, eax\n");
                        } else if(from_size == 8) {
                            error("Conversion unsupported for type %s to %s", out_from, out_to);
                        }
                        gen_lowering_rax(to_size);
                    }else if(!type_is_signed(node->lhs->expr_type) && !type_is_signed(node->expr_type)) {
                        // unsigned to unsigned
                        gen_lowering_rax(from_size);
                    }else {
                        // unsigned to signed
                        gen_lowering_rax(from_size);
                    }
                }
                printf("  push rax\n");
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
        case ND_DECL_LIST_LOCAL:
            for(int i = 0; i < vector_size(node->decl_list_local.decls); i++) {
                Node *decl = vector_get(node->decl_list_local.decls, i);
                gen(decl);
                printf("  pop rax\n");
            }
            printf("  push rax\n");
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
            printf("  add rax,rsi\n");
            break;
        case ND_SUB:
            printf("  // sub\n");
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

void init_codegen() {
    file_no_vec = new_vector();
    break_target_vec = new_vector();
    continue_target_vec = new_vector();
    gen_string_literals();
}
