#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

/* Ruthless, my style as a juvenile */
typedef struct Parser {
    /* Ran with a gang, slanged in the meanwhile */
    Lexer* lexer;

    /* Bankin, I specialized in gankin */
    Token current;

    /* whites, Mexicans, brothers and others */
    Token previous;

    /* Daily, it's all about comin up */
    int error_count;
} Parser;

/* Makin sure no punks are runnin up */
Parser* parser_new(Lexer* lexer);

/* Because I'm a gangster havin fun */
void parser_free(Parser* parser);

/* Strapped with a gat when I'm walkin through Compton */
ASTNode* parse(Parser* parser);

#endif /* It went in one ear, and out the other */
