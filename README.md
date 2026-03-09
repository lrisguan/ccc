# ccc

`ccc` is a toy compiler frontend project that parses a C language subset with a hand-written frontend and uses LLVM as backend for object code generation and optimization.

## Support matrix
- Function declarations (prototypes) and definitions.
- `extern` function declarations.
- Function definitions with `int` / `void` return type.
- Parameters of type `int`.
- Statements: block, variable declaration, expression statement, `if/else`, `while`, `return`.
- Expressions: integer literals, identifiers, function calls, assignment, unary (`-`, `!`), binary arithmetic/comparison/logical operators.
- End-to-end pipeline: source -> AST -> semantic analysis -> LLVM IR -> object file -> executable.
| | |

## Quick start

> [!Important]
> Requirements:
> - CMake >= 3.20
> - A C++ compiler
> - LLVM development package
> - LLD development libraries

> [!Note]
> You'd better use llvm and lld with version equals to 18.
> If you don't have that version, you can install it like this:
> ```bash
> # For Ubuntu 22.04, for other version of Ubuntu, you need to replace 
> # version name.
> wget -qO- https://apt.llvm.org/llvm-snapshot.gpg.key | sudo tee /etc/apt/trusted.gpg.d/llvm.asc
> echo "deb http://apt.llvm.org/jammy/ llvm-toolchain-jammy-18 main" | sudo tee /etc/apt/sources.list.d/llvm.list
> sudo apt-get update
> sudo apt-get install -y llvm-18 llvm-18-dev clang-18 lld-18 liblld-18-dev 
> ```

### Build steps:

```bash
git clone https://github.com/lrisguan/ccc.git
cmake -S . -B build
cmake --build build -j
```

### Usage

```bash
ccc=./build/ccc
$ccc examples/fib.c -o fib
./fib
```

- Compile and link multiple source files:

```bash
$ccc examples/multi/main.c examples/multi/math.c -o output
./output
```

- Use an external libc function through declaration:

```c
extern int putchar(int c);

int main() {
	return putchar(65) - 65;
}
```

- Emit LLVM IR:

```bash
./build/ccc examples/fib.c -o fib --emit-ir fib.ll
```

- Emit object file only:

```bash
./build/ccc examples/fib.c -o fib.o --emit-obj
```

> [!Note]
> `--emit-ir` and `--emit-obj` currently accept one input file at a time.

- Linking is performed in-process using the LLD C++ API (`lld::lldMain`) rather than invoking an external compiler driver.

- Optimization levels:

```bash
$ccc input.c -o output -O0
$ccc input.c -o output -O2
```

## Project structure

- `include/`: public interfaces for frontend, semantic checks, and LLVM generation.
- `src/`: implementations.
- `examples/`: small sample programs.
- `tests/`: future test suites.

## Documents

- `docs/project-overview.md`: project goals, capabilities, layout, and limitations.
- `docs/compiler-pipeline.md`: end-to-end compile pipeline and module responsibilities.
- `docs/cli-and-usage.md`: build, command-line options, and common workflows.
- `docs/testing-and-debugging.md`: test organization, timeout-safe test strategy, and debugging tips.
- `docs/standard-library-and-variadic-support.md`: detailed notes on standard headers and variadic support.

## Notes

This is intentionally a subset compiler, not a full ISO C compiler. The design emphasizes clear architecture and incremental extensibility.
