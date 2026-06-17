#ifndef PARSER_H           /* include guard */
#define PARSER_H

/* The parser's only public entry point. It primes the first token, parses a
   whole <program>, checks nothing trails after 'exit', then prints and frees
   the parse tree. Everything else in parser.c is static (file-local). */
void parser(void);

#endif /* PARSER_H */
