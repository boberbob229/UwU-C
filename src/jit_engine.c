#include "jit_engine.h"
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

JITContext *jit_create(TargetArch arch, JITTier tier) {
    JITContext *jit = calloc(1, sizeof(JITContext));
    jit->tier = tier;
    jit->arch = arch;
    jit->module = NULL;
    jit->code_blocks = NULL;
    jit->symtab = NULL;
    jit->reg_alloc = NULL;
    jit->codegen = codegen_create(8192);
    
    jit->config.enable_prof = (tier == JIT_ADAPTIVE);
    jit->config.enable_type_feedback = (tier == JIT_ADAPTIVE);
    jit->config.enable_inline_cache = (tier == JIT_ADAPTIVE);
    jit->config.enable_speculative = (tier == JIT_OPTIMIZED || tier == JIT_ADAPTIVE);
    jit->config.recomp_threshold = 1000;
    
    jit->symbol_resolver = NULL;
    
    return jit;
}

void jit_destroy(JITContext *jit) {
    if (!jit) return;
    
    CodeBlock *cb = jit->code_blocks;
    while (cb) {
        CodeBlock *next = cb->next;
        free_exec_mem(cb);
        cb = next;
    }
    
    Symbol *sym = jit->symtab;
    while (sym) {
        Symbol *next = sym->next;
        free(sym->symbol);
        free(sym);
        sym = next;
    }
    
    codegen_destroy(jit->codegen);
    free(jit);
}

