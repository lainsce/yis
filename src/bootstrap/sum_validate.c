#include "sum_validate.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef enum {
    SUM_MODE_OFF = 0,
    SUM_MODE_WARN,
    SUM_MODE_STRICT
} SumMode;

typedef enum {
    DIAG_WARN = 0,
    DIAG_ERROR
} DiagLevel;

typedef struct {
    char *text;
    char *file;
    int line;
} SourceLine;

typedef struct {
    SourceLine *items;
    size_t len;
    size_t cap;
} SourceLineVec;

typedef struct {
    char **items;
    size_t len;
    size_t cap;
} StrVec;

typedef struct {
    int errors;
    int warnings;
} DiagCounts;

typedef struct {
    int indent;
    bool is_when;
} BlockFrame;

typedef struct {
    BlockFrame *items;
    size_t len;
    size_t cap;
} BlockStack;

typedef struct {
    bool have;
    int indent;
} RuleContext;

static const char *known_types[] = {
    "appbar", "bottom-nav", "button", "checkbox", "chip", "colorpicker", "datepicker",
    "dialog", "dialogslot", "divider", "dropdown", "fab", "fixed", "grid", "hstack",
    "iconbtn", "image", "label", "list", "menu", "nav-rail", "popover", "progress",
    "scroller", "searchfield", "buttongroup", "slider", "stepper", "switch", "tabs",
    "textfield", "textview", "toast", "toasts", "toolbar", "tooltip", "treeview", "viewswitcher",
    "bottom_nav", "dialog-slot", "dialog_slot"
};

static const char *known_states[] = {
    "hover", "active", "checked", "disabled", "selection"
};

static const char *known_properties[] = {
    "background", "background-color", "color", "text-color", "opacity", "border", "border-color",
    "border-width", "border-radius", "radius", "box-shadow", "elevation", "font", "font-family",
    "font-size", "font-weight", "font-variant-numeric", "letter-spacing", "padding", "padding-left",
    "padding-top", "padding-right", "padding-bottom", "margin", "margin-left", "margin-top",
    "margin-right", "margin-bottom", "min-width", "min-height", "max-width", "max-height",
    "selection-color", "selection-background", "highlight-color", "transition", "transition-duration",
    "transition-easing", "transition-timing-function", "icon-size", "icon-color", "icon-tint",
    "knob-width", "knob-w",
    "knob-height", "knob-h", "check-color", "check", "item-padding", "menu-item-padding",
    "item-height", "menu-item-height", "appbar-btn-size", "appbar-btn-gap", "appbar-btn-top",
    "appbar-btn-right", "appbar-btn-close-color", "appbar-btn-min-color", "appbar-btn-max-color",
    "appbar-btn-border-color", "appbar-btn-border-width"
};

static const char *known_functions[] = { "alpha", "mix", "rgb", "rgba" };
static StrVec injected_types = {0};
static StrVec injected_states = {0};
static StrVec injected_properties = {0};
static StrVec injected_functions = {0};

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s);
    char *out = (char *)malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s, n + 1);
    return out;
}

static bool strvec_push(StrVec *v, const char *s) {
    if (!v || !s) return false;
    if (v->len == v->cap) {
        size_t next = v->cap ? v->cap * 2 : 8;
        char **items = (char **)realloc(v->items, next * sizeof(char *));
        if (!items) return false;
        v->items = items;
        v->cap = next;
    }
    v->items[v->len] = dupstr(s);
    if (!v->items[v->len]) return false;
    v->len++;
    return true;
}

static void strvec_free(StrVec *v) {
    if (!v) return;
    for (size_t i = 0; i < v->len; i++) free(v->items[i]);
    free(v->items);
    v->items = NULL;
    v->len = 0;
    v->cap = 0;
}

