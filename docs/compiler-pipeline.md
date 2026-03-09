# Compiler Pipeline

## Overview

The CLI entrypoint is `src/ccc.cc`, which orchestrates:

1. Parse arguments.
2. Compile each input C file into an object file.
3. Link objects into an executable (unless `--emit-obj`).

## Stage Details

### 1. CLI and Request Construction

- File: `src/ccc.cc`
- Builds a `CompileRequest` per input file.
- Forwards include directories (`-I`), macros (`-D`, `-U`), optimization level, output paths.

### 2. Preprocessing

- File: `src/Preprocess.cc`
- Entry: `PreprocessSource(...)`
- Responsibilities:
  - Read source and includes.
  - Handle directives and macro maps.
  - Resolve system/include paths.
  - Avoid hangs via guarded expansion strategy.

### 3. Lexing

- File: `src/Lexer.cc`
- Converts preprocessed text into tokens.
- Tracks source location for diagnostics.

### 4. Parsing

- File: `src/Parser.cc`
- Produces AST (`Program`, `FunctionDecl`, statements, expressions).
- Includes support for variadic parameter lists and cast expressions.

### 5. Semantic Analysis

- File: `src/SemanticAnalyzer.cc`
- Performs type checks, symbol checks, and call validation.
- Uses symbol table (`src/SymbolTable.cc`).

### 6. IR Generation and Optimization

- File: `src/IRGenerator.cc`
- Generates LLVM IR from AST.
- Applies optimization pipeline by level.
- Emits IR or object files.

### 7. Linking

- File: `src/Link.cc`
- Uses LLD API (`lldMain`) to link runtime objects and system libs.
- Resolves host-specific runtime startup files.

## Error Handling

- Files: `include/error.h`, `src/error.cc`
- Diagnostics collect and format file/line/column errors.
- Pipeline fails fast when a stage reports errors.
