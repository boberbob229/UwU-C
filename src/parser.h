#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"


typedef struct Parser {

    Lexer* lexer;


    Token current;


    Token previous;


    int error_count;
} Parser;


Parser* parser_new(Lexer* lexer);


void parser_free(Parser* parser);


ASTNode* parse(Parser* parser);

#endif