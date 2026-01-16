#ifndef IR_H
#define IR_H

#include "ast.h"

typedef struct IRInstruction {
    char* opcode;
    char* operands[3];
    struct IRInstruction* next;
} IRInstruction;

typedef struct {
    IRInstruction* head;
    IRInstruction* tail;
    int temp_count;
    int frame_size;
} IRProgram;

IRProgram* ir_generate(ASTNode* root);
void ir_program_free(IRProgram* program);
void ir_dump(IRProgram* program, FILE* out);

#endif
