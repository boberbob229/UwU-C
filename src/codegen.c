/*
* @file    codegen_v2.c
* @brief   Assembly code generation for the uwuc IR
* @author  bober
* @version 1.0.0

* We aren't fucking around anymore.
* No more bullshit and ignoring the problems with ABI Handling.
* The AMD64 support is held together with duck tape
* ARM and X86 has been smoothed out..
* Fucks a little bit with the compiler but not major....
* Need some thigh highs and monster this is getting too hard.....
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

#ifdef UWUCC_ARCH_X86_64

static const char* x86_64_arg_regs[] = {"rdi", "rsi", "rdx", "rcx", "r8", "r9"};
static const int x86_64_num_arg_regs = 6;

static const char* x86_64_caller_saved[] = {"rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11"};
static const int x86_64_num_caller_saved = 9;

static const char* x86_64_callee_saved[] = {"rbx", "r12", "r13", "r14", "r15"};
static const int x86_64_num_callee_saved = 5;

static void emit_x86_64_prologue(FILE* f, const char* func_name, int frame_size) {
    fprintf(f, ".globl _%s\n", func_name);
    fprintf(f, "_%s:\n", func_name);

    fprintf(f, "    pushq %%rbp\n");
    fprintf(f, "    movq %%rsp, %%rbp\n");

    int aligned_frame = align_to(frame_size, 16);
    if (aligned_frame > 0) {
        fprintf(f, "    subq $%d, %%rsp\n", aligned_frame);
    }

    if (config.enable_stack_checks) {
        fprintf(f, "    leaq -%d(%%rsp), %%rax\n", aligned_frame);
        fprintf(f, "    cmpq $0, (%%rax)\n");
    }

    fprintf(f, "    pushq %%rbx\n");
    fprintf(f, "    pushq %%r12\n");
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

static void emit_x86_64_bounds_check(FILE* f, const char* index, const char* size) {
    if (!config.enable_bounds_checks) return;

    emit_x86_64_load(f, index, 0);
    fprintf(f, "    cmpq $%s, %%rax\n", size);
    fprintf(f, "    jl .Lbounds_ok_%s\n", index);

    fprintf(f, "    leaq .Lbounds_error(%%rip), %%rdi\n");
    fprintf(f, "    call _uwu_bounds_error\n");
    fprintf(f, ".Lbounds_ok_%s:\n", index);
}

static void emit_x86_64_null_check(FILE* f, const char* ptr) {
    if (!config.enable_null_checks) return;

    emit_x86_64_load(f, ptr, 0);
    fprintf(f, "    testq %%rax, %%rax\n");
    fprintf(f, "    jnz .Lnull_ok_%s\n", ptr);

    fprintf(f, "    leaq .Lnull_error(%%rip), %%rdi\n");
    fprintf(f, "    call _uwu_null_error\n");
    fprintf(f, ".Lnull_ok_%s:\n", ptr);
}

static void emit_x86_64_call(FILE* f, const char* func, const char** args, int num_args, int frame_size) {
    bool need_align = ((num_args > 6 ? (num_args - 6) : 0) * 8) % 16 != 0;
    if (need_align) {
        fprintf(f, "    subq $8, %%rsp\n");
    }

    for (int i = num_args - 1; i >= 6; i--) {
        emit_x86_64_load(f, args[i], frame_size);
        fprintf(f, "    pushq %%rax\n");
    }

    for (int i = 0; i < num_args && i < 6; i++) {
        emit_x86_64_load(f, args[i], frame_size);
        fprintf(f, "    movq %%rax, %%%s\n", x86_64_arg_regs[i]);
    }

    fprintf(f, "    call _%s\n", func);

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
        const char* args[16] = {NULL};
        int num_args = 0;
        emit_x86_64_call(f, inst->operands[0], args, num_args, frame_size);
    }
    else if (strcmp(inst->opcode, "print_str") == 0) {
        fprintf(f, "    leaq %s(%%rip), %%rdi\n", inst->operands[0]);
        fprintf(f, "    call _puts\n");
    }
    else if (strcmp(inst->opcode, "ret") == 0) {
        if (inst->operands[0]) {
            emit_x86_64_load(f, inst->operands[0], frame_size);
        }
        emit_x86_64_epilogue(f);
    }
    else if (strcmp(inst->opcode, "func") == 0) {
        emit_x86_64_prologue(f, inst->operands[0], frame_size);
    }
}

#endif

#ifdef UWUCC_ARCH_ARM64


// im terry davis if he was a fag
static const char* arm64_arg_regs[] = {"x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7"};
static const int arm64_num_arg_regs = 8;

static void emit_arm64_prologue(FILE* f, const char* func_name, int frame_size) {
    fprintf(f, ".globl _%s\n", func_name);
    fprintf(f, "_%s:\n", func_name);

    fprintf(f, "    stp x29, x30, [sp, #-16]!\n");
    fprintf(f, "    mov x29, sp\n");

    int aligned_frame = align_to(frame_size, 16);
    if (aligned_frame > 0) {
        fprintf(f, "    sub sp, sp, #%d\n", aligned_frame);
    }

    if (config.enable_stack_checks) {
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
    } else {
        fprintf(f, "    adrp x0, %s@PAGE\n", src);
        fprintf(f, "    add x0, x0, %s@PAGEOFF\n", src);
    }
}

static void emit_arm64_store(FILE* f, const char* dest, int frame_size) {
    if (is_var(dest) || is_temp(dest)) {
        int offset = get_stack_offset(dest, frame_size);
        fprintf(f, "    str x0, [sp, #%d]\n", -offset);
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
    else if (strcmp(inst->opcode, "print_str") == 0) {
        fprintf(f, "    adrp x0, %s@PAGE\n", inst->operands[0]);
        fprintf(f, "    add x0, x0, %s@PAGEOFF\n", inst->operands[0]);
        fprintf(f, "    bl _puts\n");
    }
    else if (strcmp(inst->opcode, "ret") == 0) {
        if (inst->operands[0]) {
            emit_arm64_load(f, inst->operands[0], frame_size);
        }
        emit_arm64_epilogue(f, frame_size);
    }
    else if (strcmp(inst->opcode, "func") == 0) {
        emit_arm64_prologue(f, inst->operands[0], frame_size);
    }
}

#endif

static void emit_string_table(FILE* f, IRProgram* program) {
    fprintf(f, ".section __TEXT,__cstring,cstring_literals\n");
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
        fprintf(f, "    .asciz \"runtime error: null pointer dereference\\\n\"\n");
    }
    fprintf(f, ".text\n");
}

void codegen_emit_asm(IRProgram* program, const char* output_file) {
    FILE* f = fopen(output_file, "w");
    if (!f) {
        error("Cannot open output file: %s", output_file);
    }

#ifdef UWUCC_ARCH_X86_64
    fprintf(f, ".section __TEXT,__text,regular,pure_instructions\n");
    emit_string_table(f, program);

    for (IRInstruction* i = program->head; i; i = i->next) {
        if (strcmp(i->opcode, "string") != 0) {
            emit_x86_64_instruction(f, i, program->frame_size);
        }
    }

#elif defined(UWUCC_ARCH_ARM64)
    fprintf(f, ".section __TEXT,__text,regular,pure_instructions\n");
    fprintf(f, ".macosx_version_min 11, 0\n");
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