/*
* @file    codegen.c
* @brief   Assembly code generation for the UwU-C IR
* @author  Bober
* @version 1.0.0

* We aren't playing around anymore.
* No more bs and ignoring the problems with ABI Handling.
* Everything about AMD64 has been done well.
* ARM and x86 has been smoothed out.
* Apple wants Mach-O, Linux wants ELF.
* That’s why there is now an APPLE flag.
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
#include <stdarg.h>

typedef struct {
    bool enable_bounds_checks;
    bool enable_null_checks;
    bool enable_stack_checks;
    bool enable_optimization;
    bool emit_debug_info;
    int optimization_level;
} CodegenConfig;

static CodegenConfig config = {
    .enable_bounds_checks = true,
    .enable_null_checks = true,
    .enable_stack_checks = true,
    .enable_optimization = false,
    .emit_debug_info = false,
    .optimization_level = 0
};

static bool is_immediate(const char* s) {
    if (!s || !s[0]) return false;
    if (*s == '-' || *s == '+') s++;
    while (*s) {
        if (!isdigit(*s++)) return false;
    }
    return true;
}

static bool is_var(const char* s) {
    return s && s[0] == 'v' && isdigit(s[1]);
}

static bool is_temp(const char* s) {
    return s && s[0] == 't' && isdigit(s[1]);
}

static bool is_label(const char* s) {
    return s && s[0] == 'L' && isdigit(s[1]);
}

static bool is_string_literal(const char* s) {
    return s && s[0] == '.';
}

static int parse_offset(const char* s) {
    if (!s) return 0;
    if (s[0] == 'v' || s[0] == 't') {
        return atoi(s + 1);
    }
    return 0;
}

static int get_stack_offset(const char* name, int frame_size) {
    int offset = parse_offset(name);
    if (offset > 0) {
        return -(offset * 8 + 8);
    }
    return 0;
}

static int align_to(int value, int alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

static int parse_arg_count(const char* s) {
    if (!s) return 0;
    return atoi(s);
}

#ifdef UWUCC_ARCH_X86_64

static const char* x86_64_arg_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static const int x86_64_num_arg_regs = 6;

static void emit_x86_64_prologue(FILE* f, const char* func_name, int frame_size) {
#ifdef __APPLE__
    fprintf(f, ".globl _%s\n", func_name);
    fprintf(f, "_%s:\n", func_name);
#else
    fprintf(f, ".globl %s\n", func_name);
    fprintf(f, ".type %s, @function\n", func_name);
    fprintf(f, "%s:\n", func_name);
#endif

    fprintf(f, "    pushq %%rbp\n");
    fprintf(f, "    movq %%rsp, %%rbp\n");
    fprintf(f, "    pushq %%rbx\n");
    fprintf(f, "    pushq %%r12\n");

    int aligned_frame = align_to(frame_size, 16);

    if (aligned_frame > 0) {
        fprintf(f, "    subq $%d, %%rsp\n", aligned_frame);
    }

    if (config.enable_stack_checks && aligned_frame > 0) {
        fprintf(f, "    leaq -%d(%%rsp), %%rax\n", aligned_frame);
        fprintf(f, "    cmpq $0, (%%rax)\n");
    }
}

static void emit_x86_64_epilogue(FILE* f) {
    fprintf(f, "    popq %%r12\n");
    fprintf(f, "    popq %%rbx\n");
    fprintf(f, "    leave\n");
    fprintf(f, "    retq\n");
}

static void emit_x86_64_load(FILE* f, const char* src, int frame_size) {
    if (is_immediate(src)) {
        fprintf(f, "    movq $%s, %%rax\n", src);
    } else if (is_var(src) || is_temp(src)) {
        int offset = get_stack_offset(src, frame_size);
        fprintf(f, "    movq %d(%%rbp), %%rax\n", offset);
    } else if (is_string_literal(src)) {
        fprintf(f, "    leaq %s(%%rip), %%rax\n", src);
    } else {
        fprintf(f, "    leaq %s(%%rip), %%rax\n", src);
    }
}

static void emit_x86_64_store(FILE* f, const char* dest, int frame_size) {
    if (is_var(dest) || is_temp(dest)) {
        int offset = get_stack_offset(dest, frame_size);
        fprintf(f, "    movq %%rax, %d(%%rbp)\n", offset);
    }
}

static void emit_x86_64_call(FILE* f, const char* func, IRInstruction* inst, int frame_size) {
    int num_args = 0;

    for (int i = 1; i < 16 && inst->operands[i]; i++) {
        num_args++;
    }

    bool is_print_str = strcmp(func, "print_str") == 0;
    const char* actual_func = is_print_str ? "puts" : func;

    bool need_align = ((num_args > 6 ? (num_args - 6) : 0) * 8) % 16 != 0;
    if (need_align) {
        fprintf(f, "    subq $8, %%rsp\n");
    }

    for (int i = num_args; i > 6; i--) {
        emit_x86_64_load(f, inst->operands[i], frame_size);
        fprintf(f, "    pushq %%rax\n");
    }

    for (int i = 1; i <= num_args && i <= 6; i++) {
        emit_x86_64_load(f, inst->operands[i], frame_size);
        fprintf(f, "    movq %%rax, %%%s\n", x86_64_arg_regs[i-1]);
    }

#ifdef __APPLE__
    fprintf(f, "    call _%s\n", actual_func);
#else
    fprintf(f, "    call %s@PLT\n", actual_func);
#endif

    int stack_args = num_args > 6 ? num_args - 6 : 0;
    if (stack_args > 0 || need_align) {
        int cleanup = stack_args * 8 + (need_align ? 8 : 0);
        fprintf(f, "    addq $%d, %%rsp\n", cleanup);
    }
}

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
    else if (strcmp(inst->opcode, "sub") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    movq %%rax, %%rbx\n");
        emit_x86_64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    subq %%rax, %%rbx\n");
        fprintf(f, "    movq %%rbx, %%rax\n");
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "mul") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    movq %%rax, %%rbx\n");
        emit_x86_64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    imulq %%rbx, %%rax\n");
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "div") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    movq %%rax, %%rbx\n");
        emit_x86_64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    movq %%rbx, %%rax\n");
        fprintf(f, "    cqo\n");
        fprintf(f, "    idivq %d(%%rbp)\n", get_stack_offset(inst->operands[2], frame_size));
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "mod") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    movq %%rax, %%rbx\n");
        emit_x86_64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    movq %%rbx, %%rax\n");
        fprintf(f, "    cqo\n");
        fprintf(f, "    idivq %d(%%rbp)\n", get_stack_offset(inst->operands[2], frame_size));
        fprintf(f, "    movq %%rdx, %%rax\n");
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "lt") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    movq %%rax, %%rbx\n");
        emit_x86_64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    cmpq %%rax, %%rbx\n");
        fprintf(f, "    setl %%al\n");
        fprintf(f, "    movzbq %%al, %%rax\n");
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "le") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    movq %%rax, %%rbx\n");
        emit_x86_64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    cmpq %%rax, %%rbx\n");
        fprintf(f, "    setle %%al\n");
        fprintf(f, "    movzbq %%al, %%rax\n");
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "gt") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    movq %%rax, %%rbx\n");
        emit_x86_64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    cmpq %%rax, %%rbx\n");
        fprintf(f, "    setg %%al\n");
        fprintf(f, "    movzbq %%al, %%rax\n");
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "ge") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    movq %%rax, %%rbx\n");
        emit_x86_64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    cmpq %%rax, %%rbx\n");
        fprintf(f, "    setge %%al\n");
        fprintf(f, "    movzbq %%al, %%rax\n");
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "eq") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    movq %%rax, %%rbx\n");
        emit_x86_64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    cmpq %%rax, %%rbx\n");
        fprintf(f, "    sete %%al\n");
        fprintf(f, "    movzbq %%al, %%rax\n");
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "ne") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    movq %%rax, %%rbx\n");
        emit_x86_64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    cmpq %%rax, %%rbx\n");
        fprintf(f, "    setne %%al\n");
        fprintf(f, "    movzbq %%al, %%rax\n");
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "and") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    movq %%rax, %%rbx\n");
        emit_x86_64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    andq %%rbx, %%rax\n");
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "or") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    movq %%rax, %%rbx\n");
        emit_x86_64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    orq %%rbx, %%rax\n");
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "xor") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    movq %%rax, %%rbx\n");
        emit_x86_64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    xorq %%rbx, %%rax\n");
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "shl") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    movq %%rax, %%rbx\n");
        emit_x86_64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    movq %%rax, %%rcx\n");
        fprintf(f, "    movq %%rbx, %%rax\n");
        fprintf(f, "    shlq %%cl, %%rax\n");
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "shr") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    movq %%rax, %%rbx\n");
        emit_x86_64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    movq %%rax, %%rcx\n");
        fprintf(f, "    movq %%rbx, %%rax\n");
        fprintf(f, "    shrq %%cl, %%rax\n");
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "neg") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    negq %%rax\n");
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "not") == 0) {
        emit_x86_64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    notq %%rax\n");
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "label") == 0) {
        fprintf(f, "%s:\n", inst->operands[0]);
    }
    else if (strcmp(inst->opcode, "jmp") == 0) {
        fprintf(f, "    jmp %s\n", inst->operands[0]);
    }
    else if (strcmp(inst->opcode, "jz") == 0 || strcmp(inst->opcode, "brz") == 0) {
        emit_x86_64_load(f, inst->operands[0], frame_size);
        fprintf(f, "    testq %%rax, %%rax\n");
        fprintf(f, "    jz %s\n", inst->operands[1]);
    }
    else if (strcmp(inst->opcode, "jnz") == 0) {
        emit_x86_64_load(f, inst->operands[0], frame_size);
        fprintf(f, "    testq %%rax, %%rax\n");
        fprintf(f, "    jnz %s\n", inst->operands[1]);
    }
    else if (strcmp(inst->opcode, "call") == 0) {
        emit_x86_64_call(f, inst->operands[0], inst, frame_size);
    }
    else if (strcmp(inst->opcode, "getret") == 0) {
        emit_x86_64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "ret") == 0) {
        if (inst->operands[0]) {
            emit_x86_64_load(f, inst->operands[0], frame_size);
        }
        emit_x86_64_epilogue(f);
    }
    else if (strcmp(inst->opcode, "endfunc") == 0) {
        fprintf(f, "    xorq %%rax, %%rax\n");
        emit_x86_64_epilogue(f);
    }
    else if (strcmp(inst->opcode, "func") == 0) {
        emit_x86_64_prologue(f, inst->operands[0], frame_size);
    }
}

#endif

#ifdef UWUCC_ARCH_ARM64

static const char* arm64_arg_regs[] = {"x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7"};
static const int arm64_num_arg_regs = 8;

static void emit_arm64_prologue(FILE* f, const char* func_name, int frame_size) {
#ifdef __APPLE__
    fprintf(f, ".globl _%s\n", func_name);
    fprintf(f, "_%s:\n", func_name);
#else
    fprintf(f, ".globl %s\n", func_name);
    fprintf(f, ".type %s, %%function\n", func_name);
    fprintf(f, "%s:\n", func_name);
#endif

    fprintf(f, "    stp x29, x30, [sp, #-16]!\n");
    fprintf(f, "    mov x29, sp\n");

    int aligned_frame = align_to(frame_size, 16);
    if (aligned_frame > 0) {
        fprintf(f, "    sub sp, sp, #%d\n", aligned_frame);
    }

    if (config.enable_stack_checks && aligned_frame > 0) {
        fprintf(f, "    sub x9, sp, #%d\n", aligned_frame);
        fprintf(f, "    ldr xzr, [x9]\n");
    }
}

static void emit_arm64_epilogue(FILE* f, int frame_size) {
    int aligned_frame = align_to(frame_size, 16);
    if (aligned_frame > 0) {
        fprintf(f, "    add sp, sp, #%d\n", aligned_frame);
    }
    fprintf(f, "    ldp x29, x30, [sp], #16\n");
    fprintf(f, "    ret\n");
}

static void emit_arm64_load(FILE* f, const char* src, int frame_size) {
    if (is_immediate(src)) {
        long val = atol(src);
        if (val >= 0 && val <= 65535) {
            fprintf(f, "    mov x0, #%s\n", src);
        } else {
            fprintf(f, "    movz x0, #%ld, lsl #0\n", val & 0xFFFF);
            if ((val >> 16) & 0xFFFF) {
                fprintf(f, "    movk x0, #%ld, lsl #16\n", (val >> 16) & 0xFFFF);
            }
            if ((val >> 32) & 0xFFFF) {
                fprintf(f, "    movk x0, #%ld, lsl #32\n", (val >> 32) & 0xFFFF);
            }
            if ((val >> 48) & 0xFFFF) {
                fprintf(f, "    movk x0, #%ld, lsl #48\n", (val >> 48) & 0xFFFF);
            }
        }
    } else if (is_var(src) || is_temp(src)) {
        int offset = get_stack_offset(src, frame_size);
        fprintf(f, "    ldr x0, [sp, #%d]\n", -offset);
    } else if (is_string_literal(src)) {
#ifdef __APPLE__
        fprintf(f, "    adrp x0, %s@PAGE\n", src);
        fprintf(f, "    add x0, x0, %s@PAGEOFF\n", src);
#else
        fprintf(f, "    adrp x0, %s\n", src);
        fprintf(f, "    add x0, x0, :lo12:%s\n", src);
#endif
    } else {
#ifdef __APPLE__
        fprintf(f, "    adrp x0, %s@PAGE\n", src);
        fprintf(f, "    add x0, x0, %s@PAGEOFF\n", src);
#else
        fprintf(f, "    adrp x0, %s\n", src);
        fprintf(f, "    add x0, x0, :lo12:%s\n", src);
#endif
    }
}

static void emit_arm64_store(FILE* f, const char* dest, int frame_size) {
    if (is_var(dest) || is_temp(dest)) {
        int offset = get_stack_offset(dest, frame_size);
        fprintf(f, "    str x0, [sp, #%d]\n", -offset);
    }
}

static void emit_arm64_call(FILE* f, const char* func, IRInstruction* inst, int frame_size) {
    int num_args = 0;

    for (int i = 1; i < 16 && inst->operands[i]; i++) {
        num_args++;
    }

    bool is_print_str = strcmp(func, "print_str") == 0;
    const char* actual_func = is_print_str ? "puts" : func;

    int stack_args = num_args > 8 ? num_args - 8 : 0;
    bool need_align = (stack_args * 8) % 16 != 0;

    if (need_align) {
        fprintf(f, "    sub sp, sp, #8\n");
    }

    for (int i = num_args; i > 8; i--) {
        emit_arm64_load(f, inst->operands[i], frame_size);
        fprintf(f, "    str x0, [sp, #-8]!\n");
    }

    for (int i = 1; i <= num_args && i <= 8; i++) {
        emit_arm64_load(f, inst->operands[i], frame_size);
        if (i < 8) {
            fprintf(f, "    mov x%d, x0\n", i-1);
        }
    }

#ifdef __APPLE__
    fprintf(f, "    bl _%s\n", actual_func);
#else
    fprintf(f, "    bl %s\n", actual_func);
#endif

    if (stack_args > 0 || need_align) {
        int cleanup = stack_args * 8 + (need_align ? 8 : 0);
        fprintf(f, "    add sp, sp, #%d\n", cleanup);
    }
}

static void emit_arm64_instruction(FILE* f, IRInstruction* inst, int frame_size) {
    if (strcmp(inst->opcode, "mov") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "add") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mov x1, x0\n");
        emit_arm64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    add x0, x1, x0\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "sub") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mov x1, x0\n");
        emit_arm64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    sub x0, x1, x0\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "mul") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mov x1, x0\n");
        emit_arm64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    mul x0, x1, x0\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "div") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mov x1, x0\n");
        emit_arm64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    sdiv x0, x1, x0\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "mod") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mov x1, x0\n");
        emit_arm64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    sdiv x2, x1, x0\n");
        fprintf(f, "    msub x0, x2, x0, x1\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "lt") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mov x1, x0\n");
        emit_arm64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    cmp x1, x0\n");
        fprintf(f, "    cset x0, lt\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "le") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mov x1, x0\n");
        emit_arm64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    cmp x1, x0\n");
        fprintf(f, "    cset x0, le\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "gt") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mov x1, x0\n");
        emit_arm64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    cmp x1, x0\n");
        fprintf(f, "    cset x0, gt\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "ge") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mov x1, x0\n");
        emit_arm64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    cmp x1, x0\n");
        fprintf(f, "    cset x0, ge\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "eq") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mov x1, x0\n");
        emit_arm64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    cmp x1, x0\n");
        fprintf(f, "    cset x0, eq\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "ne") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mov x1, x0\n");
        emit_arm64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    cmp x1, x0\n");
        fprintf(f, "    cset x0, ne\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "and") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mov x1, x0\n");
        emit_arm64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    and x0, x1, x0\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "or") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mov x1, x0\n");
        emit_arm64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    orr x0, x1, x0\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "xor") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mov x1, x0\n");
        emit_arm64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    eor x0, x1, x0\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "shl") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mov x1, x0\n");
        emit_arm64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    lsl x0, x1, x0\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "shr") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mov x1, x0\n");
        emit_arm64_load(f, inst->operands[2], frame_size);
        fprintf(f, "    lsr x0, x1, x0\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "neg") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    neg x0, x0\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "not") == 0) {
        emit_arm64_load(f, inst->operands[1], frame_size);
        fprintf(f, "    mvn x0, x0\n");
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "label") == 0) {
        fprintf(f, "%s:\n", inst->operands[0]);
    }
    else if (strcmp(inst->opcode, "jmp") == 0) {
        fprintf(f, "    b %s\n", inst->operands[0]);
    }
    else if (strcmp(inst->opcode, "jz") == 0 || strcmp(inst->opcode, "brz") == 0) {
        emit_arm64_load(f, inst->operands[0], frame_size);
        fprintf(f, "    cbz x0, %s\n", inst->operands[1]);
    }
    else if (strcmp(inst->opcode, "jnz") == 0) {
        emit_arm64_load(f, inst->operands[0], frame_size);
        fprintf(f, "    cbnz x0, %s\n", inst->operands[1]);
    }
    else if (strcmp(inst->opcode, "call") == 0) {
        emit_arm64_call(f, inst->operands[0], inst, frame_size);
    }
    else if (strcmp(inst->opcode, "getret") == 0) {
        emit_arm64_store(f, inst->operands[0], frame_size);
    }
    else if (strcmp(inst->opcode, "ret") == 0) {
        if (inst->operands[0]) {
            emit_arm64_load(f, inst->operands[0], frame_size);
        }
        emit_arm64_epilogue(f, frame_size);
    }
    else if (strcmp(inst->opcode, "endfunc") == 0) {
        fprintf(f, "    mov x0, #0\n");
        emit_arm64_epilogue(f, frame_size);
    }
    else if (strcmp(inst->opcode, "func") == 0) {
        emit_arm64_prologue(f, inst->operands[0], frame_size);
    }
}

#endif

static void emit_string_table(FILE* f, IRProgram* program) {
#ifdef __APPLE__
    fprintf(f, ".section __TEXT,__cstring,cstring_literals\n");
#else
    fprintf(f, ".section .rodata\n");
#endif
    for (IRInstruction* inst = program->head; inst; inst = inst->next) {
        if (strcmp(inst->opcode, "string") == 0) {
            fprintf(f, "%s:\n", inst->operands[0]);
            fprintf(f, "    .asciz \"%s\"\n", inst->operands[1]);
        }
    }

    if (config.enable_bounds_checks) {
        fprintf(f, ".Lbounds_error:\n");
        fprintf(f, "    .asciz \"runtime error: array index out of bounds\\n\"\n");
    }
    if (config.enable_null_checks) {
        fprintf(f, ".Lnull_error:\n");
        fprintf(f, "    .asciz \"runtime error: null pointer dereference\\n\"\n");
    }
#ifdef __APPLE__
    fprintf(f, ".text\n");
#else
    fprintf(f, ".section .text\n");
#endif
}

void codegen_emit_asm(IRProgram* program, const char* output_file) {
    FILE* f = fopen(output_file, "w");
    if (!f) {
        error("Cannot open output file: %s", output_file);
    }

#ifdef UWUCC_ARCH_X86_64
#ifdef __APPLE__
    fprintf(f, ".section __TEXT,__text,regular,pure_instructions\n");
#else
    fprintf(f, ".section .text\n");
#endif
    emit_string_table(f, program);

    for (IRInstruction* i = program->head; i; i = i->next) {
        if (strcmp(i->opcode, "string") != 0) {
            emit_x86_64_instruction(f, i, program->frame_size);
        }
    }

#elif defined(UWUCC_ARCH_ARM64)
#ifdef __APPLE__
    fprintf(f, ".section __TEXT,__text,regular,pure_instructions\n");
    fprintf(f, ".macosx_version_min 11, 0\n");
#else
    fprintf(f, ".section .text\n");
#endif
    emit_string_table(f, program);

    for (IRInstruction* i = program->head; i; i = i->next) {
        if (strcmp(i->opcode, "string") != 0) {
            emit_arm64_instruction(f, i, program->frame_size);
        }
    }

#else
    #error "Unsupported architecture"
#endif

    fclose(f);
}

void codegen_set_config(bool bounds_checks, bool null_checks, bool stack_checks, int opt_level) {
    config.enable_bounds_checks = bounds_checks;
    config.enable_null_checks = null_checks;
    config.enable_stack_checks = stack_checks;
    config.optimization_level = opt_level;
    config.enable_optimization = (opt_level > 0);
}
