# Standard Library And Variadic Support Guide

This document summarizes the current status of standard library support in `ccc`, what was implemented, how to test safely (with timeout), and what can be improved next.

## 1. Goals And Current Result

Implemented goals:

- Generic preprocessor behavior for common system headers.
- Variadic function declaration and call support (`...`).
- `stdio.h` usage in realistic call sites (for example `printf`).
- Timeout-safe testing workflow to avoid terminal lockup.

Current result:

- Core `std` integration tests pass under timeout guard.
- Previous hang cases (`brk` growth) were fixed by parser and macro expansion changes.

## 2. Key Design Choices

### 2.1 Preprocessor Is Generic, Not Per-Header Hardcoding

The implementation avoids fake redefinitions of standard headers and instead supports:

- include directory discovery for common Linux paths.
- conditional directives (`#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif`).
- macro define/undef tracking.
- comment stripping before directive parsing.
- `#include_next` handling.

### 2.2 Variadic Support Is Language-Level

Variadic support is implemented in parsing, symbols, semantic checking, and IR call generation rather than in single-library special cases.

## 3. Implemented Capabilities

### 3.1 Parser / AST

- Parses function parameter lists with ellipsis:
  - `int f(...);`
  - `int f(int a, ...);`
- Supports C-style cast expressions:
  - `(void*)0`
  - `(int)x`

### 3.2 Symbols / Semantic

- Function symbol records `is_variadic`.
- Call checks:
  - non-variadic call: argument count must match exactly.
  - variadic call: argument count must be at least fixed parameter count.
- Undeclared function calls are accepted as implicit `extern int f(...);` for compatibility while headers evolve.
- Equality checks accept pointer/integer forms used by null comparisons:
  - `ptr == 0`
  - `NULL == 0`

### 3.3 IR Generation

- Variadic calls are emitted correctly for known variadic declarations.
- Unknown callees are emitted as external `int (...)` and can be linked at runtime.
- Cast expressions are lowered using existing type conversion logic.

### 3.4 Preprocessor Stability

- Comment-aware line handling prevents false directives from commented text.
- Function-like macros are tracked separately to avoid incorrect object-style replacement.
- Macro expansion has growth guards to prevent runaway expansion and memory blow-up.
- Parser fallback path now consumes unexpected tokens to avoid infinite loops.

## 4. Timeout-Safe Test Strategy

Use timeout in both compile and run phases.

Example loop:

```bash
cd /root/code/.code/ccc
BIN=/root/code/.code/ccc/build/ccc

for t in tests/integration/std/*.c; do
  n=$(basename "$t" .c)
  out="build/${n}_app"

  timeout -k 2s 10s "$BIN" "$t" -o "$out" || echo "compile failed: $n"
  timeout -k 2s 8s "$out" || echo "run failed: $n"
done
```

Notes:

- `-k 2s` sends SIGKILL after SIGTERM grace period.
- Do not use `exit` in shared terminal sessions if you want the shell to remain open.

## 5. Current Std Integration Coverage

Directory:

- `tests/integration/std/`

Representative passing cases:

- `include_stdio_printf.c`
- `include_stdlib_abs.c`
- `include_string_strlen.c`
- `include_ctype_isdigit.c`
- `include_errno_macro.c`
- `include_stdarg_only.c`
- `include_stddef_null.c`
- `include_limits_macro.c` (expected non-zero app return by test design)
- `variadic_decl_call.c`

## 6. Known Boundaries

The project is still a subset C compiler. Remaining limitations include:

- Full function-like macro argument substitution is not complete.
- Many advanced preprocessor features are partial.
- Full C standard library semantics are not implemented; current focus is parse/compile/link usability for common patterns.

## 7. Recommended Next Work

1. Complete function-like macro expansion with argument parsing and replacement.
2. Add `#if` expression operator coverage (bitwise/shift/ternary if needed by system headers).
3. Add stress tests for nested include chains and macro recursion detection.
4. Add CI script for timeout-guarded std test matrix.
5. Optionally add strict mode to reject implicit undeclared function calls.

## 8. Quick Reproduction Commands

Build compiler:

```bash
cd /root/code/.code/ccc
cmake -S . -B build
cmake --build build -j
```

Run one std test:

```bash
timeout -k 2s 10s ./build/ccc tests/integration/std/include_stdio_printf.c -o build/include_stdio_printf_app
timeout -k 2s 8s ./build/include_stdio_printf_app
```

Run full std matrix:

```bash
for t in tests/integration/std/*.c; do
  n=$(basename "$t" .c)
  timeout -k 2s 10s ./build/ccc "$t" -o "build/${n}_app" || echo "compile fail: $n"
  timeout -k 2s 8s "./build/${n}_app" || echo "run fail: $n"
done
```
