# Testing And Debugging

## Test Layout

- `tests/integration/return/`: simple return-path checks.
- `tests/integration/multi_file/`: multi-file compile/link checks.
- `tests/integration/type/`: type and pointer coverage.
- `tests/integration/std/`: standard header and variadic scenarios.
- `tests/invalid/`: expected semantic/compile failures.

## Timeout-Safe Testing

Use `timeout` to avoid terminal hangs:

```bash
cd /root/code/.code/ccc
BIN=./build/ccc

for t in tests/integration/std/*.c; do
  n=$(basename "$t" .c)
  out="build/${n}_app"

  timeout -k 2s 10s "$BIN" "$t" -o "$out" || echo "compile fail: $n"
  timeout -k 2s 8s "$out" || echo "run fail: $n"
done
```

Notes:

- Do not use `exit` in shared shell loops if you want to keep the terminal session.
- Some tests intentionally use non-zero return values as expected outputs.

## Investigating Hangs

When a case times out:

1. Re-run with a small timeout and capture logs.
2. Use `strace` with timeout to inspect behavior.
3. Check for parser non-advancing loops and macro expansion blowups.

Example:

```bash
timeout -k 2s 8s strace -f -o /tmp/ccc.trace ./build/ccc test.c -o test_app
```

## Regression Strategy

When fixing a bug:

1. Add a focused integration test reproducing the issue.
2. Add timeout-guarded run in your validation loop.
3. Confirm no regressions in existing `std` and `type` tests.
