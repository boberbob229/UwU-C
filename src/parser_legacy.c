/**
 * @file parser.c
 * @brief Recursive descent parser for uwuc language
 * @author bober
 * @version 0.0.1
 * 
    * This file implements a complete recursive descent parser for the uwuc language.
    * This is going to be replaced with parsec_new.c soon.
 */

#include "parser.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

// Advance to next token
static void advance(Parser* p) {
    p->previous = p->current;
    p->current = lexer_next_token(p->lexer);
}

static bool check(Parser* p, TokenKind kind) {
    return p->current.kind == kind;
}

static bool match(Parser* p, TokenKind kind) {
    if (check(p, kind)) {
        advance(p);
        return true;
    }
    return false;
}

static void consume(Parser* p, TokenKind kind, const char* msg) {
    if (!match(p, kind)) {
        error_at(p->current.line, p->current.column, "Expected %s", msg);
    }
}

static ASTNode* parse_expression(Parser* p);
static ASTNode* parse_statement(Parser* p);
static ASTNode* parse_type(Parser* p);
static ASTNode* parse_declaration(Parser* p);

// Parse primary expressions (literals, identifiers, parenthesized)
static ASTNode* parse_primary(Parser* p) {
    ASTNode* node = NULL;
    
    if (match(p, TOKEN_NUMBER)) {
        node = ast_node_new(AST_NUMBER);
        node->line = p->previous.line;
        node->column = p->previous.column;
        node->data.int_value = p->previous.value.int_value;
    }
    else if (match(p, TOKEN_TRUE) || match(p, TOKEN_FALSE)) {
        node = ast_node_new(AST_BOOLEAN);
        node->line = p->previous.line;
        node->column = p->previous.column;
        node->data.bool_value = (p->previous.kind == TOKEN_TRUE);
    }
    else if (match(p, TOKEN_STRING)) {
        node = ast_node_new(AST_STRING);
        node->line = p->previous.line;
        node->column = p->previous.column;
        node->data.string_value = xstrdup(p->previous.lexeme);
    }
    else if (match(p, TOKEN_NUWW)) {
        node = ast_node_new(AST_NULL);
        node->line = p->previous.line;
        node->column = p->previous.column;
    }
    else if (match(p, TOKEN_IDENT)) {
        node = ast_node_new(AST_IDENTIFIER);
        node->line = p->previous.line;
        node->column = p->previous.column;
        node->data.name = xstrdup(p->previous.lexeme);
    }
    else if (match(p, TOKEN_LPAREN)) {
        node = parse_expression(p);
        consume(p, TOKEN_RPAREN, ")");
    }
    else if (match(p, TOKEN_SIZEOF)) {
        node = ast_node_new(AST_SIZEOF);
        node->line = p->previous.line;
        node->column = p->previous.column;
        consume(p, TOKEN_LPAREN, "(");
        ast_node_add_child(node, parse_type(p));
        consume(p, TOKEN_RPAREN, ")");
    }
    else {
        error_at(p->current.line, p->current.column, "Unexpected token: %s", token_kind_to_string(p->current.kind));
    }
    
    return node;
}

// Parse postfix expressions (function calls, array access, member access)
static ASTNode* parse_postfix(Parser* p) {
    ASTNode* node = parse_primary(p);
    
    while (true) {
        if (match(p, TOKEN_LPAREN)) {
            // Function call
            ASTNode* call = ast_node_new(AST_CALL);
            call->line = node->line;
            call->column = node->column;
            call->children = xcalloc(1, sizeof(ASTNode*));
            call->children[0] = node;
            call->child_count = 1;
            
            if (!check(p, TOKEN_RPAREN)) {
                do {
                    ASTNode* arg = parse_expression(p);
                    ast_node_add_child(call, arg);
                } while (match(p, TOKEN_COMMA));
            }
            consume(p, TOKEN_RPAREN, ")");
            node = call;
        }
        else if (match(p, TOKEN_LBRACKET)) {
            // Array access
            ASTNode* index = ast_node_new(AST_INDEX);
            index->line = node->line;
            index->column = node->column;
            ast_node_add_child(index, node);
            ast_node_add_child(index, parse_expression(p));
            consume(p, TOKEN_RBRACKET, "]");
            node = index;
        }
        else if (match(p, TOKEN_DOT)) {
            // Member access
            ASTNode* member = ast_node_new(AST_MEMBER);
            member->line = node->line;
            member->column = node->column;
            ast_node_add_child(member, node);
            consume(p, TOKEN_IDENT, "identifier");
            ASTNode* name = ast_node_new(AST_IDENTIFIER);
            name->data.name = xstrdup(p->previous.lexeme);
            ast_node_add_child(member, name);
            node = member;
        }
        else {
            break;
        }
    }
    
    return node;
}

