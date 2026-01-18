#ifndef IR_H
#define IR_H

#include "ast.h"
#include <stdio.h>

typedef struct IRInstruction {
    char* opcode;
    char* operands[16]; // Went from only supporting 3 to now support 16
    struct IRInstruction* next;
} IRInstruction;

typedef struct {
    IRInstruction* head;
    IRInstruction* tail;
    int frame_size;
    int temp_count;
} IRProgram;

IRProgram* ir_generate(ASTNode* root);
void ir_program_free(IRProgram* program);
void ir_dump(IRProgram* program, FILE* out);

#endif