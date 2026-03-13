#include "diag.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// ANSI color codes
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_GRAY    "\033[90m"

// Check if stderr is a terminal (for color support) — cached
static bool use_color(void) {
    static int cached = -1;
    if (cached < 0) {
        const char *no_color = getenv("NO_COLOR");
        if (no_color && no_color[0]) { cached = 0; }
        else if (!isatty(fileno(stderr))) { cached = 0; }
        else { const char *term = getenv("TERM"); cached = (term && term[0]) ? 1 : 0; }
    }
    return cached != 0;
}

// Print with optional color
static void print_colored(FILE *out, const char *color, const char *fmt, ...) {
    bool color_enabled = use_color();
    
    if (color_enabled && color) {
        fprintf(out, "%s", color);
    }
    
    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    
    if (color_enabled && color) {
        fprintf(out, "%s", COLOR_RESET);
    }
}

// Read a line from source file at given line number
// Print a code snippet with line numbers and error highlighting
// Reads the file once and extracts all needed lines
static void print_code_snippet(const char *path, int line, int col, int context_lines) {
    if (!path || line <= 0) {
        return;
    }

    FILE *f = fopen(path, "r");
    if (!f) return;

    int first_line = line - context_lines;
    if (first_line < 1) first_line = 1;
    int last_line = line + context_lines;

    // Skip to first_line
    char *buf = NULL;
    size_t buf_cap = 0;
    int current = 1;
    while (current < first_line) {
        if (getline(&buf, &buf_cap, f) == -1) { free(buf); fclose(f); return; }
        current++;
    }

    // Read and display lines from first_line to last_line
    for (int i = first_line; i <= last_line; i++) {
        if (getline(&buf, &buf_cap, f) == -1) break;

        // Strip trailing newline
        size_t len = strlen(buf);
        if (len > 0 && buf[len - 1] == '\n') buf[--len] = '\0';

        if (i == line) {
            print_colored(stderr, COLOR_BOLD COLOR_CYAN, "%4d | ", i);
        } else {
            print_colored(stderr, COLOR_GRAY, "%4d | ", i);
        }
        fprintf(stderr, "%s\n", buf);

        if (i == line && col > 0) {
            print_colored(stderr, COLOR_GRAY, "     | ");
            int visual_col = 0;
            for (int j = 0; j < col - 1 && buf[j]; j++) {
                if (buf[j] == '\t') visual_col += 4 - (visual_col % 4);
                else visual_col++;
            }
            for (int j = 0; j < visual_col; j++) fprintf(stderr, " ");
            print_colored(stderr, COLOR_BOLD COLOR_RED, "^");

            int token_len = 1;
            for (int j = col - 1; buf[j] && !isspace((unsigned char)buf[j]); j++) token_len++;
            if (token_len > 8) token_len = 8;
            for (int j = 1; j < token_len; j++) print_colored(stderr, COLOR_RED, "~");
            fprintf(stderr, "\n");
        }
    }

    free(buf);
    fclose(f);
}

