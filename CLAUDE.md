# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

CS 4880 Compiler Design — P2, a recursive descent parser built on top of the P1
lexical analyzer. The parser consumes tokens from `getNextToken()`, validates the
program against the course BNF grammar, builds a parse tree, and prints it in
preorder traversal.

## Build & Run

```bash
make          # produces ./P2 executable
make clean    # removes .o files and executable
```

Run the parser:
```bash
./P2 basename      # reads basename.ext, prints the parse tree
./P2 < file.ext    # stdin redirect
./P2               # interactive stdin (Ctrl+D to end)
```

Lex-only mode (driver extension, not required by the assignment) prints one token
per line instead of the tree:
```bash
./P2 -l basename
./P2 -l < file.ext
```

There is no `make` on the Windows/MSYS2 dev box; build manually with the same flags:
```bash
gcc -Wall -Wextra -pedantic -std=c11 -g -o P2 P2.c parser.c tree.c lexer.c
```

Test with the provided files: `test_good1.ext`..`test_good12.ext` (must parse with
no error) and `test_bad1.ext`..`test_bad12.ext` (must report a syntax error and
exit 1). There is no automated runner — compare output manually against the
reference trees/outputs in `docs/`.

Compiler flags: `-Wall -Wextra -pedantic -std=c11 -g` (enforced by Makefile).

## Architecture

Four-layer design with strict separation:

**`token.h`** — shared data definitions only (no logic):
- `tokenID` enum: `IDTk`, `IntTk`, `KW_tk`, `OpTk`, `OpDelTk`, `EOFTk`
- `Token` struct: `{ tokenID id; char instance[9]; int lineNumber; }`

**`lexer.h` / `lexer.c`** — stateful character-by-character scanner (the P1 layer,
preserved as the parser's input interface):
- `lexer_init(FILE*)` — sets internal `FILE *src` and resets `lineNum`
- `getNextToken()` — returns the next `Token`; exits(1) on lexical error
- Skips `@...@` comments silently; tracks line numbers for error reporting
- Identifiers must start with `x`; max 8 chars. Integers max 8 digits.
- Multi-char ops: `?le ?ge ?lt ?gt ?ne ?eq ** //`
- Keywords: `go og loop int exit scan output cond then set func program`

**`tree.h` / `tree.c`** — parse-tree node and preorder printer:
- `Node`: a `label`, up to 3 stored semantic tokens, up to `MAX_CHILDREN` (4) children
- `newNode` / `addChild` / `addToken` / `printTree` / `freeTree`
- Only semantic tokens (identifiers, integers, operators/delimiters) are stored;
  syntactic tokens (keywords, brackets, colons) are discarded

**`parser.h` / `parser.c`** — recursive descent parser:
- One static function per nonterminal, named after it (`program_rule`, `vars`,
  `varList`, `block`, `stats`, `mStat`, `stat`, `read`, `print`, `cond`, `loop`,
  `assign`, `relational`, `expr`, `M`, `N`, `R`)
- Single static "current token" `tk`; `consume()` advances via `getNextToken()`.
  Each function assumes an unconsumed token on entry and leaves one on exit
- `<exp>`/`<M>`/`<N>` common prefixes use the "delay" technique (no left-factoring)
- Errors print `SYNTAX ERROR: expected <X>, got '<Y>' (line <n>)` to stderr, exit 1
- `parser()` is the entry point: primes the first token, parses `<program>`,
  checks for trailing tokens after `exit`, then prints and frees the tree

**`P2.c`** — driver only:
- Parses `[-l]` flag and optional basename; opens file (appends `.ext`) or stdin
- Calls `lexer_init()`, then `parser()` (or the lex-only loop with `-l`)

## Grammar (P2 BNF)

```
<program>    -> go <vars> <block> exit
<vars>       -> empty | int identifier = integer <varList> :
<varList>    -> identifier = integer <varList> | empty
<block>      -> { <vars> <stats> }
<stats>      -> <stat> <mStat>
<mStat>      -> empty | <stat> <mStat>
<stat>       -> <read> | <print> | <block> | <cond> | <loop> | <assign>
<read>       -> scan identifier :
<print>      -> output <exp> :
<cond>       -> cond [ identifier <relational> <exp> ] <stat>
<loop>       -> loop [ identifier <relational> <exp> ] <stat>
<assign>     -> set identifier = <exp> :
<relational> -> ?le | ?ge | ?lt | ?eq | = = | ;
<exp>        -> <M> ** <exp> | <M> // <exp> | <M>
<M>          -> <N> + <M> | <N>
<N>          -> <R> - <N> | - <N> | <R>
<R>          -> ( <exp> ) | identifier | integer
```

Note: `<relational>` accepts only the four comparison operators listed in the BNF
(`?le ?ge ?lt ?eq`); `?gt` and `?ne` are valid P1 tokens but are not part of the
relational production, so the parser rejects them in that position.
