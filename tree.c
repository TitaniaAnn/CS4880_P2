#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tree.h"

/* Allocate a fresh node, copy in its label, and zero the counts so it starts
   with no tokens and no children. Aborts if malloc fails. */
Node *newNode(const char *label)
{
    Node *n = malloc(sizeof(Node));
    if (!n) { fprintf(stderr, "out of memory\n"); exit(1); }
    /* strncpy + manual NUL guarantees termination even if label is too long */
    strncpy(n->label, label, sizeof(n->label) - 1);
    n->label[sizeof(n->label) - 1] = '\0';
    n->tokenCount = 0;
    n->childCount = 0;
    for (int i = 0; i < MAX_CHILDREN; i++) n->children[i] = NULL;
    return n;
}

/* Append child to parent's child list. A NULL child (e.g. an empty <vars>
   production returning NULL) is silently ignored so callers don't have to check. */
void addChild(Node *parent, Node *child)
{
    if (!child) return;
    if (parent->childCount >= MAX_CHILDREN) {     /* should never happen for this grammar */
        fprintf(stderr, "internal error: too many children on '%s'\n", parent->label);
        exit(1);
    }
    parent->children[parent->childCount++] = child;
}

/* Store one semantic token in the node. Tokens are copied by value, so the
   parser's single global token can be reused safely. Caps at 3 (the most any
   one production needs); extras are dropped rather than overflowing. */
void addToken(Node *n, Token t)
{
    if (n->tokenCount >= 3) return;
    n->tokens[n->tokenCount++] = t;
}

/* Print a single stored token as " group:instance:line". Syntactic-only
   token kinds (EOF, keywords) never reach here, so default does nothing. */
static void printToken(Token t)
{
    switch (t.id) {
        case IDTk:    printf(" ID:%s:%d",  t.instance, t.lineNumber); break;
        case IntTk:   printf(" #:%s:%d",   t.instance, t.lineNumber); break;
        case OpTk:
        case OpDelTk: printf(" Op:%s:%d",  t.instance, t.lineNumber); break;
        default: break;
    }
}

/* Preorder traversal: print this node (indented by depth), its tokens, then
   recurse into each child one level deeper. This is the tree dump used for
   grading/verification. */
void printTree(const Node *n, int depth)
{
    if (!n) return;
    for (int i = 0; i < depth; i++) printf("  ");     /* two spaces per level */
    printf("%s", n->label);                            /* the nonterminal name */
    for (int i = 0; i < n->tokenCount; i++) printToken(n->tokens[i]);
    printf("\n");
    for (int i = 0; i < n->childCount; i++) printTree(n->children[i], depth + 1);
}

/* Post-order free: release children first, then the node itself, so we never
   follow a dangling pointer. */
void freeTree(Node *n)
{
    if (!n) return;
    for (int i = 0; i < n->childCount; i++) freeTree(n->children[i]);
    free(n);
}
