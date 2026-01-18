/**
 * @file semantic.c
 * @brief Semantic analysis for the UwU-C language
 * @author bober
 * @version 1.0.0
 *
 * This file implements the semantic analysis phase of the UwU-C compiler.
 * It walks the Abstract Syntax Tree (AST) produced by the parser and performs
 * scope resolution, type checking, and validation of language rules.
 *
 * The semantic analyzer ensures that identifiers are declared before use,
 * expressions are type-correct, and constructs are valid before lowering
 * to the Intermediate Representation (IR).
 */

#include "semantic.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

typedef struct Symbol {
    char* name;
    Type* type;
    bool is_function;
    int stack_offset;
    struct Symbol* next;
} Symbol;

typedef struct SymbolTable {
    Symbol* head;
    struct SymbolTable* parent;
} SymbolTable;

static SymbolTable* symtab_new(SymbolTable* parent) {
    SymbolTable* st = xcalloc(1, sizeof(SymbolTable));
    st->parent = parent;
    return st;
}

static SymbolTable* current_scope = NULL;
static int current_stack_offset = 0;

static void symtab_add(SymbolTable* st, const char* name, Type* type, bool is_func) {
    Symbol* sym = xmalloc(sizeof(Symbol));
    sym->name = xstrdup(name);
    sym->type = type;
    sym->is_function = is_func;
    sym->stack_offset = 0;

    if (!is_func && type) {
        int align = type->align > 0 ? type->align : 8;
        current_stack_offset = (current_stack_offset + align - 1) & ~(align - 1);
        sym->stack_offset = current_stack_offset;
        current_stack_offset += type->size;
    }

    sym->next = st->head;
    st->head = sym;
}

static Symbol* symtab_lookup(SymbolTable* st, const char* name) {
    for (SymbolTable* current = st; current; current = current->parent) {
        for (Symbol* sym = current->head; sym; sym = sym->next) {
            if (strcmp(sym->name, name) == 0) {
                return sym;
            }
        }
    }
    return NULL;
}

static Type* resolve_type(ASTNode* type_node) {
    if (!type_node) return NULL;

    if (type_node->kind == AST_TYPE) {
        Type* t = NULL;

        if (strcmp(type_node->data.name, "chonk") == 0) {
            t = type_new(TYPE_CHONK);
        }
        else if (strcmp(type_node->data.name, "smol") == 0) {
            t = type_new(TYPE_SMOL);
        }
        else if (strcmp(type_node->data.name, "megachonk") == 0) {
            t = type_new(TYPE_MEGACHONK);
        }
        else if (strcmp(type_node->data.name, "floof") == 0) {
            t = type_new(TYPE_FLOOF);
        }
        else if (strcmp(type_node->data.name, "bigfloof") == 0) {
            t = type_new(TYPE_BIGFLOOF);
        }
        else if (strcmp(type_node->data.name, "boop") == 0) {
            t = type_new(TYPE_BOOP);
        }
        else if (strcmp(type_node->data.name, "byte") == 0) {
            t = type_new(TYPE_BYTE);
        }
        else if (strcmp(type_node->data.name, "void") == 0) {
            t = type_new(TYPE_VOID);
        }
        else {
            t = type_new(TYPE_STRUCT);
            t->name = xstrdup(type_node->data.name);
        }
        return t;
    }
    else if (type_node->kind == AST_POINTER_TYPE) {
        Type* base = resolve_type(type_node->children[0]);
        return type_pointer(base);
    }
    else if (type_node->kind == AST_ARRAY_TYPE) {
        Type* base = resolve_type(type_node->children[0]);
        int size = 0;
        if (type_node->child_count > 1 &&
            type_node->children[1]->kind == AST_NUMBER) {
            size = type_node->children[1]->data.int_value;
        }
        return type_array(base, size);
    }

    return NULL;
}

static Type* check_expression(ASTNode* node);
static void check_statement(ASTNode* node);
static void check_block_for_declarations(ASTNode* node);

static void check_statement(ASTNode* node) {
    if (!node) return;

    switch (node->kind) {
        case AST_RETURN:
            if (node->child_count > 0) {
                check_expression(node->children[0]);
            }
            break;

        case AST_IF:
        case AST_WHILE:
            check_expression(node->children[0]);
            check_statement(node->children[1]);
            if (node->child_count > 2) {
                check_statement(node->children[2]);
            }
            break;

        case AST_FOR:
            if (node->child_count > 0) check_statement(node->children[0]);
            if (node->child_count > 1) check_expression(node->children[1]);
            if (node->child_count > 2) check_expression(node->children[2]);
            if (node->child_count > 3) check_statement(node->children[3]);
            break;

        case AST_BLOCK:
            for (int i = 0; i < node->child_count; i++) {
                check_statement(node->children[i]);
            }
            break;

        case AST_UNSAFE_BLOCK:
            for (int i = 0; i < node->child_count; i++) {
                check_statement(node->children[i]);
            }
            break;

        case AST_VAR_DECL:
            if (node->child_count > 1) {
                check_expression(node->children[1]);
            }
            break;

        default:
            check_expression(node);
            break;
    }
}

