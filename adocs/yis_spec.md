# Yis Language Reference

This reference tracks what the current compiler accepts in this repository.
It focuses on working syntax and conventions used in real Yis code.

## 1. Status

- This document is implementation-facing.
- It describes lexer/parser/codegen behavior that compiles today.
- It prefers current style conventions over older historical forms.

## 2. Current Syntax Conventions

Yis uses symbol-first declarations.

```yis
cask hello
bring stdr

: greet(name = string) (( -- ))
  writef("Hello $$name$$\n")
;

-> ()
  greet("World")
;
```

Quick map:

- Function declaration: `: name(...) (( ret ))` and `:: name(...) (( ret ))`
- Entry declaration: `-> ()`
- Return statement: `<- value`
- Macro declaration: `!: name(...) (( ret ))`
- Class declaration: `,: Name ... ;`
- Struct declaration: `=: Name ... ;`
- Enum declaration: `|: Name ... ;`
- Interface declaration: `.: Name(...) (( ret ))`

## 3. Lexical Structure

### 3.1 Identifiers

- Start with letter or `_`
- Continue with letter, digit, or `_`
- Case-sensitive

### 3.2 Keyword Tokens

Current lexer keywords:

- `let`, `if`, `else`, `elif`
- `for`, `in`, `match`
- `bring`, `cask`, `class`, `pub`
- `def`, `const`
- `true`, `false`, `null`
- `break`, `continue`
- `new` (tokenized)

Notes:

- Declaration forms like function, entry, macro, class/struct/enum/interface use punctuation tokens (`:`, `::`, `->`, `!:`, `,:`, `=:`, `|:`, `.:`) instead of keyword starters.

### 3.3 Comments

- Line comment: `-- ...`
- Block comment: `-| ... |-`
- Block comments are nestable.

### 3.4 Statement Terminators

- Explicit `;` is supported.
- Auto semicolon insertion happens on line breaks in statement-ending contexts.
- Function and type bodies commonly use semicolon-terminated block style.

## 4. Strings and Literals

### 4.1 Literals

- Integers: `42`
- Bare hex integers: exact `xx` or `xxxx` forms like `7f`, `00ff`, `0070`, `FF00`
  - Accepted conservatively: the token must be exactly 2 or 4 hex digits, must include at least one decimal digit, and must either start with a decimal digit or use uppercase `A-F` if it starts with a letter
  - This keeps ordinary decimals like `42` and `1234` decimal, and keeps lowercase identifier-like names such as `ff` and `b0` from being reinterpreted as numbers
- Floats: `3.14`
- Booleans: `true`, `false`
- Null: `null`

### 4.2 Strings

- Strings use double quotes.
- Interpolation uses `$$ ... $$` inside string literals.

```yis
writef("value=$$x + 1$$\n")
```

- Interpolation bodies are parsed as expressions, not just simple paths.
- Supported escapes include: `\n`, `\t`, `\r`, `\\`, `\"`, `\'`, `\<`, `\>`, `\$`, `\?`, octal escapes, `\xNN`, and `\u{...}`.

## 5. Modules and Imports

### 5.1 `cask`

- `cask name` is optional.
- It sets module identity used for symbol mangling.

### 5.2 `bring`

- Use `bring module_name`.
- Dotted brings are supported: `bring foo.bar.baz`.
- Resolver maps dotted names to file paths and appends `.yi`.
- Common built-in modules include `stdr` and `math`.

## 6. Declarations

### 6.1 Variables

- Local immutable: `let x = expr`
- Local mutable: `let ?x = expr`
- Local constant: `const x = expr`
- Module immutable: `def x = expr`
- Module mutable: `def ?x = expr`
- Public module globals: `pub def x = expr`

### 6.2 Functions and Entry

- Private function: `: name(params) (( ret )) ... ;`
- Public function: `:: name(params) (( ret )) ... ;`
- Entry point: `-> (params) (( ret )) ... ;` (typically `-> ()`)
- Bare `name(params) ...` form is parsed but not preferred as style.

Return statements use `<-`.

```yis
: add(a = num, b = num) (( num ))
  <- a + b
;
```

### 6.3 Macros

Macros use `!:` declarations.

```yis
!: mod(divisor = num) (( bool ))
  this % divisor == 0
;
```

Receiver-style call syntax:

```yis
i !mod 3
```

