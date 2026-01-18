/**
 * @file parser_new.c
 * @brief Recursive descent parser for the UwU-C language
 * @author Bober
 * @version 1.0.0
 *
 * This file implements a complete recursive descent parser for the UwU-C language.
 * The parser consumes tokens from the lexer and produces an Abstract Syntax Tree
 * suitable for semantic analysis and IR generation.
 */

#include "parser.h"
#include "lexer.h"
#include "ast.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

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

static bool panic_mode = false;

static void synchronize(Parser* p);
static void advance(Parser* p);
static bool is_at_end(Parser* p);
static bool check(Parser* p, TokenKind kind);
static bool match(Parser* p, TokenKind kind);
static void consume(Parser* p, TokenKind kind, const char* message);
static bool is_type_token(TokenKind kind);

static void advance(Parser* p) {
    p->previous = p->current;

    for (;;) {
        p->current = lexer_next_token(p->lexer);
        if (p->current.kind != TOKEN_ERROR) break;

        error_at(p->current.line, p->current.column, "%s", p->current.lexeme);
        p->error_count++;
    }
}

static bool is_at_end(Parser* p) {
    return p->current.kind == TOKEN_EOF;
}

static bool check(Parser* p, TokenKind kind) {
    return p->current.kind == kind;
}

static bool match(Parser* p, TokenKind kind) {
    if (!check(p, kind)) return false;
    advance(p);
    return true;
}

static void consume(Parser* p, TokenKind kind, const char* message) {
    if (p->current.kind == kind) {
        advance(p);
        return;
    }

    error_at(p->current.line, p->current.column, "%s", message);
    p->error_count++;
    panic_mode = true;
}

static bool is_type_token(TokenKind kind) {
    return kind == TOKEN_CHONK || kind == TOKEN_SMOL ||
           kind == TOKEN_MEGACHONK || kind == TOKEN_FLOOF ||
           kind == TOKEN_BIGFLOOF || kind == TOKEN_BOOP ||
           kind == TOKEN_BYTE || kind == TOKEN_VOID;
}

static void synchronize(Parser* p) {
    panic_mode = false;

    while (!is_at_end(p)) {
        if (p->previous.kind == TOKEN_SEMICOLON) return;

        switch (p->current.kind) {
            case TOKEN_NUZZLE:
            case TOKEN_PWEASE:
            case TOKEN_WEPEAT:
            case TOKEN_FOW:
            case TOKEN_GIMME:
            case TOKEN_STWUCT:
            case TOKEN_ENUM:
                return;
            default:
                ;
        }

        advance(p);
    }
}

static Token peek_ahead(Parser* p) {
    int saved_pos = p->lexer->pos;
    int saved_line = p->lexer->line;
    int saved_col = p->lexer->column;

    Token next = lexer_next_token(p->lexer);

    p->lexer->pos = saved_pos;
    p->lexer->line = saved_line;
    p->lexer->column = saved_col;

    return next;
}

static ASTNode* parse_program(Parser* p) {
    ASTNode* program = ast_node_new(AST_PROGRAM);

    while (!is_at_end(p)) {
        ASTNode* decl = parse_declaration(p);
        if (decl != NULL) {
            ast_node_add_child(program, decl);
        }

        if (panic_mode) {
            synchronize(p);
        }
    }

    return program;
}

static ASTNode* parse_declaration(Parser* p) {
    if (match(p, TOKEN_NUZZLE)) {
        return parse_function(p);
    }

    if (check(p, TOKEN_IDENT)) {
        Token next = peek_ahead(p);
        if (next.kind == TOKEN_COLON) {
            ASTNode* var = parse_var_decl(p);
            consume(p, TOKEN_SEMICOLON, "Expected ';' after variable declaration");
            return var;
        }
    }

    error_at(p->current.line, p->current.column, "Expected declaration");
    p->error_count++;
    return NULL;
}

