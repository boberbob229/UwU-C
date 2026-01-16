/**
 * @file ir.c
 * @brief Intermediate Representation (IR) generation for the uwuc compiler
 * @author bober
 * @version 0.0.5
 *
 * This file implements the lowering phase from the Abstract Syntax Tree (AST)
 * into a linear, three-address-code-style Intermediate Representation (IR).
 *
 * The IR acts as a target-independent layer between semantic analysis and
 * machine-specific code generation. It encodes expressions, control flow,
 * function structure, variable access, and builtin side effects in a normalized
 * form suitable for backend translation.
 *
 * This module is responsible for:
 *  - Emitting IR instructions for expressions and statements
 *  - Managing temporary values and labels
 *  - Computing function stack frame sizes
 */

#include "ir.h"
#include "util.h"
#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static IRInstruction* ir_inst_new(const char* opcode) {
    IRInstruction* inst = xcalloc(1, sizeof(IRInstruction));
    inst->opcode = xstrdup(opcode);
    inst->next = NULL;
    return inst;
}

static void ir_inst_add_operand(IRInstruction* inst, int idx, const char* op) {
    if (idx >= 0 && idx < 3 && op) {
        inst->operands[idx] = xstrdup(op);
    }
}

static void ir_program_append(IRProgram* prog, IRInstruction* inst) {
    if (!prog->head) {
        prog->head = prog->tail = inst;
    } else {
        prog->tail->next = inst;
        prog->tail = inst;
    }
}

static int temp_counter = 0;
static int label_counter = 0;
static int string_counter = 0;
static IRProgram* current_prog = NULL;

static char* new_temp(void) {
    char* temp = xmalloc(16);
    snprintf(temp, 16, "t%d", temp_counter++);
    if (current_prog) {
        current_prog->temp_count++;
    }
    return temp;
}

static char* new_label(void) {
    char* label = xmalloc(16);
    snprintf(label, 16, "L%d", label_counter++);
    return label;
}

static void ir_emit_string(IRProgram* prog, const char* label, const char* value) {
    IRInstruction* inst = ir_inst_new("string");
    ir_inst_add_operand(inst, 0, label);
    ir_inst_add_operand(inst, 1, value);
    ir_program_append(prog, inst);
}

static char* gen_expr_ir(IRProgram* prog, ASTNode* node);

static char* gen_expr_ir(IRProgram* prog, ASTNode* node) {
    if (!node) return NULL;

    char* result = NULL;

    switch (node->kind) {
        case AST_NUMBER: {
            result = new_temp();
            IRInstruction* inst = ir_inst_new("mov");
            ir_inst_add_operand(inst, 0, result);
            char val[32];
            snprintf(val, sizeof(val), "%lld", node->data.int_value);
            ir_inst_add_operand(inst, 1, val);
            ir_program_append(prog, inst);
            break;
        }

        case AST_IDENTIFIER: {
            char var_name[64];
            snprintf(var_name, sizeof(var_name), "v%d", node->stack_offset);
            result = xstrdup(var_name);
            break;
        }

        case AST_BINARY_OP: {
            char* left = gen_expr_ir(prog, node->children[0]);
            char* right = gen_expr_ir(prog, node->children[1]);
            result = new_temp();

            const char* op;
            switch (node->data.op) {
                case TOKEN_PLUS:    op = "add"; break;
                case TOKEN_MINUS:   op = "sub"; break;
                case TOKEN_STAR:    op = "mul"; break;
                case TOKEN_SLASH:   op = "div"; break;
                case TOKEN_PERCENT: op = "mod"; break;
                case TOKEN_EQ:      op = "eq";  break;
                case TOKEN_NE:      op = "ne";  break;
                case TOKEN_LT:      op = "lt";  break;
                case TOKEN_GT:      op = "gt";  break;
                case TOKEN_LE:      op = "le";  break;
                case TOKEN_GE:      op = "ge";  break;
                case TOKEN_AND:     op = "and"; break;
                case TOKEN_OR:      op = "or";  break;
                default:            op = "add"; break;
            }

            IRInstruction* inst = ir_inst_new(op);
            ir_inst_add_operand(inst, 0, result);
            ir_inst_add_operand(inst, 1, left);
            ir_inst_add_operand(inst, 2, right);
            ir_program_append(prog, inst);

            free(left);
            free(right);
            break;
        }

        case AST_UNARY_OP: {
            char* operand = gen_expr_ir(prog, node->children[0]);
            result = new_temp();

            const char* op;
            switch (node->data.op) {
                case TOKEN_MINUS: op = "neg"; break;
                case TOKEN_NOT:   op = "not"; break;
                case TOKEN_TILDE: op = "complement"; break;
                default:          op = "mov"; break;
            }

            IRInstruction* inst = ir_inst_new(op);
            ir_inst_add_operand(inst, 0, result);
            ir_inst_add_operand(inst, 1, operand);
            ir_program_append(prog, inst);

            free(operand);
            break;
        }

        case AST_CALL: {
            result = new_temp();

            for (int i = 1; i < node->child_count; i++) {
                char* arg = gen_expr_ir(prog, node->children[i]);
                IRInstruction* arg_inst = ir_inst_new("arg");
                ir_inst_add_operand(arg_inst, 0, arg);
                ir_program_append(prog, arg_inst);
                free(arg);
            }

            IRInstruction* call = ir_inst_new("call");
            ir_inst_add_operand(call, 0, result);
            ir_inst_add_operand(call, 1, node->children[0]->data.name);
            ir_program_append(prog, call);
            break;
        }

        default:
            result = new_temp();
            break;
    }

    return result;
}

