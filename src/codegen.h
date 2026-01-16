#ifndef CODEGEN_H
#define CODEGEN_H

#include "ir.h"

void codegen_emit_asm(IRProgram* program, const char* output_file);

#endif // CODEGEN_H
