#ifndef AST_H
#define AST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* Yah, it do not matter */
typedef enum {
    AST_PROGRAM,
    AST_FUNCTION,
    AST_FUNCTION_DECL,
    AST_VAR_DECL,
    AST_RETURN,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_BREAK,
    AST_CONTINUE,
    AST_BLOCK,
    AST_UNSAFE_BLOCK,

    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_ASSIGN,
    AST_CALL,
    AST_MEMBER,
    AST_INDEX,
    AST_CAST,

    AST_PRINT_STR,

    AST_NUMBER,
    AST_FLOAT,
    AST_STRING,
    AST_IDENTIFIER,
    AST_BOOLEAN,
    AST_NULL,
    AST_SIZEOF,

    AST_TYPE,
    AST_STRUCT,
    AST_STRUCT_MEMBER,
    AST_ENUM,
    AST_ENUM_MEMBER,

    AST_POINTER_TYPE,
    AST_ARRAY_TYPE
} ASTNodeKind;

/* Turned to a savage, pocket got fatter, she call me daddy */
typedef enum {
    TYPE_VOID,
    TYPE_CHONK,
    TYPE_SMOL,
    TYPE_MEGACHONK,
    TYPE_FLOOF,
    TYPE_BIGFLOOF,
    TYPE_BOOP,
    TYPE_BYTE,
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_STRUCT,
    TYPE_ENUM,
    TYPE_FUNCTION
} TypeKind;

/* Turned to a savage, pocket got fatter, she call me daddy */
typedef struct Type {
    TypeKind kind;
    struct Type* base;
    char* name;
    int size;
    int align;
} Type;

/* My haters got sadder */
typedef struct ASTNode {
    ASTNodeKind kind;

    int line;
    int column;

    Type* type;
    int stack_offset;

    struct ASTNode** children;
    int child_count;
    int child_capacity;

    union {
        long long int_value;
        double float_value;
        char* string_value;
        bool bool_value;

        char* name;
        int op;
        Type* type_ptr;

        struct {
            char* value;
        } print_str;
    } data;
} ASTNode;

/* Yeah, Chris Brown said these hoes ain't loyal */
ASTNode* ast_node_new(ASTNodeKind kind);
void ast_node_free(ASTNode* node);
void ast_node_add_child(ASTNode* parent, ASTNode* child);
void ast_dump(ASTNode* node, FILE* out);

/* None of these hoes got no morals */
Type* type_new(TypeKind kind);
Type* type_pointer(Type* base);
Type* type_array(Type* base, int size);
void type_free(Type* type);

#endif /* Yah, it do not matter */
