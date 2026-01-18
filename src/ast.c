#include "ast.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

ASTNode* ast_node_new(ASTNodeKind kind) {
    ASTNode* node = xcalloc(1, sizeof(ASTNode));
    node->kind = kind;
    node->line = 0;
    node->column = 0;
    node->type = NULL;
    node->stack_offset = 0;
    node->child_count = 0;  // Initialize child count
    node->child_capacity = 4;
    node->children = xcalloc(node->child_capacity, sizeof(ASTNode*));
    return node;
}

// izi lazy fix
void ast_dump(ASTNode* node, FILE* out) {
    (void)node;
    (void)out;
}


void ast_node_free(ASTNode* node) {
    if (!node) return;
    
    for (int i = 0; i < node->child_count; i++) {
        ast_node_free(node->children[i]);
    }
    
    if (node->children) {
        free(node->children);
    }
    
    // Free string data
    if (node->kind == AST_IDENTIFIER || node->kind == AST_STRING || 
        node->kind == AST_TYPE) {
        if (node->data.name) {
            free(node->data.name);
        }
    }
    
    if (node->type) {
        type_free(node->type);
    }
    
    free(node);
}

void ast_node_add_child(ASTNode* parent, ASTNode* child) {
    if (parent->child_count >= parent->child_capacity) {
        parent->child_capacity *= 2;
        parent->children = xrealloc(parent->children, 
                                   parent->child_capacity * sizeof(ASTNode*));
    }
    parent->children[parent->child_count++] = child;
}

// Type system implementation
Type* type_new(TypeKind kind) {
    Type* t = xcalloc(1, sizeof(Type));
    t->kind = kind;
    
    // Set default sizes
    switch (kind) {
        case TYPE_VOID: t->size = 0; t->align = 1; break;
        case TYPE_BYTE: t->size = 1; t->align = 1; break;
        case TYPE_BOOP: t->size = 1; t->align = 1; break;
        case TYPE_SMOL: t->size = 2; t->align = 2; break;
        case TYPE_CHONK: t->size = 4; t->align = 4; break;
        case TYPE_MEGACHONK: t->size = 8; t->align = 8; break;
        case TYPE_FLOOF: t->size = 4; t->align = 4; break;
        case TYPE_BIGFLOOF: t->size = 8; t->align = 8; break;
        case TYPE_POINTER: t->size = 8; t->align = 8; break;
        default: t->size = 0; t->align = 1; break;
    }
    
    return t;
}

Type* type_pointer(Type* base) {
    Type* t = type_new(TYPE_POINTER);
    t->base = base;
    return t;
}

Type* type_array(Type* base, int size) {
    Type* t = type_new(TYPE_ARRAY);
    t->base = base;
    t->size = base->size * size;
    t->align = base->align;
    return t;
}

void type_free(Type* type) {
    if (!type) return;
    if (type->base) {
        type_free(type->base);
    }
    if (type->name) {
        free(type->name);
    }
    free(type);
}