// Parse unary expressions
static ASTNode* parse_unary(Parser* p) {
    if (match(p, TOKEN_MINUS) || match(p, TOKEN_PLUS) || 
        match(p, TOKEN_NOT) || match(p, TOKEN_TILDE) || match(p, TOKEN_AMP) || 
        match(p, TOKEN_STAR)) {
        ASTNode* node = ast_node_new(AST_UNARY_OP);
        node->line = p->previous.line;
        node->column = p->previous.column;
        node->data.op = p->previous.kind;
        ast_node_add_child(node, parse_unary(p));
        return node;
    }
    return parse_postfix(p);
}

// Parse multiplicative expressions
static ASTNode* parse_multiplicative(Parser* p) {
    ASTNode* node = parse_unary(p);
    
    while (match(p, TOKEN_STAR) || match(p, TOKEN_SLASH) || match(p, TOKEN_PERCENT)) {
        ASTNode* binop = ast_node_new(AST_BINARY_OP);
        binop->line = p->previous.line;
        binop->column = p->previous.column;
        binop->data.op = p->previous.kind;
        ast_node_add_child(binop, node);
        ast_node_add_child(binop, parse_unary(p));
        node = binop;
    }
    
    return node;
}

// Parse additive expressions
static ASTNode* parse_additive(Parser* p) {
    ASTNode* node = parse_multiplicative(p);
    
    while (match(p, TOKEN_PLUS) || match(p, TOKEN_MINUS)) {
        ASTNode* binop = ast_node_new(AST_BINARY_OP);
        binop->line = p->previous.line;
        binop->column = p->previous.column;
        binop->data.op = p->previous.kind;
        ast_node_add_child(binop, node);
        ast_node_add_child(binop, parse_multiplicative(p));
        node = binop;
    }
    
    return node;
}

// Parse shift expressions
static ASTNode* parse_shift(Parser* p) {
    ASTNode* node = parse_additive(p);
    
    while (match(p, TOKEN_LSHIFT) || match(p, TOKEN_RSHIFT)) {
        ASTNode* binop = ast_node_new(AST_BINARY_OP);
        binop->line = p->previous.line;
        binop->column = p->previous.column;
        binop->data.op = p->previous.kind;
        ast_node_add_child(binop, node);
        ast_node_add_child(binop, parse_additive(p));
        node = binop;
    }
    
    return node;
}

// Parse relational expressions
static ASTNode* parse_relational(Parser* p) {
    ASTNode* node = parse_shift(p);
    
    while (match(p, TOKEN_LT) || match(p, TOKEN_GT) || 
           match(p, TOKEN_LE) || match(p, TOKEN_GE)) {
        ASTNode* binop = ast_node_new(AST_BINARY_OP);
        binop->line = p->previous.line;
        binop->column = p->previous.column;
        binop->data.op = p->previous.kind;
        ast_node_add_child(binop, node);
        ast_node_add_child(binop, parse_shift(p));
        node = binop;
    }
    
    return node;
}

// Parse equality expressions
static ASTNode* parse_equality(Parser* p) {
    ASTNode* node = parse_relational(p);
    
    while (match(p, TOKEN_EQ) || match(p, TOKEN_NE)) {
        ASTNode* binop = ast_node_new(AST_BINARY_OP);
        binop->line = p->previous.line;
        binop->column = p->previous.column;
        binop->data.op = p->previous.kind;
        ast_node_add_child(binop, node);
        ast_node_add_child(binop, parse_relational(p));
        node = binop;
    }
    
    return node;
}

// Parse bitwise AND
static ASTNode* parse_bitwise_and(Parser* p) {
    ASTNode* node = parse_equality(p);
    
    while (match(p, TOKEN_AMP)) {
        ASTNode* binop = ast_node_new(AST_BINARY_OP);
        binop->line = p->previous.line;
        binop->column = p->previous.column;
        binop->data.op = TOKEN_AMP;
        ast_node_add_child(binop, node);
        ast_node_add_child(binop, parse_equality(p));
        node = binop;
    }
    
    return node;
}

