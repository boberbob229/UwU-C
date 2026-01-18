/**
 * @file parser_new.c
 * @brief Recursive descent parser for uwuc language
 * @author Bober
 * @version 1.0.0
 *
 * This file implements a complete recursive descent parser for the uwuc language.
 * The parser consumes tokens from the lexer and builds an Abstract Syntax Tree
 * for semantic analysis and IR generation.
 */

#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>

static ASTNode* parse_program(Parser* p);
static ASTNode* parse_declaration(Parser* p);
static ASTNode* parse_function(Parser* p);
static ASTNode* parse_block(Parser* p);
static ASTNode* parse_statement(Parser* p);
static ASTNode* parse_expression(Parser* p);
static ASTNode* parse_assignment(Parser* p);
static ASTNode* parse_logical_or(Parser* p);
static ASTNode* parse_logical_and(Parser* p);
static ASTNode* parse_bitwise_or(Parser* p);
static ASTNode* parse_bitwise_xor(Parser* p);
static ASTNode* parse_bitwise_and(Parser* p);
static ASTNode* parse_equality(Parser* p);
static ASTNode* parse_relational(Parser* p);
static ASTNode* parse_shift(Parser* p);
static ASTNode* parse_additive(Parser* p);
static ASTNode* parse_multiplicative(Parser* p);
static ASTNode* parse_unary(Parser* p);
static ASTNode* parse_postfix(Parser* p);
static ASTNode* parse_primary(Parser* p);
static ASTNode* parse_type(Parser* p);
static ASTNode* parse_parameter(Parser* p);
static ASTNode* parse_var_decl(Parser* p);

static bool check(Parser* p, TokenKind kind) {
    return p->current.kind == kind;
}

static bool match(Parser* p, TokenKind kind) {
    if (check(p, kind)) {
        p->previous = p->current;
        p->current = lexer_next_token(p->lexer);
        return true;
    }
    return false;
}

static void consume(Parser* p, TokenKind kind, const char* msg) {
    if (!match(p, kind)) {
        error_at(p->current.line, p->current.column,
                 "Expected %s, got %s",
                 msg, token_kind_to_string(p->current.kind));
    }
}

static ASTNode* parse_program(Parser* p) {
    ASTNode* program = ast_node_new(AST_PROGRAM);
    while (!check(p, TOKEN_EOF)) {
        ASTNode* decl = parse_declaration(p);
        if (decl) ast_node_add_child(program, decl);
    }
    return program;
}

static ASTNode* parse_declaration(Parser* p) {
    if (match(p, TOKEN_NUZZLE)) {
        return parse_function(p);
    }
    return parse_var_decl(p);
}

static ASTNode* parse_function(Parser* p) {
    ASTNode* fn = ast_node_new(AST_FUNCTION);

    consume(p, TOKEN_IDENT, "function name");
    fn->data.name = xstrdup(p->previous.lexeme);

    consume(p, TOKEN_LPAREN, "(");
    ASTNode* params = ast_node_new(AST_BLOCK);
    if (!check(p, TOKEN_RPAREN)) {
        do {
            ast_node_add_child(params, parse_parameter(p));
        } while (match(p, TOKEN_COMMA));
    }
    consume(p, TOKEN_RPAREN, ")");

    ASTNode* ret_type;
    if (match(p, TOKEN_ARROW)) {
        ret_type = parse_type(p);
    } else {
        ret_type = ast_node_new(AST_TYPE);
        ret_type->data.name = xstrdup("void");
    }

    ast_node_add_child(fn, ret_type);
    ast_node_add_child(fn, params);

    consume(p, TOKEN_LBRACE, "{");
    ast_node_add_child(fn, parse_block(p));

    return fn;
}

static ASTNode* parse_block(Parser* p) {
    ASTNode* block = ast_node_new(AST_BLOCK);
    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        if (check(p, TOKEN_IDENT)) {
            int saved_pos = p->lexer->pos;
            int saved_line = p->lexer->line;
            int saved_col = p->lexer->column;
            Token saved_prev = p->previous;
            Token saved_curr = p->current;
            
            match(p, TOKEN_IDENT);
            bool is_decl = check(p, TOKEN_COLON);
            
            p->lexer->pos = saved_pos;
            p->lexer->line = saved_line;
            p->lexer->column = saved_col;
            p->previous = saved_prev;
            p->current = saved_curr;
            
            if (is_decl) {
                ast_node_add_child(block, parse_var_decl(p));
                consume(p, TOKEN_SEMICOLON, ";");
            } else {
                ast_node_add_child(block, parse_statement(p));
            }
        } else {
            ast_node_add_child(block, parse_statement(p));
        }
    }
    consume(p, TOKEN_RBRACE, "}");
    return block;
}

