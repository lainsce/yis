# Yis

> Vala was the war. Yis is the spoils.

This repository contains:

- `yis`: the Yis language compiler, which generates C

## Requirements

- GCC or Clang with C11 support and GNU extensions (statement expressions,
  cleanup attributes, and checked arithmetic builtins)
- Meson
- Ninja
- Python 3 for the test suites and benchmarks

Example (macOS/Homebrew):

```sh
brew install meson ninja
```

## Build Yis

```sh
meson setup build
meson compile -C build
```

Run the compiler from the build tree:

```sh
./build/yis --help
./build/yis examples/hello.yi
./build/yis run examples/hello.yi
```

Compile mode writes an executable in the current directory. `run` compiles into
the binary cache and executes with the source directory as its working directory.
Pass program arguments after `--`:

```sh
./build/yis run examples/hello.yi -- "argument with spaces"
```

## Validation

Run the bootstrap, compiler, parser, optimizer, ownership, and native runtime regressions:

```sh
meson test -C build --print-errorlogs
```

Linux tests include AddressSanitizer and UndefinedBehaviorSanitizer checks for
the native runtime. To run that harness separately:

```sh
python3 tests/test_runtime.py --sanitizers
```

Measure cold runs, cached runs, uncached compilation, and standalone runtime on
bounded integer, array, and string workloads:

```sh
python3 tests/benchmark_compiler.py build/yis . --output /tmp/yis-benchmark.json
```

The JSON report includes timings, peak child-process RSS, and retained workload
files. RSS can include the measurement worker's inherited memory at process
startup. Compare repeated runs on the same machine; these workloads provide
measurements without guaranteeing performance for other programs.

CI is configured for Ubuntu with GCC and Clang, and macOS with Clang. Validate
the release commit's jobs before claiming support for those platforms. Windows
has no CI coverage.

## Optional Install

Install Yis and stdlib:

```sh
meson install -C build
```

## Useful Environment Variables

| Variable | Behavior |
| --- | --- |
| `CC` | C compiler executable or command with arguments; defaults to `cc`. |
| `YIS_CC_FLAGS` | Extra compiler/linker arguments appended after the defaults, such as `-O2 -g`. |
| `YIS_STDLIB` | Override the standard library directory. |
| `YIS_CACHE_DIR` | Override the `run` binary cache directory. |
| `YIS_NO_CACHE=1` | Force recompilation on `run`; compile mode always rebuilds. |
| `YIS_KEEP_C=1` | Keep generated `.yis-c-*` files in the output directory and force recompilation. |
| `NO_COLOR=1` | Disable colored bootstrap diagnostics. |

The cache defaults to `$XDG_CACHE_HOME/yis`, or `~/.cache/yis`, with `.yi-cache`
as a fallback. Changes to source, transitive imports, runtime, compiler, or flags
invalidate cached runs. Native-module programs and commands with untracked
native inputs rebuild each time.

`CC` and `YIS_CC_FLAGS` accept quoted arguments and backslash escapes. Compile
mode defaults to `-Os`; `run` defaults to `-O1`, which discards unused runtime
helpers while keeping compilation fast.
Override the optimization level when measuring runtime performance:

```sh
CC=clang YIS_CC_FLAGS="-O2" ./build/yis run examples/fizzbuzz.yi
```

## Runtime behavior and limits

Integer arithmetic uses signed 64-bit values. Addition, subtraction,
multiplication, and negation abort with `runtime error: integer overflow` on
overflow. Integer division and remainder reject `INT64_MIN / -1` and
`INT64_MIN % -1` with the same error. A zero divisor aborts with `runtime error:
division by zero`. Floating-point arithmetic follows the C floating-point
behavior.
Integer literals must fit the signed 64-bit range. Float-to-integer conversion
rejects nonfinite and out-of-range values; valid values truncate toward zero.

- Numeric values use tagged runtime representations; tight numeric loops can
  be substantially slower than equivalent primitive C loops.
- Heap values use reference counting. Cycles, such as an array containing
  itself, need to be broken explicitly to release their memory.
- Dictionary lookup and insertion scan entries linearly, so large dictionaries
  can be expensive.
- Repeated immutable string concatenation copies accumulated prefixes. Collect
  pieces and use `stdr.join` when building large strings.
