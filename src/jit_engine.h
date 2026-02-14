#ifndef JIT_ENGINE_H
#define JIT_ENGINE_H

#include "ssa_ir.h"
#include <stddef.h>

typedef enum {
    JIT_INTERP,
    JIT_BASELINE,
    JIT_OPTIMIZED,
    JIT_ADAPTIVE
} JITTier;

typedef enum {
    ARCH_X86_64,
    ARCH_AARCH64,
    ARCH_X86_32
} TargetArch;

typedef struct {
    void *code_mem;
    void *data_mem;
    size_t code_size;
    size_t data_size;
    bool is_executable;
    struct CodeBlock *next;
} CodeBlock;

typedef struct {
    char *symbol;
    void *addr;
    bool external;
    struct Symbol *next;
} Symbol;

typedef struct {
    int *vreg_to_phys;
    int vreg_count;
    bool *phys_used;
    int phys_count;
    struct {
        int *start_pos;
        int *end_pos;
        int interval_count;
    } live_intervals;
    int *spill_slots;
    int spill_count;
} RegisterAlloc;

typedef struct {
    uint8_t *buf;
    size_t cap;
    size_t pos;
    struct Fixup **fixups;
    int fixup_count;
} CodeGen;

typedef struct Fixup {
    size_t offset;
    enum {
        FIX_ABS32, FIX_ABS64, FIX_REL32, FIX_GOT, FIX_PLT
    } type;
    char *symbol;
    int addend;
} Fixup;

typedef struct {
    JITTier tier;
    TargetArch arch;
    Module *module;
    CodeBlock *code_blocks;
    Symbol *symtab;
    RegisterAlloc *reg_alloc;
    CodeGen *codegen;
    struct {
        bool enable_prof;
        bool enable_type_feedback;
        bool enable_inline_cache;
        bool enable_speculative;
        int recomp_threshold;
    } config;
    struct {
        int64_t *exec_counts;
        int64_t *branch_taken;
        void **call_targets;
    } profile_data;
    void *(*symbol_resolver)(const char *);
} JITContext;

typedef struct {
    JITContext *jit;
    void **func_cache;
    int cache_size;
    struct {
        int total_comps;
        int total_recomps;
        int64_t exec_time;
        int64_t comp_time;
    } stats;
} ExecEngine;

JITContext *jit_create(TargetArch arch, JITTier tier);
void jit_destroy(JITContext *jit);

void *jit_compile_func(JITContext *jit, Function *f);
void *jit_compile_module(JITContext *jit, Module *m);

void jit_add_symbol(JITContext *jit, const char *name, void *addr);
void *jit_lookup_symbol(JITContext *jit, const char *name);

CodeBlock *alloc_exec_mem(size_t code_sz, size_t data_sz);
void free_exec_mem(CodeBlock *cb);
void make_executable(CodeBlock *cb);

ExecEngine *engine_create(JITTier tier);
void engine_destroy(ExecEngine *ee);

void *engine_get_func_ptr(ExecEngine *ee, const char *name);
int engine_run_main(ExecEngine *ee);
void engine_finalize_module(ExecEngine *ee, Module *m);

void engine_print_stats(ExecEngine *ee);

RegisterAlloc *regalloc_create(int vreg_cnt);
void regalloc_destroy(RegisterAlloc *ra);
void regalloc_allocate(RegisterAlloc *ra, Function *f);
int regalloc_get_phys(RegisterAlloc *ra, int vreg);
bool regalloc_is_spilled(RegisterAlloc *ra, int vreg);

CodeGen *codegen_create(size_t init_sz);
void codegen_destroy(CodeGen *cg);
void codegen_emit_u8(CodeGen *cg, uint8_t b);
void codegen_emit_u16(CodeGen *cg, uint16_t w);
void codegen_emit_u32(CodeGen *cg, uint32_t dw);
void codegen_emit_u64(CodeGen *cg, uint64_t qw);
void codegen_emit_bytes(CodeGen *cg, const uint8_t *data, size_t len);
void codegen_add_fixup(CodeGen *cg, Fixup *fix);
void codegen_apply_fixups(CodeGen *cg, Symbol *syms);

void x64_emit_prologue(CodeGen *cg, int frame_sz);
void x64_emit_epilogue(CodeGen *cg);
void x64_emit_inst(CodeGen *cg, Instruction *i, RegisterAlloc *ra);
void x64_emit_mov(CodeGen *cg, int dst, int src);
void x64_emit_add(CodeGen *cg, int dst, int src);
void x64_emit_sub(CodeGen *cg, int dst, int src);
void x64_emit_mul(CodeGen *cg, int dst, int src);
void x64_emit_div(CodeGen *cg, int dst, int src);
void x64_emit_and(CodeGen *cg, int dst, int src);
void x64_emit_or(CodeGen *cg, int dst, int src);
void x64_emit_xor(CodeGen *cg, int dst, int src);
void x64_emit_shl(CodeGen *cg, int dst, int amt);
void x64_emit_shr(CodeGen *cg, int dst, int amt);
void x64_emit_cmp(CodeGen *cg, int r1, int r2);
void x64_emit_jmp(CodeGen *cg, int32_t off);
void x64_emit_je(CodeGen *cg, int32_t off);
void x64_emit_jne(CodeGen *cg, int32_t off);
void x64_emit_jl(CodeGen *cg, int32_t off);
void x64_emit_jle(CodeGen *cg, int32_t off);
void x64_emit_jg(CodeGen *cg, int32_t off);
void x64_emit_jge(CodeGen *cg, int32_t off);
void x64_emit_call(CodeGen *cg, void *target);
void x64_emit_ret(CodeGen *cg);
void x64_emit_push(CodeGen *cg, int reg);
void x64_emit_pop(CodeGen *cg, int reg);
void x64_emit_load_imm(CodeGen *cg, int reg, int64_t val);
void x64_emit_load_mem(CodeGen *cg, int reg, int base, int off);
void x64_emit_store_mem(CodeGen *cg, int reg, int base, int off);

void arm64_emit_inst(CodeGen *cg, Instruction *i, RegisterAlloc *ra);

typedef struct {
    int64_t *regs;
    int reg_count;
    uint8_t *mem;
    size_t mem_size;
    void **stack;
    int stack_sz;
    int sp;
    BasicBlock *cur_block;
    Instruction *cur_inst;
} InterpState;

InterpState *interp_create(void);
void interp_destroy(InterpState *is);
int64_t interp_exec_func(Function *f, int64_t *args, int argc);

typedef struct PGO {
    struct {
        int64_t *block_counts;
        int64_t *edge_counts;
        int64_t *call_counts;
    } profile;
    struct {
        BasicBlock **hot_blocks;
        int hot_count;
        BasicBlock **cold_blocks;
        int cold_count;
    } layout;
} PGO;

PGO *pgo_create(Function *f);
void pgo_destroy(PGO *pgo);
void pgo_optimize_layout(PGO *pgo, Function *f);
void pgo_inline_hot_calls(PGO *pgo, Function *f);

typedef struct {
    void **ic_targets;
    int *ic_counts;
    int ic_size;
} InlineCache;

InlineCache *ic_create(int size);
void ic_destroy(InlineCache *ic);
void ic_record_call(InlineCache *ic, int site, void *target);
void *ic_get_monomorphic_target(InlineCache *ic, int site);

typedef struct {
    Instruction *guard_inst;
    void *deopt_handler;
    void *metadata;
} Speculation;

Speculation *spec_create(Instruction *guard);
void spec_destroy(Speculation *spec);
void spec_install_guard(Speculation *spec, void *handler);

#endif