static ASTNode* parse_function(Parser* p) {
    ASTNode* fn = ast_node_new(AST_FUNCTION);

    consume(p, TOKEN_IDENT, "Expected function name");
    fn->data.name = xstrdup(p->previous.lexeme);

    consume(p, TOKEN_LPAREN, "Expected '(' after function name");

    ASTNode* params = ast_node_new(AST_BLOCK);

    if (!check(p, TOKEN_RPAREN)) {
        do {
            if (params->child_count >= 255) {
                error_at(p->current.line, p->current.column,
                        "Cannot have more than 255 parameters");
                p->error_count++;
                break;
            }

            ASTNode* param = parse_parameter(p);
            if (param != NULL) {
                ast_node_add_child(params, param);
            }
        } while (match(p, TOKEN_COMMA));
    }

    consume(p, TOKEN_RPAREN, "Expected ')' after parameters");

    ASTNode* ret_type;
    if (match(p, TOKEN_ARROW)) {
        ret_type = parse_type(p);
        if (ret_type == NULL) {
            ret_type = ast_node_new(AST_TYPE);
            ret_type->data.name = xstrdup("void");
        }
    } else {
        ret_type = ast_node_new(AST_TYPE);
        ret_type->data.name = xstrdup("void");
    }

    ast_node_add_child(fn, ret_type);
    ast_node_add_child(fn, params);

    consume(p, TOKEN_LBRACE, "Expected '{' before function body");
    ASTNode* body = parse_block(p);
    ast_node_add_child(fn, body);

    return fn;
}

static ASTNode* parse_block(Parser* p) {
    ASTNode* block = ast_node_new(AST_BLOCK);

    while (!check(p, TOKEN_RBRACE) && !is_at_end(p)) {
        ASTNode* stmt = parse_statement(p);
        if (stmt != NULL) {
            ast_node_add_child(block, stmt);
        }

        if (panic_mode) {
            synchronize(p);
        }
    }

    consume(p, TOKEN_RBRACE, "Expected '}' after block");
    return block;
}

static ASTNode* parse_statement(Parser* p) {
    if (match(p, TOKEN_PWEASE)) {
        ASTNode* node = ast_node_new(AST_IF);

        consume(p, TOKEN_LPAREN, "Expected '(' after 'pwease'");
        ASTNode* condition = parse_expression(p);
        consume(p, TOKEN_RPAREN, "Expected ')' after condition");

        if (condition != NULL) {
            ast_node_add_child(node, condition);
        }

        ASTNode* then_stmt = parse_statement(p);
        if (then_stmt != NULL) {
            ast_node_add_child(node, then_stmt);
        }

        if (match(p, TOKEN_NOWU)) {
            ASTNode* else_stmt = parse_statement(p);
            if (else_stmt != NULL) {
                ast_node_add_child(node, else_stmt);
            }
        }

        return node;
    }

    if (match(p, TOKEN_WEPEAT)) {
        ASTNode* node = ast_node_new(AST_WHILE);

        consume(p, TOKEN_LPAREN, "Expected '(' after 'wepeat'");
        ASTNode* condition = parse_expression(p);
        consume(p, TOKEN_RPAREN, "Expected ')' after condition");

        if (condition != NULL) {
            ast_node_add_child(node, condition);
        }

        ASTNode* body = parse_statement(p);
        if (body != NULL) {
            ast_node_add_child(node, body);
        }

        return node;
    }

    if (match(p, TOKEN_FOW)) {
        ASTNode* node = ast_node_new(AST_FOR);

        consume(p, TOKEN_LPAREN, "Expected '(' after 'fow'");

        ASTNode* init = NULL;
        if (!check(p, TOKEN_SEMICOLON)) {
            if (check(p, TOKEN_IDENT)) {
                Token next = peek_ahead(p);
                if (next.kind == TOKEN_COLON) {
                    init = parse_var_decl(p);
                } else {
                    init = parse_expression(p);
                }
            } else {
                init = parse_expression(p);
            }
        }
        consume(p, TOKEN_SEMICOLON, "Expected ';' after for loop initializer");

        ASTNode* condition = NULL;
        if (!check(p, TOKEN_SEMICOLON)) {
            condition = parse_expression(p);
        }
        consume(p, TOKEN_SEMICOLON, "Expected ';' after for loop condition");

        ASTNode* increment = NULL;
        if (!check(p, TOKEN_RPAREN)) {
            increment = parse_expression(p);
        }
        consume(p, TOKEN_RPAREN, "Expected ')' after for clauses");

        if (init != NULL) ast_node_add_child(node, init);
        if (condition != NULL) ast_node_add_child(node, condition);
        if (increment != NULL) ast_node_add_child(node, increment);

        ASTNode* body = parse_statement(p);
        if (body != NULL) {
            ast_node_add_child(node, body);
        }

        return node;
    }

    if (match(p, TOKEN_GIMME)) {
        ASTNode* node = ast_node_new(AST_RETURN);

        if (!check(p, TOKEN_SEMICOLON)) {
            ASTNode* value = parse_expression(p);
            if (value != NULL) {
                ast_node_add_child(node, value);
            }
        }

        consume(p, TOKEN_SEMICOLON, "Expected ';' after return statement");
        return node;
    }

    if (match(p, TOKEN_BWEAK)) {
        ASTNode* node = ast_node_new(AST_BREAK);
        consume(p, TOKEN_SEMICOLON, "Expected ';' after 'bweak'");
        return node;
    }

    if (match(p, TOKEN_CONTINYUE)) {
        ASTNode* node = ast_node_new(AST_CONTINUE);
        consume(p, TOKEN_SEMICOLON, "Expected ';' after 'continyue'");
        return node;
    }

    if (match(p, TOKEN_LBRACE)) {
        return parse_block(p);
    }

    if (check(p, TOKEN_IDENT)) {
        Token next = peek_ahead(p);

        if (next.kind == TOKEN_COLON) {
            ASTNode* var = parse_var_decl(p);
            consume(p, TOKEN_SEMICOLON, "Expected ';' after variable declaration");
            return var;
        }
    }

    ASTNode* expr = parse_expression(p);
    if (expr != NULL) {
        consume(p, TOKEN_SEMICOLON, "Expected ';' after expression");
    }
    return expr;
}

