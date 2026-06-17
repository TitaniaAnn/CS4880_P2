#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "parser.h"
#include "tree.h"
#include "lexer.h"
#include "token.h"

/* ── global current token ─────────────────────────────────────────────
   The whole parser shares one "current token". Invariant for every
   function below: it is entered with tk holding an unconsumed token, and
   it leaves with tk holding the next unconsumed token. consume() is the
   only thing that advances the stream. */
static Token tk;

/* ── forward declarations for mutual recursion ────────────────────────
   The grammar is recursive (e.g. <exp> contains <exp>), so each rule's
   function must be visible before the others call it. */
static Node *program_rule(void);
static Node *vars(void);
static Node *varList(void);
static Node *block(void);
static Node *stats(void);
static Node *mStat(void);
static Node *stat(void);
static Node *read(void);
static Node *print(void);
static Node *cond(void);
static Node *loop(void);
static Node *assign(void);
static Node *relational(void);
static Node *expr(void);
static Node *M(void);
static Node *N(void);
static Node *R(void);

/* ── helpers ──────────────────────────────────────────────────────────*/

/* Print a detailed syntax error (what was expected, what was found, where)
   and abort. Called whenever the current token can't continue any production. */
static void error(const char *expected)
{
    fprintf(stderr, "SYNTAX ERROR: expected %s, got '%s' (line %d)\n",
            expected, tk.instance, tk.lineNumber);
    exit(1);
}

/* Advance: pull the next token from the lexer into the shared tk. */
static void consume(void) { tk = getNextToken(); }

/* Is the current token the keyword kw?  (group must be KW and text must match) */
static int isKW(const char *kw)
{
    return tk.id == KW_tk && strcmp(tk.instance, kw) == 0;
}

/* Is the current token the single-char operator/delimiter op? */
static int isOpDel(const char *op)
{
    return tk.id == OpDelTk && strcmp(tk.instance, op) == 0;
}

/* Is the current token the multi-char operator op (e.g. "**")? */
static int isOp(const char *op)
{
    return tk.id == OpTk && strcmp(tk.instance, op) == 0;
}

/* FIRST(<stat>): can the current token begin a statement?  Used by <mStat>
   to decide between "another statement" and the empty production. */
static int isStatFirst(void)
{
    return isKW("scan") || isKW("output") || isKW("cond") ||
           isKW("loop") || isKW("set")    || isOpDel("{");
}

/* ── nonterminal implementations ──────────────────────────────────────
   Each function creates one tree node, matches the tokens of its rule
   (consuming as it goes), attaches semantic tokens + child subtrees, and
   returns its node. Syntactic tokens (go, {, :, ...) are matched but not
   stored. A missing required token is a syntax error. */

/* <program> -> go <vars> <block> exit */
static Node *program_rule(void)
{
    Node *n = newNode("program");
    if (!isKW("go")) error("'go'");     /* every program must open with go */
    consume();
    addChild(n, vars());                /* optional leading declarations */
    addChild(n, block());               /* the program body { ... } */
    if (!isKW("exit")) error("'exit'"); /* every program must close with exit */
    consume();
    return n;
}

/* <vars> -> empty | int identifier = integer <varList> :
   The whole rule is optional: if there's no 'int', take the empty production
   by returning NULL (addChild ignores NULL, so no 'vars' node appears). */
static Node *vars(void)
{
    if (!isKW("int")) return NULL;      /* empty production */
    Node *n = newNode("vars");
    consume();                          /* int */
    if (tk.id != IDTk) error("identifier after 'int'");
    addToken(n, tk);                    /* store the variable name */
    consume();
    if (!isOpDel("=")) error("'='");
    consume();
    if (tk.id != IntTk) error("integer");
    addToken(n, tk);                    /* store the initial value */
    consume();
    addChild(n, varList());             /* any further "id = int" pairs */
    if (!isOpDel(":")) error("':'");    /* declaration list ends with ':' */
    consume();
    return n;
}

/* <varList> -> identifier = integer <varList> | empty
   Right-recursive: each pair becomes a varList node whose child is the rest.
   The chain ends with an empty varList (a node with no tokens/children). */