static bool source_line_vec_push(SourceLineVec *v, const char *text, const char *file, int line) {
    if (!v || !text || !file || line < 1) return false;
    if (v->len == v->cap) {
        size_t next = v->cap ? v->cap * 2 : 128;
        SourceLine *items = (SourceLine *)realloc(v->items, next * sizeof(SourceLine));
        if (!items) return false;
        v->items = items;
        v->cap = next;
    }
    SourceLine *dst = &v->items[v->len++];
    dst->text = dupstr(text);
    dst->file = dupstr(file);
    dst->line = line;
    if (!dst->text || !dst->file) return false;
    return true;
}

static void source_line_vec_free(SourceLineVec *v) {
    if (!v) return;
    for (size_t i = 0; i < v->len; i++) {
        free(v->items[i].text);
        free(v->items[i].file);
    }
    free(v->items);
    v->items = NULL;
    v->len = 0;
    v->cap = 0;
}

static bool block_stack_push(BlockStack *s, int indent, bool is_when) {
    if (!s) return false;
    if (s->len == s->cap) {
        size_t next = s->cap ? s->cap * 2 : 8;
        BlockFrame *items = (BlockFrame *)realloc(s->items, next * sizeof(BlockFrame));
        if (!items) return false;
        s->items = items;
        s->cap = next;
    }
    s->items[s->len].indent = indent;
    s->items[s->len].is_when = is_when;
    s->len++;
    return true;
}

static void block_stack_pop_to_indent(BlockStack *s, int indent) {
    if (!s) return;
    while (s->len > 0) {
        BlockFrame *top = &s->items[s->len - 1];
        if (indent > top->indent) break;
        s->len--;
    }
}

static bool path_is_absolute(const char *path) {
    if (!path || !path[0]) return false;
#if defined(_WIN32)
    if ((isalpha((unsigned char)path[0]) && path[1] == ':') || path[0] == '\\' || path[0] == '/') return true;
#endif
    return path[0] == '/';
}

static void path_dirname(const char *path, char *out, size_t out_cap) {
    if (!out || out_cap == 0) return;
    out[0] = 0;
    if (!path || !path[0]) {
        snprintf(out, out_cap, ".");
        return;
    }
    const char *last = strrchr(path, '/');
#if defined(_WIN32)
    const char *back = strrchr(path, '\\');
    if (!last || (back && back > last)) last = back;
#endif
    if (!last) {
        snprintf(out, out_cap, ".");
        return;
    }
    size_t n = (size_t)(last - path);
    if (n == 0) {
        snprintf(out, out_cap, "/");
        return;
    }
    if (n >= out_cap) n = out_cap - 1;
    memcpy(out, path, n);
    out[n] = 0;
}

static void path_join(const char *base_dir, const char *rel, char *out, size_t out_cap) {
    if (!out || out_cap == 0) return;
    out[0] = 0;
    if (!rel || !rel[0]) return;
    if (path_is_absolute(rel) || !base_dir || !base_dir[0] || strcmp(base_dir, ".") == 0) {
        snprintf(out, out_cap, "%s", rel);
        return;
    }
    size_t bl = strlen(base_dir);
    bool has_sep = bl > 0 && (base_dir[bl - 1] == '/' || base_dir[bl - 1] == '\\');
    if (has_sep) snprintf(out, out_cap, "%s%s", base_dir, rel);
    else snprintf(out, out_cap, "%s/%s", base_dir, rel);
}

static void path_normalize(const char *path, char *out, size_t out_cap) {
    if (!out || out_cap == 0) return;
    out[0] = 0;
    if (!path || !path[0]) return;
#if defined(_WIN32)
    snprintf(out, out_cap, "%s", path);
#else
    char resolved[PATH_MAX];
    if (realpath(path, resolved)) snprintf(out, out_cap, "%s", resolved);
    else snprintf(out, out_cap, "%s", path);
#endif
}

static void trim_right(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) {
        s[--n] = 0;
    }
}

static void strip_comment(char *s) {
    if (!s) return;
    for (size_t i = 0; s[i]; i++) {
        if (s[i] == ';') {
            s[i] = 0;
            return;
        }
    }
}