static ASTNode* parse_expression(Parser* p) {
    return parse_assignment(p);
}

static ASTNode* parse_assignment(Parser* p) {
    ASTNode* expr = parse_logical_or(p);

    if (match(p, TOKEN_ASSIGN) || match(p, TOKEN_PLUS_EQ) ||
        match(p, TOKEN_MINUS_EQ) || match(p, TOKEN_STAR_EQ) ||
        match(p, TOKEN_SLASH_EQ)) {

        TokenKind op = p->previous.kind;
        ASTNode* node = ast_node_new(AST_ASSIGN);
        node->data.op = op;
        ast_node_add_child(node, expr);

        ASTNode* value = parse_assignment(p);
        if (value != NULL) {
            ast_node_add_child(node, value);
        } else {
            error_at(p->current.line, p->current.column,
                    "Expected expression after assignment operator");
            p->error_count++;
        }

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

        ASTNode* right = parse_logical_and(p);
        if (right != NULL) {
            ast_node_add_child(node, right);
        }

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

        ASTNode* right = parse_bitwise_or(p);
        if (right != NULL) {
            ast_node_add_child(node, right);
        }

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

        ASTNode* right = parse_bitwise_xor(p);
        if (right != NULL) {
            ast_node_add_child(node, right);
        }

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

        ASTNode* right = parse_bitwise_and(p);
        if (right != NULL) {
            ast_node_add_child(node, right);
        }

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

        ASTNode* right = parse_equality(p);
        if (right != NULL) {
            ast_node_add_child(node, right);
        }

        expr = node;
    }

    return expr;
}

static ASTNode* parse_equality(Parser* p) {
    ASTNode* expr = parse_relational(p);

    while (match(p, TOKEN_EQ) || match(p, TOKEN_NE)) {
        TokenKind op = p->previous.kind;
        ASTNode* node = ast_node_new(AST_BINARY_OP);
        node->data.op = op;
        ast_node_add_child(node, expr);

        ASTNode* right = parse_relational(p);
        if (right != NULL) {
            ast_node_add_child(node, right);
        }

        expr = node;
    }

    return expr;
}

