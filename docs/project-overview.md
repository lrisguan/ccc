# Project Overview

## What This Project Is

`ccc` is a teaching compiler for a C subset. It uses a hand-written frontend and LLVM for IR generation, optimization, and object emission.

The project is designed for clarity and incremental evolution rather than full ISO C compliance.

## Current Capabilities

- Function declarations and definitions.
- `extern` declarations.
- Types: `int`, `char`, `float`, `double`, `void`, pointers, and multi-level pointers.
- Variadic declarations and calls (`...`) support in core compile flow.
- Statements: block, variable declaration, expression statement, `if/else`, `while`, `return`.
- Expressions: literals, identifiers, calls, assignment, unary/binary operators, C-style casts.
- Preprocessing support for common workflows (`#include`, conditional directives, `-D`, `-U`, comment-aware parsing).
- End-to-end output: source -> object -> executable.

## Repository Layout

- `include/`: public interfaces (`Lexer`, `Parser`, `SemanticAnalyzer`, `IRGenerator`, etc.).
- `src/`: implementation of each stage and CLI entrypoint.
- `examples/`: sample source programs.
- `tests/`: integration and invalid-case tests.
- `docs/`: project documentation.

## Key Limitations

- This is still a subset compiler.
- Full preprocessor compatibility is not complete.
- Full function-like macro substitution is partial.
- Full C standard library semantics are not fully modeled in frontend type system.

## Design Priorities

1. Keep architecture understandable.
2. Preserve deterministic, testable behavior.
3. Add features in vertical slices (lexer -> parser -> semantic -> IR -> tests).