static const char *skip_ws(const char *s) {
    while (s && (*s == ' ' || *s == '\t')) s++;
    return s;
}

static bool is_blank_after_trim(const char *s) {
    if (!s) return true;
    s = skip_ws(s);
    return *s == 0;
}

static void emit_diag(DiagCounts *counts, DiagLevel level, const char *file, int line, const char *msg) {
    if (!counts || !file || !msg) return;
    if (level == DIAG_WARN) {
        counts->warnings++;
        fprintf(stderr, "warn: %s:%d: %s\n", file, line, msg);
    } else {
        counts->errors++;
        fprintf(stderr, "error: %s:%d: %s\n", file, line, msg);
    }
}

static bool in_list(const char *needle, const char **list, size_t len) {
    if (!needle) return false;
    for (size_t i = 0; i < len; i++) {
        if (strcmp(needle, list[i]) == 0) return true;
    }
    return false;
}

static bool in_strvec(const char *needle, const StrVec *v) {
    if (!needle || !v) return false;
    for (size_t i = 0; i < v->len; i++) {
        if (strcmp(needle, v->items[i]) == 0) return true;
    }
    return false;
}

static int min3(int a, int b, int c) {
    int m = a < b ? a : b;
    return m < c ? m : c;
}

static int levenshtein(const char *a, const char *b) {
    if (!a || !b) return 999;
    size_t n = strlen(a);
    size_t m = strlen(b);
    if (n == 0) return (int)m;
    if (m == 0) return (int)n;

    int *prev = (int *)malloc((m + 1) * sizeof(int));
    int *curr = (int *)malloc((m + 1) * sizeof(int));
    if (!prev || !curr) {
        free(prev);
        free(curr);
        return 999;
    }

    for (size_t j = 0; j <= m; j++) prev[j] = (int)j;

    for (size_t i = 1; i <= n; i++) {
        curr[0] = (int)i;
        for (size_t j = 1; j <= m; j++) {
            int cost = (tolower((unsigned char)a[i - 1]) == tolower((unsigned char)b[j - 1])) ? 0 : 1;
            curr[j] = min3(prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost);
        }
        int *tmp = prev;
        prev = curr;
        curr = tmp;
    }

    int d = prev[m];
    free(prev);
    free(curr);
    return d;
}

static const char *closest_match(const char *s, const char **list, size_t len, int max_dist) {
    const char *best = NULL;
    int best_d = 999;
    for (size_t i = 0; i < len; i++) {
        int d = levenshtein(s, list[i]);
        if (d < best_d) {
            best_d = d;
            best = list[i];
        }
    }
    if (!best || best_d > max_dist) return NULL;
    return best;
}

static const char *closest_match_strvec(const char *s, const StrVec *v, int max_dist) {
    if (!s || !v) return NULL;
    const char *best = NULL;
    int best_d = 999;
    for (size_t i = 0; i < v->len; i++) {
        int d = levenshtein(s, v->items[i]);
        if (d < best_d) {
            best_d = d;
            best = v->items[i];
        }
    }
    if (!best || best_d > max_dist) return NULL;
    return best;
}

static bool in_combined_list(const char *needle, const char **builtins, size_t builtins_len, const StrVec *injected) {
    if (in_list(needle, builtins, builtins_len)) return true;
    return in_strvec(needle, injected);
}

static const char *closest_match_combined(const char *name, const char **builtins, size_t builtins_len,
                                          const StrVec *injected, int max_dist) {
    const char *a = closest_match(name, builtins, builtins_len, max_dist);
    const char *b = closest_match_strvec(name, injected, max_dist);
    if (!a) return b;
    if (!b) return a;
    int da = levenshtein(name, a);
    int db = levenshtein(name, b);
    return (db < da) ? b : a;
}

