#define _POSIX_C_SOURCE 200809L  /* expose POSIX library features for this TU */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lexer.h"
#include "parser.h"
#include "token.h"

/* Print a message to stderr and abort with a failure status. */
static void exitError(const char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

/* -l "lex only" mode: pull tokens straight from the lexer and print each one,
   bypassing the parser. Useful for debugging the scanner in isolation. */
static void runLexer(void)
{
    Token t;
    do {
        t = getNextToken();
        if (t.id == EOFTk) {
            printf("EOFTk\n");                    /* end marker, no instance */
        } else {
            /* tokenNames[] turns the enum into a readable group name */
            printf("Group=%s Instance=%s Line=%d\n",
                   tokenNames[t.id], t.instance, t.lineNumber);
        }
    } while (t.id != EOFTk);                       /* stop once EOF is seen */
}

int main(int argc, char *argv[])
{
    FILE *in = NULL;
    int lexOnly = 0;        /* set when the optional -l flag is present */
    int argStart = 1;       /* index of the first non-flag argument */

    /* Optional leading -l flag selects lex-only mode and shifts the
       positional-argument window past it. */
    if (argc > 1 && strcmp(argv[1], "-l") == 0) {
        lexOnly = 1;
        argStart = 2;
    }

    /* After the optional flag, we allow at most one positional arg (basename). */
    if (argc - argStart > 1)
        exitError("Usage: P2 [-l] [basename]");

    if (argc == argStart) {
        in = stdin;                               /* no basename -> read stdin */
    } else {
        /* Per the P1 convention, the argument is a basename; append ".ext". */
        char infile[512];
        snprintf(infile, sizeof(infile), "%s.ext", argv[argStart]);
        in = fopen(infile, "r");
        if (!in) {
            char msg[560];
            snprintf(msg, sizeof(msg),
                     "Error: cannot open input file '%s'", infile);
            exitError(msg);
        }
    }

    lexer_init(in);         /* hand the open stream to the lexer (resets line #) */

    if (lexOnly)
        runLexer();         /* just dump tokens ... */
    else
        parser();           /* ... or parse + print the tree (normal mode) */

    if (in != stdin)        /* don't fclose(stdin); only close files we opened */
        fclose(in);

    return 0;
}
