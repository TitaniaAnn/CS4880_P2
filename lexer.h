#ifndef LEXER_H            /* include guard */
#define LEXER_H

#include <stdio.h>         /* for FILE */
#include "token.h"         /* for the Token type returned below */

/* Associate the lexer with an open FILE and reset the line counter.
   Must be called once before getNextToken(). */
void  lexer_init(FILE *fp);

/* Return the next token from the source.  Silently skips comments.
   On lexical error, prints to stderr and exits.
   This single function is the entire interface the parser depends on. */
Token getNextToken(void);

#endif /* LEXER_H */
