#include "lexer.h"
#include "util.h"
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

// Check if we've reached end of source
static bool is_at_end(Lexer* lex) {
    return lex->source[lex->pos] == '\0';
}

static char peek(Lexer* lex) {
    return lex->source[lex->pos];
}

static char peek_next(Lexer* lex) {
    if (is_at_end(lex)) return '\0';
    return lex->source[lex->pos + 1];
}

static char advance(Lexer* lex) {
    char c = lex->source[lex->pos++];
    if (c == '\n') {
        lex->line++;
        lex->column = 1;
    } else {
        lex->column++;
    }
    return c;
}

static void skip_whitespace(Lexer* lex) {
    while (true) {
        char c = peek(lex);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(lex);
        } else if (c == '/' && peek_next(lex) == '/') {
            while (peek(lex) != '\n' && !is_at_end(lex)) {
                advance(lex);
            }
        } else {
            break;
        }
    }
}

static Token make_token(Lexer* lex, TokenKind kind, const char* lexeme) {
    Token token;
    token.kind = kind;
    token.lexeme = xstrdup(lexeme);
    token.line = lex->line;
    token.column = lex->column;
    return token;
}

static Token read_number(Lexer* lex) {
    int start = lex->pos;
    while (isdigit(peek(lex))) {
        advance(lex);
    }
    
    bool is_float = false;
    if (peek(lex) == '.' && isdigit(peek_next(lex))) {
        is_float = true;
        advance(lex);
        while (isdigit(peek(lex))) {
            advance(lex);
        }
    }
    
    int length = lex->pos - start;
    char* lexeme = xmalloc(length + 1);
    strncpy(lexeme, &lex->source[start], length);
    lexeme[length] = '\0';
    
    Token token = make_token(lex, TOKEN_NUMBER, lexeme);
    if (is_float) {
        token.value.float_value = atof(lexeme);
    } else {
        token.value.int_value = atoll(lexeme);
    }
    
    free(lexeme);
    return token;
}

static Token read_identifier(Lexer* lex) {
    int start = lex->pos;
    while (isalnum(peek(lex)) || peek(lex) == '_') {
        advance(lex);
    }
    
    int length = lex->pos - start;
    char* lexeme = xmalloc(length + 1);
    strncpy(lexeme, &lex->source[start], length);
    lexeme[length] = '\0';
    
    TokenKind kind = TOKEN_IDENT;
    if (str_eq(lexeme, "nuzzle")) kind = TOKEN_NUZZLE;
    else if (str_eq(lexeme, "gimme")) kind = TOKEN_GIMME;
    else if (str_eq(lexeme, "pwease")) kind = TOKEN_PWEASE;
    else if (str_eq(lexeme, "nowu")) kind = TOKEN_NOWU;
    else if (str_eq(lexeme, "wepeat")) kind = TOKEN_WEPEAT;
    else if (str_eq(lexeme, "fow")) kind = TOKEN_FOW;
    else if (str_eq(lexeme, "bweak")) kind = TOKEN_BWEAK;
    else if (str_eq(lexeme, "continyue")) kind = TOKEN_CONTINYUE;
    else if (str_eq(lexeme, "stwuct")) kind = TOKEN_STWUCT;
    else if (str_eq(lexeme, "enum")) kind = TOKEN_ENUM;
    else if (str_eq(lexeme, "smoosh")) kind = TOKEN_SMOOSH;
    else if (str_eq(lexeme, "const")) kind = TOKEN_CONST;
    else if (str_eq(lexeme, "static")) kind = TOKEN_STATIC;
    else if (str_eq(lexeme, "extern")) kind = TOKEN_EXTERN;
    else if (str_eq(lexeme, "typedef")) kind = TOKEN_TYPEDEF;
    else if (str_eq(lexeme, "sizeof")) kind = TOKEN_SIZEOF;
    else if (str_eq(lexeme, "nuww")) kind = TOKEN_NUWW;
    else if (str_eq(lexeme, "unsafe")) kind = TOKEN_UNSAFE;
    else if (str_eq(lexeme, "smol")) kind = TOKEN_SMOL;
    else if (str_eq(lexeme, "chonk")) kind = TOKEN_CHONK;
    else if (str_eq(lexeme, "megachonk")) kind = TOKEN_MEGACHONK;
    else if (str_eq(lexeme, "floof")) kind = TOKEN_FLOOF;
    else if (str_eq(lexeme, "bigfloof")) kind = TOKEN_BIGFLOOF;
    else if (str_eq(lexeme, "boop")) kind = TOKEN_BOOP;
    else if (str_eq(lexeme, "void")) kind = TOKEN_VOID;
    else if (str_eq(lexeme, "byte")) kind = TOKEN_BYTE;
    else if (str_eq(lexeme, "true")) kind = TOKEN_TRUE;
    else if (str_eq(lexeme, "false")) kind = TOKEN_FALSE;
    
    Token token = make_token(lex, kind, lexeme);
    free(lexeme);
    return token;
}