static void load_injected_list(const char *env_name, StrVec *out) {
    const char *v = getenv(env_name);
    if (!v || !v[0] || !out) return;
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s", v);
    char *tok = strtok(buf, ",");
    while (tok) {
        tok = (char *)skip_ws(tok);
        trim_right(tok);
        if (*tok) {
            strvec_push(out, tok);
        }
        tok = strtok(NULL, ",");
    }
}

static bool parse_bring_target(const char *line, char *out, size_t out_cap) {
    if (!line || !out || out_cap == 0) return false;
    out[0] = 0;
    if (strncmp(line, "@bring", 6) != 0) return false;
    char c = line[6];
    if (!(c == 0 || c == ':' || c == ' ' || c == '\t')) return false;

    const char *p = line + 6;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == ':') {
        p++;
        while (*p == ' ' || *p == '\t') p++;
    }
    if (!*p) return false;

    if (*p == '"') {
        p++;
        size_t i = 0;
        while (*p && *p != '"') {
            if (i + 1 < out_cap) out[i++] = *p;
            p++;
        }
        if (*p != '"') return false;
        out[i] = 0;
        p++;
        p = skip_ws(p);
        return *p == 0 && out[0] != 0;
    }

    size_t i = 0;
    while (*p && *p != ' ' && *p != '\t') {
        if (i + 1 < out_cap) out[i++] = *p;
        p++;
    }
    out[i] = 0;
    p = skip_ws(p);
    return *p == 0 && out[0] != 0;
}

static bool path_in_stack(const char *path, StrVec *stack) {
    if (!path || !stack) return false;
    for (size_t i = 0; i < stack->len; i++) {
        if (strcmp(path, stack->items[i]) == 0) return true;
    }
    return false;
}

static bool append_file_lines_with_bring(const char *path, StrVec *stack, SourceLineVec *out, DiagCounts *counts) {
    char norm[PATH_MAX];
    path_normalize(path, norm, sizeof(norm));
    if (path_in_stack(norm, stack)) {
        char msg[PATH_MAX + 64];
        snprintf(msg, sizeof(msg), "@bring cycle detected at '%s'", norm);
        emit_diag(counts, DIAG_ERROR, path, 1, msg);
        return false;
    }
    if (!strvec_push(stack, norm)) {
        emit_diag(counts, DIAG_ERROR, path, 1, "out of memory while processing @bring");
        return false;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        char msg[256];
        snprintf(msg, sizeof(msg), "failed to open file (%s)", strerror(errno));
        emit_diag(counts, DIAG_ERROR, path, 1, msg);
        stack->len--;
        return false;
    }

    char base_dir[PATH_MAX];
    path_dirname(path, base_dir, sizeof(base_dir));

    char raw[2048];
    int line_no = 0;
    while (fgets(raw, sizeof(raw), f)) {
        line_no++;
        char line[2048];
        snprintf(line, sizeof(line), "%s", raw);
        trim_right(line);

        char analysis[2048];
        snprintf(analysis, sizeof(analysis), "%s", line);
        strip_comment(analysis);
        trim_right(analysis);
        const char *t = skip_ws(analysis);

        char bring_rel[PATH_MAX];
        if (parse_bring_target(t, bring_rel, sizeof(bring_rel))) {
            char bring_path[PATH_MAX];
            path_join(base_dir, bring_rel, bring_path, sizeof(bring_path));
            if (!append_file_lines_with_bring(bring_path, stack, out, counts)) {
                char msg[PATH_MAX + 128];
                snprintf(msg, sizeof(msg), "failed to @bring '%s'", bring_rel);
                emit_diag(counts, DIAG_ERROR, path, line_no, msg);
            }
            continue;
        }

        bool looks_like_bring = strncmp(t, "@bring", 6) == 0 &&
            (t[6] == 0 || t[6] == ':' || t[6] == ' ' || t[6] == '\t');
        if (looks_like_bring) {
            emit_diag(counts, DIAG_ERROR, path, line_no, "malformed @bring directive");
            continue;
        }

        if (!source_line_vec_push(out, line, path, line_no)) {
            emit_diag(counts, DIAG_ERROR, path, line_no, "out of memory while storing source map");
            fclose(f);
            stack->len--;
            return false;
        }
    }

    fclose(f);
    stack->len--;
    return true;
}