static ASTNode* parse_unsafe_block(Parser* p) {
    ASTNode* unsafe = ast_node_new(AST_UNSAFE_BLOCK);
    unsafe->line = p->current.line;
    unsafe->column = p->current.column;
    consume(p, TOKEN_LBRACE, "{");
    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        if (check(p, TOKEN_IDENT)) {
            int saved_pos = p->lexer->pos;
            int saved_line = p->lexer->line;
            int saved_col = p->lexer->column;
            Token saved_prev = p->previous;
            Token saved_curr = p->current;
            
            match(p, TOKEN_IDENT);
            bool is_decl = check(p, TOKEN_COLON);
            
            p->lexer->pos = saved_pos;
            p->lexer->line = saved_line;
            p->lexer->column = saved_col;
            p->previous = saved_prev;
            p->current = saved_curr;
            
            if (is_decl) {
                ast_node_add_child(unsafe, parse_var_decl(p));
                consume(p, TOKEN_SEMICOLON, ";");
            } else {
                ast_node_add_child(unsafe, parse_statement(p));
            }
        } else {
            ast_node_add_child(unsafe, parse_statement(p));
        }
    }
    consume(p, TOKEN_RBRACE, "}");
    return unsafe;
}

static ASTNode* parse_statement(Parser* p) {
    if (match(p, TOKEN_PWEASE)) {
        ASTNode* node = ast_node_new(AST_IF);
        consume(p, TOKEN_LPAREN, "(");
        ast_node_add_child(node, parse_expression(p));
        consume(p, TOKEN_RPAREN, ")");
        ast_node_add_child(node, parse_statement(p));
        if (match(p, TOKEN_NOWU)) {
            ast_node_add_child(node, parse_statement(p));
        }
        return node;
    }
    else if (match(p, TOKEN_WEPEAT)) {
        ASTNode* node = ast_node_new(AST_WHILE);
        consume(p, TOKEN_LPAREN, "(");
        ast_node_add_child(node, parse_expression(p));
        consume(p, TOKEN_RPAREN, ")");
        ast_node_add_child(node, parse_statement(p));
        return node;
    }
    else if (match(p, TOKEN_FOW)) {
        ASTNode* node = ast_node_new(AST_FOR);
        consume(p, TOKEN_LPAREN, "(");

        if (!check(p, TOKEN_SEMICOLON))
            ast_node_add_child(node, parse_expression(p));
        else
            ast_node_add_child(node, NULL);
        consume(p, TOKEN_SEMICOLON, ";");

        if (!check(p, TOKEN_SEMICOLON))
            ast_node_add_child(node, parse_expression(p));
        else
            ast_node_add_child(node, NULL);
        consume(p, TOKEN_SEMICOLON, ";");

        if (!check(p, TOKEN_RPAREN))
            ast_node_add_child(node, parse_expression(p));
        else
            ast_node_add_child(node, NULL);
        consume(p, TOKEN_RPAREN, ")");

        ast_node_add_child(node, parse_statement(p));
        return node;
    }
    else if (match(p, TOKEN_BWEAK)) {
        return ast_node_new(AST_BREAK);
    }
    else if (match(p, TOKEN_CONTINYUE)) {
        return ast_node_new(AST_CONTINUE);
    }
    else if (match(p, TOKEN_GIMME)) {
        ASTNode* node = ast_node_new(AST_RETURN);
        if (!check(p, TOKEN_SEMICOLON)) {
            ast_node_add_child(node, parse_expression(p));
        }
        consume(p, TOKEN_SEMICOLON, ";");
        return node;
    }
    else if (match(p, TOKEN_UNSAFE)) {
        return parse_unsafe_block(p);
    }
    else if (check(p, TOKEN_LBRACE)) {
        match(p, TOKEN_LBRACE);
        return parse_block(p);
    }
    else if (match(p, TOKEN_IDENT) &&
             strcmp(p->previous.lexeme, "uwu_print") == 0) {

        ASTNode* node = ast_node_new(AST_PRINT_STR);
        consume(p, TOKEN_LPAREN, "(");
        consume(p, TOKEN_STRING, "string literal");
        node->data.print_str.value = xstrdup(p->previous.lexeme);
        consume(p, TOKEN_RPAREN, ")");
        consume(p, TOKEN_SEMICOLON, ";");
        return node;
    }
    else {
        ASTNode* expr = parse_expression(p);
        consume(p, TOKEN_SEMICOLON, ";");
        return expr;
    }
}