static void gen_stmt_ir(IRProgram* prog, ASTNode* node);

static void gen_stmt_ir(IRProgram* prog, ASTNode* node) {
    if (!node) return;

    switch (node->kind) {
        case AST_PRINT_STR: {
            char label[32];
            snprintf(label, sizeof(label), ".Lstr%d", string_counter++);

           ir_emit_string(prog, label, node->data.print_str.value);

            IRInstruction* inst = ir_inst_new("print_str");
            ir_inst_add_operand(inst, 0, label);
            ir_program_append(prog, inst);
            break;
        }

        case AST_RETURN: {
            IRInstruction* inst = ir_inst_new("ret");
            if (node->child_count > 0) {
                char* val = gen_expr_ir(prog, node->children[0]);
                ir_inst_add_operand(inst, 0, val);
                free(val);
            }
            ir_program_append(prog, inst);
            break;
        }

        case AST_VAR_DECL: {
            if (node->child_count > 1) {
                char* val = gen_expr_ir(prog, node->children[1]);
                IRInstruction* inst = ir_inst_new("mov");
                char var_name[64];
                snprintf(var_name, sizeof(var_name), "v%d", node->stack_offset);
                ir_inst_add_operand(inst, 0, var_name);
                ir_inst_add_operand(inst, 1, val);
                ir_program_append(prog, inst);
                free(val);
            }
            break;
        }

        case AST_ASSIGN: {
            char* val = gen_expr_ir(prog, node->children[1]);
            IRInstruction* inst = ir_inst_new("mov");
            ASTNode* lhs = node->children[0];
            char var_name[64];
            snprintf(var_name, sizeof(var_name), "v%d", lhs->stack_offset);
            ir_inst_add_operand(inst, 0, var_name);
            ir_inst_add_operand(inst, 1, val);
            ir_program_append(prog, inst);
            free(val);
            break;
        }

        case AST_IF: {
            char* cond = gen_expr_ir(prog, node->children[0]);
            char* else_label = new_label();

            IRInstruction* br = ir_inst_new("brz");
            ir_inst_add_operand(br, 0, cond);
            ir_inst_add_operand(br, 1, else_label);
            ir_program_append(prog, br);

            gen_stmt_ir(prog, node->children[1]);

            if (node->child_count > 2) {
                char* end_label = new_label();

                IRInstruction* jmp = ir_inst_new("jmp");
                ir_inst_add_operand(jmp, 0, end_label);
                ir_program_append(prog, jmp);

                IRInstruction* lbl_else = ir_inst_new("label");
                ir_inst_add_operand(lbl_else, 0, else_label);
                ir_program_append(prog, lbl_else);

                gen_stmt_ir(prog, node->children[2]);

                IRInstruction* lbl_end = ir_inst_new("label");
                ir_inst_add_operand(lbl_end, 0, end_label);
                ir_program_append(prog, lbl_end);

                free(end_label);
            } else {
                IRInstruction* lbl = ir_inst_new("label");
                ir_inst_add_operand(lbl, 0, else_label);
                ir_program_append(prog, lbl);
            }

            free(cond);
            free(else_label);
            break;
        }

        case AST_WHILE: {
            char* start = new_label();
            char* end = new_label();

            IRInstruction* lbl_start = ir_inst_new("label");
            ir_inst_add_operand(lbl_start, 0, start);
            ir_program_append(prog, lbl_start);

            char* cond = gen_expr_ir(prog, node->children[0]);
            IRInstruction* br = ir_inst_new("brz");
            ir_inst_add_operand(br, 0, cond);
            ir_inst_add_operand(br, 1, end);
            ir_program_append(prog, br);

            gen_stmt_ir(prog, node->children[1]);

            IRInstruction* jmp = ir_inst_new("jmp");
            ir_inst_add_operand(jmp, 0, start);
            ir_program_append(prog, jmp);

            IRInstruction* lbl_end = ir_inst_new("label");
            ir_inst_add_operand(lbl_end, 0, end);
            ir_program_append(prog, lbl_end);

            free(cond);
            free(start);
            free(end);
            break;
        }

        case AST_BLOCK:
            for (int i = 0; i < node->child_count; i++) {
                gen_stmt_ir(prog, node->children[i]);
            }
            break;

        default:
            if (node->kind >= AST_BINARY_OP && node->kind <= AST_CALL) {
                char* tmp = gen_expr_ir(prog, node);
                free(tmp);
            }
            break;
    }
}