static Token read_string(Lexer* lex) {
    advance(lex);
    int start = lex->pos;
    
    while (peek(lex) != '"' && !is_at_end(lex)) {
        if (peek(lex) == '\\') {
            advance(lex);
            if (!is_at_end(lex)) advance(lex);
        } else {
            advance(lex);
        }
    }
    
    if (is_at_end(lex)) {
        return make_token(lex, TOKEN_ERROR, "Unterminated string");
    }
    
    int length = lex->pos - start;
    char* lexeme = xmalloc(length + 1);
    strncpy(lexeme, &lex->source[start], length);
    lexeme[length] = '\0';
    
    advance(lex);
    
    Token token = make_token(lex, TOKEN_STRING, lexeme);
    free(lexeme);
    return token;
}

Lexer* lexer_new(const char* source) {
    Lexer* lex = xmalloc(sizeof(Lexer));
    lex->source = source;
    lex->pos = 0;
    lex->line = 1;
    lex->column = 1;
    return lex;
}

void lexer_free(Lexer* lexer) {
    free(lexer);
}

Token lexer_next_token(Lexer* lex) {
    skip_whitespace(lex);
    
    if (is_at_end(lex)) {
        return make_token(lex, TOKEN_EOF, "");
    }
    
    char c = peek(lex);
    
    if (isdigit(c)) {
        return read_number(lex);
    }
    
    if (isalpha(c) || c == '_') {
        return read_identifier(lex);
    }
    
    if (c == '"') {
        return read_string(lex);
    }
    
    advance(lex);
    switch (c) {
        case '+':
            if (peek(lex) == '=') { advance(lex); return make_token(lex, TOKEN_PLUS_EQ, "+="); }
            return make_token(lex, TOKEN_PLUS, "+");
        case '-':
            if (peek(lex) == '=') { advance(lex); return make_token(lex, TOKEN_MINUS_EQ, "-="); }
            if (peek(lex) == '>') { advance(lex); return make_token(lex, TOKEN_ARROW, "->"); }
            return make_token(lex, TOKEN_MINUS, "-");
        case '*':
            if (peek(lex) == '=') { advance(lex); return make_token(lex, TOKEN_STAR_EQ, "*="); }
            return make_token(lex, TOKEN_STAR, "*");
        case '/':
            if (peek(lex) == '=') { advance(lex); return make_token(lex, TOKEN_SLASH_EQ, "/="); }
            return make_token(lex, TOKEN_SLASH, "/");
        case '%': return make_token(lex, TOKEN_PERCENT, "%");
        case '&':
            if (peek(lex) == '&') { advance(lex); return make_token(lex, TOKEN_AND, "&&"); }
            return make_token(lex, TOKEN_AMP, "&");
        case '|':
            if (peek(lex) == '|') { advance(lex); return make_token(lex, TOKEN_OR, "||"); }
            return make_token(lex, TOKEN_PIPE, "|");
        case '^': return make_token(lex, TOKEN_CARET, "^");
        case '~': return make_token(lex, TOKEN_TILDE, "~");
        case '!':
            if (peek(lex) == '=') { advance(lex); return make_token(lex, TOKEN_NE, "!="); }
            return make_token(lex, TOKEN_NOT, "!");
        case '=':
            if (peek(lex) == '=') { advance(lex); return make_token(lex, TOKEN_EQ, "=="); }
            return make_token(lex, TOKEN_ASSIGN, "=");
        case '<':
            if (peek(lex) == '=') { advance(lex); return make_token(lex, TOKEN_LE, "<="); }
            if (peek(lex) == '<') { advance(lex); return make_token(lex, TOKEN_LSHIFT, "<<"); }
            return make_token(lex, TOKEN_LT, "<");
        case '>':
            if (peek(lex) == '=') { advance(lex); return make_token(lex, TOKEN_GE, ">="); }
            if (peek(lex) == '>') { advance(lex); return make_token(lex, TOKEN_RSHIFT, ">>"); }
            return make_token(lex, TOKEN_GT, ">");
        case '(': return make_token(lex, TOKEN_LPAREN, "(");
        case ')': return make_token(lex, TOKEN_RPAREN, ")");
        case '{': return make_token(lex, TOKEN_LBRACE, "{");
        case '}': return make_token(lex, TOKEN_RBRACE, "}");
        case '[': return make_token(lex, TOKEN_LBRACKET, "[");
        case ']': return make_token(lex, TOKEN_RBRACKET, "]");
        case ',': return make_token(lex, TOKEN_COMMA, ",");
        case ':': return make_token(lex, TOKEN_COLON, ":");
        case ';': return make_token(lex, TOKEN_SEMICOLON, ";");
        case '.': return make_token(lex, TOKEN_DOT, ".");
        default:
            return make_token(lex, TOKEN_ERROR, "Unexpected character");
    }
}

const char* token_kind_to_string(TokenKind kind) {
    switch (kind) {
        case TOKEN_NUZZLE: return "nuzzle";
        case TOKEN_GIMME: return "gimme";
        case TOKEN_IDENT: return "identifier";
        case TOKEN_NUMBER: return "number";
        case TOKEN_EOF: return "EOF";
        default: return "unknown";
    }
}
