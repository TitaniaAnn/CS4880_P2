#ifndef TREE_H             /* include guard */
#define TREE_H

#include "token.h"         /* a Node stores Token values */

#define MAX_CHILDREN 4     /* most nonterminals on any RHS: <cond>/<loop> have 3,
                              4 leaves headroom (the doc says 3 or 4 max) */

/* One node of the parse tree. Each parser function builds exactly one of
   these, labels it with its own name, attaches the semantic tokens it
   processed, and links the subtrees its child nonterminals returned. */
typedef struct Node {
    char label[32];               /* nonterminal name, e.g. "program", "exp" */
    Token tokens[3];              /* semantic tokens kept here (ids/ints/operators);
                                     3 is the max any one production stores */
    int tokenCount;               /* how many slots of tokens[] are in use */
    struct Node *children[MAX_CHILDREN]; /* subtrees, left-to-right */
    int childCount;               /* how many slots of children[] are in use */
} Node;

Node *newNode(const char *label);        /* allocate + label a fresh node */
void  addChild(Node *parent, Node *child); /* link a subtree (NULL is ignored) */
void  addToken(Node *n, Token t);        /* store one semantic token in the node */
void  printTree(const Node *n, int depth); /* preorder dump, indented by depth */
void  freeTree(Node *n);                 /* recursively release the whole tree */

#endif /* TREE_H */