static ASTNode* parse_relational(Parser* p) {
    ASTNode* expr = parse_shift(p);

    while (match(p, TOKEN_LT) || match(p, TOKEN_GT) ||
           match(p, TOKEN_LE) || match(p, TOKEN_GE)) {
        TokenKind op = p->previous.kind;
        ASTNode* node = ast_node_new(AST_BINARY_OP);
        node->data.op = op;
        ast_node_add_child(node, expr);

        ASTNode* right = parse_shift(p);
        if (right != NULL) {
            ast_node_add_child(node, right);
        }

        expr = node;
    }

    return expr;
}

static ASTNode* parse_shift(Parser* p) {
    ASTNode* expr = parse_additive(p);

    while (match(p, TOKEN_LSHIFT) || match(p, TOKEN_RSHIFT)) {
        TokenKind op = p->previous.kind;
        ASTNode* node = ast_node_new(AST_BINARY_OP);
        node->data.op = op;
        ast_node_add_child(node, expr);

        ASTNode* right = parse_additive(p);
        if (right != NULL) {
            ast_node_add_child(node, right);
        }

        expr = node;
    }

    return expr;
}

static ASTNode* parse_additive(Parser* p) {
    ASTNode* expr = parse_multiplicative(p);

    while (match(p, TOKEN_PLUS) || match(p, TOKEN_MINUS)) {
        TokenKind op = p->previous.kind;
        ASTNode* node = ast_node_new(AST_BINARY_OP);
        node->data.op = op;
        ast_node_add_child(node, expr);

        ASTNode* right = parse_multiplicative(p);
        if (right != NULL) {
            ast_node_add_child(node, right);
        }

        expr = node;
    }

    return expr;
}

static ASTNode* parse_multiplicative(Parser* p) {
    ASTNode* expr = parse_unary(p);

    while (match(p, TOKEN_STAR) || match(p, TOKEN_SLASH) || match(p, TOKEN_PERCENT)) {
        TokenKind op = p->previous.kind;
        ASTNode* node = ast_node_new(AST_BINARY_OP);
        node->data.op = op;
        ast_node_add_child(node, expr);

        ASTNode* right = parse_unary(p);
        if (right != NULL) {
            ast_node_add_child(node, right);
        }

        expr = node;
    }

    return expr;
}

static ASTNode* parse_unary(Parser* p) {
    if (match(p, TOKEN_NOT) || match(p, TOKEN_MINUS) ||
        match(p, TOKEN_TILDE) || match(p, TOKEN_AMP) || match(p, TOKEN_STAR)) {

        TokenKind op = p->previous.kind;
        ASTNode* node = ast_node_new(AST_UNARY_OP);
        node->data.op = op;

        ASTNode* operand = parse_unary(p);
        if (operand != NULL) {
            ast_node_add_child(node, operand);
        }

        return node;
    }

    if (match(p, TOKEN_SIZEOF)) {
        ASTNode* node = ast_node_new(AST_SIZEOF);

        consume(p, TOKEN_LPAREN, "Expected '(' after 'sizeof'");

        if (is_type_token(p->current.kind)) {
            ASTNode* type = parse_type(p);
            if (type != NULL) {
                ast_node_add_child(node, type);
            }
        } else {
            ASTNode* expr = parse_expression(p);
            if (expr != NULL) {
                ast_node_add_child(node, expr);
            }
        }

        consume(p, TOKEN_RPAREN, "Expected ')' after sizeof operand");
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
                    if (call->child_count >= 256) {
                        error_at(p->current.line, p->current.column,
                                "Cannot have more than 255 arguments");
                        p->error_count++;
                        break;
                    }

                    ASTNode* arg = parse_expression(p);
                    if (arg != NULL) {
                        ast_node_add_child(call, arg);
                    }
                } while (match(p, TOKEN_COMMA));
            }

            consume(p, TOKEN_RPAREN, "Expected ')' after arguments");
            expr = call;
        } else if (match(p, TOKEN_LBRACKET)) {
            ASTNode* index = ast_node_new(AST_INDEX);
            ast_node_add_child(index, expr);

            ASTNode* idx_expr = parse_expression(p);
            if (idx_expr != NULL) {
                ast_node_add_child(index, idx_expr);
            }

            consume(p, TOKEN_RBRACKET, "Expected ']' after array index");
            expr = index;
        } else if (match(p, TOKEN_DOT)) {
            ASTNode* member = ast_node_new(AST_MEMBER);
            ast_node_add_child(member, expr);

            consume(p, TOKEN_IDENT, "Expected member name after '.'");
            ASTNode* name = ast_node_new(AST_IDENTIFIER);
            name->data.name = xstrdup(p->previous.lexeme);
            ast_node_add_child(member, name);

            expr = member;
        } else {
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

    if (match(p, TOKEN_STRING)) {
        ASTNode* node = ast_node_new(AST_STRING);
        node->data.string_value = xstrdup(p->previous.lexeme);
        return node;
    }

    if (match(p, TOKEN_TRUE)) {
        ASTNode* node = ast_node_new(AST_NUMBER);
        node->data.int_value = 1;
        return node;
    }

    if (match(p, TOKEN_FALSE)) {
        ASTNode* node = ast_node_new(AST_NUMBER);
        node->data.int_value = 0;
        return node;
    }

    if (match(p, TOKEN_NUWW)) {
        ASTNode* node = ast_node_new(AST_NULL);
        return node;
    }

    if (match(p, TOKEN_IDENT)) {
        ASTNode* node = ast_node_new(AST_IDENTIFIER);
        node->data.name = xstrdup(p->previous.lexeme);
        return node;
    }

    if (match(p, TOKEN_LPAREN)) {
        ASTNode* expr = parse_expression(p);
        consume(p, TOKEN_RPAREN, "Expected ')' after expression");
        return expr;
    }

    error_at(p->current.line, p->current.column, "Expected expression");
    p->error_count++;
    return NULL;
}