CodeBlock *alloc_exec_mem(size_t code_sz, size_t data_sz) {
    CodeBlock *cb = calloc(1, sizeof(CodeBlock));
    
    size_t page_sz = sysconf(_SC_PAGESIZE);
    size_t code_pages = (code_sz + page_sz - 1) / page_sz;
    size_t data_pages = (data_sz + page_sz - 1) / page_sz;
    
    cb->code_size = code_pages * page_sz;
    cb->data_size = data_pages * page_sz;
    
    cb->code_mem = mmap(NULL, cb->code_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    
    if (cb->code_mem == MAP_FAILED) {
        free(cb);
        return NULL;
    }
    
    if (data_sz > 0) {
        cb->data_mem = mmap(NULL, cb->data_size, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        
        if (cb->data_mem == MAP_FAILED) {
            munmap(cb->code_mem, cb->code_size);
            free(cb);
            return NULL;
        }
    }
    
    cb->is_executable = false;
    cb->next = NULL;
    return cb;
}

void free_exec_mem(CodeBlock *cb) {
    if (!cb) return;
    if (cb->code_mem) munmap(cb->code_mem, cb->code_size);
    if (cb->data_mem) munmap(cb->data_mem, cb->data_size);
    free(cb);
}

void make_executable(CodeBlock *cb) {
    if (!cb || cb->is_executable) return;
    mprotect(cb->code_mem, cb->code_size, PROT_READ | PROT_EXEC);
    cb->is_executable = true;
}

void jit_add_symbol(JITContext *jit, const char *name, void *addr) {
    if (!jit || !name) return;
    
    Symbol *sym = calloc(1, sizeof(Symbol));
    sym->symbol = strdup(name);
    sym->addr = addr;
    sym->external = false;
    sym->next = jit->symtab;
    jit->symtab = sym;
}

void *jit_lookup_symbol(JITContext *jit, const char *name) {
    if (!jit || !name) return NULL;
    
    Symbol *sym = jit->symtab;
    while (sym) {
        if (strcmp(sym->symbol, name) == 0) {
            return sym->addr;
        }
        sym = sym->next;
    }
    
    return NULL;
}

void *jit_compile_func(JITContext *jit, Function *f) {
    if (!jit || !f) return NULL;
    
    jit->reg_alloc = regalloc_create(f->vreg_counter);
    regalloc_allocate(jit->reg_alloc, f);
    
    jit->codegen->pos = 0;
    
    int frame_sz = f->vreg_counter * 8;
    if (jit->arch == ARCH_X86_64) {
        x64_emit_prologue(jit->codegen, frame_sz);
        
        for (int i = 0; i < f->block_count; i++) {
            BasicBlock *bb = f->blocks[i];
            Instruction *inst = bb->first_inst;
            
            while (inst) {
                x64_emit_inst(jit->codegen, inst, jit->reg_alloc);
                inst = inst->next;
            }
        }
        
        x64_emit_epilogue(jit->codegen);
    }
    
    CodeBlock *cb = alloc_exec_mem(jit->codegen->pos, 0);
    if (!cb) return NULL;
    
    memcpy(cb->code_mem, jit->codegen->buf, jit->codegen->pos);
    make_executable(cb);
    
    cb->next = jit->code_blocks;
    jit->code_blocks = cb;
    
    jit_add_symbol(jit, f->name, cb->code_mem);
    
    regalloc_destroy(jit->reg_alloc);
    jit->reg_alloc = NULL;
    
    return cb->code_mem;
}

RegisterAlloc *regalloc_create(int vreg_cnt) {
    RegisterAlloc *ra = calloc(1, sizeof(RegisterAlloc));
    ra->vreg_count = vreg_cnt;
    ra->vreg_to_phys = calloc(vreg_cnt, sizeof(int));
    ra->phys_count = 14;
    ra->phys_used = calloc(ra->phys_count, sizeof(bool));
    ra->spill_slots = calloc(vreg_cnt, sizeof(int));
    ra->spill_count = 0;
    return ra;
}

void regalloc_destroy(RegisterAlloc *ra) {
    if (!ra) return;
    free(ra->vreg_to_phys);
    free(ra->phys_used);
    free(ra->spill_slots);
    free(ra);
}

void regalloc_allocate(RegisterAlloc *ra, Function *f) {
    if (!ra || !f) return;
    
    for (int i = 0; i < ra->vreg_count; i++) {
        int assigned = -1;
        for (int r = 0; r < ra->phys_count; r++) {
            if (!ra->phys_used[r]) {
                assigned = r;
                ra->phys_used[r] = true;
                break;
            }
        }
        
        if (assigned >= 0) {
            ra->vreg_to_phys[i] = assigned;
        } else {
            ra->vreg_to_phys[i] = -1;
            ra->spill_slots[i] = ra->spill_count++;
        }
    }
}

int regalloc_get_phys(RegisterAlloc *ra, int vreg) {
    if (!ra || vreg < 0 || vreg >= ra->vreg_count) return 0;
    return ra->vreg_to_phys[vreg] >= 0 ? ra->vreg_to_phys[vreg] : 0;
}

bool regalloc_is_spilled(RegisterAlloc *ra, int vreg) {
    if (!ra || vreg < 0 || vreg >= ra->vreg_count) return false;
    return ra->vreg_to_phys[vreg] < 0;
}

CodeGen *codegen_create(size_t init_sz) {
    CodeGen *cg = calloc(1, sizeof(CodeGen));
    cg->buf = malloc(init_sz);
    cg->cap = init_sz;
    cg->pos = 0;
    cg->fixups = NULL;
    cg->fixup_count = 0;
    return cg;
}

void codegen_destroy(CodeGen *cg) {
    if (!cg) return;
    free(cg->buf);
    for (int i = 0; i < cg->fixup_count; i++) {
        free(cg->fixups[i]->symbol);
        free(cg->fixups[i]);
    }
    free(cg->fixups);
    free(cg);
}

void codegen_emit_u8(CodeGen *cg, uint8_t b) {
    if (!cg) return;
    if (cg->pos >= cg->cap) {
        cg->cap *= 2;
        cg->buf = realloc(cg->buf, cg->cap);
    }
    cg->buf[cg->pos++] = b;
}

void codegen_emit_u16(CodeGen *cg, uint16_t w) {
    codegen_emit_u8(cg, w & 0xFF);
    codegen_emit_u8(cg, (w >> 8) & 0xFF);
}

void codegen_emit_u32(CodeGen *cg, uint32_t dw) {
    codegen_emit_u16(cg, dw & 0xFFFF);
    codegen_emit_u16(cg, (dw >> 16) & 0xFFFF);
}

void codegen_emit_u64(CodeGen *cg, uint64_t qw) {
    codegen_emit_u32(cg, qw & 0xFFFFFFFF);
    codegen_emit_u32(cg, (qw >> 32) & 0xFFFFFFFF);
}

void x64_emit_prologue(CodeGen *cg, int frame_sz) {
    codegen_emit_u8(cg, 0x55);
    codegen_emit_u8(cg, 0x48);
    codegen_emit_u8(cg, 0x89);
    codegen_emit_u8(cg, 0xE5);
    
    if (frame_sz > 0) {
        codegen_emit_u8(cg, 0x48);
        codegen_emit_u8(cg, 0x81);
        codegen_emit_u8(cg, 0xEC);
        codegen_emit_u32(cg, frame_sz);
    }
}

void x64_emit_epilogue(CodeGen *cg) {
    codegen_emit_u8(cg, 0x48);
    codegen_emit_u8(cg, 0x89);
    codegen_emit_u8(cg, 0xEC);
    codegen_emit_u8(cg, 0x5D);
    codegen_emit_u8(cg, 0xC3);
}

void x64_emit_mov(CodeGen *cg, int dst, int src) {
    codegen_emit_u8(cg, 0x48);
    codegen_emit_u8(cg, 0x89);
    codegen_emit_u8(cg, 0xC0 | (src << 3) | dst);
}

void x64_emit_add(CodeGen *cg, int dst, int src) {
    codegen_emit_u8(cg, 0x48);
    codegen_emit_u8(cg, 0x01);
    codegen_emit_u8(cg, 0xC0 | (src << 3) | dst);
}

void x64_emit_ret(CodeGen *cg) {
    codegen_emit_u8(cg, 0xC3);
}

void x64_emit_inst(CodeGen *cg, Instruction *i, RegisterAlloc *ra) {
    if (!cg || !i || !ra) return;
    
    int dst_reg = 0, src1_reg = 0, src2_reg = 0;
    
    if (i->result && i->result->kind == VAL_VREG) {
        dst_reg = regalloc_get_phys(ra, i->result->as.vreg_num);
    }
    
    if (i->operand_count > 0 && i->operands[0]->kind == VAL_VREG) {
        src1_reg = regalloc_get_phys(ra, i->operands[0]->as.vreg_num);
    }
    
    if (i->operand_count > 1 && i->operands[1]->kind == VAL_VREG) {
        src2_reg = regalloc_get_phys(ra, i->operands[1]->as.vreg_num);
    }
    
    switch (i->op) {
        case OP_MOV:
            if (i->operand_count > 0 && i->operands[0]->kind == VAL_IMMEDIATE) {
                codegen_emit_u8(cg, 0x48);
                codegen_emit_u8(cg, 0xB8 | dst_reg);
                codegen_emit_u64(cg, i->operands[0]->as.imm);
            } else if (src1_reg != dst_reg) {
                x64_emit_mov(cg, dst_reg, src1_reg);
            }
            break;
            
        case OP_ADD:
            if (src1_reg != dst_reg) {
                x64_emit_mov(cg, dst_reg, src1_reg);
            }
            x64_emit_add(cg, dst_reg, src2_reg);
            break;
            
        case OP_RET:
            if (i->operand_count > 0 && i->operands[0]->kind == VAL_VREG) {
                if (src1_reg != 0) {
                    x64_emit_mov(cg, 0, src1_reg);
                }
            }
            x64_emit_ret(cg);
            break;
            
        default:
            break;
    }
}

ExecEngine *engine_create(JITTier tier) {
    ExecEngine *ee = calloc(1, sizeof(ExecEngine));
    ee->jit = jit_create(ARCH_X86_64, tier);
    ee->func_cache = NULL;
    ee->cache_size = 0;
    memset(&ee->stats, 0, sizeof(ee->stats));
    return ee;
}

void engine_destroy(ExecEngine *ee) {
    if (!ee) return;
    jit_destroy(ee->jit);
    free(ee->func_cache);
    free(ee);
}

void *engine_get_func_ptr(ExecEngine *ee, const char *name) {
    if (!ee || !name) return NULL;
    return jit_lookup_symbol(ee->jit, name);
}

void engine_finalize_module(ExecEngine *ee, Module *m) {
    if (!ee || !m) return;
    
    Function *f = m->funcs;
    while (f) {
        if (!f->is_external) {
            jit_compile_func(ee->jit, f);
            ee->stats.total_comps++;
        }
        f = f->next_func;
    }
}

int engine_run_main(ExecEngine *ee) {
    if (!ee) return -1;
    
    typedef int (*MainFunc)(void);
    MainFunc main_fn = (MainFunc)engine_get_func_ptr(ee, "main");
    
    if (!main_fn) return -1;
    return main_fn();
}

void engine_print_stats(ExecEngine *ee) {
    if (!ee) return;
    printf("ez");
    printf("total comp:    %d", ee->stats.total_comps);
    printf("total recomp:  %d\n", ee->stats.total_recomps);
    printf("execution time:        %lld ms\n", (long long)ee->stats.exec_time);
    printf("compilation time:      %lld ms\n", (long long)ee->stats.comp_time);
}
