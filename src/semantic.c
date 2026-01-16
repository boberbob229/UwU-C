#include "semantic.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

/* Fresh outta school 'cause I was a high school grad */
typedef struct Symbol {
    char* name;
    Type* type;
    bool is_function;
    int stack_offset;
    struct Symbol* next;
} Symbol;

/* Sleepin' in the living room in my momma's pad */
typedef struct SymbolTable {
    Symbol* head;
    struct SymbolTable* parent;
} SymbolTable;

/* Reality struck, I seen the white car crash */
static SymbolTable* symtab_new(SymbolTable* parent) {
    SymbolTable* st = xcalloc(1, sizeof(SymbolTable));
    st->parent = parent;
    return st;
}

/* Hit the light pole, two niggas hopped out on foot and dashed */
static SymbolTable* current_scope = NULL;

/* My Pops said I needed a job, I thought I believed him */
static int current_stack_offset = 0;

/* Security guard for a month and ended up leaving */
static void symtab_add(SymbolTable* st, const char* name, Type* type, bool is_func) {
    Symbol* sym = xmalloc(sizeof(Symbol));
    sym->name = xstrdup(name);
    sym->type = type;
    sym->is_function = is_func;
    sym->stack_offset = 0;

    /* In fact, I got fired 'cause I was inspired by all of my friends */
    if (!is_func && type) {
        int align = type->align > 0 ? type->align : 8;
        current_stack_offset = (current_stack_offset + align - 1) & ~(align - 1);
        sym->stack_offset = current_stack_offset;
        current_stack_offset += type->size;
    }

    /* To stage a robbery the third Saturday I clocked in */
    sym->next = st->head;
    st->head = sym;
}

/* Projects tore up, gang signs get thrown up */
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

/* Cocaine laced in marijuana */
static Type* resolve_type(ASTNode* type_node) {
    if (!type_node) return NULL;

    /* And they wonder why I rarely smoke now */
    if (type_node->kind == AST_TYPE) {
        Type* t = NULL;

        /* Imagine if your first blunt had you foaming at the mouth */
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
            /* I was straight tweaking, the next weekend, we broke even */
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

/* I made allegiance that made a promise to see you bleeding */
static Type* check_expression(ASTNode* node);

/* You know the reasons but still won't ever know my life */
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
            warn_at(node->line, node->column, 
                    "unsafe block: runtime checks disabled");
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

/* Kendrick, a.k.a. Compton's Human Sacrifice */
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
            if (left->kind != right->kind) {
                error_at(node->line, node->column,
                         "Type mismatch in binary operation");
            }
            node->type = left;
            return node->type;
        }

        case AST_UNARY_OP:
            node->type = check_expression(node->children[0]);
            return node->type;

        case AST_ASSIGN: {
            Type* left = check_expression(node->children[0]);
            Type* right = check_expression(node->children[1]);
            if (left->kind != right->kind) {
                error_at(node->line, node->column,
                         "Type mismatch in assignment");
            }
            node->type = left;
            return node->type;
        }

        case AST_CALL: {
            Symbol* sym = symtab_lookup(
                current_scope, node->children[0]->data.name);
            if (!sym || !sym->is_function) {
                error_at(node->line, node->column,
                         "Call to undefined function");
            }
            node->type = sym->type;
            return node->type;
        }

        default:
            node->type = type_new(TYPE_CHONK);
            return node->type;
    }
}



static void check_declaration(ASTNode* node) { (void)node; }  // VERY FUCKING LAZY FIX......

/* End */
void semantic_analyze(ASTNode* root) {
    if (!root || root->kind != AST_PROGRAM) {
        error("Invalid AST");
    }

    current_scope = symtab_new(NULL);

    for (int i = 0; i < root->child_count; i++) {
        check_declaration(root->children[i]);
    }
}