static SumMode parse_mode(const char *s, bool *ok) {
    if (ok) *ok = true;
    if (!s) return SUM_MODE_WARN;
    if (strcmp(s, "off") == 0) return SUM_MODE_OFF;
    if (strcmp(s, "warn") == 0) return SUM_MODE_WARN;
    if (strcmp(s, "strict") == 0) return SUM_MODE_STRICT;
    if (ok) *ok = false;
    return SUM_MODE_WARN;
}

static void maybe_emit_unknown(DiagCounts *counts, SumMode mode, const char *file, int line,
                               const char *kind, const char *name,
                               const char **known, size_t known_len,
                               const StrVec *injected) {
    if (mode == SUM_MODE_OFF) return;

    char msg[512];
    const char *s = closest_match_combined(name, known, known_len, injected, 3);
    if (s) snprintf(msg, sizeof(msg), "unknown %s '%s' (did you mean '%s'?)", kind, name, s);
    else snprintf(msg, sizeof(msg), "unknown %s '%s'", kind, name);

    emit_diag(counts, mode == SUM_MODE_STRICT ? DIAG_ERROR : DIAG_WARN, file, line, msg);
}

static bool looks_like_property_decl(const char *trimmed, char *prop_out, size_t prop_cap, const char **value_out) {
    if (!trimmed || !trimmed[0]) return false;
    const char *p = trimmed;
    if (!(isalpha((unsigned char)*p) || *p == '_')) return false;

    size_t i = 0;
    while (*p && *p != ':') {
        if (*p == ' ' || *p == '\t') return false;
        if (!(isalnum((unsigned char)*p) || *p == '-' || *p == '_')) return false;
        if (i + 1 < prop_cap) prop_out[i++] = *p;
        p++;
    }
    if (*p != ':' || i == 0) return false;
    prop_out[i] = 0;
    p++;
    p = skip_ws(p);
    if (value_out) *value_out = p;
    return true;
}

static void validate_value_functions(const char *value, DiagCounts *counts, SumMode mode, const char *file, int line) {
    if (!value || mode == SUM_MODE_OFF) return;

    bool in_string = false;
    char quote = 0;
    for (size_t i = 0; value[i]; i++) {
        char c = value[i];
        if (in_string) {
            if (c == quote) in_string = false;
            continue;
        }
        if (c == '"' || c == '\'') {
            in_string = true;
            quote = c;
            continue;
        }
        if (!(isalpha((unsigned char)c) || c == '_')) continue;

        size_t j = i;
        char ident[64];
        size_t k = 0;
        while (value[j] && (isalnum((unsigned char)value[j]) || value[j] == '_' || value[j] == '-')) {
            if (k + 1 < sizeof(ident)) ident[k++] = value[j];
            j++;
        }
        ident[k] = 0;

        const char *p = value + j;
        p = skip_ws(p);
        if (*p == '(') {
            if (!in_combined_list(ident, known_functions, sizeof(known_functions) / sizeof(known_functions[0]),
                                  &injected_functions)) {
                maybe_emit_unknown(counts, mode, file, line, "function", ident,
                                   known_functions, sizeof(known_functions) / sizeof(known_functions[0]),
                                   &injected_functions);
            }
            i = (size_t)(j - 1);
        }
    }
}

