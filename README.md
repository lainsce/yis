# Yis

> Vala was the war. Yis is the spoils.

This repository contains:

- `yis`: the Yis language compiler (C11)

## Requirements

- C compiler (`cc`) with C11 support
- Meson
- Ninja

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

Run the compiler regression tests (requires Python 3):

```sh
meson test -C build --print-errorlogs
```

## Optional Install

Install Yis and stdlib:

```sh
meson install -C build
```

## Useful Environment Variables

- `YIS_STDLIB`: override stdlib path
- `YIS_CACHE_DIR`: cache directory for compiled binaries
- `YIS_NO_CACHE=1`: disable binary cache
- `YIS_KEEP_C=1`: keep generated C files
- `YIS_CC_FLAGS`: extra C compiler flags
- `NO_COLOR=1`: disable colored compiler output