// Parse bitwise XOR
static ASTNode* parse_bitwise_xor(Parser* p) {
    ASTNode* node = parse_bitwise_and(p);
    
    while (match(p, TOKEN_CARET)) {
        ASTNode* binop = ast_node_new(AST_BINARY_OP);
        binop->line = p->previous.line;
        binop->column = p->previous.column;
        binop->data.op = TOKEN_CARET;
        ast_node_add_child(binop, node);
        ast_node_add_child(binop, parse_bitwise_and(p));
        node = binop;
    }
    
    return node;
}

// Parse bitwise OR
static ASTNode* parse_bitwise_or(Parser* p) {
    ASTNode* node = parse_bitwise_xor(p);
    
    while (match(p, TOKEN_PIPE)) {
        ASTNode* binop = ast_node_new(AST_BINARY_OP);
        binop->line = p->previous.line;
        binop->column = p->previous.column;
        binop->data.op = TOKEN_PIPE;
        ast_node_add_child(binop, node);
        ast_node_add_child(binop, parse_bitwise_xor(p));
        node = binop;
    }
    
    return node;
}

// Parse logical AND
static ASTNode* parse_logical_and(Parser* p) {
    ASTNode* node = parse_bitwise_or(p);
    
    while (match(p, TOKEN_AND)) {
        ASTNode* binop = ast_node_new(AST_BINARY_OP);
        binop->line = p->previous.line;
        binop->column = p->previous.column;
        binop->data.op = TOKEN_AND;
        ast_node_add_child(binop, node);
        ast_node_add_child(binop, parse_bitwise_or(p));
        node = binop;
    }
    
    return node;
}

// Parse logical OR
static ASTNode* parse_logical_or(Parser* p) {
    ASTNode* node = parse_logical_and(p);
    
    while (match(p, TOKEN_OR)) {
        ASTNode* binop = ast_node_new(AST_BINARY_OP);
        binop->line = p->previous.line;
        binop->column = p->previous.column;
        binop->data.op = TOKEN_OR;
        ast_node_add_child(binop, node);
        ast_node_add_child(binop, parse_logical_and(p));
        node = binop;
    }
    
    return node;
}

// Parse assignment expressions
static ASTNode* parse_assignment(Parser* p) {
    ASTNode* node = parse_logical_or(p);
    
    if (match(p, TOKEN_ASSIGN) || match(p, TOKEN_PLUS_EQ) || 
        match(p, TOKEN_MINUS_EQ) || match(p, TOKEN_STAR_EQ) || 
        match(p, TOKEN_SLASH_EQ)) {
        ASTNode* assign = ast_node_new(AST_ASSIGN);
        assign->line = p->previous.line;
        assign->column = p->previous.column;
        assign->data.op = p->previous.kind;
        ast_node_add_child(assign, node);
        ast_node_add_child(assign, parse_assignment(p));
        return assign;
    }
    
    return node;
}

// Top-level expression parser
static ASTNode* parse_expression(Parser* p) {
    return parse_assignment(p);
}

// Parse type
static ASTNode* parse_type(Parser* p) {
    ASTNode* node = ast_node_new(AST_TYPE);
    node->line = p->current.line;
    node->column = p->current.column;
    
    // Parse base type
    if (match(p, TOKEN_CHONK)) {
        node->data.name = xstrdup("chonk");
    }
    else if (match(p, TOKEN_SMOL)) {
        node->data.name = xstrdup("smol");
    }
    else if (match(p, TOKEN_MEGACHONK)) {
        node->data.name = xstrdup("megachonk");
    }
    else if (match(p, TOKEN_FLOOF)) {
        node->data.name = xstrdup("floof");
    }
    else if (match(p, TOKEN_BIGFLOOF)) {
        node->data.name = xstrdup("bigfloof");
    }
    else if (match(p, TOKEN_BOOP)) {
        node->data.name = xstrdup("boop");
    }
    else if (match(p, TOKEN_BYTE)) {
        node->data.name = xstrdup("byte");
    }
    else if (match(p, TOKEN_VOID)) {
        node->data.name = xstrdup("void");
    }
    else if (match(p, TOKEN_IDENT)) {
        node->data.name = xstrdup(p->previous.lexeme);
    }
    else {
        error_at(p->current.line, p->current.column, "Expected type");
    }
    
    // Parse pointer/array modifiers
    while (match(p, TOKEN_STAR)) {
        ASTNode* ptr = ast_node_new(AST_POINTER_TYPE);
        ast_node_add_child(ptr, node);
        node = ptr;
    }
    
    if (match(p, TOKEN_LBRACKET)) {
        ASTNode* arr = ast_node_new(AST_ARRAY_TYPE);
        ast_node_add_child(arr, node);
        if (!match(p, TOKEN_RBRACKET)) {
            ASTNode* size = parse_expression(p);
            ast_node_add_child(arr, size);
            consume(p, TOKEN_RBRACKET, "]");
        }
        node = arr;
    }
    
    return node;
}