static void validate_selector_atom(const char *atom, DiagCounts *counts, SumMode mode, const char *file, int line) {
    if (!atom || !atom[0] || mode == SUM_MODE_OFF) return;

    char work[128];
    snprintf(work, sizeof(work), "%s", atom);

    char *state_sep = strchr(work, ':');
    if (state_sep) {
        *state_sep = 0;
        const char *state = state_sep + 1;
        if (!in_combined_list(state, known_states, sizeof(known_states) / sizeof(known_states[0]), &injected_states)) {
            maybe_emit_unknown(counts, mode, file, line, "state suffix", state,
                               known_states, sizeof(known_states) / sizeof(known_states[0]),
                               &injected_states);
        }
    }

    const char *base = work;
    if (base[0] == 0 || strcmp(base, "*") == 0) return;

    char type_name[128] = {0};
    if (base[0] == '.') {
        return;
    }

    const char *dot = strchr(base, '.');
    if (dot) {
        size_t n = (size_t)(dot - base);
        if (n >= sizeof(type_name)) n = sizeof(type_name) - 1;
        memcpy(type_name, base, n);
        type_name[n] = 0;
    } else {
        snprintf(type_name, sizeof(type_name), "%s", base);
    }

    if (type_name[0] && !in_combined_list(type_name, known_types, sizeof(known_types) / sizeof(known_types[0]), &injected_types)) {
        maybe_emit_unknown(counts, mode, file, line, "type selector", type_name,
                           known_types, sizeof(known_types) / sizeof(known_types[0]),
                           &injected_types);
    }
}

static void validate_selector_line(const char *trimmed, DiagCounts *counts, SumMode mode,
                                   const char *file, int line) {
    char linebuf[256];
    snprintf(linebuf, sizeof(linebuf), "%s", trimmed);

    char *saveptr = NULL;
    char *piece = strtok_r(linebuf, ",", &saveptr);
    while (piece) {
        piece = (char *)skip_ws(piece);
        trim_right(piece);
        if (*piece == 0) {
            piece = strtok_r(NULL, ",", &saveptr);
            continue;
        }

        char temp[256];
        snprintf(temp, sizeof(temp), "%s", piece);

        char *token1 = strtok(temp, " \t");
        char *token2 = strtok(NULL, " \t");
        char *token3 = strtok(NULL, " \t");

        if (!token1) {
            piece = strtok_r(NULL, ",", &saveptr);
            continue;
        }

        if (token3) {
            emit_diag(counts, DIAG_ERROR, file, line, "selector supports at most one descendant hop");
            piece = strtok_r(NULL, ",", &saveptr);
            continue;
        }

        validate_selector_atom(token1, counts, mode, file, line);
        if (token2) validate_selector_atom(token2, counts, mode, file, line);

        piece = strtok_r(NULL, ",", &saveptr);
    }
}