Implementation note: macros are emitted as helper functions with receiver `this` in current codegen.

### 6.4 Classes, Structs, Enums, Interfaces

Primary style:

- Class: `,: Name ... ;`
- Struct: `=: Name ... ;`
- Enum: `|: Name ... ;`
- Interface: `.: Name(...) (( ret )) ...`

Methods inside nominal/interface bodies use `:` or `::`.

```yis
,: Point
  pub x = num
  pub y = num

  :: to_string(this) (( string ))
    <- "($$this.x$$, $$this.y$$)"
  ;
;
```

Additional behavior:

- `class Name { ... }` remains parsed.
- Optional base/interface name can follow `: BaseName`.
- Class fields use `[pub] field = Type`.
- Class and top-level `exit` style blocks are supported via `<- () ... ;`.

## 7. Types

### 7.1 Core Types

- `num`, `bool`, `string`, `any`, `null`

### 7.2 Type Annotations

- Parameter annotation: `name = Type`
- Return annotation: `(( Type ))`
- Void return: `(( -- ))`
- Function/delegate marker: `(( -> ))`
- Arrays: `[T]`
- Dict annotation shape: `[string => T]`
- Empty typed literals are common: `[]: [T]`, `[]: [string => any]`

Implementation note: parser/type tracking still treats several complex annotation forms conservatively as `any`.

## 8. Control Flow

### 8.1 If / Elif / Else

Supported:

- Indentation style in semicolon blocks
- Braced blocks
- `elif` chains

```yis
if cond
  write("yes\n")
elif other
  write("maybe\n")
else
  write("no\n")
```

### 8.2 For

Supported forms:

- C-style: `for (init; cond; step)`
- Foreach: `for (item in expr)`

### 8.3 Loop Control

- `break`
- `continue`

## 9. Expressions

### 9.1 Core Forms

- Literals and identifiers
- Unary: `!`, unary `-`, `#`
- Binary: `+ - * / % == != < <= > >= && || ??`
- Assignment: `=`, `+=`, `-=`, `*=`, `/=`, `%=`
- Call: `fn(args...)`
- Member: `a.b`
- Index: `a[i]`
- Bang-call: `recv !name args...`
- Lambda: `(x = num) => x + 1`, `(x) => { ... }`
- Match expression: brace-arm and inline forms
- If expression: `if cond { expr } else { expr }`

### 9.2 Match Patterns

Current parser patterns:

- `_`
- identifier bind
- int literal
- string literal
- bool literal
- `null`

### 9.3 Operator Precedence

Lowest to highest:

1. Assignment (`=`, `+=`, `-=`, `*=`, `/=`, `%=`), right-associative
2. `??`
3. `||`
4. `&&`
5. `==`, `!=`
6. `<`, `<=`, `>`, `>=`
7. `+`, `-`
8. `*`, `/`, `%`
9. Unary (`!`, `-`, `#`)
10. Postfix (call, member, index, bang-call)

## 10. Runtime and Semantics Notes

- Conditions use truthiness at runtime.
- `&&` and `||` short-circuit.
- Array indexing can yield `null` when out of bounds.
- String indexing yields an empty string when out of bounds.
- Entry function generation uses `->`; if absent, program main still builds but does not call `yis_entry`.

## 11. Standard Library Snapshot

Common `stdr` functions used by current code:

- `write`, `writef`, `readf`
- `len`, `str`, `num`, `is_null`
- `read_text_file`, `write_text_file`
- `run_command`
- `exit_process(code)`: terminate with a process exit code
- `file_mtime(path)`: Unix timestamp in seconds, or `-1` if unavailable
- `file_mtime_ns(path)`: Unix timestamp in nanoseconds, or `-1` if unavailable
- `open_file_dialog`, `save_file_dialog`, `open_folder_dialog`

`run_command` returns the command's exit code and captured stdout. On POSIX,
a command terminated by a signal returns `128 + signal number`.

Common `math` functions:

- `sin`, `cos`, `tan`, `sqrt`, `abs`, `min`, `max`

## 12. Compatibility Notes

The following older descriptions are not current conventions in this repository:

- Keyword-first declaration style (`fun`, `macro`, `entry`, `return`) as primary syntax
- Angle-bracket string interpolation (`<expr>`) style
- `lock` and `seal` modifiers
- `move(x)` language feature
- `-- @include` pre-lex include directive

Use the symbol-first forms in this document for new code.
