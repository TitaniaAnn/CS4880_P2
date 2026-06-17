# P2 Recursive Descent Parser — Code Review Walkthrough

A walkthrough of the P2 parser built on the P1 lexer. The codebase is small,
clean, and well-organized; this review traces it module by module, confirms it
behaves correctly against the test suite, and flags one real correctness bug
(fixed on this branch) plus a few minor observations.

## 1. Architecture at a glance

The project keeps the same disciplined layering as P1, now with a parser and a
parse-tree module added:

| File | Role |
|------|------|
| `token.h` | Shared `Token` struct + `tokenID` enum. Pure data, no logic. |
| `lexer.h` / `lexer.c` | Character scanner. `lexer_init()` + `getNextToken()`. |
| `tree.h` / `tree.c` | Parse-tree node: `newNode`, `addChild`, `addToken`, `printTree`, `freeTree`. |
| `parser.h` / `parser.c` | Recursive descent parser, one function per nonterminal. |
| `P2.c` | Driver. Opens input, then runs either lex-only mode (`-l`) or the parser. |

The interface contract from P1 is preserved: the parser consumes the token
stream solely through `getNextToken()`, so the lexer didn't have to change.

## 2. Driver (`P2.c`)

Clean and defensive:

- Optional `-l` flag selects lex-only mode (prints one line per token), which is
  a nice way to keep the P1 behavior available for debugging.
- Builds the input filename as `<basename>.ext`, matching the assignment
  convention, and falls back to `stdin`.
- Bounds-checked `snprintf` into fixed buffers; closes the file only when it
  isn't `stdin`.

No issues.

## 3. Parser (`parser.c`)

### 3.1 Structure

Textbook recursive descent. One `static Node *` function per nonterminal, a
single global lookahead token `tk`, and three thin predicates (`isKW`,
`isOpDel`, `isOp`) that keep the grammar code readable. `consume()` advances the
lookahead; `error()` reports `expected X, got 'Y' (line N)` and exits — exactly
the format documented in the README.

The mapping from BNF to code is faithful and easy to audit:

- `<program> -> go <vars> <block> exit` — checks `go`, parses vars/block,
  checks `exit`, then `parser()` verifies EOF afterward.
- `<vars>` / `<varList>` — `vars` returns `NULL` on the empty production (keyed
  on `int`), which `addChild` correctly ignores, so no empty `vars` node is
  emitted. Good.
- `<exp>`, `<M>`, `<N>` — the "delay technique" (right recursion) is implemented
  as written in the grammar, producing right-leaning trees. `<N>` correctly
  distinguishes unary minus (`- <N>`) from binary minus (`<R> - <N>`).
- `<R> -> ( <exp> ) | identifier | integer` — parentheses are consumed but not
  emitted as tokens, consistent with "syntactic tokens are discarded."

### 3.2 First-sets

`isStatFirst()` gates the `<mStat>` empty production on the FIRST set of
`<stat>` (`scan output cond loop set {`). Correctly excludes identifiers, since
assignment begins with `set`, not a bare identifier.

### 3.3 Bug found and fixed: `<relational>` over-accepted `**` and `//`

The grammar restricts relational operators to six comparisons:

```
<relational> -> ?le | ?ge | ?lt | ?gt | ?ne | ?eq | = = | ;
```

The lexer, however, also tags `**` and `//` as `OpTk`. The original code
accepted **any** `OpTk`:

```c
if (tk.id == OpTk) { addToken(n, tk); consume(); }
```

So a malformed condition such as `cond [ x ** 0 ]` parsed successfully —
`**` was accepted as if it were a comparison operator. None of the bundled
`test_bad*` programs exercise this case, so it slipped through.

**Fix (this branch):** added an `isRelationalOp()` predicate that matches only
the six comparison operators, and gated the `OpTk` branch on it. After the fix:

```
$ printf "go { cond [ x ** 0 ] output 1 : } exit" | ./P2
SYNTAX ERROR: expected relational operator, got '**' (line 1)
```

All `test_good*` programs still parse and all `test_bad*` programs still error.

## 4. Parse tree (`tree.c` / `tree.h`)

- Fixed-capacity node: up to `MAX_CHILDREN` (4) children and 3 tokens. The
  busiest nonterminals (`cond`/`loop` with relational+exp+stat) stay within
  these bounds, and `addChild`/`addToken` guard the limits — `addChild` aborts
  loudly on overflow, `addToken` silently drops past 3.
- `printTree` does a preorder traversal, two-space indent per depth, emitting
  `ID:/#:/Op:` token annotations exactly as the README's example shows.
- `freeTree` recursively frees the whole tree. Verified leak-free under
  Valgrind: *52 allocs, 52 frees, 0 bytes in use at exit, 0 errors.*

One small note: `addToken` silently truncating beyond 3 tokens is fine for this
grammar but would hide a bug if a future rule attached a 4th token — an
assert/error (as `addChild` does) would be more consistent.

## 5. Lexer (`lexer.c`) — spot check

Unchanged from P1 and still solid: line tracking via `nextChar`/`pushBack`,
`@...@` comments skipped with a `goto restart` (avoids stack growth on runs of
comments — nicely commented), identifiers required to start with `x`, and the
multi-char operators (`?xx`, `**`, `//`) handled explicitly. Length caps (8
chars / 8 digits) are enforced with clear errors.

## 6. Test results

| Suite | Result |
|-------|--------|
| `test_good1..12` | All parse and print a tree |
| `test_bad1..12` | All rejected with a `SYNTAX ERROR` and exit 1 |
| Valgrind (`test_good11`) | 0 leaks, 0 errors |
| Build | `-Wall -Wextra -pedantic -std=c11` clean, no warnings |

Edge cases also behave: empty input, `go exit` with no block, and trailing
tokens after `exit` all produce the right syntax error.

## 7. Minor suggestions (non-blocking)

1. **`addToken` overflow** — consider erroring like `addChild` instead of
   silently dropping, to catch future grammar mistakes.
2. **Grammar comment ordering** — the `<N>` comment in `parser.c`
   (`- <N> | <R> - <N> | <R>`) lists the alternatives in a different order than
   the README (`<R> - <N> | - <N> | <R>`). Same language; aligning them avoids
   confusion.
3. **Coverage gap** — add a bad-program test that uses `**`/`//` where a
   relational is expected, so the bug above can't silently regress.

## Summary

The parser is well-structured, readable, leak-free, and a faithful translation
of the BNF. The one substantive defect — `<relational>` accepting `**`/`//` —
is fixed on this branch. The remaining items are minor polish.