static ASTNode* parse_var_decl(Parser* p) {
    if (!match(p, TOKEN_IDENT)) {
        error_at(p->current.line, p->current.column, "Expected identifier");
    }
    ASTNode* decl = ast_node_new(AST_VAR_DECL);
    decl->line = p->previous.line;
    decl->column = p->previous.column;
    decl->data.name = xstrdup(p->previous.lexeme);
    
    consume(p, TOKEN_COLON, ":");
    ASTNode* type_node = parse_type(p);
    ast_node_add_child(decl, type_node);
    
    if (match(p, TOKEN_ASSIGN)) {
        ast_node_add_child(decl, parse_expression(p));
    }
    
    return decl;
}

// Parse return statement
static ASTNode* parse_return(Parser* p) {
    ASTNode* node = ast_node_new(AST_RETURN);
    node->line = p->previous.line;
    node->column = p->previous.column;
    
    if (!check(p, TOKEN_SEMICOLON)) {
        ast_node_add_child(node, parse_expression(p));
    }
    
    return node;
}

// Parse if statement
static ASTNode* parse_if(Parser* p) {
    ASTNode* node = ast_node_new(AST_IF);
    node->line = p->previous.line;
    node->column = p->previous.column;
    
    consume(p, TOKEN_LPAREN, "(");
    ast_node_add_child(node, parse_expression(p));
    consume(p, TOKEN_RPAREN, ")");
    ast_node_add_child(node, parse_statement(p));
    
    if (match(p, TOKEN_NOWU)) {
        ast_node_add_child(node, parse_statement(p));
    }
    
    return node;
}

// Parse while statement
static ASTNode* parse_while(Parser* p) {
    ASTNode* node = ast_node_new(AST_WHILE);
    node->line = p->previous.line;
    node->column = p->previous.column;
    
    consume(p, TOKEN_LPAREN, "(");
    ast_node_add_child(node, parse_expression(p));
    consume(p, TOKEN_RPAREN, ")");
    ast_node_add_child(node, parse_statement(p));
    
    return node;
}

// Parse for statement
static ASTNode* parse_for(Parser* p) {
    ASTNode* node = ast_node_new(AST_FOR);
    node->line = p->previous.line;
    node->column = p->previous.column;
    
    consume(p, TOKEN_LPAREN, "(");
    
    if (!check(p, TOKEN_SEMICOLON)) {
        ast_node_add_child(node, parse_declaration(p));
    } else {
        advance(p);
    }
    
    if (!check(p, TOKEN_SEMICOLON)) {
        ast_node_add_child(node, parse_expression(p));
    }
    consume(p, TOKEN_SEMICOLON, ";");
    
    if (!check(p, TOKEN_RPAREN)) {
        ast_node_add_child(node, parse_expression(p));
    }
    consume(p, TOKEN_RPAREN, ")");
    
    ast_node_add_child(node, parse_statement(p));
    
    return node;
}