static bool validate_lines(SourceLineVec *lines, SumMode forced_mode, bool use_forced_mode, DiagCounts *counts) {
    SumMode mode = SUM_MODE_WARN;
    if (use_forced_mode) mode = forced_mode;

    BlockStack blocks = {0};
    RuleContext rule = {0};
    int prev_indent = 0;

    for (size_t i = 0; i < lines->len; i++) {
        SourceLine *sl = &lines->items[i];
        char work[2048];
        snprintf(work, sizeof(work), "%s", sl->text);

        strip_comment(work);
        trim_right(work);
        if (is_blank_after_trim(work)) continue;

        int spaces = 0;
        int tabs = 0;
        const char *p = work;
        while (*p == ' ' || *p == '\t') {
            if (*p == ' ') spaces++;
            else tabs++;
            p++;
        }
        const char *trimmed = p;

        if (tabs > 0 && spaces > 0) {
            emit_diag(counts, DIAG_ERROR, sl->file, sl->line, "mixed tabs and spaces in indentation");
        } else if (tabs > 0) {
            emit_diag(counts, DIAG_ERROR, sl->file, sl->line, "tabs in indentation are not allowed");
        }

        int indent_cols = spaces + tabs * 2;
        if ((indent_cols % 2) != 0) {
            emit_diag(counts, DIAG_ERROR, sl->file, sl->line, "indentation must be in 2-space steps");
        }
        int indent = indent_cols / 2;

        if (mode != SUM_MODE_OFF && indent > prev_indent + 1) {
            emit_diag(counts, DIAG_ERROR, sl->file, sl->line, "inconsistent indentation depth (jumped more than one level)");
        }
        prev_indent = indent;

        block_stack_pop_to_indent(&blocks, indent);
        if (rule.have && indent <= rule.indent) {
            rule.have = false;
        }

        if (*trimmed == '@') {
            if (strncmp(trimmed, "@diagnostics", 12) == 0) {
                const char *c = strchr(trimmed, ':');
                if (!c) {
                    emit_diag(counts, DIAG_ERROR, sl->file, sl->line, "malformed @diagnostics directive");
                } else if (!use_forced_mode) {
                    c++;
                    c = skip_ws(c);
                    bool ok = false;
                    SumMode next = parse_mode(c, &ok);
                    if (!ok) {
                        emit_diag(counts, DIAG_ERROR, sl->file, sl->line, "unknown diagnostics mode (expected off|warn|strict)");
                    } else {
                        mode = next;
                    }
                }
                continue;
            }

            if (strncmp(trimmed, "@when", 5) == 0) {
                const char *expr = trimmed + 5;
                expr = skip_ws(expr);
                if (!*expr) {
                    emit_diag(counts, DIAG_ERROR, sl->file, sl->line, "malformed @when condition");
                }
                if (!block_stack_push(&blocks, indent, true)) {
                    emit_diag(counts, DIAG_ERROR, sl->file, sl->line, "out of memory while tracking @when block");
                    free(blocks.items);
                    return false;
                }
                continue;
            }

            bool looks_like_bring = strncmp(trimmed, "@bring", 6) == 0 &&
                (trimmed[6] == 0 || trimmed[6] == ':' || trimmed[6] == ' ' || trimmed[6] == '\t');
            if (looks_like_bring) {
                emit_diag(counts, DIAG_ERROR, sl->file, sl->line,
                          "@bring should have been resolved before validation");
                continue;
            }

            const char *colon = strchr(trimmed, ':');
            if (!colon) {
                emit_diag(counts, DIAG_ERROR, sl->file, sl->line, "malformed directive");
            }
            continue;
        }

        bool can_be_decl = false;
        if (rule.have && indent > rule.indent) can_be_decl = true;

        char prop[96] = {0};
        const char *value = NULL;
        bool is_decl = can_be_decl && looks_like_property_decl(trimmed, prop, sizeof(prop), &value);

        if (is_decl) {
            if (!in_combined_list(prop, known_properties, sizeof(known_properties) / sizeof(known_properties[0]),
                                  &injected_properties)) {
                maybe_emit_unknown(counts, mode, sl->file, sl->line, "property", prop,
                                   known_properties, sizeof(known_properties) / sizeof(known_properties[0]),
                                   &injected_properties);
            }
            validate_value_functions(value, counts, mode, sl->file, sl->line);
            continue;
        }

        if (indent > 0 && !rule.have && blocks.len == 0) {
            emit_diag(counts, DIAG_ERROR, sl->file, sl->line,
                      "indented line without an active @when/rule block");
        }

        rule.have = true;
        rule.indent = indent;
        validate_selector_line(trimmed, counts, mode, sl->file, sl->line);
    }

    free(blocks.items);
    return true;
}

static bool path_is_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

static bool path_is_file(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return false;
    return S_ISREG(st.st_mode);
}

static bool ends_with_sum(const char *path) {
    if (!path) return false;
    size_t n = strlen(path);
    return n > 4 && strcmp(path + n - 4, ".sum") == 0;
}

static bool collect_sum_files_recursive(const char *root, StrVec *out) {
    DIR *d = opendir(root);
    if (!d) return false;

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", root, ent->d_name);

        if (path_is_dir(path)) {
            if (!collect_sum_files_recursive(path, out)) {
                closedir(d);
                return false;
            }
        } else if (path_is_file(path) && ends_with_sum(path)) {
            if (!strvec_push(out, path)) {
                closedir(d);
                return false;
            }
        }
    }

    closedir(d);
    return true;
}