// Get a helpful tip based on error message content
static const char *get_error_tip(const char *msg) {
    if (!msg) return NULL;
    
    // Lexer errors
    if (strstr(msg, "unexpected character")) {
        // Check if the error message includes the actual character
        if (strstr(msg, "0x")) {
            return "Non-printable characters are not allowed. Check for encoding issues or stray bytes.";
        }
        return "Remove this invalid character from your source file.";
    }
    if (strstr(msg, "unterminated string")) {
        return "Make sure all string literals are closed with a matching quote.";
    }
    if (strstr(msg, "bad \\\\u{...} escape")) {
        return "Unicode escapes must be valid hex values within braces, e.g., \\\\u{41} for 'A'.";
    }
    if (strstr(msg, "bad \\\\xHH escape")) {
        return "Hex escapes must be exactly two hex digits, e.g., \\\\x41 for 'A'.";
    }
    if (strstr(msg, "bad \\\\ooo escape")) {
        return "Octal escapes use up to 3 digits (0-7), e.g., \\\\101 for 'A'.";
    }
    if (strstr(msg, "unknown escape")) {
        return "Valid escapes are: \\a, \\b, \\e, \\f, \\n, \\r, \\t, \\v, \\\\, \\\", \\', \\<, \\>, \\$, \\?, \\xHH, \\ooo, and \\u{...}.";
    }
    
    // Parser errors
    if (strstr(msg, "expected") && strstr(msg, "got")) {
        if (strstr(msg, "RPAR") && strstr(msg, "=>")) {
            return "Check for mismatched parentheses or lambda syntax. Lambdas use (param = Type) => expr syntax.";
        }
        if (strstr(msg, "SEMI")) {
            return "You may be missing a semicolon or newline between statements.";
        }
        if (strstr(msg, "RPAR")) {
            return "Check for mismatched parentheses - you may have an extra opening '(' or missing ')'.";
        }
        if (strstr(msg, "RBRACE")) {
            return "Check for mismatched braces - you may have an extra opening '{' or missing '}'.";
        }
        return "Check for syntax errors like missing punctuation or incorrect keywords.";
    }
    if (strstr(msg, "unexpected token") && strstr(msg, "in expression")) {
        return "This expression is not valid here. Check the syntax of your expression.";
    }
    if (strstr(msg, "unexpected token") && strstr(msg, "in pattern")) {
        return "This pattern is not valid in a match expression. Use literals, identifiers, or _.";
    }
    if (strstr(msg, "unexpected token")) {
        return "This token doesn't belong here. Check the surrounding syntax.";
    }
    
    // Type errors
    if (strstr(msg, "type mismatch")) {
        return "The types on both sides of this operation don't match. Check your variable types.";
    }
    if (strstr(msg, "unknown type")) {
        if (strstr(msg, "use num")) {
            return "Yis uses 'num' for all numeric types instead of 'int' or 'float'.";
        }
        return "This type name is not recognized. Check for typos, missing imports, or explicit generic names like T.";
    }
    if (strstr(msg, "unknown name")) {
        return "This identifier is not defined. Check for typos or missing variable declarations.";
    }
    if (strstr(msg, "unknown function")) {
        return "This function is not defined. Check for typos or missing imports.";
    }
    if (strstr(msg, "cannot assign to const")) {
        return "Constants cannot be modified after declaration. Use 'let ?name = ...' for mutable variables.";
    }
    if (strstr(msg, "cannot assign to immutable")) {
        return "This variable was declared without '?' so it's immutable. Use 'let ?name = ...' for mutability.";
    }
    if (strstr(msg, "call on nullable value")) {
        return "This value might be null. Use 'if x != null { ... }' to check before calling methods.";
    }
    if (strstr(msg, "member access on nullable value")) {
        return "This value might be null. Use 'if x != null { ... }' to check before accessing members.";
    }
    if (strstr(msg, "indexing nullable value")) {
        return "This value might be null. Use 'if x != null { ... }' to check before indexing.";
    }
    if (strstr(msg, "numeric op on nullable value")) {
        return "Cannot perform arithmetic on nullable values. Check for null first.";
    }
    if (strstr(msg, "comparison on nullable value")) {
        return "Cannot compare nullable values. Check for null first.";
    }
    if (strstr(msg, "logical op on nullable value")) {
        return "Logical operators require boolean values, not nullable ones.";
    }
    if (strstr(msg, "tuple arity mismatch")) {
        return "Tuples must have the same number of elements on both sides.";
    }
    if (strstr(msg, "fn arity mismatch")) {
        return "Function call has wrong number of arguments. Check the function signature.";
    }
    if (strstr(msg, "expects") && strstr(msg, "args")) {
        return "The number of arguments doesn't match the function definition.";
    }
    if (strstr(msg, "global") && strstr(msg, "used before definition")) {
        return "Global variables must be defined before they are used. Move the definition earlier.";
    }
    if (strstr(msg, "duplicate")) {
        return "This name is already defined. Use a different name or remove the duplicate.";
    }
    if (strstr(msg, "missing required `bring stdr;`")) {
        return "Add 'bring stdr;' at the top of your file to import the standard library.";
    }
    if (strstr(msg, "entry() is only allowed in init.yi")) {
        return "The entry() function can only be defined in the main init.yi file.";
    }
    if (strstr(msg, "init.yi must contain exactly one entry()")) {
        return "Your main file must have exactly one entry() function as the program starting point.";
    }
    if (strstr(msg, "method") && strstr(msg, "must be called")) {
        return "Methods must be called with parentheses, e.g., obj.method() not obj.method.";
    }
    if (strstr(msg, "cask function") && strstr(msg, "must be called")) {
        return "Cast functions must be called with parentheses, e.g., cst.func() not cst.func.";
    }
    if (strstr(msg, "cannot access field") && strstr(msg, "lock class")) {
        return "Fields of 'lock' classes can only be accessed within the same file or class methods.";
    }
    if (strstr(msg, "method") && strstr(msg, "requires mutable receiver")) {
        return "This method modifies the object, so the receiver must be mutable: '?obj.method()'.";
    }
    if (strstr(msg, "array.add requires mutable binding")) {
        return "The array variable must be declared as mutable: 'let ?arr = ...'.";
    }
    if (strstr(msg, "array.remove requires mutable binding")) {
        return "The array variable must be declared as mutable: 'let ?arr = ...'.";
    }
    if (strstr(msg, "cannot mutate through immutable binding")) {
        return "To modify this value, the base variable must be declared with '?': 'let ?x = ...'.";
    }
    if (strstr(msg, "shadows cask")) {
        return "This local variable has the same name as a cask. Rename the variable to avoid confusion.";
    }
    if (strstr(msg, "out of memory")) {
        return "The compiler ran out of memory. Try simplifying your code or closing other programs.";
    }
    if (strstr(msg, "failed to resolve")) {
        return "Check that the file path exists and is accessible.";
    }
    if (strstr(msg, "'.e' files are no longer supported")) {
        return "Rename your file from .e to .yi extension.";
    }
    if (strstr(msg, "bring expects a stdlib module")) {
        return "Use 'bring stdr;', 'bring math;', a valid external module name, or a valid .yi file path.";
    }
    if (strstr(msg, "stdr.yi not found")) {
        return "The standard library is not installed. Set YIS_STDLIB to the stdlib directory.";
    }
    if (strstr(msg, "external module") && strstr(msg, "not found")) {
        return "The external module was not found. Install it and ensure its .yi file is in the stdlib path, or set YIS_<NAME>_PATH.";
    }
    if (strstr(msg, "missing entry() in init.yi")) {
        return "Your main file needs an entry() function: 'entry() { ... }'.";
    }
    if (strstr(msg, "free function") && strstr(msg, "cannot take this")) {
        return "Only class methods can have 'this' as a parameter. Remove 'this' from this function.";
    }
    if (strstr(msg, "method") && strstr(msg, "must begin with this")) {
        return "Class methods must have 'this' or '?this' as their first parameter.";
    }
    if (strstr(msg, "only first param may be this")) {
        return "'this' can only be used as the first parameter of a method.";
    }
    if (strstr(msg, "lambda params cannot be this")) {
        return "Lambda functions cannot have 'this' as a parameter.";
    }
    if (strstr(msg, "cannot infer type of empty array")) {
        return "Empty arrays need a type annotation. Use '[]: [num]' or provide at least one element.";
    }
    if (strstr(msg, "cask declaration") && strstr(msg, "must match file name")) {
        return "Use 'cask <name>' only when it matches the .yi file basename.";
    }
    if (strstr(msg, "foreach expects array or string")) {
        return "for (x in y) requires 'y' to be an array or string. Check the type of your iterable.";
    }
    if (strstr(msg, "match requires at least one arm")) {
        return "Add at least one pattern arm to your match expression: 'pattern => expression'.";
    }
    if (strstr(msg, "unsupported match pattern")) {
        return "Match patterns can be: integers, strings, booleans, null, identifiers, or _ (wildcard).";
    }
    if (strstr(msg, "ternary condition cannot be void")) {
        return "The condition in 'cond ? a : b' must return a value, not void.";
    }
    if (strstr(msg, "if condition cannot be void")) {
        return "The condition in 'if' must return a value, not void.";
    }
    if (strstr(msg, "for condition cannot be void")) {
        return "The condition in 'for' must return a value, not void.";
    }
    if (strstr(msg, "return value in void function")) {
        return "This function doesn't return a value, but you're trying to return something.";
    }
    if (strstr(msg, "missing return value")) {
        return "This function expects a return value. Add an expression after 'return'.";
    }
    if (strstr(msg, "const expression must be a literal")) {
        return "Constants can only be simple literals or basic numeric expressions.";
    }
    if (strstr(msg, "const string cannot interpolate")) {
        return "String constants cannot contain $variable interpolation.";
    }
    if (strstr(msg, "tuple index out of range")) {
        return "The index is too large or negative for this tuple's size.";
    }
    if (strstr(msg, "tuple index must be integer literal")) {
        return "Use a literal number like 'tuple.0' or 'tuple.1', not a variable.";
    }
    if (strstr(msg, "indexing requires array or string")) {
        return "You can only use [index] on arrays and strings.";
    }
    if (strstr(msg, "member access on non-object")) {
        return "The '.' operator can only be used on class instances or casks.";
    }
    if (strstr(msg, "unknown member")) {
        return "This field or method doesn't exist on the class. Check for typos.";
    }
    if (strstr(msg, "unknown cask member")) {
        return "This name doesn't exist in the cask. Check for typos or missing exports.";
    }
    if (strstr(msg, "unknown class")) {
        return "This class is not defined. Check for typos or missing imports.";
    }
    if (strstr(msg, "class has no init method")) {
        return "This class doesn't have an 'init' method, so use 'new ClassName()' without arguments.";
    }
    if (strstr(msg, "init must return void")) {
        return "The 'init' method should not return a value (it implicitly returns the new instance).";
    }
    if (strstr(msg, "unsupported call form")) {
        return "This expression cannot be called as a function. Check that you're calling a function value.";
    }
    if (strstr(msg, "unknown name") && strstr(msg, "cask not in scope")) {
        return "This cask is not imported. Add 'bring name;' at the top of your file.";
    }
    if (strstr(msg, "C compiler failed")) {
        return "The C compiler encountered an error. Check the generated C code or your C compiler setup.";
    }
    if (strstr(msg, "module") && strstr(msg, "linker")) {
        return "Module library linking failed. Ensure the library is installed or set YIS_<NAME>_FLAGS with the correct linker path.";
    }
    if (strstr(msg, "compile command too long")) {
        return "The compilation command exceeded the buffer size. Try moving files to a shorter path.";
    }
    if (strstr(msg, "--emit-c is not supported")) {
        return "The C backend doesn't support --emit-c. Use the default compilation instead.";
    }
    if (strstr(msg, "unknown option")) {
        return "Use 'yis --help' to see available options.";
    }
    if (strstr(msg, "run needs a source path")) {
        return "Usage: yis run <file.yi>";
    }
    if (strstr(msg, "multiple source paths provided")) {
        return "Provide only one source file. Use 'yis run file.yi'.";
    }
    if (strstr(msg, "unexpected extra arguments")) {
        return "Too many arguments provided. Use 'yis <file.yi>' or 'yis run <file.yi>'.";
    }
    
    return NULL;
}

