# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

CS 4880 Compiler Design — starting from a P1 (Lexical Analyzer) foundation. The directory is named P2, so the next phase (recursive descent parser) will be built on top of the existing lexer.

## Build & Run

```bash
make          # produces ./P1 executable
make clean    # removes .o files and executable
```

Run the lexer:
```bash
./P1 basename      # reads basename.ext
./P1 < file.ext    # stdin redirect
./P1               # interactive stdin (Ctrl+D to end)
```

Test with the provided `.ext` files: `P1_test1.ext` through `P1_test4.ext`.

There is no automated test runner — compare output manually against expected output in `docs/`.

Compiler flags: `-Wall -Wextra -pedantic -std=c11 -g` (enforced by Makefile).

## Architecture

Three-layer design with strict separation:

**`token.h`** — shared data definitions only (no logic):
- `tokenID` enum: `IDTk`, `IntTk`, `KW_tk`, `OpTk`, `OpDelTk`, `EOFTk`
- `Token` struct: `{ tokenID id; char instance[9]; int line; }`

**`lexer.h` / `lexer.c`** — stateful character-by-character scanner:
- `lexer_init(FILE*)` — sets internal `FILE *src` and resets `lineNum`
- `getNextToken()` — returns the next `Token`; exits(1) on lexical error
- Skips `@...@` comments silently; tracks line numbers for error reporting
- Identifiers must start with `x`; max 8 chars. Integers max 8 digits.
- Multi-char ops: `?le ?ge ?lt ?gt ?ne ?eq ** //`
- Keywords: `go og loop int exit scan output cond then set func program`

**`P1.c`** — driver only:
- Opens file or uses stdin, calls `lexer_init()`, loops `getNextToken()` until `EOFTk`
- Prints `Group=<type> Instance=<value> Line=<n>` per token

## P2 Extension Notes

The `docs/` folder contains PDFs with the BNF grammar and recursive descent parser reference for the next assignment. The parser will consume tokens from `getNextToken()` — `lexer.h` is the interface to preserve.
