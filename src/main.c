/**
 * UwUCC compiler entry point
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

    char exe_dir[512];
    char* last_slash = strrchr(argv[0], '/');
    if (last_slash) {
        size_t dir_len = last_slash - argv[0];
        strncpy(exe_dir, argv[0], dir_len);
        exe_dir[dir_len] = '\0';
    } else {
        strcpy(exe_dir, ".");
    }

    const char* stdlib_locations[] = {
        "../../stdlib/uwu_stdlib.o",
        "../../../stdlib/uwu_stdlib.o",
        "stdlib/uwu_stdlib.o",
        "../stdlib/uwu_stdlib.o",
        NULL
    };

    const char* stdlib_source_locations[] = {
        "../../stdlib/uwu_stdlib.c",
        "../../../stdlib/uwu_stdlib.c",
        "stdlib/uwu_stdlib.c",
        "../stdlib/uwu_stdlib.c",
        NULL
    };

    char stdlib_path[1024];
    char stdlib_source[1024];
    bool found = false;

    for (int i = 0; stdlib_locations[i]; i++) {
        snprintf(stdlib_path, sizeof(stdlib_path), "%s/%s", exe_dir, stdlib_locations[i]);
        snprintf(stdlib_source, sizeof(stdlib_source), "%s/%s", exe_dir, stdlib_source_locations[i]);

        FILE* test = fopen(stdlib_path, "r");
        if (test) {
            fclose(test);
            found = true;
            break;
        }

        FILE* source_test = fopen(stdlib_source, "r");
        if (source_test) {
            fclose(source_test);

            fprintf(stderr, "Building stdlib (first time setup)...\n");

            char build_cmd[2048];
#ifdef UWUCC_PLATFORM_MACOS
            snprintf(build_cmd, sizeof(build_cmd), "gcc -c -O2 %s -o %s 2>&1",
                    stdlib_source, stdlib_path);
#else
            snprintf(build_cmd, sizeof(build_cmd), "gcc -c -O2 %s -o %s 2>&1",
                    stdlib_source, stdlib_path);
#endif

            int result = system(build_cmd);
            if (result == 0) {
                fprintf(stderr, "Stdlib built successfully!\n");
                found = true;
                break;
            } else {
                fprintf(stderr, "Warning: Failed to build stdlib automatically\n");
            }
        }
    }

    if (!found) {
        error("Could not find or build uwu_stdlib.o\n"
              "Searched in %s\n"
              "Please run: make clean && make", exe_dir);
    }

#ifdef UWUCC_PLATFORM_MACOS
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "clang %s %s -o %s", asm_file, stdlib_path, output_file);
#elif defined(UWUCC_PLATFORM_LINUX)
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "gcc %s %s -no-pie -o %s", asm_file, stdlib_path, output_file);
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