void diag_print_enhanced(const Diag *d, bool verbose) {
    if (!d) {
        return;
    }
    
    const char *msg = d->message ? d->message : "unknown error";
    bool has_valid_path = d->path && d->path[0] != '\0';
    bool has_location = d->line > 0 || d->col > 0;
    
    // Print error header with color
    print_colored(stderr, COLOR_BOLD COLOR_RED, "error: ");
    print_colored(stderr, COLOR_BOLD, "%s\n", msg);
    
    // Print location
    if (has_valid_path && has_location) {
        print_colored(stderr, COLOR_GRAY, "  --> ");
        fprintf(stderr, "%s:%d:%d\n", d->path, d->line, d->col);
    } else if (has_valid_path) {
        print_colored(stderr, COLOR_GRAY, "  --> ");
        fprintf(stderr, "%s\n", d->path);
    }
    
    // Print code snippet if we have location info
    if (has_valid_path && d->line > 0) {
        fprintf(stderr, "\n");
        int context = verbose ? 2 : 1;
        print_code_snippet(d->path, d->line, d->col, context);
        fprintf(stderr, "\n");
    }
    
    // Print helpful tip
    const char *tip = get_error_tip(msg);
    if (tip) {
        print_colored(stderr, COLOR_BOLD COLOR_YELLOW, "help: ");
        fprintf(stderr, "%s\n", tip);
    }
}

