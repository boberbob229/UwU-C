/**
 * @file codegen.c
 * @brief Assembly code generation for uwuc IR
 * @author bober
 * @version 0.0.5
 *
 * Not pretty, but it gets the job done.
 * This file implements the backend code generation phase for the uwuc compiler.
 */

#define _POSIX_C_SOURCE 200809L
#include "codegen.h"
#include "util.h"
#include "platform.h"
#include "ast.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/*
 * Light as a feather when I'm floating through
 * reading through the daily news
 */
static bool is_immediate(const char* s) {
    if (!s || !s[0]) return false;
    if (*s == '-' || *s == '+') s++;
    while (*s) {
        if (!isdigit(*s++)) return false;
    }
    return true;
}

/*
 * Measuring the hurt within the golden rule
 */
static bool is_var(const char* s) {
    return s && s[0] == 'v' && isdigit(s[1]);
}

/*
 * Centimeters in ether I'm heating the speaker
 */
static bool is_temp(const char* s) {
    return s && s[0] == 't' && isdigit(s[1]);
}

/*
 * Motivational teacher with words that burn people
 */
static int parse_offset(const char* s) {
    if (!s) return 0;
    if (s[0] == 'v' || s[0] == 't') {
        return atoi(s + 1);
    }
    return 0;
}

/*
 * Seeing the headlines lined with discord
 */
static int get_stack_offset(const char* name, int frame_size) {
    int offset = parse_offset(name);
    if (offset > 0) {
        return -(offset * 8 + 8);
    }
    return 0;
}

/*
 * Seeing the genocide or the planet in uproar
 */
static void emit_string_table(FILE* f, IRProgram* program) {
    fprintf(f, ".section __TEXT,__cstring,cstring_literals\n");
    for (IRInstruction* inst = program->head; inst; inst = inst->next) {
        if (strcmp(inst->opcode, "string") == 0) {
            fprintf(f, "%s:\n", inst->operands[0]);
            fprintf(f, "    .asciz \"%s\"\n", inst->operands[1]);
        }
    }
    fprintf(f, ".text\n");
}

#ifdef UWUCC_ARCH_X86_64

/*
 * Never good, the rules of paradise are never nice
 */
static void emit_x86_64_load(FILE* f, const char* src, int frame_size) {
    if (is_immediate(src)) {
        fprintf(f, "    movq $%s, %%rax\n", src);
    } else if (is_var(src) || is_temp(src)) {
        int offset = get_stack_offset(src, frame_size);
        fprintf(f, "    movq %d(%%rbp), %%rax\n", offset);
    } else {
        fprintf(f, "    movq $0, %%rax\n");
    }
}

/*
 * The best laid plans of mice and men are never right
 */
static void emit_x86_64_store(FILE* f, const char* dest, int frame_size) {
    if (is_var(dest) || is_temp(dest)) {
        int offset = get_stack_offset(dest, frame_size);
        fprintf(f, "    movq %%rax, %d(%%rbp)\n", offset);
    }
}

/*
 * I'm just a vagabond with flowers for Algernon
 */
static void emit_x86_64_instruction(FILE* f, IRInstruction* inst, int frame_size) {
    if (strcmp(inst->opcode, "mov") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "add") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    movq %%rax, %%rbx\n");
        emit_x86_64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    addq %%rbx, %%rax\n");
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "print_str") == 0) {
        fprintf(f, "    leaq %s(%%rip), %%rdi\n", inst->operands[0]);
        fprintf(f, "    call _puts\n");
    }
    else if (strcmp(inst->opcode, "ret") == 0) {
        fprintf(f, "    leave\n");
        fprintf(f, "    retq\n");
    }
}

/*
 * The average joe who knows what the fuck is going on
 */
static void emit_x86_64_prologue(FILE* f) {
    fprintf(f, ".section __TEXT,__text,regular,pure_instructions\n");
}

#endif