static ASTNode* parse_expression(Parser* p) {
    return parse_assignment(p);
}

static ASTNode* parse_assignment(Parser* p) {
    ASTNode* expr = parse_logical_or(p);
    if (match(p, TOKEN_ASSIGN)) {
        ASTNode* node = ast_node_new(AST_ASSIGN);
        ast_node_add_child(node, expr);
        ast_node_add_child(node, parse_assignment(p));
        return node;
    }
    return expr;
}

static ASTNode* parse_logical_or(Parser* p) {
    ASTNode* expr = parse_logical_and(p);
    while (match(p, TOKEN_OR)) {
        ASTNode* node = ast_node_new(AST_BINARY_OP);
        node->data.op = TOKEN_OR;
        ast_node_add_child(node, expr);
        ast_node_add_child(node, parse_logical_and(p));
        expr = node;
    }
    return expr;
}

static ASTNode* parse_logical_and(Parser* p) {
    ASTNode* expr = parse_bitwise_or(p);
    while (match(p, TOKEN_AND)) {
        ASTNode* node = ast_node_new(AST_BINARY_OP);
        node->data.op = TOKEN_AND;
        ast_node_add_child(node, expr);
        ast_node_add_child(node, parse_bitwise_or(p));
        expr = node;
    }
    return expr;
}

static ASTNode* parse_bitwise_or(Parser* p) {
    ASTNode* expr = parse_bitwise_xor(p);
    while (match(p, TOKEN_PIPE)) {
        ASTNode* node = ast_node_new(AST_BINARY_OP);
        node->data.op = TOKEN_PIPE;
        ast_node_add_child(node, expr);
        ast_node_add_child(node, parse_bitwise_xor(p));
        expr = node;
    }
    return expr;
}

static ASTNode* parse_bitwise_xor(Parser* p) {
    ASTNode* expr = parse_bitwise_and(p);
    while (match(p, TOKEN_CARET)) {
        ASTNode* node = ast_node_new(AST_BINARY_OP);
        node->data.op = TOKEN_CARET;
        ast_node_add_child(node, expr);
        ast_node_add_child(node, parse_bitwise_and(p));
        expr = node;
    }
    return expr;
}

static ASTNode* parse_bitwise_and(Parser* p) {
    ASTNode* expr = parse_equality(p);
    while (match(p, TOKEN_AMP)) {
        ASTNode* node = ast_node_new(AST_BINARY_OP);
        node->data.op = TOKEN_AMP;
        ast_node_add_child(node, expr);
        ast_node_add_child(node, parse_equality(p));
        expr = node;
    }
    return expr;
}

static ASTNode* parse_equality(Parser* p) {
    ASTNode* expr = parse_relational(p);
    while (match(p, TOKEN_EQ) || match(p, TOKEN_NE)) {
        ASTNode* node = ast_node_new(AST_BINARY_OP);
        node->data.op = p->previous.kind;
        ast_node_add_child(node, expr);
        ast_node_add_child(node, parse_relational(p));
        expr = node;
    }
    return expr;
}

static ASTNode* parse_relational(Parser* p) {
    ASTNode* expr = parse_shift(p);
    while (match(p, TOKEN_LT) || match(p, TOKEN_GT) ||
           match(p, TOKEN_LE) || match(p, TOKEN_GE)) {
        ASTNode* node = ast_node_new(AST_BINARY_OP);
        node->data.op = p->previous.kind;
        ast_node_add_child(node, expr);
        ast_node_add_child(node, parse_shift(p));
        expr = node;
    }
    return expr;
}

static ASTNode* parse_shift(Parser* p) {
    ASTNode* expr = parse_additive(p);
    while (match(p, TOKEN_LSHIFT) || match(p, TOKEN_RSHIFT)) {
        ASTNode* node = ast_node_new(AST_BINARY_OP);
        node->data.op = p->previous.kind;
        ast_node_add_child(node, expr);
        ast_node_add_child(node, parse_additive(p));
        expr = node;
    }
    return expr;
}

static ASTNode* parse_additive(Parser* p) {
    ASTNode* expr = parse_multiplicative(p);
    while (match(p, TOKEN_PLUS) || match(p, TOKEN_MINUS)) {
        ASTNode* node = ast_node_new(AST_BINARY_OP);
        node->data.op = p->previous.kind;
        ast_node_add_child(node, expr);
        ast_node_add_child(node, parse_multiplicative(p));
        expr = node;
    }
    return expr;
}

