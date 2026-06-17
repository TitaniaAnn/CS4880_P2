#define _POSIX_C_SOURCE 200809L  /* expose POSIX library features for this TU */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>                /* isspace, isalpha, isdigit */

#include "lexer.h"
#include "token.h"

/* Readable names for each tokenID, indexed by the enum value. Used by the
   -l lex-only dump in P2.c. Order must match the enum in token.h. */
const char * const tokenNames[] = {
    "IDTk", "IntTk", "KW_tk", "OpTk", "OpDelTk", "EOFTk"
};

/* Lexer state, file-local: the source stream and the current line number. */
static FILE *src     = NULL;
static int   lineNum = 1;

/* Bind the lexer to an open stream and reset the line counter. */
void lexer_init(FILE *fp)
{
    src     = fp;
    lineNum = 1;
}

/* Read one character, advancing the line counter on newlines so every token
   can be tagged with the line it starts on. */
static int nextChar(void)
{
    int c = fgetc(src);
    if (c == '\n')
        lineNum++;
    return c;
}

/* Put a character back for the next read. We undo the line bump here too, so
   pushing back a newline keeps the counter consistent. */
static void pushBack(int c)
{
    if (c == '\n')
        lineNum--;
    ungetc(c, src);
}

/* Report a lexical error (with the current line) and abort. */
static void lexError(const char *detail)
{
    fprintf(stderr, "LEXICAL ERROR: %s line %d\n", detail, lineNum);
    exit(1);
}

/* The reserved words of the language. NULL-terminated so isKeyword can loop
   without a separate count. */
static const char * const keywords[] = {
    "go", "og", "loop", "int", "exit", "scan",
    "output", "cond", "then", "set", "func", "program",
    NULL
};

/* Return 1 if s exactly matches a reserved word, else 0. */
static int isKeyword(const char *s)
{
    int i;
    for (i = 0; keywords[i] != NULL; i++)
        if (strcmp(s, keywords[i]) == 0)
            return 1;
    return 0;
}

/* The operators/delimiters that are a single character on their own. */
static int isSingleCharOp(int c)
{
    return c == '+' || c == '-' || c == ':' || c == ';' ||
           c == '=' || c == '(' || c == ')' || c == '{' ||
           c == '}' || c == '[' || c == ']';
}

/* Build a Token value, copying the instance text with guaranteed termination. */
static Token makeToken(tokenID id, const char *instance, int line)
{
    Token t;
    t.id         = id;
    t.lineNumber = line;
    strncpy(t.instance, instance, TOKEN_INSTANCE_MAX - 1);
    t.instance[TOKEN_INSTANCE_MAX - 1] = '\0';
    return t;
}

/* Scan and return the next token. Skips whitespace and @...@ comments. */
Token getNextToken(void)
{
    int  c;
    int  tokenLine;
    char buf[TOKEN_INSTANCE_MAX];
    int  len;

restart:
    /* Skip whitespace, counting newlines via nextChar. EOF here means a clean
       end of input -> hand back the EOF token. */
    do {
        c = nextChar();
        if (c == EOF)
            return makeToken(EOFTk, "", lineNum);
    } while (isspace(c));

    /* Remember where this token begins (whitespace may have crossed lines). */
    tokenLine = lineNum;

    /* Comment: @...@ with no internal whitespace. We consume it and loop back
       to scan the token that actually follows. */
    if (c == '@') {
        int c2;
        while (1) {
            c2 = nextChar();
            if (c2 == EOF)
                lexError("unterminated comment");
            if (isspace(c2))
                lexError("whitespace inside comment");
            if (c2 == '@')
                break;                 /* closing @ found */
        }
        /* goto instead of recursive getNextToken() call: recursion would grow
           the stack once per consecutive comment; a label+goto is O(1) space */
        goto restart;
    }

    /* Letter: a keyword or an identifier. Read the whole alphanumeric run. */
    if (isalpha(c)) {
        len = 0;
        buf[len++] = (char)c;
        while (1) {
            int c2 = nextChar();
            if (isalpha(c2) || isdigit(c2) || c2 == '_') {
                if (len < TOKEN_INSTANCE_MAX - 1)
                    buf[len++] = (char)c2;
                else {                 /* more than 8 significant chars */
                    buf[len] = '\0';
                    lexError("identifier or keyword exceeds 8 characters");
                }
            } else {
                pushBack(c2);          /* this char belongs to the next token */
                break;
            }
        }
        buf[len] = '\0';

        if (isKeyword(buf))
            return makeToken(KW_tk, buf, tokenLine);

        /* Non-keyword words are identifiers, which must start with 'x'. */
        if (buf[0] != 'x') {
            char msg[64];
            snprintf(msg, sizeof(msg), "invalid identifier '%s'", buf);
            lexError(msg);
        }
        return makeToken(IDTk, buf, tokenLine);
    }

    /* Digit: an integer literal. Read the whole digit run (max 8 digits). */
    if (isdigit(c)) {
        len = 0;
        buf[len++] = (char)c;
        while (1) {
            int c2 = nextChar();
            if (isdigit(c2)) {
                if (len < TOKEN_INSTANCE_MAX - 1)
                    buf[len++] = (char)c2;
                else {
                    buf[len] = '\0';
                    lexError("integer exceeds 8 digits");
                }
            } else {
                pushBack(c2);
                break;
            }
        }
        buf[len] = '\0';
        return makeToken(IntTk, buf, tokenLine);
    }

    /* '?': a 3-character comparison operator. Read exactly two more chars and
       validate against the allowed set. */
    if (c == '?') {
        int c2 = nextChar();
        int c3;
        if (c2 == EOF)
            lexError("bare '?' at end of input");
        c3 = nextChar();
        if (c3 == EOF)
            lexError("incomplete operator after '?'");
        buf[0] = '?';
        buf[1] = (char)c2;
        buf[2] = (char)c3;
        buf[3] = '\0';
        if (strcmp(buf, "?le") == 0 || strcmp(buf, "?ge") == 0 ||
            strcmp(buf, "?lt") == 0 || strcmp(buf, "?gt") == 0 ||
            strcmp(buf, "?ne") == 0 || strcmp(buf, "?eq") == 0)
            return makeToken(OpTk, buf, tokenLine);
        {                              /* anything else after '?' is invalid */
            char msg[32];
            snprintf(msg, sizeof(msg), "unknown operator '%s'", buf);
            lexError(msg);
        }
    }

    /* '*': only valid doubled as '**'. */
    if (c == '*') {
        int c2 = nextChar();
        if (c2 == '*')
            return makeToken(OpTk, "**", tokenLine);
        pushBack(c2);
        lexError("bare '*' is not a valid token");
    }

    /* '/': only valid doubled as '//'. */
    if (c == '/') {
        int c2 = nextChar();
        if (c2 == '/')
            return makeToken(OpTk, "//", tokenLine);
        pushBack(c2);
        lexError("bare '/' is not a valid token");
    }

    /* Remaining single-character operators and delimiters. */
    if (isSingleCharOp(c)) {
        buf[0] = (char)c;
        buf[1] = '\0';
        return makeToken(OpDelTk, buf, tokenLine);
    }

    /* Any other character is not part of the language. */
    {
        char msg[32];
        snprintf(msg, sizeof(msg), "unexpected character '%c'", (char)c);
        lexError(msg);
    }

    /* unreachable — lexError exits, but the compiler wants a return here */
    return makeToken(EOFTk, "", lineNum);
}