static Node *varList(void)
{
    Node *n = newNode("varList");
    if (tk.id != IDTk) return n;           /* empty production: no more pairs */
    addToken(n, tk);                       /* variable name */
    consume();
    if (!isOpDel("=")) error("'='");
    consume();
    if (tk.id != IntTk) error("integer");
    addToken(n, tk);                       /* value */
    consume();
    addChild(n, varList());                /* recurse for the remaining pairs */
    return n;
}

/* <block> -> { <vars> <stats> } */
static Node *block(void)
{
    if (!isOpDel("{")) error("'{'");
    Node *n = newNode("block");
    consume();
    addChild(n, vars());                   /* optional block-local declarations */
    addChild(n, stats());                  /* one or more statements (required) */
    if (!isOpDel("}")) error("'}'");
    consume();
    return n;
}

/* <stats> -> <stat> <mStat>   (one statement, then zero or more) */
static Node *stats(void)
{
    Node *n = newNode("stats");
    addChild(n, stat());                   /* at least one statement required */
    addChild(n, mStat());                  /* the rest, if any */
    return n;
}

/* <mStat> -> empty | <stat> <mStat>
   Look ahead: if the token starts a statement, take another; otherwise this
   is the empty production (a bare mStat node ends the chain). */
static Node *mStat(void)
{
    Node *n = newNode("mStat");
    if (!isStatFirst()) return n;          /* empty production */
    addChild(n, stat());
    addChild(n, mStat());                  /* recurse for further statements */
    return n;
}

/* <stat> -> <read> | <print> | <block> | <cond> | <loop> | <assign>
   LL(1): the current token uniquely selects which kind of statement. */
static Node *stat(void)
{
    Node *n = newNode("stat");
    if      (isKW("scan"))   addChild(n, read());
    else if (isKW("output")) addChild(n, print());
    else if (isOpDel("{"))   addChild(n, block());
    else if (isKW("cond"))   addChild(n, cond());
    else if (isKW("loop"))   addChild(n, loop());
    else if (isKW("set"))    addChild(n, assign());
    else                     error("statement keyword or '{'");
    return n;
}

/* <read> -> scan identifier : */
static Node *read(void)
{
    Node *n = newNode("read");
    consume();                          /* scan (already confirmed by caller) */
    if (tk.id != IDTk) error("identifier after 'scan'");
    addToken(n, tk);                    /* the variable being read into */
    consume();
    if (!isOpDel(":")) error("':'");
    consume();
    return n;
}

/* <print> -> output <exp> : */
static Node *print(void)
{
    Node *n = newNode("print");
    consume();                          /* output */
    addChild(n, expr());                /* the expression to print */
    if (!isOpDel(":")) error("':'");
    consume();
    return n;
}

/* <cond> -> cond [ identifier <relational> <exp> ] <stat>
   An if-style guard: compare identifier against an expression, run a stat. */
static Node *cond(void)
{
    Node *n = newNode("cond");
    consume();                          /* cond */
    if (!isOpDel("[")) error("'['");
    consume();
    if (tk.id != IDTk) error("identifier");
    addToken(n, tk);                    /* left-hand side of the comparison */
    consume();
    addChild(n, relational());          /* the comparison operator */
    addChild(n, expr());                /* right-hand side */
    if (!isOpDel("]")) error("']'");
    consume();
    addChild(n, stat());                /* body executed when guard holds */
    return n;
}

/* <loop> -> loop [ identifier <relational> <exp> ] <stat>
   Same shape as <cond>, but the body is a loop body. */
static Node *loop(void)
{
    Node *n = newNode("loop");
    consume();                          /* loop */
    if (!isOpDel("[")) error("'['");
    consume();
    if (tk.id != IDTk) error("identifier");
    addToken(n, tk);
    consume();
    addChild(n, relational());
    addChild(n, expr());
    if (!isOpDel("]")) error("']'");
    consume();
    addChild(n, stat());
    return n;
}

/* <assign> -> set identifier = <exp> : */
static Node *assign(void)
{
    Node *n = newNode("assign");
    consume();                          /* set */
    if (tk.id != IDTk) error("identifier after 'set'");
    addToken(n, tk);                    /* the variable being assigned */
    consume();
    if (!isOpDel("=")) error("'='");
    consume();
    addChild(n, expr());                /* the value expression */
    if (!isOpDel(":")) error("':'");
    consume();
    return n;
}