static int validate_single_file(const char *file, SumMode forced_mode, bool use_forced_mode,
                                DiagCounts *totals) {
    SourceLineVec lines = {0};
    StrVec stack = {0};
    DiagCounts local = {0};

    bool ok = append_file_lines_with_bring(file, &stack, &lines, &local);
    strvec_free(&stack);
    if (ok) {
        validate_lines(&lines, forced_mode, use_forced_mode, &local);
    }

    totals->errors += local.errors;
    totals->warnings += local.warnings;
    source_line_vec_free(&lines);
    return local.errors == 0 ? 0 : 1;
}

static void print_sum_usage(FILE *out) {
    fprintf(out, "Usage: yis sum validate [--mode off|warn|strict] <path>\n");
    fprintf(out, "\n");
    fprintf(out, "Validate SUM theme files with @bring expansion and source-mapped diagnostics.\n");
    fprintf(out, "If <path> is a directory, all .sum files are validated recursively.\n");
    fprintf(out, "Optional registry injection via CSV env vars:\n");
    fprintf(out, "  YIS_SUM_PROPERTIES, YIS_SUM_TYPES, YIS_SUM_STATES, YIS_SUM_FUNCTIONS\n");
}

int sum_validate_cli(int argc, char **argv) {
    if (argc < 3 || strcmp(argv[1], "sum") != 0) {
        print_sum_usage(stderr);
        return 2;
    }
    if (strcmp(argv[2], "validate") != 0) {
        print_sum_usage(stderr);
        return 2;
    }

    const char *target = NULL;
    bool use_forced_mode = false;
    SumMode forced_mode = SUM_MODE_WARN;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--mode") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --mode requires one of off|warn|strict\n");
                return 2;
            }
            bool ok = false;
            forced_mode = parse_mode(argv[++i], &ok);
            if (!ok) {
                fprintf(stderr, "error: unknown mode '%s'\n", argv[i]);
                return 2;
            }
            use_forced_mode = true;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "error: unknown option %s\n", argv[i]);
            return 2;
        }
        if (target) {
            fprintf(stderr, "error: multiple paths provided\n");
            return 2;
        }
        target = argv[i];
    }

    if (!target) {
        fprintf(stderr, "error: missing path to .sum file or directory\n");
        return 2;
    }

    DiagCounts totals = {0};
    int rc = 0;

    load_injected_list("YIS_SUM_TYPES", &injected_types);
    load_injected_list("YIS_SUM_STATES", &injected_states);
    load_injected_list("YIS_SUM_PROPERTIES", &injected_properties);
    load_injected_list("YIS_SUM_FUNCTIONS", &injected_functions);

    if (path_is_file(target)) {
        rc = validate_single_file(target, forced_mode, use_forced_mode, &totals);
    } else if (path_is_dir(target)) {
        StrVec files = {0};
        if (!collect_sum_files_recursive(target, &files)) {
            fprintf(stderr, "error: failed to scan directory '%s'\n", target);
            strvec_free(&files);
            return 1;
        }
        if (files.len == 0) {
            fprintf(stderr, "warn: no .sum files found under %s\n", target);
        }
        for (size_t i = 0; i < files.len; i++) {
            int one = validate_single_file(files.items[i], forced_mode, use_forced_mode, &totals);
            if (one != 0) rc = 1;
        }
        strvec_free(&files);
    } else {
        fprintf(stderr, "error: path not found: %s\n", target);
        return 1;
    }

    strvec_free(&injected_types);
    strvec_free(&injected_states);
    strvec_free(&injected_properties);
    strvec_free(&injected_functions);

    fprintf(stderr, "summary: %d error(s), %d warning(s)\n", totals.errors, totals.warnings);
    return rc;
}