#ifdef UWUCC_ARCH_ARM64

/*
 * Its the hope of my thoughts that I travel upon
 */
static void emit_arm64_load(FILE* f, const char* src, int frame_size) {
    if (is_immediate(src)) {
        fprintf(f, "    mov x0, #%s\n", src);
    } else if (is_var(src) || is_temp(src)) {
        int offset = get_stack_offset(src, frame_size);
        fprintf(f, "    ldr x0, [sp, #%d]\n", -offset);
    } else {
        fprintf(f, "    mov x0, #0\n");
    }
}

/*
 * Fly like an arrow of god until I'm gone
 */
static void emit_arm64_store(FILE* f, const char* dest, int frame_size) {
    if (is_var(dest) || is_temp(dest)) {
        int offset = get_stack_offset(dest, frame_size);
        fprintf(f, "    str x0, [sp, #%d]\n", -offset);
    }
}

/*
 * So I'm drifting away like a feather in air
 */
static void emit_arm64_instruction(FILE* f, IRInstruction* inst, int frame_size) {
    if (strcmp(inst->opcode, "mov") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "add") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mov x1, x0\n");
        emit_arm64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    add x0, x0, x1\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "print_str") == 0) {
        fprintf(f, "    adrp x0, %s@PAGE\n", inst->operands[0]);
        fprintf(f, "    add  x0, x0, %s@PAGEOFF\n", inst->operands[0]);
        fprintf(f, "    bl _puts\n");
    }
    else if (strcmp(inst->opcode, "ret") == 0) {
        fprintf(f, "    ldp x29, x30, [sp], #16\n");
        fprintf(f, "    ret\n");
    }
    else if (strcmp(inst->opcode, "func") == 0) {
        fprintf(f, ".globl _%s\n", inst->operands[0]);
        fprintf(f, "_%s:\n", inst->operands[0]);
        fprintf(f, "    stp x29, x30, [sp, #-16]!\n");
        fprintf(f, "    mov x29, sp\n");
    }
    else if (strcmp(inst->opcode, "endfunc") == 0) {
        fprintf(f, "    ldp x29, x30, [sp], #16\n");
        fprintf(f, "    ret\n");
    }
}

/*
 * Riding the escalator to the something that's greater
 */
static void emit_arm64_prologue(FILE* f) {
    fprintf(f, ".section __TEXT,__text,regular,pure_instructions\n");
    fprintf(f, ".macosx_version_min 11, 0\n");
}

/*
 * Taking chances, we're tap dancing with wolves
 */
static void emit_arm64_frame(FILE* f, int frame_size) {
    if (frame_size > 0) {
        fprintf(f, "    sub sp, sp, #%d\n", frame_size);
    }
}

#endif

/*
 * Riding the escalator to the something that's greater
 */
void codegen_emit_asm(IRProgram* program, const char* output_file) {
    FILE* f = fopen(output_file, "w");
    if (!f) error("Cannot open output file: %s", output_file);

#ifdef UWUCC_ARCH_X86_64
    emit_x86_64_prologue(f);
    emit_string_table(f, program);
    for (IRInstruction* i = program->head; i; i = i->next) {
        emit_x86_64_instruction(f, i, program->frame_size);
    }
#elif defined(UWUCC_ARCH_ARM64)
    emit_arm64_prologue(f);
    emit_string_table(f, program);
    int frame = 0;
    for (IRInstruction* i = program->head; i; i = i->next) {
        if (!strcmp(i->opcode, "func")) {
            emit_arm64_instruction(f, i, 0);
        } else if (!strcmp(i->opcode, "endfunc")) {
            frame = program->frame_size;
            emit_arm64_frame(f, frame);
            emit_arm64_instruction(f, i, frame);
            frame = 0;
        } else {
            emit_arm64_instruction(f, i, frame);
        }
    }
#else
#error Unsupported architecture
#endif

    fclose(f);
}