static Type* check_expression(ASTNode* node) {
    if (!node) return NULL;

    switch (node->kind) {
        case AST_NUMBER:
            node->type = type_new(TYPE_CHONK);
            return node->type;

        case AST_FLOAT:
            node->type = type_new(TYPE_BIGFLOOF);
            return node->type;

        case AST_BOOLEAN:
            node->type = type_new(TYPE_BOOP);
            return node->type;

        case AST_STRING:
            node->type = type_pointer(type_new(TYPE_BYTE));
            return node->type;

        case AST_NULL:
            node->type = type_new(TYPE_POINTER);
            return node->type;

        case AST_IDENTIFIER: {
            Symbol* sym = symtab_lookup(current_scope, node->data.name);
            if (!sym) {
                error_at(node->line, node->column,
                         "Undefined identifier: %s", node->data.name);
            }
            node->type = sym->type;
            node->stack_offset = sym->stack_offset;
            return node->type;
        }

        case AST_BINARY_OP: {
            Type* left = check_expression(node->children[0]);
            Type* right = check_expression(node->children[1]);
            node->type = left;
            return node->type;
        }

        case AST_UNARY_OP:
            node->type = check_expression(node->children[0]);
            return node->type;

        case AST_ASSIGN: {
            Type* left = check_expression(node->children[0]);
            Type* right = check_expression(node->children[1]);
            node->type = left;
            return node->type;
        }

        case AST_CALL: {
            Symbol* sym = symtab_lookup(
                current_scope, node->children[0]->data.name);
            if (sym && sym->is_function) {
                node->type = sym->type;
            } else {
                node->type = type_new(TYPE_CHONK);
            }
            return node->type;
        }

        default:
            node->type = type_new(TYPE_CHONK);
            return node->type;
    }
}

static void check_block_for_declarations(ASTNode* node) {
    if (!node) return;

    if (node->kind == AST_BLOCK) {
        for (int i = 0; i < node->child_count; i++) {
            ASTNode* stmt = node->children[i];

            if (stmt->kind == AST_VAR_DECL) {
                Type* var_type = resolve_type(stmt->children[0]);
                symtab_add(current_scope, stmt->data.name, var_type, false);

                Symbol* sym = symtab_lookup(current_scope, stmt->data.name);
                if (sym) {
                    stmt->stack_offset = sym->stack_offset;
                }

                if (stmt->child_count > 1) {
                    check_expression(stmt->children[1]);
                }
            }
            else if (stmt->kind == AST_BLOCK || stmt->kind == AST_IF ||
                     stmt->kind == AST_WHILE || stmt->kind == AST_FOR) {
                check_block_for_declarations(stmt);
            }
        }
    }
    else if (node->kind == AST_IF) {
        if (node->child_count > 1) check_block_for_declarations(node->children[1]);
        if (node->child_count > 2) check_block_for_declarations(node->children[2]);
    }
    else if (node->kind == AST_WHILE || node->kind == AST_FOR) {
        if (node->child_count > 1) check_block_for_declarations(node->children[1]);
    }
}

static void check_declaration(ASTNode* node) {
    if (!node) return;

    if (node->kind == AST_FUNCTION) {
        current_stack_offset = 0;

        SymbolTable* func_scope = symtab_new(current_scope);
        SymbolTable* old_scope = current_scope;
        current_scope = func_scope;

        symtab_add(current_scope, node->data.name,
                   resolve_type(node->children[0]), true);

        ASTNode* params = node->children[1];
        for (int i = 0; i < params->child_count; i++) {
            ASTNode* param = params->children[i];
            if (param->kind == AST_VAR_DECL) {
                Type* param_type = resolve_type(param->children[0]);
                symtab_add(current_scope, param->data.name, param_type, false);

                Symbol* sym = symtab_lookup(current_scope, param->data.name);
                if (sym) {
                    param->stack_offset = sym->stack_offset;
                }
            }
        }

        ASTNode* body = node->children[2];
        check_block_for_declarations(body);
        check_statement(body);

        node->stack_offset = current_stack_offset;

        current_scope = old_scope;
    }
    else if (node->kind == AST_VAR_DECL) {
        Type* var_type = resolve_type(node->children[0]);
        symtab_add(current_scope, node->data.name, var_type, false);

        Symbol* sym = symtab_lookup(current_scope, node->data.name);
        if (sym) {
            node->stack_offset = sym->stack_offset;
        }

        if (node->child_count > 1) {
            check_expression(node->children[1]);
        }
    }
}

void semantic_analyze(ASTNode* root) {
    if (!root || root->kind != AST_PROGRAM) {
        error("Invalid AST");
    }

    current_scope = symtab_new(NULL);

    for (int i = 0; i < root->child_count; i++) {
        check_declaration(root->children[i]);
    }
}