// Legacy print function for backward compatibility
void diag_print(const Diag *d) {
    diag_print_enhanced(d, false);
}

// Print a simple error without location info
void diag_print_simple(const char *msg) {
    if (!msg) return;
    print_colored(stderr, COLOR_BOLD COLOR_RED, "error: ");
    fprintf(stderr, "%s\n", msg);
    
    const char *tip = get_error_tip(msg);
    if (tip) {
        print_colored(stderr, COLOR_BOLD COLOR_YELLOW, "help: ");
        fprintf(stderr, "%s\n", tip);
    }
}

// Print a warning
void diag_print_warning(const char *path, int line, int col, const char *msg) {
    if (!msg) return;
    
    if (path && path[0] && line > 0) {
        print_colored(stderr, COLOR_BOLD COLOR_YELLOW, "warning: ");
        fprintf(stderr, "%s\n", msg);
        print_colored(stderr, COLOR_GRAY, "  --> ");
        fprintf(stderr, "%s:%d:%d\n", path, line, col);
    } else {
        print_colored(stderr, COLOR_BOLD COLOR_YELLOW, "warning: ");
        fprintf(stderr, "%s\n", msg);
    }
}

// Print an info/note message
void diag_print_note(const char *msg) {
    if (!msg) return;
    print_colored(stderr, COLOR_BOLD COLOR_BLUE, "note: ");
    fprintf(stderr, "%s\n", msg);
}