/* <relational> -> ?le | ?ge | ?lt | ?eq | = = | ;   (per P2 BNF)

   Only these four ?-operators are relational. The lexer also recognizes ?gt
   and ?ne, but the BNF does not list them here, so this predicate deliberately
   excludes them (and excludes ** / //, which a plain "tk.id == OpTk" test would
   have wrongly accepted in the comparison position). */
static int isRelOp(void)
{
    return tk.id == OpTk &&
           (strcmp(tk.instance, "?le") == 0 || strcmp(tk.instance, "?ge") == 0 ||
            strcmp(tk.instance, "?lt") == 0 || strcmp(tk.instance, "?eq") == 0);
}

static Node *relational(void)
{
    Node *n = newNode("relational");
    if (isRelOp()) {                    /* one of ?le ?ge ?lt ?eq */
        addToken(n, tk);
        consume();
    } else if (isOpDel("=")) {          /* the "= =" form: two '=' tokens */
        addToken(n, tk);
        consume();
        if (!isOpDel("=")) error("second '=' in '= ='");
        addToken(n, tk);               /* store both halves of the operator */
        consume();
    } else if (isOpDel(";")) {          /* ';' is also a relational here */
        addToken(n, tk);
        consume();
    } else {
        error("relational operator");
    }
    return n;
}

/* <exp> -> <M> ** <exp> | <M> // <exp> | <M>
   The three productions share the prefix <M>. Rather than left-factor, we use
   the "delay technique": always parse <M> first, then peek — if ** or // is
   next, consume it (store it on this node) and recurse for the rest. If not,
   <M> alone was the whole expression. */
static Node *expr(void)
{
    Node *n = newNode("exp");
    addChild(n, M());
    if (isOp("**") || isOp("//")) {     /* optional trailing operator + tail */
        addToken(n, tk);
        consume();
        addChild(n, expr());
    }
    return n;
}

/* <M> -> <N> + <M> | <N>   (delay technique on the shared prefix <N>) */
static Node *M(void)
{
    Node *n = newNode("M");
    addChild(n, N());
    if (isOpDel("+")) {                  /* optional "+ <M>" tail */
        addToken(n, tk);
        consume();
        addChild(n, M());
    }
    return n;
}

/* <N> -> - <N> | <R> - <N> | <R>
   Two cases: a leading '-' (unary minus) is its own production; otherwise
   parse <R> and then optionally a "- <N>" tail (binary minus). */
static Node *N(void)
{
    Node *n = newNode("N");
    if (isOpDel("-")) {                  /* unary minus: - <N> */
        addToken(n, tk);
        consume();
        addChild(n, N());
    } else {
        addChild(n, R());               /* <R>, then maybe "- <N>" */
        if (isOpDel("-")) {
            addToken(n, tk);
            consume();
            addChild(n, N());
        }
    }
    return n;
}

/* <R> -> ( <exp> ) | identifier | integer
   The atoms of an expression: a parenthesized subexpression, a variable, or
   a number. Parentheses are matched but not stored; id/int are stored. */
static Node *R(void)
{
    Node *n = newNode("R");
    if (isOpDel("(")) {
        consume();
        addChild(n, expr());            /* recurse for the inner expression */
        if (!isOpDel(")")) error("')'");
        consume();
    } else if (tk.id == IDTk) {
        addToken(n, tk);                /* a variable reference */
        consume();
    } else if (tk.id == IntTk) {
        addToken(n, tk);                /* a numeric literal */
        consume();
    } else {
        error("identifier, integer, or '('");
    }
    return n;
}

/* ── public entry point ───────────────────────────────────────────────*/
void parser(void)
{
    tk = getNextToken();                /* prime the first token */
    Node *tree = program_rule();        /* parse the entire program */
    if (tk.id != EOFTk)                 /* nothing may follow 'exit' */
        error("end of input");
    printTree(tree, 0);                 /* dump the parse tree (for grading) */
    freeTree(tree);                     /* release all nodes */
}