static ASTNode* parse_type(Parser* p) {
    const char* type_name = NULL;

    if (match(p, TOKEN_CHONK)) type_name = "chonk";
    else if (match(p, TOKEN_SMOL)) type_name = "smol";
    else if (match(p, TOKEN_MEGACHONK)) type_name = "megachonk";
    else if (match(p, TOKEN_FLOOF)) type_name = "floof";
    else if (match(p, TOKEN_BIGFLOOF)) type_name = "bigfloof";
    else if (match(p, TOKEN_BOOP)) type_name = "boop";
    else if (match(p, TOKEN_BYTE)) type_name = "byte";
    else if (match(p, TOKEN_VOID)) type_name = "void";
    else {
        error_at(p->current.line, p->current.column, "Expected type name");
        p->error_count++;
        return NULL;
    }

    ASTNode* node = ast_node_new(AST_TYPE);
    node->data.name = xstrdup(type_name);

    while (match(p, TOKEN_STAR)) {
        ASTNode* ptr = ast_node_new(AST_POINTER_TYPE);
        ast_node_add_child(ptr, node);
        node = ptr;
    }

    return node;
}

static ASTNode* parse_parameter(Parser* p) {
    ASTNode* type = parse_type(p);
    if (type == NULL) return NULL;

    consume(p, TOKEN_IDENT, "Expected parameter name");

    ASTNode* param = ast_node_new(AST_VAR_DECL);
    param->data.name = xstrdup(p->previous.lexeme);
    ast_node_add_child(param, type);

    return param;
}

static ASTNode* parse_var_decl(Parser* p) {
    consume(p, TOKEN_IDENT, "Expected variable name");

    ASTNode* decl = ast_node_new(AST_VAR_DECL);
    decl->data.name = xstrdup(p->previous.lexeme);

    consume(p, TOKEN_COLON, "Expected ':' after variable name");

    ASTNode* type = parse_type(p);
    if (type == NULL) {
        ast_node_free(decl);
        return NULL;
    }

    ast_node_add_child(decl, type);

    if (match(p, TOKEN_ASSIGN)) {
        ASTNode* init = parse_expression(p);
        if (init != NULL) {
            ast_node_add_child(decl, init);
        }
    }

    return decl;
}

Parser* parser_new(Lexer* lexer) {
    Parser* parser = xcalloc(1, sizeof(Parser));
    parser->lexer = lexer;
    parser->error_count = 0;
    parser->current = lexer_next_token(lexer);
    return parser;
}

void parser_free(Parser* parser) {
    free(parser);
}

ASTNode* parse(Parser* parser) {
    panic_mode = false;
    return parse_program(parser);
}