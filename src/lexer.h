#ifndef LEXER_H
#define LEXER_H

#include <stdbool.h>

typedef enum {
    // Keywords
    TOKEN_NUZZLE, TOKEN_GIMME, TOKEN_PWEASE, TOKEN_NOWU,
    TOKEN_WEPEAT, TOKEN_FOW, TOKEN_BWEAK, TOKEN_CONTINYUE,
    TOKEN_STWUCT, TOKEN_ENUM, TOKEN_SMOOSH, TOKEN_CONST,
    TOKEN_STATIC, TOKEN_EXTERN, TOKEN_TYPEDEF, TOKEN_SIZEOF, TOKEN_NUWW,
    TOKEN_UNSAFE,
    
    // Types
    TOKEN_SMOL, TOKEN_CHONK, TOKEN_MEGACHONK,
    TOKEN_FLOOF, TOKEN_BIGFLOOF, TOKEN_BOOP, TOKEN_VOID, TOKEN_BYTE,
    
    // Literals
    TOKEN_IDENT, TOKEN_NUMBER, TOKEN_STRING, TOKEN_TRUE, TOKEN_FALSE,
    
    // Operators
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
    TOKEN_AMP, TOKEN_PIPE, TOKEN_CARET, TOKEN_TILDE,
    TOKEN_LSHIFT, TOKEN_RSHIFT,
    TOKEN_EQ, TOKEN_NE, TOKEN_LT, TOKEN_GT, TOKEN_LE, TOKEN_GE,
    TOKEN_AND, TOKEN_OR, TOKEN_NOT,
    TOKEN_ASSIGN, TOKEN_PLUS_EQ, TOKEN_MINUS_EQ, TOKEN_STAR_EQ, TOKEN_SLASH_EQ,
    TOKEN_ARROW,
    
    // Delimiters
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE,
    TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_COMMA, TOKEN_COLON, TOKEN_SEMICOLON, TOKEN_DOT,
    
    TOKEN_EOF, TOKEN_ERROR
} TokenKind;

typedef struct {
    TokenKind kind;
    char* lexeme;
    int line;
    int column;
    union {
        long long int_value;
        double float_value;
    } value;
} Token;

typedef struct {
    const char* source;
    int pos;
    int line;
    int column;
    Token current;
} Lexer;

Lexer* lexer_new(const char* source);
void lexer_free(Lexer* lexer);
Token lexer_next_token(Lexer* lexer);
const char* token_kind_to_string(TokenKind kind);

#endif // LEXER_H