static ASTNode* parse_multiplicative(Parser* p) {
    ASTNode* expr = parse_unary(p);
    while (match(p, TOKEN_STAR) || match(p, TOKEN_SLASH) || match(p, TOKEN_PERCENT)) {
        ASTNode* node = ast_node_new(AST_BINARY_OP);
        node->data.op = p->previous.kind;
        ast_node_add_child(node, expr);
        ast_node_add_child(node, parse_unary(p));
        expr = node;
    }
    return expr;
}

static ASTNode* parse_unary(Parser* p) {
    if (match(p, TOKEN_NOT) || match(p, TOKEN_MINUS) || match(p, TOKEN_TILDE)) {
        ASTNode* node = ast_node_new(AST_UNARY_OP);
        node->data.op = p->previous.kind;
        ast_node_add_child(node, parse_unary(p));
        return node;
    }
    return parse_postfix(p);
}

static ASTNode* parse_postfix(Parser* p) {
    ASTNode* expr = parse_primary(p);
    while (true) {
        if (match(p, TOKEN_LPAREN)) {
            ASTNode* call = ast_node_new(AST_CALL);
            ast_node_add_child(call, expr);
            if (!check(p, TOKEN_RPAREN)) {
                do {
                    ast_node_add_child(call, parse_expression(p));
                } while (match(p, TOKEN_COMMA));
            }
            consume(p, TOKEN_RPAREN, ")");
            expr = call;
        }
        else {
            break;
        }
    }
    return expr;
}

static ASTNode* parse_primary(Parser* p) {
    if (match(p, TOKEN_NUMBER)) {
        ASTNode* node = ast_node_new(AST_NUMBER);
        node->data.int_value = p->previous.value.int_value;
        return node;
    }
    else if (match(p, TOKEN_STRING)) {
        ASTNode* node = ast_node_new(AST_STRING);
        node->data.string_value = xstrdup(p->previous.lexeme);
        return node;
    }
    else if (match(p, TOKEN_IDENT)) {
        ASTNode* node = ast_node_new(AST_IDENTIFIER);
        node->data.name = xstrdup(p->previous.lexeme);
        return node;
    }
    else if (match(p, TOKEN_LPAREN)) {
        ASTNode* expr = parse_expression(p);
        consume(p, TOKEN_RPAREN, ")");
        return expr;
    }
    error_at(p->current.line, p->current.column,
             "Unexpected token");
    return NULL;
}

static ASTNode* parse_type(Parser* p) {
    ASTNode* node = ast_node_new(AST_TYPE);

    if (match(p, TOKEN_CHONK)) node->data.name = xstrdup("chonk");
    else if (match(p, TOKEN_SMOL)) node->data.name = xstrdup("smol");
    else if (match(p, TOKEN_MEGACHONK)) node->data.name = xstrdup("megachonk");
    else if (match(p, TOKEN_FLOOF)) node->data.name = xstrdup("floof");
    else if (match(p, TOKEN_BIGFLOOF)) node->data.name = xstrdup("bigfloof");
    else if (match(p, TOKEN_BOOP)) node->data.name = xstrdup("boop");
    else if (match(p, TOKEN_BYTE)) node->data.name = xstrdup("byte");
    else if (match(p, TOKEN_VOID)) node->data.name = xstrdup("void");
    else if (match(p, TOKEN_IDENT)) node->data.name = xstrdup(p->previous.lexeme);
    else {
        error_at(p->current.line, p->current.column, "Expected type");
        return NULL;
    }

    return node;
}

static ASTNode* parse_parameter(Parser* p) {
    ASTNode* type = parse_type(p);
    consume(p, TOKEN_IDENT, "parameter name");
    ASTNode* param = ast_node_new(AST_VAR_DECL);
    param->data.name = xstrdup(p->previous.lexeme);
    ast_node_add_child(param, type);
    return param;
}

static ASTNode* parse_var_decl(Parser* p) {
    consume(p, TOKEN_IDENT, "variable name");
    ASTNode* decl = ast_node_new(AST_VAR_DECL);
    decl->data.name = xstrdup(p->previous.lexeme);
    consume(p, TOKEN_COLON, ":");
    ASTNode* type = parse_type(p);
    ast_node_add_child(decl, type);
    
    if (match(p, TOKEN_ASSIGN)) {
        ast_node_add_child(decl, parse_expression(p));
    }
    
    return decl;
}

Parser* parser_new(Lexer* lexer) {
    Parser* parser = xcalloc(1, sizeof(Parser));
    parser->lexer = lexer;
    parser->current = lexer_next_token(lexer);
    return parser;
}

void parser_free(Parser* parser) {
    free(parser);
}

ASTNode* parse(Parser* parser) {
    return parse_program(parser);
}