static void gen_function_ir(IRProgram* prog, ASTNode* node) {
    temp_counter = 0;
    current_prog = prog;

    IRInstruction* start = ir_inst_new("func");
    ir_inst_add_operand(start, 0, node->data.name);
    ir_program_append(prog, start);

    gen_stmt_ir(prog, node->children[2]);

    int local_size = node->stack_offset;
    int temp_size = temp_counter * 8;
    prog->frame_size = ((local_size + temp_size + 15) & ~15);

    ir_program_append(prog, ir_inst_new("endfunc"));
    current_prog = NULL;
}

IRProgram* ir_generate(ASTNode* root) {
    if (!root || root->kind != AST_PROGRAM) return NULL;

    label_counter = 0;
    string_counter = 0;

    IRProgram* prog = xcalloc(1, sizeof(IRProgram));

    for (int i = 0; i < root->child_count; i++) {
        if (root->children[i]->kind == AST_FUNCTION) {
            gen_function_ir(prog, root->children[i]);
        }
    }

    return prog;
}

void ir_program_free(IRProgram* program) {
    if (!program) return;

    IRInstruction* cur = program->head;
    while (cur) {
        IRInstruction* next = cur->next;
        free(cur->opcode);
        for (int i = 0; i < 3; i++) {
            free(cur->operands[i]);
        }
        free(cur);
        cur = next;
    }

    free(program);
}

void ir_dump(IRProgram* program, FILE* out) {
    if (!program) {
        fprintf(out, "IR: (empty)\n");
        return;
    }
    
    fprintf(out, "IR (frame_size=%d, temps=%d):\n", 
            program->frame_size, program->temp_count);
    
    IRInstruction* inst = program->head;
    while (inst) {
        fprintf(out, "  %s", inst->opcode);
        for (int i = 0; i < 3; i++) {
            if (inst->operands[i]) {
                fprintf(out, " %s", inst->operands[i]);
            }
        }
        fprintf(out, "\n");
        inst = inst->next;
    }
}
