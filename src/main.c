/**
 * uwucc compiler entry point
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "ir.h"
#include "codegen.h"
#include "util.h"

static void print_usage(const char* program) {
    fprintf(stderr, "Usage: %s <input.uwu> [options]\n", program);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -o <file>        Output binary (default: a.out)\n");
    fprintf(stderr, "  --dump-ast       Print AST and exit\n");
    fprintf(stderr, "  --dump-ir        Print IR and exit\n");
    fprintf(stderr, "  --emit-asm       Keep assembly file\n");
    fprintf(stderr, "  --version, -v    Show version\n");
    fprintf(stderr, "  --help, -h       Show this help\n");
}

static void print_version(void) {
    printf("uwucc 1.0\n");
    printf("Platform: %s (%s)\n", UWUCC_PLATFORM_NAME, UWUCC_ARCH_NAME);
}

int main(int argc, char** argv) {
    if (argc >= 2) {
        if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
            print_version();
            return 0;
        }
        if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char* input_file = argv[1];
    const char* output_file = "a.out";
    bool dump_ast = false;
    bool dump_ir = false;
    bool keep_asm = false;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else if (strcmp(argv[i], "--dump-ast") == 0) {
            dump_ast = true;
        } else if (strcmp(argv[i], "--dump-ir") == 0) {
            dump_ir = true;
        } else if (strcmp(argv[i], "--emit-asm") == 0) {
            keep_asm = true;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return 1;
        }
    }

    char* source = read_file(input_file);
    if (!source) {
        error("Failed to read file: %s", input_file);
    }

    Lexer* lexer = lexer_new(source);
    Parser* parser = parser_new(lexer);

    if (!lexer || !parser) {
        error("Failed to initialize compiler");
    }

    ASTNode* ast = parse(parser);
    if (!ast) {
        error("Parsing failed");
    }

    if (dump_ast) {
        ast_dump(ast, stdout);
        return 0;
    }

    semantic_analyze(ast);

    IRProgram* ir = ir_generate(ast);
    if (!ir) {
        error("IR generation failed");
    }

    if (dump_ir) {
        ir_dump(ir, stdout);
        return 0;
    }

    char asm_file[512];
    snprintf(asm_file, sizeof(asm_file), "%s.s", output_file);

    codegen_emit_asm(ir, asm_file);

#ifdef UWUCC_PLATFORM_MACOS
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "clang %s -o %s", asm_file, output_file);
#elif defined(UWUCC_PLATFORM_LINUX)
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "gcc %s -no-pie -o %s", asm_file, output_file);
#endif

    if (system(cmd) != 0) {
        error("Assembly or linking failed");
    }

    ir_program_free(ir);
    ast_node_free(ast);
    parser_free(parser);
    lexer_free(lexer);
    free(source);
    
    if (!keep_asm) {
        remove(asm_file);
    }

    return 0;
}