static ASTNode* parse_block(Parser* p) {
    ASTNode* node = ast_node_new(AST_BLOCK);
    node->line = p->previous.line;
    node->column = p->previous.column;
    
    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        if (check(p, TOKEN_IDENT)) {
            int saved_pos = p->lexer->pos;
            int saved_line = p->lexer->line;
            int saved_col = p->lexer->column;
            Token saved_prev = p->previous;
            Token saved_curr = p->current;
            
            advance(p);
            bool is_decl = check(p, TOKEN_COLON);
            
            p->lexer->pos = saved_pos;
            p->lexer->line = saved_line;
            p->lexer->column = saved_col;
            p->previous = saved_prev;
            p->current = saved_curr;
            
            if (is_decl) {
                ast_node_add_child(node, parse_var_decl(p));
                consume(p, TOKEN_SEMICOLON, ";");
            } else {
                ast_node_add_child(node, parse_statement(p));
            }
        } else {
            ast_node_add_child(node, parse_statement(p));
        }
    }
    
    consume(p, TOKEN_RBRACE, "}");
    return node;
}

// Parse statement
static ASTNode* parse_statement(Parser* p) {
    if (match(p, TOKEN_GIMME)) {
        return parse_return(p);
    }
    else if (match(p, TOKEN_PWEASE)) {
        return parse_if(p);
    }
    else if (match(p, TOKEN_WEPEAT)) {
        return parse_while(p);
    }
    else if (match(p, TOKEN_FOW)) {
        return parse_for(p);
    }
    else if (match(p, TOKEN_BWEAK)) {
        ASTNode* node = ast_node_new(AST_BREAK);
        node->line = p->previous.line;
        node->column = p->previous.column;
        consume(p, TOKEN_SEMICOLON, ";");
        return node;
    }
    else if (match(p, TOKEN_CONTINYUE)) {
        ASTNode* node = ast_node_new(AST_CONTINUE);
        node->line = p->previous.line;
        node->column = p->previous.column;
        consume(p, TOKEN_SEMICOLON, ";");
        return node;
    }
    else if (match(p, TOKEN_LBRACE)) {
        return parse_block(p);
    }
    else {
        // Expression statement
        ASTNode* expr = parse_expression(p);
        consume(p, TOKEN_SEMICOLON, ";");
        return expr;
    }
}

// Parse function parameter
static ASTNode* parse_parameter(Parser* p) {
    ASTNode* type_node = parse_type(p);
    consume(p, TOKEN_IDENT, "identifier");
    
    ASTNode* param = ast_node_new(AST_VAR_DECL);
    param->line = p->previous.line;
    param->column = p->previous.column;
    param->data.name = xstrdup(p->previous.lexeme);
    ast_node_add_child(param, type_node);
    
    return param;
}

// Parse function declaration
static ASTNode* parse_function(Parser* p) {
    ASTNode* node = ast_node_new(AST_FUNCTION);
    node->line = p->previous.line;
    node->column = p->previous.column;
    
    consume(p, TOKEN_IDENT, "identifier");
    node->data.name = xstrdup(p->previous.lexeme);
    
    consume(p, TOKEN_LPAREN, "(");
    
    ASTNode* params = ast_node_new(AST_BLOCK);
    if (!check(p, TOKEN_RPAREN)) {
        do {
            ast_node_add_child(params, parse_parameter(p));
        } while (match(p, TOKEN_COMMA));
    }
    consume(p, TOKEN_RPAREN, ")");
    
    if (match(p, TOKEN_ARROW)) {
        ASTNode* return_type = parse_type(p);
        ast_node_add_child(node, return_type);
    } else {
        ASTNode* void_type = ast_node_new(AST_TYPE);
        void_type->data.name = xstrdup("void");
        ast_node_add_child(node, void_type);
    }
    
    ast_node_add_child(node, params);
    consume(p, TOKEN_LBRACE, "{");
    ast_node_add_child(node, parse_block(p));
    
    return node;
}

// Parse declaration (function or variable)
static ASTNode* parse_declaration(Parser* p) {
    if (match(p, TOKEN_NUZZLE)) {
        return parse_function(p);
    }
    else {
        ASTNode* decl = parse_var_decl(p);
        consume(p, TOKEN_SEMICOLON, ";");
        return decl;
    }
}

// Main parse function
ASTNode* parse(Parser* parser) {
    ASTNode* root = ast_node_new(AST_PROGRAM);
    
    while (!check(parser, TOKEN_EOF)) {
        ASTNode* decl = parse_declaration(parser);
        ast_node_add_child(root, decl);
    }
    
    return root;
}

Parser* parser_new(Lexer* lexer) {
    Parser* p = xmalloc(sizeof(Parser));
    p->lexer = lexer;
    advance(p);
    return p;
}

void parser_free(Parser* parser) {
    free(parser);
}
