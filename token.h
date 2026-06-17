#ifndef TOKEN_H            /* include guard: prevents this header being */
#define TOKEN_H            /* processed twice in a single translation unit */

/* The category every token falls into. The lexer tags each token with one
   of these, and the parser branches on them to decide what to do next. */
typedef enum {
    IDTk,                  /* identifier  — starts with 'x' (e.g. x, x1, x2) */
    IntTk,                 /* integer literal — decimal digits (e.g. 0, 42)  */
    KW_tk,                 /* keyword     — go, int, exit, scan, output, ... */
    OpTk,                  /* multi-char operator — ?le ?ge ?lt ?gt ?ne ?eq ** // */
    OpDelTk,               /* single-char operator/delimiter — + - : ; = ( ) { } [ ] */
    EOFTk                  /* end of input — signals the token stream is done */
} tokenID;

/* Human-readable names indexed by tokenID, defined in lexer.c.
   Used by the -l lex-only dump to print "Group=<name>". */
extern const char * const tokenNames[];

#define TOKEN_INSTANCE_MAX 9   /* 8 significant chars + 1 for the NUL terminator */

/* One lexical token. A token is just (category, text, source line). */
typedef struct {
    tokenID  id;                        /* which category (see enum above) */
    char     instance[TOKEN_INSTANCE_MAX]; /* the actual text, e.g. "x1" or "?le" */
    int      lineNumber;                /* line it appeared on, for error messages */
} Token;

#endif /* TOKEN_H */
