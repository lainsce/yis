#include "codegen.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"
#include "file.h"
#include "platform.h"
#include "runtime_embedded.h"
#include "str.h"
#include "typecheck.h"
#include "vec.h"

// -----------------
// Small string buf
// -----------------

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} StrBuf;

static void sb_init(StrBuf *b) {
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static void sb_free(StrBuf *b) {
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static bool sb_reserve(StrBuf *b, size_t add) {
    size_t need = b->len + add + 1;
    if (need <= b->cap) {
        return true;
    }
    size_t next = b->cap ? b->cap : 256;
    while (next < need) {
        next *= 2;
    }
    char *p = (char *)realloc(b->data, next);
    if (!p) {
        return false;
    }
    b->data = p;
    b->cap = next;
    return true;
}

static bool sb_append_n(StrBuf *b, const char *s, size_t n) {
    if (!s || n == 0) {
        return true;
    }
    if (!sb_reserve(b, n)) {
        return false;
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
    return true;
}

static bool sb_append(StrBuf *b, const char *s) {
    if (!s) {
        return true;
    }
    return sb_append_n(b, s, strlen(s));
}

static bool sb_append_char(StrBuf *b, char c) {
    if (!sb_reserve(b, 1)) {
        return false;
    }
    b->data[b->len++] = c;
    b->data[b->len] = '\0';
    return true;
}

static bool sb_appendf(StrBuf *b, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) {
        va_end(ap2);
        return false;
    }
    if (!sb_reserve(b, (size_t)needed)) {
        va_end(ap2);
        return false;
    }
    vsnprintf(b->data + b->len, (size_t)needed + 1, fmt, ap2);
    va_end(ap2);
    b->len += (size_t)needed;
    b->data[b->len] = '\0';
    return true;
}

// -----------------
// Writer helpers
// -----------------

typedef struct {
    StrBuf *buf;
    int indent;
} Writer;

static bool w_line(Writer *w, const char *fmt, ...) {
    if (!w || !w->buf) {
        return false;
    }
    for (int i = 0; i < w->indent; i++) {
        if (!sb_append(w->buf, "  ")) {
            return false;
        }
    }
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) {
        va_end(ap2);
        return false;
    }
    if (!sb_reserve(w->buf, (size_t)needed + 1)) {
        va_end(ap2);
        return false;
    }
    vsnprintf(w->buf->data + w->buf->len, (size_t)needed + 1, fmt, ap2);
    va_end(ap2);
    w->buf->len += (size_t)needed;
    w->buf->data[w->buf->len] = '\0';
    if (!sb_append_char(w->buf, '\n')) {
        return false;
    }
    return true;
}

// -----------------
// Error handling
// -----------------

static bool cg_set_errf(Diag *err, Str path, int line, int col, const char *fmt, ...) {
    if (!err) {
        return false;
    }
    static char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    err->message = buf;
    err->path = path.data;
    err->line = line;
    err->col = col;
    return false;
}

static bool cg_set_err(Diag *err, Str path, const char *msg) {
    if (!err) {
        return false;
    }
    err->path = path.data;
    err->line = 0;
    err->col = 0;
    err->message = msg;
    return false;
}

// -----------------
// String helpers
// -----------------

static char *arena_strndup(Arena *arena, const char *s, size_t len) {
    if (!arena) return NULL;
    char *buf = (char *)arena_alloc(arena, len + 1);
    if (!buf) return NULL;
    memcpy(buf, s, len);
    buf[len] = '\0';
    return buf;
}

static char *dup_cstr(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

static char *arena_printf(Arena *arena, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    va_list ap2;
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) {
        va_end(ap2);
        return NULL;
    }
    char *buf = (char *)arena_alloc(arena, (size_t)needed + 1);
    if (!buf) {
        va_end(ap2);
        return NULL;
    }
    vsnprintf(buf, (size_t)needed + 1, fmt, ap2);
    va_end(ap2);
    return buf;
}

// -----------------
// Simple hash map (Str -> size_t index)
// -----------------

static uint32_t str_hash(Str s) {
    uint32_t h = 2166136261u;
    for (size_t i = 0; i < s.len; i++) {
        h ^= (uint8_t)s.data[i];
        h *= 16777619u;
    }
    return h;
}

typedef struct {
    Str key;
    size_t value;
    bool occupied;
} StrMapEntry;

typedef struct {
    StrMapEntry *entries;
    size_t cap;
    size_t len;
} StrMap;

static void strmap_init(StrMap *m, Arena *arena, size_t expected) {
    size_t cap = 16;
    while (cap < expected * 2) cap *= 2;
    m->entries = (StrMapEntry *)arena_alloc(arena, sizeof(StrMapEntry) * cap);
    memset(m->entries, 0, sizeof(StrMapEntry) * cap);
    m->cap = cap;
    m->len = 0;
}

static void strmap_put(StrMap *m, Str key, size_t value) {
    uint32_t idx = str_hash(key) & (uint32_t)(m->cap - 1);
    for (;;) {
        if (!m->entries[idx].occupied) {
            m->entries[idx].key = key;
            m->entries[idx].value = value;
            m->entries[idx].occupied = true;
            m->len++;
            return;
        }
        if (str_eq(m->entries[idx].key, key)) {
            m->entries[idx].value = value;
            return;
        }
        idx = (idx + 1) & (uint32_t)(m->cap - 1);
    }
}

static bool strmap_get(StrMap *m, Str key, size_t *out) {
    if (!m->entries || m->len == 0) return false;
    uint32_t idx = str_hash(key) & (uint32_t)(m->cap - 1);
    for (;;) {
        if (!m->entries[idx].occupied) return false;
        if (str_eq(m->entries[idx].key, key)) {
            *out = m->entries[idx].value;
            return true;
        }
        idx = (idx + 1) & (uint32_t)(m->cap - 1);
    }
}

static char *c_escape(Arena *arena, Str s) {
    StrBuf b;
    sb_init(&b);
    for (size_t i = 0; i < s.len; i++) {
        unsigned char c = (unsigned char)s.data[i];
        switch (c) {
            case '\\': sb_append(&b, "\\\\"); break;
            case '"': sb_append(&b, "\\\""); break;
            case '%': sb_append(&b, "%%"); break;
            case '\n': sb_append(&b, "\\n"); break;
            case '\t': sb_append(&b, "\\t"); break;
            case '\r': sb_append(&b, "\\r"); break;
            default:
                sb_append_char(&b, (char)c);
                break;
        }
    }
    char *out = arena_strndup(arena, b.data ? b.data : "", b.len);
    sb_free(&b);
    return out;
}

static char *mangle_mod(Arena *arena, Str name) {
    if (!arena) return NULL;
    char *buf = (char *)arena_alloc(arena, name.len + 1);
    if (!buf) return NULL;
    for (size_t i = 0; i < name.len; i++) {
        unsigned char c = (unsigned char)name.data[i];
        if (isalnum(c) || c == '_') {
            buf[i] = (char)c;
        } else {
            buf[i] = '_';
        }
    }
    buf[name.len] = '\0';
    return buf;
}

static char *mangle_global(Arena *arena, Str mod, Str name) {
    char *mm = mangle_mod(arena, mod);
    if (!mm) return NULL;
    return arena_printf(arena, "yis_%s_%.*s", mm, (int)name.len, name.data);
}

static char *mangle_global_var(Arena *arena, Str mod, Str name) {
    char *mm = mangle_mod(arena, mod);
    if (!mm) return NULL;
    return arena_printf(arena, "yis_g_%s_%.*s", mm, (int)name.len, name.data);
}

static char *mangle_global_init(Arena *arena, Str mod) {
    char *mm = mangle_mod(arena, mod);
    if (!mm) return NULL;
    return arena_printf(arena, "yis_init_%s", mm);
}

static char *mangle_method(Arena *arena, Str mod, Str cls, Str name) {
    char *mm = mangle_mod(arena, mod);
    if (!mm) return NULL;
    return arena_printf(arena, "yis_m_%s_%.*s_%.*s", mm, (int)cls.len, cls.data, (int)name.len, name.data);
}

static void split_qname(Str qname, Str *mod, Str *name) {
    size_t dot = 0;
    while (dot < qname.len && qname.data[dot] != '.') {
        dot++;
    }
    if (dot >= qname.len) {
        if (mod) { mod->data = ""; mod->len = 0; }
        if (name) { *name = qname; }
        return;
    }
    if (mod) {
        mod->data = qname.data;
        mod->len = dot;
    }
    if (name) {
        name->data = qname.data + dot + 1;
        name->len = qname.len - dot - 1;
    }
}

// -----------------
// Codegen structs
// -----------------

typedef struct {
    Str name;
    char *cname;
} NameBinding;

typedef struct {
    NameBinding *items;
    size_t len;
    size_t cap;
} NameScope;

typedef struct {
    char **items;
    size_t len;
    size_t cap;
} LocalList;

typedef struct {
    Expr *lam;
    Str path;
    char *name;
} LambdaInfo;

typedef struct {
    Str cask;
    Str name;
    char *wrapper;
} FunValInfo;

typedef struct {
    Str qname;
    ClassDecl *decl;
} ClassDeclEntry;

typedef struct {
    char *continue_label;
} LoopCtx;

typedef struct {
    Program *prog;
    GlobalEnv *env;
    Arena *arena;
    StrBuf out;
    Writer w;

    int tmp_id;
    int var_id;
    int arr_id;
    int sym_id;
    int lambda_id;

    NameScope *scopes;
    size_t scopes_len;
    size_t scopes_cap;

    LocalList *scope_locals;
    size_t scope_locals_len;
    size_t scope_locals_cap;

    Locals ty_loc;

    Str current_cask;
    Str *current_imports;
    size_t current_imports_len;
    Str current_class;
    bool has_current_class;
    
    Str entry_path;  // Path to the entry script file

    LambdaInfo *lambdas;
    size_t lambdas_len;
    size_t lambdas_cap;

    FunValInfo *funvals;
    size_t funvals_len;
    size_t funvals_cap;

    ClassDeclEntry *class_decls;
    size_t class_decls_len;
    size_t class_decls_cap;

    LoopCtx *loop_stack;
    size_t loop_stack_len;
    size_t loop_stack_cap;

    // Hash map caches for env lookups
    StrMap class_map;       // qname -> index into env->classes
    StrMap fun_map;         // "cask\0name" -> index into env->funs
    StrMap cask_globals_map; // cask -> index into env->cask_globals
    StrMap cask_consts_map;  // cask -> index into env->cask_consts
    StrMap cask_imports_map; // cask -> index into env->cask_imports

    // #line directive tracking (avoid redundant directives)
    int last_line_num;
    Str last_line_file;
} Codegen;

static char *codegen_c_class_name(Codegen *cg, Str qname) {
    Str mod, name;
    split_qname(qname, &mod, &name);
    if (mod.len == 0) {
        return arena_printf(cg->arena, "YisObj_%.*s", (int)name.len, name.data);
    }
    return arena_printf(cg->arena, "YisObj_%s_%.*s", mangle_mod(cg->arena, mod), (int)name.len, name.data);
}

static char *codegen_c_field_name(Codegen *cg, Str name) {
    return arena_printf(cg->arena, "f_%.*s", (int)name.len, name.data);
}

// -----------------
// Scope helpers
// -----------------

static bool scope_reserve_names(NameScope *scope, size_t need) {
    if (scope->cap >= need) return true;
    size_t next = scope->cap ? scope->cap * 2 : 8;
    while (next < need) next *= 2;
    NameBinding *items = (NameBinding *)realloc(scope->items, next * sizeof(NameBinding));
    if (!items) return false;
    scope->items = items;
    scope->cap = next;
    return true;
}

static bool scope_reserve_locals(LocalList *list, size_t need) {
    if (list->cap >= need) return true;
    size_t next = list->cap ? list->cap * 2 : 8;
    while (next < need) next *= 2;
    char **items = (char **)realloc(list->items, next * sizeof(char *));
    if (!items) return false;
    list->items = items;
    list->cap = next;
    return true;
}

static bool codegen_push_scope(Codegen *cg) {
    if (cg->scopes_len + 1 > cg->scopes_cap) {
        size_t next = cg->scopes_cap ? cg->scopes_cap * 2 : 8;
        NameScope *sc = (NameScope *)realloc(cg->scopes, next * sizeof(NameScope));
        if (!sc) return false;
        cg->scopes = sc;
        cg->scopes_cap = next;
    }
    if (cg->scope_locals_len + 1 > cg->scope_locals_cap) {
        size_t next = cg->scope_locals_cap ? cg->scope_locals_cap * 2 : 8;
        LocalList *ls = (LocalList *)realloc(cg->scope_locals, next * sizeof(LocalList));
        if (!ls) return false;
        cg->scope_locals = ls;
        cg->scope_locals_cap = next;
    }
    NameScope *ns = &cg->scopes[cg->scopes_len++];
    ns->items = NULL;
    ns->len = 0;
    ns->cap = 0;
    LocalList *ll = &cg->scope_locals[cg->scope_locals_len++];
    ll->items = NULL;
    ll->len = 0;
    ll->cap = 0;
    locals_push(&cg->ty_loc);
    return true;
}

static LocalList codegen_pop_scope(Codegen *cg) {
    LocalList out = {NULL, 0, 0};
    if (cg->scopes_len == 0 || cg->scope_locals_len == 0) {
        return out;
    }
    NameScope *ns = &cg->scopes[cg->scopes_len - 1];
    free(ns->items);
    cg->scopes_len--;
    out = cg->scope_locals[cg->scope_locals_len - 1];
    cg->scope_locals_len--;
    locals_pop(&cg->ty_loc);
    return out;
}

static bool codegen_add_name(Codegen *cg, Str name, char *cname) {
    if (cg->scopes_len == 0) return false;
    NameScope *ns = &cg->scopes[cg->scopes_len - 1];
    if (!scope_reserve_names(ns, ns->len + 1)) return false;
    ns->items[ns->len].name = name;
    ns->items[ns->len].cname = cname;
    ns->len++;
    return true;
}

static bool codegen_add_local(Codegen *cg, char *cname) {
    if (cg->scope_locals_len == 0) return false;
    LocalList *ll = &cg->scope_locals[cg->scope_locals_len - 1];
    if (!scope_reserve_locals(ll, ll->len + 1)) return false;
    ll->items[ll->len++] = cname;
    return true;
}

static ModuleGlobals *codegen_cask_globals(Codegen *cg, Str cask);
static GlobalVar *codegen_find_global(ModuleGlobals *mg, Str name);
static bool gen_stmt(Codegen *cg, Str path, Stmt *s, bool ret_void, Diag *err);

static char *codegen_cname_of(Codegen *cg, Str name) {
    for (size_t i = cg->scopes_len; i-- > 0;) {
        NameScope *ns = &cg->scopes[i];
        for (size_t j = 0; j < ns->len; j++) {
            if (str_eq(ns->items[j].name, name)) {
                return ns->items[j].cname;
            }
        }
    }
    if (cg->current_cask.len) {
        ModuleGlobals *mg = codegen_cask_globals(cg, cg->current_cask);
        if (codegen_find_global(mg, name)) {
            return mangle_global_var(cg->arena, cg->current_cask, name);
        }
    }
    return NULL;
}

static char *codegen_new_tmp(Codegen *cg) {
    cg->tmp_id++;
    return arena_printf(cg->arena, "__t%d", cg->tmp_id);
}

static char *codegen_new_sym(Codegen *cg, const char *base) {
    cg->sym_id++;
    return arena_printf(cg->arena, "__%s%d", base, cg->sym_id);
}

static char *codegen_new_lambda(Codegen *cg) {
    cg->lambda_id++;
    return arena_printf(cg->arena, "yis_lambda_%d", cg->lambda_id);
}

static char *codegen_define_local(Codegen *cg, Str name, Ty *ty, bool is_mut, bool is_const) {
    cg->var_id++;
    size_t n_digits = 1;
    int tmp = cg->var_id;
    while (tmp >= 10) {
        n_digits++;
        tmp /= 10;
    }
    size_t total = name.len + 2 + n_digits;
    char *buf = (char *)arena_alloc(cg->arena, total + 1);
    if (!buf) return NULL;
    memcpy(buf, name.data, name.len);
    buf[name.len] = '_';
    buf[name.len + 1] = '_';
    snprintf(buf + name.len + 2, n_digits + 1, "%d", cg->var_id);

    Binding b = { ty, is_mut, is_const, false };
    locals_define(&cg->ty_loc, name, b);
    if (!codegen_add_name(cg, name, buf)) return NULL;
    if (!codegen_add_local(cg, buf)) return NULL;
    return buf;
}

static bool codegen_bind_temp(Codegen *cg, Str name, char *cname, Ty *ty) {
    Binding b = { ty, false, false, false };
    locals_define(&cg->ty_loc, name, b);
    return codegen_add_name(cg, name, cname);
}

static void codegen_release_scope(Codegen *cg, LocalList locals) {
    if (!cg) return;
    for (size_t i = locals.len; i-- > 0;) {
        w_line(&cg->w, "yis_release_val(%s);", locals.items[i]);
    }
    free(locals.items);
}

static bool codegen_loop_push(Codegen *cg, char *continue_label) {
    if (cg->loop_stack_len + 1 > cg->loop_stack_cap) {
        size_t next = cg->loop_stack_cap ? cg->loop_stack_cap * 2 : 8;
        LoopCtx *arr = (LoopCtx *)realloc(cg->loop_stack, next * sizeof(LoopCtx));
        if (!arr) return false;
        cg->loop_stack = arr;
        cg->loop_stack_cap = next;
    }
    cg->loop_stack[cg->loop_stack_len].continue_label = continue_label;
    cg->loop_stack_len++;
    return true;
}

static void codegen_loop_pop(Codegen *cg) {
    if (cg->loop_stack_len > 0) {
        cg->loop_stack_len--;
    }
}

static LoopCtx *codegen_loop_current(Codegen *cg) {
    if (!cg || cg->loop_stack_len == 0) return NULL;
    return &cg->loop_stack[cg->loop_stack_len - 1];
}

// -----------------
// Env helpers
// -----------------

static Str cask_name_for_path(Arena *arena, Str path) {
    const char *p = path.data;
    size_t len = path.len;
    size_t start = 0;
    for (size_t i = 0; i < len; i++) {
        char c = p[i];
        if (c == '/' || c == '\\') {
            start = i + 1;
        }
    }
    size_t name_len = len - start;
    if (name_len >= 3 && p[start + name_len - 3] == '.' &&
        p[start + name_len - 2] == 'y' && p[start + name_len - 1] == 'i') {
        name_len -= 3;
    } else if (name_len >= 2 && p[start + name_len - 2] == '.' && p[start + name_len - 1] == 'e') {
        name_len -= 2;
    }
    char *buf = arena_strndup(arena, p + start, name_len);
    Str out = { buf ? buf : "", name_len };
    
    return out;
}

static Str codegen_cask_name(Codegen *cg, Str path) {
    for (size_t i = 0; i < cg->env->cask_names_len; i++) {
        if (str_eq(cg->env->cask_names[i].path, path)) {
            return cg->env->cask_names[i].name;
        }
    }
    return cask_name_for_path(cg->arena, path);
}

static ModuleImport *codegen_cask_imports(Codegen *cg, Str cask_name) {
    size_t idx;
    if (strmap_get(&cg->cask_imports_map, cask_name, &idx)) {
        return &cg->env->cask_imports[idx];
    }
    return NULL;
}

static ClassDecl *codegen_class_decl(Codegen *cg, Str qname) {
    for (size_t i = 0; i < cg->class_decls_len; i++) {
        if (str_eq(cg->class_decls[i].qname, qname)) {
            return cg->class_decls[i].decl;
        }
    }
    return NULL;
}

static ClassInfo *codegen_class_info(Codegen *cg, Str qname) {
    size_t idx;
    if (strmap_get(&cg->class_map, qname, &idx)) {
        return &cg->env->classes[idx];
    }
    return NULL;
}

static FunSig *codegen_fun_sig(Codegen *cg, Str cask, Str name) {
    size_t klen = cask.len + 1 + name.len;
    char kbuf[256];
    char *key = (klen <= sizeof(kbuf)) ? kbuf : (char *)arena_alloc(cg->arena, klen);
    if (key) {
        memcpy(key, cask.data, cask.len);
        key[cask.len] = '\0';
        memcpy(key + cask.len + 1, name.data, name.len);
        size_t idx;
        if (strmap_get(&cg->fun_map, (Str){key, klen}, &idx)) {
            return &cg->env->funs[idx];
        }
    }
    return NULL;
}

static ModuleConsts *codegen_cask_consts(Codegen *cg, Str cask) {
    size_t idx;
    if (strmap_get(&cg->cask_consts_map, cask, &idx)) {
        return &cg->env->cask_consts[idx];
    }
    return NULL;
}

static ModuleGlobals *codegen_cask_globals(Codegen *cg, Str cask) {
    size_t idx;
    if (strmap_get(&cg->cask_globals_map, cask, &idx)) {
        return &cg->env->cask_globals[idx];
    }
    return NULL;
}

static GlobalVar *codegen_find_global(ModuleGlobals *mg, Str name) {
    if (!mg) return NULL;
    for (size_t i = 0; i < mg->len; i++) {
        if (str_eq(mg->vars[i].name, name)) {
            return &mg->vars[i];
        }
    }
    return NULL;
}

static ConstEntry *codegen_find_const(ModuleConsts *mc, Str name) {
    if (!mc) return NULL;
    for (size_t i = 0; i < mc->len; i++) {
        if (str_eq(mc->entries[i].name, name)) {
            return &mc->entries[i];
        }
    }
    return NULL;
}

static bool is_stdr_prelude(Str name) {
    return str_eq_c(name, "write") || str_eq_c(name, "writef") || str_eq_c(name, "readf") ||
           str_eq_c(name, "len") || str_eq_c(name, "slice") || str_eq_c(name, "concat") || str_eq_c(name, "char_code") || str_eq_c(name, "is_null") || str_eq_c(name, "str") || str_eq_c(name, "num");
}

static bool is_extern_stub_sig(FunSig *sig) {
    return sig && sig->extern_stub;
}

static Ctx codegen_ctx_for(Codegen *cg, Str path) {
    Ctx ctx;
    ctx.cask_path = path;
    ctx.cask_name = cg->current_cask.len ? cg->current_cask : codegen_cask_name(cg, path);
    ModuleImport *mi = codegen_cask_imports(cg, ctx.cask_name);
    ctx.imports = mi ? mi->imports : NULL;
    ctx.imports_len = mi ? mi->imports_len : 0;
    ctx.has_current_class = cg->has_current_class;
    ctx.current_class = cg->current_class;
    ctx.loop_depth = (int)cg->loop_stack_len;
    return ctx;
}

static bool codegen_cask_in_scope(Codegen *cg, Str name) {
    if (locals_lookup(&cg->ty_loc, name)) return false;
    if (str_eq(name, cg->current_cask)) return true;
    for (size_t i = 0; i < cg->current_imports_len; i++) {
        if (str_eq(cg->current_imports[i], name)) return true;
    }
    return false;
}

// -----------------
// Type helpers (for codegen)
// -----------------

static Ty *cg_ty_new(Codegen *cg, TyTag tag) {
    Ty *t = (Ty *)arena_alloc(cg->arena, sizeof(Ty));
    if (!t) return NULL;
    memset(t, 0, sizeof(Ty));
    t->tag = tag;
    return t;
}

static Ty *cg_ty_prim(Codegen *cg, const char *name) {
    Ty *t = cg_ty_new(cg, TY_PRIM);
    if (!t) return NULL;
    t->name = str_from_c(name);
    return t;
}

static Ty *cg_ty_class(Codegen *cg, Str name) {
    Ty *t = cg_ty_new(cg, TY_CLASS);
    if (!t) return NULL;
    t->name = name;
    return t;
}

static Ty *cg_ty_array(Codegen *cg, Ty *elem) {
    Ty *t = cg_ty_new(cg, TY_ARRAY);
    if (!t) return NULL;
    t->elem = elem;
    return t;
}

static Ty *cg_ty_void(Codegen *cg) {
    return cg_ty_new(cg, TY_VOID);
}

static Ty *cg_ty_gen(Codegen *cg, Str name) {
    Ty *t = cg_ty_new(cg, TY_GEN);
    if (!t) return NULL;
    t->name = name;
    return t;
}

static bool str_is_explicit_generic_name(Str s) {
    if (s.len == 0) return false;
    if (!(s.data[0] >= 'A' && s.data[0] <= 'Z')) return false;
    for (size_t i = 0; i < s.len; i++) {
        char c = s.data[i];
        if (!(c == '_' || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) {
            return false;
        }
    }
    return true;
}

static Ty *cg_ty_from_type_ref(Codegen *cg, TypeRef *tref, Str ctx_mod, Str *imports, size_t imports_len, Diag *err) {
    if (!tref) return NULL;
    if (tref->kind == TYPE_ARRAY) {
        Ty *elem = cg_ty_from_type_ref(cg, tref->as.elem, ctx_mod, imports, imports_len, err);
        return cg_ty_array(cg, elem);
    }
    Str n = tref->as.name;
    if (str_eq_c(n, "str")) n = str_from_c("string");
    if (str_eq_c(n, "bool") || str_eq_c(n, "string") || str_eq_c(n, "num") || str_eq_c(n, "any")) {
        return cg_ty_prim(cg, n.data);
    }
    if (str_eq_c(n, "void")) {
        return cg_ty_void(cg);
    }
    if (memchr(n.data, '.', n.len)) {
        size_t dot = 0;
        while (dot < n.len && n.data[dot] != '.') dot++;
        Str mod = { n.data, dot };
        bool in_scope = str_eq(mod, ctx_mod);
        for (size_t i = 0; i < imports_len && !in_scope; i++) {
            if (str_eq(imports[i], mod)) in_scope = true;
        }
        if (!in_scope) {
            cg_set_errf(err, ctx_mod, tref->line, tref->col, "unknown type '%.*s'", (int)n.len, n.data);
            return NULL;
        }
        if (codegen_class_info(cg, n)) {
            return cg_ty_class(cg, n);
        }
        cg_set_errf(err, ctx_mod, tref->line, tref->col, "unknown type '%.*s'", (int)n.len, n.data);
        return NULL;
    }
    // unqualified class name
    Str qname;
    size_t len = ctx_mod.len + 1 + n.len;
    char *buf = (char *)arena_alloc(cg->arena, len + 1);
    if (!buf) return NULL;
    memcpy(buf, ctx_mod.data, ctx_mod.len);
    buf[ctx_mod.len] = '.';
    memcpy(buf + ctx_mod.len + 1, n.data, n.len);
    buf[len] = '\0';
    qname.data = buf;
    qname.len = len;
    if (codegen_class_info(cg, qname)) {
        return cg_ty_class(cg, qname);
    }
    if (str_is_explicit_generic_name(n)) {
        return cg_ty_gen(cg, n);
    }
    cg_set_errf(err, ctx_mod, tref->line, tref->col, "unknown type '%.*s'", (int)n.len, n.data);
    return NULL;
}

// -----------------
// Lambda collection
// -----------------

static LambdaInfo *codegen_lambda_info(Codegen *cg, Expr *lam) {
    for (size_t i = 0; i < cg->lambdas_len; i++) {
        if (cg->lambdas[i].lam == lam) {
            return &cg->lambdas[i];
        }
    }
    return NULL;
}

static bool codegen_add_lambda(Codegen *cg, Expr *lam, Str path) {
    if (cg->lambdas_len + 1 > cg->lambdas_cap) {
        size_t next = cg->lambdas_cap ? cg->lambdas_cap * 2 : 8;
        LambdaInfo *arr = (LambdaInfo *)realloc(cg->lambdas, next * sizeof(LambdaInfo));
        if (!arr) return false;
        cg->lambdas = arr;
        cg->lambdas_cap = next;
    }
    LambdaInfo *li = &cg->lambdas[cg->lambdas_len++];
    li->lam = lam;
    li->path = path;
    li->name = codegen_new_lambda(cg);
    return li->name != NULL;
}

static FunValInfo *codegen_funval_info(Codegen *cg, Str cask, Str name) {
    for (size_t i = 0; i < cg->funvals_len; i++) {
        if (str_eq(cg->funvals[i].cask, cask) && str_eq(cg->funvals[i].name, name)) {
            return &cg->funvals[i];
        }
    }
    return NULL;
}

static bool codegen_add_funval(Codegen *cg, Str cask, Str name) {
    if (codegen_funval_info(cg, cask, name)) return true;
    if (cg->funvals_len + 1 > cg->funvals_cap) {
        size_t next = cg->funvals_cap ? cg->funvals_cap * 2 : 8;
        FunValInfo *arr = (FunValInfo *)realloc(cg->funvals, next * sizeof(FunValInfo));
        if (!arr) return false;
        cg->funvals = arr;
        cg->funvals_cap = next;
    }
    FunValInfo *fi = &cg->funvals[cg->funvals_len++];
    fi->cask = cask;
    fi->name = name;
    fi->wrapper = arena_printf(cg->arena, "__fnwrap_%s_%.*s", mangle_mod(cg->arena, cask), (int)name.len, name.data);
    return fi->wrapper != NULL;
}

static void collect_expr(Codegen *cg, Expr *e, Str path, bool allow_funval);
static void collect_stmt(Codegen *cg, Stmt *s, Str path);

static void collect_expr(Codegen *cg, Expr *e, Str path, bool allow_funval) {
    if (!e) return;
    if (e->kind == EXPR_LAMBDA) {
        if (!codegen_lambda_info(cg, e)) {
            codegen_add_lambda(cg, e, path);
        }
        collect_expr(cg, e->as.lambda.body, path, true);
        return;
    }
    switch (e->kind) {
        case EXPR_IDENT:
            if (allow_funval) {
                Str mod = codegen_cask_name(cg, path);
                FunSig *sig = codegen_fun_sig(cg, mod, e->as.ident.name);
                if (sig) {
                    codegen_add_funval(cg, sig->cask, sig->name);
                } else if (is_stdr_prelude(e->as.ident.name)) {
                    codegen_add_funval(cg, str_from_c("stdr"), e->as.ident.name);
                }
            }
            break;
        case EXPR_UNARY:
            collect_expr(cg, e->as.unary.x, path, true);
            break;
        case EXPR_BINARY:
            collect_expr(cg, e->as.binary.a, path, true);
            collect_expr(cg, e->as.binary.b, path, true);
            break;
        case EXPR_ASSIGN:
            collect_expr(cg, e->as.assign.target, path, true);
            collect_expr(cg, e->as.assign.value, path, true);
            break;
        case EXPR_CALL:
            collect_expr(cg, e->as.call.fn, path, false);
            for (size_t i = 0; i < e->as.call.args_len; i++) {
                collect_expr(cg, e->as.call.args[i], path, true);
            }
            break;
        case EXPR_INDEX:
            collect_expr(cg, e->as.index.a, path, true);
            collect_expr(cg, e->as.index.i, path, true);
            break;
        case EXPR_MEMBER:
            collect_expr(cg, e->as.member.a, path, true);
            if (e->as.member.a->kind == EXPR_IDENT && codegen_cask_in_scope(cg, e->as.member.a->as.ident.name)) {
                codegen_add_funval(cg, e->as.member.a->as.ident.name, e->as.member.name);
            }
            break;
        case EXPR_PAREN:
            collect_expr(cg, e->as.paren.x, path, true);
            break;
        case EXPR_TERNARY:
            collect_expr(cg, e->as.ternary.cond, path, true);
            collect_expr(cg, e->as.ternary.then_expr, path, true);
            collect_expr(cg, e->as.ternary.else_expr, path, true);
            break;
        case EXPR_MOVE:
            collect_expr(cg, e->as.move.x, path, true);
            break;
        case EXPR_ARRAY:
            for (size_t i = 0; i < e->as.array_lit.items_len; i++) {
                collect_expr(cg, e->as.array_lit.items[i], path, true);
            }
            break;
        case EXPR_DICT:
            for (size_t i = 0; i < e->as.dict_lit.pairs_len; i++) {
                collect_expr(cg, e->as.dict_lit.keys[i], path, true);
                collect_expr(cg, e->as.dict_lit.vals[i], path, true);
            }
            break;
        case EXPR_TUPLE:
            for (size_t i = 0; i < e->as.tuple_lit.items_len; i++) {
                collect_expr(cg, e->as.tuple_lit.items[i], path, true);
            }
            break;
        case EXPR_MATCH:
            collect_expr(cg, e->as.match_expr.scrut, path, true);
            for (size_t i = 0; i < e->as.match_expr.arms_len; i++) {
                collect_expr(cg, e->as.match_expr.arms[i]->expr, path, true);
            }
            break;
        case EXPR_BLOCK:
            collect_stmt(cg, e->as.block_expr.block, path);
            break;
        case EXPR_NEW:
            for (size_t i = 0; i < e->as.new_expr.args_len; i++) {
                collect_expr(cg, e->as.new_expr.args[i], path, true);
            }
            break;
        case EXPR_IF:
            for (size_t i = 0; i < e->as.if_expr.arms_len; i++) {
                ExprIfArm *arm = e->as.if_expr.arms[i];
                if (arm->cond) collect_expr(cg, arm->cond, path, true);
                collect_expr(cg, arm->value, path, true);
            }
            break;
        default:
            break;
    }
}

static void collect_stmt(Codegen *cg, Stmt *s, Str path) {
    if (!s) return;
    switch (s->kind) {
        case STMT_LET:
            collect_expr(cg, s->as.let_s.expr, path, true);
            break;
        case STMT_CONST:
            collect_expr(cg, s->as.const_s.expr, path, true);
            break;
        case STMT_EXPR:
            collect_expr(cg, s->as.expr_s.expr, path, true);
            break;
        case STMT_RETURN:
            if (s->as.ret_s.expr) collect_expr(cg, s->as.ret_s.expr, path, true);
            break;
        case STMT_IF:
            for (size_t i = 0; i < s->as.if_s.arms_len; i++) {
                IfArm *arm = s->as.if_s.arms[i];
                if (arm->cond) collect_expr(cg, arm->cond, path, true);
                collect_stmt(cg, arm->body, path);
            }
            break;
        case STMT_FOR:
            if (s->as.for_s.init) collect_stmt(cg, s->as.for_s.init, path);
            if (s->as.for_s.cond) collect_expr(cg, s->as.for_s.cond, path, true);
            if (s->as.for_s.step) collect_expr(cg, s->as.for_s.step, path, true);
            collect_stmt(cg, s->as.for_s.body, path);
            break;
        case STMT_FOREACH:
            collect_expr(cg, s->as.foreach_s.expr, path, true);
            collect_stmt(cg, s->as.foreach_s.body, path);
            break;
        case STMT_BREAK:
        case STMT_CONTINUE:
            break;
        case STMT_BLOCK:
            for (size_t i = 0; i < s->as.block_s.stmts_len; i++) {
                collect_stmt(cg, s->as.block_s.stmts[i], path);
            }
            break;
        default:
            break;
    }
}

// -----------------
// Free variable analysis for closures
// -----------------

typedef VEC(Str) StrVec;

static bool strvec_contains(StrVec *v, Str s) {
    for (size_t i = 0; i < v->len; i++) {
        if (str_eq(v->data[i], s)) return true;
    }
    return false;
}

static void fv_expr(Expr *e, StrVec *defined, StrVec *free_vars);
static void fv_stmt(Stmt *s, StrVec *defined, StrVec *free_vars);

static void fv_expr(Expr *e, StrVec *defined, StrVec *free_vars) {
    if (!e) return;
    switch (e->kind) {
        case EXPR_IDENT: {
            Str name = e->as.ident.name;
            if (!strvec_contains(defined, name) && !strvec_contains(free_vars, name)) {
                VEC_PUSH(*free_vars, name);
            }
            break;
        }
        case EXPR_STR: {
            if (e->as.str_lit.parts) {
                for (size_t i = 0; i < e->as.str_lit.parts->len; i++) {
                    StrPart *p = &e->as.str_lit.parts->parts[i];
                    if (p->kind == STR_PART_EXPR && p->as.expr) {
                        fv_expr(p->as.expr, defined, free_vars);
                    }
                }
            }
            break;
        }
        case EXPR_UNARY: fv_expr(e->as.unary.x, defined, free_vars); break;
        case EXPR_BINARY: fv_expr(e->as.binary.a, defined, free_vars); fv_expr(e->as.binary.b, defined, free_vars); break;
        case EXPR_ASSIGN: fv_expr(e->as.assign.target, defined, free_vars); fv_expr(e->as.assign.value, defined, free_vars); break;
        case EXPR_CALL:
            fv_expr(e->as.call.fn, defined, free_vars);
            for (size_t i = 0; i < e->as.call.args_len; i++) fv_expr(e->as.call.args[i], defined, free_vars);
            break;
        case EXPR_INDEX: fv_expr(e->as.index.a, defined, free_vars); fv_expr(e->as.index.i, defined, free_vars); break;
        case EXPR_MEMBER: fv_expr(e->as.member.a, defined, free_vars); break;
        case EXPR_PAREN: fv_expr(e->as.paren.x, defined, free_vars); break;
        case EXPR_MATCH:
            fv_expr(e->as.match_expr.scrut, defined, free_vars);
            for (size_t i = 0; i < e->as.match_expr.arms_len; i++) {
                MatchArm *arm = e->as.match_expr.arms[i];
                if (arm->pat && arm->pat->kind == PAT_IDENT) {
                    VEC_PUSH(*defined, arm->pat->as.name);
                }
                fv_expr(arm->expr, defined, free_vars);
            }
            break;
        case EXPR_LAMBDA: {
            for (size_t i = 0; i < e->as.lambda.params_len; i++) {
                VEC_PUSH(*defined, e->as.lambda.params[i]->name);
            }
            fv_expr(e->as.lambda.body, defined, free_vars);
            break;
        }
        case EXPR_BLOCK: fv_stmt(e->as.block_expr.block, defined, free_vars); break;
        case EXPR_NEW:
            for (size_t i = 0; i < e->as.new_expr.args_len; i++) fv_expr(e->as.new_expr.args[i], defined, free_vars);
            break;
        case EXPR_IF:
            for (size_t i = 0; i < e->as.if_expr.arms_len; i++) {
                ExprIfArm *arm = e->as.if_expr.arms[i];
                if (arm->cond) fv_expr(arm->cond, defined, free_vars);
                fv_expr(arm->value, defined, free_vars);
            }
            break;
        case EXPR_TERNARY:
            fv_expr(e->as.ternary.cond, defined, free_vars);
            fv_expr(e->as.ternary.then_expr, defined, free_vars);
            fv_expr(e->as.ternary.else_expr, defined, free_vars);
            break;
        case EXPR_MOVE: fv_expr(e->as.move.x, defined, free_vars); break;
        case EXPR_TUPLE:
            for (size_t i = 0; i < e->as.tuple_lit.items_len; i++) fv_expr(e->as.tuple_lit.items[i], defined, free_vars);
            break;
        case EXPR_ARRAY:
            for (size_t i = 0; i < e->as.array_lit.items_len; i++) fv_expr(e->as.array_lit.items[i], defined, free_vars);
            break;
        case EXPR_DICT:
            for (size_t i = 0; i < e->as.dict_lit.pairs_len; i++) {
                fv_expr(e->as.dict_lit.keys[i], defined, free_vars);
                fv_expr(e->as.dict_lit.vals[i], defined, free_vars);
            }
            break;
        default: break;
    }
}

static void fv_stmt(Stmt *s, StrVec *defined, StrVec *free_vars) {
    if (!s) return;
    switch (s->kind) {
        case STMT_LET:
            fv_expr(s->as.let_s.expr, defined, free_vars);
            VEC_PUSH(*defined, s->as.let_s.name);
            break;
        case STMT_CONST:
            fv_expr(s->as.const_s.expr, defined, free_vars);
            VEC_PUSH(*defined, s->as.const_s.name);
            break;
        case STMT_EXPR: fv_expr(s->as.expr_s.expr, defined, free_vars); break;
        case STMT_RETURN: fv_expr(s->as.ret_s.expr, defined, free_vars); break;
        case STMT_IF:
            for (size_t i = 0; i < s->as.if_s.arms_len; i++) {
                fv_expr(s->as.if_s.arms[i]->cond, defined, free_vars);
                fv_stmt(s->as.if_s.arms[i]->body, defined, free_vars);
            }
            break;
        case STMT_FOR:
            fv_stmt(s->as.for_s.init, defined, free_vars);
            fv_expr(s->as.for_s.cond, defined, free_vars);
            fv_expr(s->as.for_s.step, defined, free_vars);
            fv_stmt(s->as.for_s.body, defined, free_vars);
            break;
        case STMT_FOREACH:
            fv_expr(s->as.foreach_s.expr, defined, free_vars);
            VEC_PUSH(*defined, s->as.foreach_s.name);
            fv_stmt(s->as.foreach_s.body, defined, free_vars);
            break;
        case STMT_BREAK:
        case STMT_CONTINUE:
            break;
        case STMT_BLOCK:
            for (size_t i = 0; i < s->as.block_s.stmts_len; i++) {
                fv_stmt(s->as.block_s.stmts[i], defined, free_vars);
            }
            break;
    }
}

static void codegen_collect_lambdas(Codegen *cg) {
    for (size_t i = 0; i < cg->prog->mods_len; i++) {
        Module *m = cg->prog->mods[i];
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind == DECL_FUN) {
                collect_stmt(cg, d->as.fun.body, m->path);
            } else if (d->kind == DECL_CLASS) {
                for (size_t k = 0; k < d->as.class_decl.methods_len; k++) {
                    collect_stmt(cg, d->as.class_decl.methods[k]->body, m->path);
                }
            } else if (d->kind == DECL_ENTRY) {
                collect_stmt(cg, d->as.entry.body, m->path);
            } else if (d->kind == DECL_DEF) {
                collect_expr(cg, d->as.def_decl.expr, m->path, true);
            }
        }
    }
}

// -----------------
// Expr generation
// -----------------

typedef struct {
    char *tmp;
    VEC(char *) cleanup;
} GenExpr;

static void gen_expr_init(GenExpr *ge) {
    ge->tmp = NULL;
    ge->cleanup.data = NULL;
    ge->cleanup.len = 0;
    ge->cleanup.cap = 0;
}

static void gen_expr_free(GenExpr *ge) {
    VEC_FREE(ge->cleanup);
    ge->tmp = NULL;
}

static void gen_expr_add(GenExpr *ge, char *tmp) {
    VEC_PUSH(ge->cleanup, tmp);
}

static void gen_expr_release_except(Codegen *cg, GenExpr *ge, char *keep) {
    for (size_t i = 0; i < ge->cleanup.len; i++) {
        char *v = ge->cleanup.data[i];
        if (v != keep) {
            w_line(&cg->w, "yis_release_val(%s);", v);
        }
    }
}

static Ty *cg_tc_expr(Codegen *cg, Str path, Expr *e, Diag *err) {
    Ctx ctx = codegen_ctx_for(cg, path);
    Ty *t = tc_expr_ctx(e, &ctx, &cg->ty_loc, cg->env, err);
    return t;
}

static bool is_assign_op(TokKind op) {
    switch (op) {
        case TOK_EQ:
        case TOK_PLUSEQ:
        case TOK_MINUSEQ:
        case TOK_STAREQ:
        case TOK_SLASHEQ:
        case TOK_PERCENTEQ:
            return true;
        default:
            return false;
    }
}

static const char *compound_assign_opfn(TokKind op) {
    switch (op) {
        case TOK_PLUSEQ: return "yis_add";
        case TOK_MINUSEQ: return "yis_sub";
        case TOK_STAREQ: return "yis_mul";
        case TOK_SLASHEQ: return "yis_div";
        case TOK_PERCENTEQ: return "yis_mod";
        default: return NULL;
    }
}

static bool gen_expr(Codegen *cg, Str path, Expr *e, GenExpr *out, Diag *err);

static bool gen_if_expr_chain(Codegen *cg,
                              Str path,
                              ExprIfArm **arms,
                              size_t idx,
                              size_t arms_len,
                              char *result_tmp,
                              Diag *err) {
    if (idx >= arms_len) return true;
    ExprIfArm *arm = arms[idx];
    if (!arm->cond) {
        GenExpr v;
        if (!gen_expr(cg, path, arm->value, &v, err)) return false;
        w_line(&cg->w, "yis_move_into(&%s, %s);", result_tmp, v.tmp);
        gen_expr_release_except(cg, &v, v.tmp);
        gen_expr_free(&v);
        return true;
    }

    GenExpr cond;
    if (!gen_expr(cg, path, arm->cond, &cond, err)) return false;
    cg->var_id++;
    char *bname = arena_printf(cg->arena, "__b%d", cg->var_id);
    w_line(&cg->w, "bool %s = yis_as_bool(%s);", bname, cond.tmp);
    w_line(&cg->w, "yis_release_val(%s);", cond.tmp);
    gen_expr_release_except(cg, &cond, cond.tmp);
    gen_expr_free(&cond);

    w_line(&cg->w, "if (%s) {", bname);
    cg->w.indent++;
    GenExpr v;
    if (!gen_expr(cg, path, arm->value, &v, err)) return false;
    w_line(&cg->w, "yis_move_into(&%s, %s);", result_tmp, v.tmp);
    gen_expr_release_except(cg, &v, v.tmp);
    gen_expr_free(&v);
    cg->w.indent--;

    if (idx + 1 < arms_len) {
        w_line(&cg->w, "} else {");
        cg->w.indent++;
        if (!gen_if_expr_chain(cg, path, arms, idx + 1, arms_len, result_tmp, err)) return false;
        cg->w.indent--;
        w_line(&cg->w, "}");
    } else {
        w_line(&cg->w, "}");
    }
    return true;
}

static bool gen_expr(Codegen *cg, Str path, Expr *e, GenExpr *out, Diag *err) {
    gen_expr_init(out);
    if (!e) {
        return cg_set_err(err, path, "codegen: null expr");
    }

    switch (e->kind) {
        case EXPR_INT: {
            char *t = codegen_new_tmp(cg);
            if (!t) return cg_set_err(err, path, "out of memory");
            w_line(&cg->w, "YisVal %s = YV_INT(%lld);", t, e->as.int_lit.v);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_FLOAT: {
            char *t = codegen_new_tmp(cg);
            if (!t) return cg_set_err(err, path, "out of memory");
            w_line(&cg->w, "YisVal %s = YV_FLOAT(%.17g);", t, e->as.float_lit.v);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_BOOL: {
            char *t = codegen_new_tmp(cg);
            if (!t) return cg_set_err(err, path, "out of memory");
            w_line(&cg->w, "YisVal %s = YV_BOOL(%s);", t, e->as.bool_lit.v ? "true" : "false");
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_NULL: {
            char *t = codegen_new_tmp(cg);
            if (!t) return cg_set_err(err, path, "out of memory");
            w_line(&cg->w, "YisVal %s = YV_NULLV;", t);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_STR: {
            StrParts *parts = e->as.str_lit.parts;
            if (!parts || parts->len == 0) {
                char *t = codegen_new_tmp(cg);
                if (!t) return cg_set_err(err, path, "out of memory");
                w_line(&cg->w, "YisVal %s = YV_STR(stdr_str_lit(\"\"));", t);
                gen_expr_add(out, t);
                out->tmp = t;
                return true;
            }
            char **part_tmps = (char **)malloc(parts->len * sizeof(char *));
            if (!part_tmps) return cg_set_err(err, path, "out of memory");
            for (size_t i = 0; i < parts->len; i++) {
                StrPart *p = &parts->parts[i];
                char *pt = codegen_new_tmp(cg);
                if (!pt) { free(part_tmps); return cg_set_err(err, path, "out of memory"); }
                if (p->kind == STR_PART_TEXT) {
                    char *esc = c_escape(cg->arena, p->as.text);
                    w_line(&cg->w, "YisVal %s = YV_STR(stdr_str_lit(\"%s\"));", pt, esc ? esc : "");
                } else if (p->kind == STR_PART_EXPR && p->as.expr) {
                    GenExpr pe;
                    if (!gen_expr(cg, path, p->as.expr, &pe, err)) {
                        free(part_tmps);
                        return false;
                    }
                    w_line(&cg->w, "YisVal %s = %s; yis_retain_val(%s);", pt, pe.tmp, pt);
                    gen_expr_release_except(cg, &pe, NULL);
                    gen_expr_free(&pe);
                } else {
                    free(part_tmps);
                    return cg_set_errf(err, path, e->line, e->col, "%.*s: invalid interpolation part", (int)path.len, path.data);
                }
                part_tmps[i] = pt;
            }
            char *parts_name = codegen_new_sym(cg, "parts");
            char *s_name = codegen_new_sym(cg, "s");
            char *arr = codegen_new_tmp(cg);
            w_line(&cg->w, "YisVal %s = YV_NULLV;", arr);
            w_line(&cg->w, "{");
            cg->w.indent++;
            {
                StrBuf line;
                sb_init(&line);
                sb_appendf(&line, "YisVal %s[%zu] = { ", parts_name, parts->len);
                for (size_t i = 0; i < parts->len; i++) {
                    if (i) sb_append(&line, ", ");
                    sb_append(&line, part_tmps[i]);
                }
                sb_append(&line, " };");
                w_line(&cg->w, "%s", line.data ? line.data : "");
                sb_free(&line);
            }
            w_line(&cg->w, "YisStr* %s = stdr_str_from_parts(%zu, %s);", s_name, parts->len, parts_name);
            w_line(&cg->w, "%s = YV_STR(%s);", arr, s_name);
            cg->w.indent--;
            w_line(&cg->w, "}");
            for (size_t i = 0; i < parts->len; i++) {
                w_line(&cg->w, "yis_release_val(%s);", part_tmps[i]);
            }
            free(part_tmps);
            gen_expr_add(out, arr);
            out->tmp = arr;
            return true;
        }
        case EXPR_TUPLE: {
            cg->arr_id++;
            char *arrsym = arena_printf(cg->arena, "__tup%d", cg->arr_id);
            char *t = codegen_new_tmp(cg);
            w_line(&cg->w, "YisArr* %s = stdr_arr_new(%zu);", arrsym, e->as.tuple_lit.items_len);
            w_line(&cg->w, "YisVal %s = YV_ARR(%s);", t, arrsym);
            for (size_t i = 0; i < e->as.tuple_lit.items_len; i++) {
                GenExpr ge;
                if (!gen_expr(cg, path, e->as.tuple_lit.items[i], &ge, err)) return false;
                w_line(&cg->w, "yis_arr_add(%s, %s);", arrsym, ge.tmp);
                gen_expr_release_except(cg, &ge, ge.tmp);
                gen_expr_free(&ge);
            }
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_ARRAY: {
            cg->arr_id++;
            char *arrsym = arena_printf(cg->arena, "__a%d", cg->arr_id);
            char *t = codegen_new_tmp(cg);
            w_line(&cg->w, "YisArr* %s = stdr_arr_new(%zu);", arrsym, e->as.array_lit.items_len);
            w_line(&cg->w, "YisVal %s = YV_ARR(%s);", t, arrsym);
            for (size_t i = 0; i < e->as.array_lit.items_len; i++) {
                GenExpr ge;
                if (!gen_expr(cg, path, e->as.array_lit.items[i], &ge, err)) return false;
                w_line(&cg->w, "yis_retain_val(%s);", ge.tmp);
                w_line(&cg->w, "yis_arr_add(%s, %s);", arrsym, ge.tmp);
                gen_expr_release_except(cg, &ge, ge.tmp);
                gen_expr_free(&ge);
            }
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_DICT: {
            cg->arr_id++;
            char *dictsym = arena_printf(cg->arena, "__d%d", cg->arr_id);
            char *t = codegen_new_tmp(cg);
            w_line(&cg->w, "YisDict* %s = stdr_dict_new();", dictsym);
            w_line(&cg->w, "YisVal %s = YV_DICT(%s);", t, dictsym);
            for (size_t i = 0; i < e->as.dict_lit.pairs_len; i++) {
                GenExpr ke, ve;
                if (!gen_expr(cg, path, e->as.dict_lit.keys[i], &ke, err)) return false;
                if (!gen_expr(cg, path, e->as.dict_lit.vals[i], &ve, err)) { gen_expr_free(&ke); return false; }
                w_line(&cg->w, "yis_dict_set(%s, %s, %s);", dictsym, ke.tmp, ve.tmp);
                w_line(&cg->w, "yis_release_val(%s);", ke.tmp);
                w_line(&cg->w, "yis_release_val(%s);", ve.tmp);
                gen_expr_release_except(cg, &ke, ke.tmp);
                gen_expr_release_except(cg, &ve, ve.tmp);
                gen_expr_free(&ke);
                gen_expr_free(&ve);
            }
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_IDENT: {
            char *t = codegen_new_tmp(cg);
            char *cname = codegen_cname_of(cg, e->as.ident.name);
            if (!cname) {
                FunSig *sig = codegen_fun_sig(cg, cg->current_cask, e->as.ident.name);
                if (!sig && is_stdr_prelude(e->as.ident.name)) {
                    bool allow = str_eq_c(cg->current_cask, "stdr");
                    for (size_t i = 0; i < cg->current_imports_len && !allow; i++) {
                        if (str_eq_c(cg->current_imports[i], "stdr")) allow = true;
                    }
                    if (allow) {
                        sig = codegen_fun_sig(cg, str_from_c("stdr"), e->as.ident.name);
                    }
                }
                if (!sig) {
                    return cg_set_errf(err, path, e->line, e->col, "unknown local '%.*s'", (int)e->as.ident.name.len, e->as.ident.name.data);
                }
                FunValInfo *fi = codegen_funval_info(cg, sig->cask, sig->name);
                if (!fi || !fi->wrapper) {
                    return cg_set_err(err, path, "missing function wrapper (internal error)");
                }
                w_line(&cg->w, "YisVal %s = YV_FN(yi_fn_new(%s, %zu));", t, fi->wrapper, sig->params_len);
                gen_expr_add(out, t);
                out->tmp = t;
                return true;
            }
            w_line(&cg->w, "YisVal %s = %s; yis_retain_val(%s);", t, cname, t);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_MEMBER: {
            Ty *base_ty = cg_tc_expr(cg, path, e->as.member.a, err);
            if (!base_ty) return false;
            if (base_ty->tag == TY_MOD) {
                ModuleConsts *mc = codegen_cask_consts(cg, base_ty->name);
                ConstEntry *ce = mc ? codegen_find_const(mc, e->as.member.name) : NULL;
                if (ce) {
                    char *t = codegen_new_tmp(cg);
                    if (ce->val.ty && ce->val.ty->tag == TY_PRIM && str_eq_c(ce->val.ty->name, "num")) {
                        if (ce->val.is_float) {
                            w_line(&cg->w, "YisVal %s = YV_FLOAT(%.17g);", t, ce->val.f);
                        } else {
                            w_line(&cg->w, "YisVal %s = YV_INT(%lld);", t, ce->val.i);
                        }
                    } else if (ce->val.ty && ce->val.ty->tag == TY_PRIM && str_eq_c(ce->val.ty->name, "bool")) {
                        w_line(&cg->w, "YisVal %s = YV_BOOL(%s);", t, ce->val.b ? "true" : "false");
                    } else if (ce->val.ty && ce->val.ty->tag == TY_PRIM && str_eq_c(ce->val.ty->name, "string")) {
                        char *esc = c_escape(cg->arena, ce->val.s);
                        w_line(&cg->w, "YisVal %s = YV_STR(stdr_str_lit(\"%s\"));", t, esc ? esc : "");
                    } else if (ce->val.ty && ce->val.ty->tag == TY_NULL) {
                        w_line(&cg->w, "YisVal %s = YV_NULLV;", t);
                    } else {
                        return cg_set_err(err, path, "unsupported const type");
                    }
                    gen_expr_add(out, t);
                    out->tmp = t;
                    return true;
                }
                ModuleGlobals *mg = codegen_cask_globals(cg, base_ty->name);
                if (codegen_find_global(mg, e->as.member.name)) {
                    char *t = codegen_new_tmp(cg);
                    char *gname = mangle_global_var(cg->arena, base_ty->name, e->as.member.name);
                    w_line(&cg->w, "YisVal %s = %s; yis_retain_val(%s);", t, gname, t);
                    gen_expr_add(out, t);
                    out->tmp = t;
                    return true;
                }
                return cg_set_errf(err, path, e->line, e->col, "unknown cask member '%.*s.%.*s'", (int)base_ty->name.len, base_ty->name.data, (int)e->as.member.name.len, e->as.member.name.data);
            }
            if (base_ty->tag == TY_CLASS) {
                GenExpr ge;
                if (!gen_expr(cg, path, e->as.member.a, &ge, err)) return false;
                char *t = codegen_new_tmp(cg);
                char *cname = codegen_c_class_name(cg, base_ty->name);
                char *field = codegen_c_field_name(cg, e->as.member.name);
                w_line(&cg->w, "YisVal %s = ((%s*)%s.as.p)->%s; yis_retain_val(%s);", t, cname, ge.tmp, field, t);
                w_line(&cg->w, "yis_release_val(%s);", ge.tmp);
                gen_expr_release_except(cg, &ge, ge.tmp);
                gen_expr_free(&ge);
                gen_expr_add(out, t);
                out->tmp = t;
                return true;
            }
            return cg_set_err(err, path, "member access not supported on this type");
        }
        case EXPR_MOVE: {
            if (!e->as.move.x || e->as.move.x->kind != EXPR_IDENT) {
                return cg_set_err(err, path, "move(...) must be an identifier");
            }
            char *slot = codegen_cname_of(cg, e->as.move.x->as.ident.name);
            if (!slot) return cg_set_err(err, path, "unknown move target");
            char *t = codegen_new_tmp(cg);
            w_line(&cg->w, "YisVal %s = yis_move(&%s);", t, slot);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_MATCH: {
            Ty *scrut_ty = cg_tc_expr(cg, path, e->as.match_expr.scrut, err);
            if (!scrut_ty) return false;
            GenExpr scrut;
            if (!gen_expr(cg, path, e->as.match_expr.scrut, &scrut, err)) return false;
            char *t = codegen_new_tmp(cg);
            char *matched = codegen_new_sym(cg, "matched");
            w_line(&cg->w, "YisVal %s = YV_NULLV;", t);
            w_line(&cg->w, "bool %s = false;", matched);
            for (size_t i = 0; i < e->as.match_expr.arms_len; i++) {
                MatchArm *arm = e->as.match_expr.arms[i];
                char *cond_name = codegen_new_sym(cg, "mc");
                char *bind_tmp = NULL;
                Str bind_name = {NULL, 0};
                if (arm->pat->kind == PAT_WILD) {
                    w_line(&cg->w, "bool %s = true;", cond_name);
                } else if (arm->pat->kind == PAT_IDENT) {
                    bind_name = arm->pat->as.name;
                    w_line(&cg->w, "bool %s = true;", cond_name);
                } else {
                    char *pv = NULL;
                    if (arm->pat->kind == PAT_INT) {
                        pv = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = YV_INT(%lld);", pv, arm->pat->as.i);
                    } else if (arm->pat->kind == PAT_BOOL) {
                        pv = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = YV_BOOL(%s);", pv, arm->pat->as.b ? "true" : "false");
                    } else if (arm->pat->kind == PAT_NULL) {
                        pv = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = YV_NULLV;", pv);
                    } else if (arm->pat->kind == PAT_STR) {
                        Expr tmp_expr;
                        memset(&tmp_expr, 0, sizeof(tmp_expr));
                        tmp_expr.kind = EXPR_STR;
                        tmp_expr.as.str_lit.parts = arm->pat->as.str;
                        GenExpr ge;
                        if (!gen_expr(cg, path, &tmp_expr, &ge, err)) return false;
                        pv = ge.tmp;
                        gen_expr_release_except(cg, &ge, pv);
                        gen_expr_free(&ge);
                    } else {
                        return cg_set_err(err, path, "unsupported match pattern in codegen");
                    }
                    char *eqt = codegen_new_tmp(cg);
                    w_line(&cg->w, "YisVal %s = yis_eq(%s, %s);", eqt, scrut.tmp, pv);
                    w_line(&cg->w, "bool %s = yis_as_bool(%s);", cond_name, eqt);
                    w_line(&cg->w, "yis_release_val(%s);", eqt);
                    w_line(&cg->w, "yis_release_val(%s);", pv);
                }
                w_line(&cg->w, "if (!%s && %s) {", matched, cond_name);
                cg->w.indent++;
                w_line(&cg->w, "%s = true;", matched);
                bool pushed = false;
                if (bind_name.data) {
                    bind_tmp = codegen_new_tmp(cg);
                    w_line(&cg->w, "YisVal %s = %s; yis_retain_val(%s);", bind_tmp, scrut.tmp, bind_tmp);
                    codegen_push_scope(cg);
                    pushed = true;
                    codegen_bind_temp(cg, bind_name, bind_tmp, scrut_ty);
                }
                GenExpr arm_expr;
                if (!gen_expr(cg, path, arm->expr, &arm_expr, err)) return false;
                w_line(&cg->w, "yis_move_into(&%s, %s);", t, arm_expr.tmp);
                gen_expr_release_except(cg, &arm_expr, arm_expr.tmp);
                gen_expr_free(&arm_expr);
                if (bind_tmp && pushed) {
                    LocalList locals = codegen_pop_scope(cg);
                    codegen_release_scope(cg, locals);
                    w_line(&cg->w, "yis_release_val(%s);", bind_tmp);
                }
                cg->w.indent--;
                w_line(&cg->w, "}");
            }
            w_line(&cg->w, "yis_release_val(%s);", scrut.tmp);
            gen_expr_release_except(cg, &scrut, scrut.tmp);
            gen_expr_free(&scrut);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_LAMBDA: {
            LambdaInfo *li = codegen_lambda_info(cg, e);
            if (!li) {
                codegen_add_lambda(cg, e, path);
                li = codegen_lambda_info(cg, e);
            }
            // Collect free variables and populate captures
            StrVec defined = {0};
            StrVec free_vars = {0};
            for (size_t p = 0; p < e->as.lambda.params_len; p++) {
                VEC_PUSH(defined, e->as.lambda.params[p]->name);
            }
            fv_expr(e->as.lambda.body, &defined, &free_vars);
            // Filter: only capture variables that resolve in current local scope
            size_t cap_count = 0;
            Capture **caps = NULL;
            for (size_t fi = 0; fi < free_vars.len; fi++) {
                Str name = free_vars.data[fi];
                // Check local scopes only (not cask globals/functions - those are always accessible)
                char *cname = NULL;
                for (size_t si = cg->scopes_len; si-- > 0;) {
                    NameScope *ns = &cg->scopes[si];
                    for (size_t ni = 0; ni < ns->len; ni++) {
                        if (str_eq(ns->items[ni].name, name)) {
                            cname = ns->items[ni].cname;
                            goto found_local;
                        }
                    }
                }
                found_local:
                if (!cname) continue;
                Binding *b = locals_lookup(&cg->ty_loc, name);
                Capture *cap = (Capture *)arena_alloc(cg->arena, sizeof(Capture));
                cap->name = name;
                cap->cname = cname;
                cap->ty = b ? (void *)b->ty : NULL;
                caps = (Capture **)realloc(caps, (cap_count + 1) * sizeof(Capture *));
                caps[cap_count++] = cap;
            }
            free(defined.data);
            free(free_vars.data);
            e->as.lambda.captures = caps;
            e->as.lambda.captures_len = cap_count;

            char *t = codegen_new_tmp(cg);
            if (cap_count > 0) {
                char *env_name = codegen_new_sym(cg, "env");
                w_line(&cg->w, "YisVal* %s = (YisVal*)malloc(sizeof(YisVal) * %zu);", env_name, cap_count);
                for (size_t ci = 0; ci < cap_count; ci++) {
                    w_line(&cg->w, "%s[%zu] = %s; yis_retain_val(%s[%zu]);", env_name, ci, caps[ci]->cname, env_name, ci);
                }
                w_line(&cg->w, "YisVal %s = YV_FN(yi_fn_new_with_env(%s, %zu, %s, %zu));", t, li->name, e->as.lambda.params_len, env_name, cap_count);
            } else {
                w_line(&cg->w, "YisVal %s = YV_FN(yi_fn_new(%s, %zu));", t, li->name, e->as.lambda.params_len);
            }
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_NEW: {
            Str name = e->as.new_expr.name;
            Str qname = name;
            if (!memchr(name.data, '.', name.len)) {
                size_t len = cg->current_cask.len + 1 + name.len;
                char *buf = (char *)arena_alloc(cg->arena, len + 1);
                if (!buf) return cg_set_err(err, path, "out of memory");
                memcpy(buf, cg->current_cask.data, cg->current_cask.len);
                buf[cg->current_cask.len] = '.';
                memcpy(buf + cg->current_cask.len + 1, name.data, name.len);
                buf[len] = '\0';
                qname.data = buf;
                qname.len = len;
            } else {
                size_t dot = 0;
                while (dot < name.len && name.data[dot] != '.') dot++;
                Str mod = { name.data, dot };
                if (!codegen_cask_in_scope(cg, mod)) {
                    return cg_set_errf(err, path, e->line, e->col, "unknown class '%.*s'", (int)name.len, name.data);
                }
            }
            ClassDecl *decl = codegen_class_decl(cg, qname);
            if (!decl) {
                return cg_set_errf(err, path, e->line, e->col, "unknown class '%.*s'", (int)name.len, name.data);
            }
            Str mod, cls_short;
            split_qname(qname, &mod, &cls_short);
            char *cname = codegen_c_class_name(cg, qname);
            char *drop_sym = mod.len ? arena_printf(cg->arena, "yis_drop_%s_%.*s", mangle_mod(cg->arena, mod), (int)cls_short.len, cls_short.data)
                                      : arena_printf(cg->arena, "yis_drop_%.*s", (int)cls_short.len, cls_short.data);
            char *obj_name = codegen_new_sym(cg, "obj");
            w_line(&cg->w, "%s* %s = (%s*)yis_obj_new(sizeof(%s), %s);", cname, obj_name, cname, cname, drop_sym);
            for (size_t i = 0; i < decl->fields_len; i++) {
                FieldDecl *fd = decl->fields[i];
                w_line(&cg->w, "%s->%s = YV_NULLV;", obj_name, codegen_c_field_name(cg, fd->name));
            }
            char *t = codegen_new_tmp(cg);
            w_line(&cg->w, "YisVal %s = YV_OBJ(%s);", t, obj_name);

            ClassInfo *ci = codegen_class_info(cg, qname);
            MethodEntry *init = NULL;
            if (ci) {
                for (size_t i = 0; i < ci->methods_len; i++) {
                    if (str_eq_c(ci->methods[i].name, "init")) {
                        init = &ci->methods[i];
                        break;
                    }
                }
            }
            bool has_named = false;
            for (size_t i = 0; i < e->as.new_expr.args_len; i++) {
                if (e->as.new_expr.arg_names && e->as.new_expr.arg_names[i].len > 0) {
                    has_named = true;
                    break;
                }
            }
            if (has_named) {
                for (size_t i = 0; i < e->as.new_expr.args_len; i++) {
                    Str aname = e->as.new_expr.arg_names[i];
                    GenExpr ge;
                    if (!gen_expr(cg, path, e->as.new_expr.args[i], &ge, err)) return false;
                    bool found = false;
                    for (size_t f = 0; f < decl->fields_len; f++) {
                        FieldDecl *fd = decl->fields[f];
                        if (str_eq(fd->name, aname)) {
                            w_line(&cg->w, "yis_move_into(&%s->%s, %s);", obj_name, codegen_c_field_name(cg, fd->name), ge.tmp);
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        gen_expr_free(&ge);
                        return cg_set_errf(err, path, e->line, e->col, "unknown field '%.*s' in constructor", (int)aname.len, aname.data);
                    }
                    gen_expr_release_except(cg, &ge, ge.tmp);
                    gen_expr_free(&ge);
                }
            } else if (!init && e->as.new_expr.args_len > 0 &&
                       (decl->kind == CLASS_KIND_STRUCT || decl->kind == CLASS_KIND_ENUM)) {
                if (e->as.new_expr.args_len != decl->fields_len) {
                    return cg_set_errf(err, path, e->line, e->col, "'%.*s' expects %zu args", (int)decl->name.len, decl->name.data, decl->fields_len);
                }
                for (size_t i = 0; i < e->as.new_expr.args_len; i++) {
                    GenExpr ge;
                    if (!gen_expr(cg, path, e->as.new_expr.args[i], &ge, err)) return false;
                    FieldDecl *fd = decl->fields[i];
                    w_line(&cg->w, "yis_move_into(&%s->%s, %s);", obj_name, codegen_c_field_name(cg, fd->name), ge.tmp);
                    gen_expr_release_except(cg, &ge, ge.tmp);
                    gen_expr_free(&ge);
                }
            } else if (init) {
                    VEC(char *) arg_ts = VEC_INIT;
                    for (size_t i = 0; i < e->as.new_expr.args_len; i++) {
                        GenExpr ge;
                        if (!gen_expr(cg, path, e->as.new_expr.args[i], &ge, err)) { VEC_FREE(arg_ts); return false; }
                        VEC_PUSH(arg_ts, ge.tmp);
                        gen_expr_release_except(cg, &ge, ge.tmp);
                        gen_expr_free(&ge);
                    }
                    // call init
                    Str mname = str_from_c("init");
                    char *mangled = mangle_method(cg->arena, mod, cls_short, mname);
                    StrBuf line; sb_init(&line);
                    sb_appendf(&line, "%s(%s", mangled, t);
                    for (size_t i = 0; i < arg_ts.len; i++) {
                        sb_append(&line, ", ");
                        sb_append(&line, arg_ts.data[i]);
                    }
                    sb_append(&line, ");");
                    w_line(&cg->w, "%s", line.data ? line.data : "");
                    sb_free(&line);
                    for (size_t i = 0; i < arg_ts.len; i++) {
                        w_line(&cg->w, "yis_release_val(%s);", arg_ts.data[i]);
                    }
                    VEC_FREE(arg_ts);
            }
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_IF: {
            char *t = codegen_new_tmp(cg);
            w_line(&cg->w, "YisVal %s = YV_NULLV;", t);
            if (!gen_if_expr_chain(cg, path, e->as.if_expr.arms, 0, e->as.if_expr.arms_len, t, err)) return false;
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_UNARY: {
            GenExpr ge;
            if (!gen_expr(cg, path, e->as.unary.x, &ge, err)) return false;
            char *t = codegen_new_tmp(cg);
            if (e->as.unary.op == TOK_BANG) {
                w_line(&cg->w, "YisVal %s = YV_BOOL(!yis_as_bool(%s));", t, ge.tmp);
            } else if (e->as.unary.op == TOK_MINUS) {
                Ty *xty = cg_tc_expr(cg, path, e->as.unary.x, err);
                if (!xty) return false;
                if (xty->tag == TY_PRIM && str_eq_c(xty->name, "num")) {
                    w_line(&cg->w, "YisVal %s = yis_neg(%s);", t, ge.tmp);
                } else {
                    w_line(&cg->w, "YisVal %s = YV_INT(-yis_as_int(%s));", t, ge.tmp);
                }
            } else {
                gen_expr_free(&ge);
                return cg_set_err(err, path, "unsupported unary op");
            }
            w_line(&cg->w, "yis_release_val(%s);", ge.tmp);
            gen_expr_release_except(cg, &ge, ge.tmp);
            gen_expr_free(&ge);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_BINARY: {
            TokKind op = e->as.binary.op;
            // Constant folding: if both operands are numeric literals, compute at compile time
            if (op != TOK_ANDAND && op != TOK_OROR && op != TOK_QQ) {
                Expr *ea = e->as.binary.a;
                Expr *eb = e->as.binary.b;
                bool a_int = ea && ea->kind == EXPR_INT;
                bool a_flt = ea && ea->kind == EXPR_FLOAT;
                bool b_int = eb && eb->kind == EXPR_INT;
                bool b_flt = eb && eb->kind == EXPR_FLOAT;
                if ((a_int || a_flt) && (b_int || b_flt)) {
                    bool is_float = a_flt || b_flt;
                    double ad = a_int ? (double)ea->as.int_lit.v : ea->as.float_lit.v;
                    double bd = b_int ? (double)eb->as.int_lit.v : eb->as.float_lit.v;
                    long long ai = a_int ? ea->as.int_lit.v : (long long)ea->as.float_lit.v;
                    long long bi = b_int ? eb->as.int_lit.v : (long long)eb->as.float_lit.v;
                    char *t = codegen_new_tmp(cg);
                    if (!t) return cg_set_err(err, path, "out of memory");
                    bool folded = true;
                    if (is_float) {
                        double result = 0;
                        switch (op) {
                            case TOK_PLUS:  result = ad + bd; break;
                            case TOK_MINUS: result = ad - bd; break;
                            case TOK_STAR:  result = ad * bd; break;
                            case TOK_SLASH: result = bd != 0 ? ad / bd : 0; break;
                            default: folded = false; break;
                        }
                        if (folded) {
                            w_line(&cg->w, "YisVal %s = YV_FLOAT(%.17g);", t, result);
                            gen_expr_add(out, t);
                            out->tmp = t;
                            return true;
                        }
                        // Comparison ops produce bool
                        bool boolresult = false;
                        folded = true;
                        switch (op) {
                            case TOK_EQEQ: boolresult = ad == bd; break;
                            case TOK_NEQ:  boolresult = ad != bd; break;
                            case TOK_LT:   boolresult = ad < bd; break;
                            case TOK_LTE:  boolresult = ad <= bd; break;
                            case TOK_GT:   boolresult = ad > bd; break;
                            case TOK_GTE:  boolresult = ad >= bd; break;
                            default: folded = false; break;
                        }
                        if (folded) {
                            w_line(&cg->w, "YisVal %s = YV_BOOL(%d);", t, boolresult ? 1 : 0);
                            gen_expr_add(out, t);
                            out->tmp = t;
                            return true;
                        }
                    } else {
                        // Both are ints
                        long long result = 0;
                        switch (op) {
                            case TOK_PLUS:    result = ai + bi; break;
                            case TOK_MINUS:   result = ai - bi; break;
                            case TOK_STAR:    result = ai * bi; break;
                            case TOK_SLASH:   result = bi != 0 ? ai / bi : 0; break;
                            case TOK_PERCENT: result = bi != 0 ? ai % bi : 0; break;
                            default: folded = false; break;
                        }
                        if (folded) {
                            w_line(&cg->w, "YisVal %s = YV_INT(%lld);", t, result);
                            gen_expr_add(out, t);
                            out->tmp = t;
                            return true;
                        }
                        // Comparison ops produce bool
                        bool boolresult = false;
                        folded = true;
                        switch (op) {
                            case TOK_EQEQ: boolresult = ai == bi; break;
                            case TOK_NEQ:  boolresult = ai != bi; break;
                            case TOK_LT:   boolresult = ai < bi; break;
                            case TOK_LTE:  boolresult = ai <= bi; break;
                            case TOK_GT:   boolresult = ai > bi; break;
                            case TOK_GTE:  boolresult = ai >= bi; break;
                            default: folded = false; break;
                        }
                        if (folded) {
                            w_line(&cg->w, "YisVal %s = YV_BOOL(%d);", t, boolresult ? 1 : 0);
                            gen_expr_add(out, t);
                            out->tmp = t;
                            return true;
                        }
                    }
                    // Not a foldable op — fall through to runtime call
                }
                // Runtime arithmetic/comparison codegen
                GenExpr a;
                GenExpr b;
                if (!gen_expr(cg, path, e->as.binary.a, &a, err)) return false;
                if (!gen_expr(cg, path, e->as.binary.b, &b, err)) { gen_expr_free(&a); return false; }
                char *t = codegen_new_tmp(cg);
                const char *opfn = NULL;
                switch (op) {
                    case TOK_PLUS: opfn = "yis_add"; break;
                    case TOK_MINUS: opfn = "yis_sub"; break;
                    case TOK_STAR: opfn = "yis_mul"; break;
                    case TOK_SLASH: opfn = "yis_div"; break;
                    case TOK_PERCENT: opfn = "yis_mod"; break;
                    case TOK_EQEQ: opfn = "yis_eq"; break;
                    case TOK_NEQ: opfn = "yis_ne"; break;
                    case TOK_LT: opfn = "yis_lt"; break;
                    case TOK_LTE: opfn = "yis_le"; break;
                    case TOK_GT: opfn = "yis_gt"; break;
                    case TOK_GTE: opfn = "yis_ge"; break;
                    default: break;
                }
                if (!opfn) {
                    gen_expr_free(&a);
                    gen_expr_free(&b);
                    return cg_set_err(err, path, "unsupported binary op");
                }
                w_line(&cg->w, "YisVal %s = %s(%s, %s);", t, opfn, a.tmp, b.tmp);
                w_line(&cg->w, "yis_release_val(%s);", a.tmp);
                w_line(&cg->w, "yis_release_val(%s);", b.tmp);
                gen_expr_release_except(cg, &a, a.tmp);
                gen_expr_release_except(cg, &b, b.tmp);
                gen_expr_free(&a);
                gen_expr_free(&b);
                gen_expr_add(out, t);
                out->tmp = t;
                return true;
            }
            if (op == TOK_QQ) {
                GenExpr left;
                if (!gen_expr(cg, path, e->as.binary.a, &left, err)) return false;
                char *t = codegen_new_tmp(cg);
                w_line(&cg->w, "YisVal %s = YV_NULLV;", t);
                w_line(&cg->w, "if (%s.tag != EVT_NULL) {", left.tmp);
                cg->w.indent++;
                w_line(&cg->w, "yis_move_into(&%s, %s);", t, left.tmp);
                w_line(&cg->w, "%s = YV_NULLV;", left.tmp);
                cg->w.indent--;
                w_line(&cg->w, "} else {");
                cg->w.indent++;
                GenExpr right;
                if (!gen_expr(cg, path, e->as.binary.b, &right, err)) { gen_expr_free(&left); return false; }
                w_line(&cg->w, "yis_move_into(&%s, %s);", t, right.tmp);
                w_line(&cg->w, "%s = YV_NULLV;", right.tmp);
                w_line(&cg->w, "yis_release_val(%s);", right.tmp);
                gen_expr_release_except(cg, &right, right.tmp);
                gen_expr_free(&right);
                cg->w.indent--;
                w_line(&cg->w, "}");
                w_line(&cg->w, "yis_release_val(%s);", left.tmp);
                gen_expr_release_except(cg, &left, left.tmp);
                gen_expr_free(&left);
                gen_expr_add(out, t);
                out->tmp = t;
                return true;
            }
            // logical ops
            GenExpr left;
            if (!gen_expr(cg, path, e->as.binary.a, &left, err)) return false;
            char *t = codegen_new_tmp(cg);
            w_line(&cg->w, "YisVal %s = YV_BOOL(false);", t);
            if (op == TOK_ANDAND) {
                w_line(&cg->w, "if (yis_as_bool(%s)) {", left.tmp);
                cg->w.indent++;
                GenExpr right;
                if (!gen_expr(cg, path, e->as.binary.b, &right, err)) { gen_expr_free(&left); return false; }
                w_line(&cg->w, "yis_move_into(&%s, YV_BOOL(yis_as_bool(%s)));", t, right.tmp);
                w_line(&cg->w, "yis_release_val(%s);", right.tmp);
                gen_expr_release_except(cg, &right, right.tmp);
                gen_expr_free(&right);
                cg->w.indent--;
                w_line(&cg->w, "} else {");
                cg->w.indent++;
                w_line(&cg->w, "yis_move_into(&%s, YV_BOOL(false));", t);
                cg->w.indent--;
                w_line(&cg->w, "}");
            } else {
                w_line(&cg->w, "if (yis_as_bool(%s)) {", left.tmp);
                cg->w.indent++;
                w_line(&cg->w, "yis_move_into(&%s, YV_BOOL(true));", t);
                cg->w.indent--;
                w_line(&cg->w, "} else {");
                cg->w.indent++;
                GenExpr right;
                if (!gen_expr(cg, path, e->as.binary.b, &right, err)) { gen_expr_free(&left); return false; }
                w_line(&cg->w, "yis_move_into(&%s, YV_BOOL(yis_as_bool(%s)));", t, right.tmp);
                w_line(&cg->w, "yis_release_val(%s);", right.tmp);
                gen_expr_release_except(cg, &right, right.tmp);
                gen_expr_free(&right);
                cg->w.indent--;
                w_line(&cg->w, "}");
            }
            w_line(&cg->w, "yis_release_val(%s);", left.tmp);
            gen_expr_release_except(cg, &left, left.tmp);
            gen_expr_free(&left);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_INDEX: {
            Ty *base_ty = cg_tc_expr(cg, path, e->as.index.a, err);
            if (!base_ty) return false;
            GenExpr at;
            GenExpr it;
            if (!gen_expr(cg, path, e->as.index.a, &at, err)) return false;
            if (!gen_expr(cg, path, e->as.index.i, &it, err)) { gen_expr_free(&at); return false; }
            char *t = codegen_new_tmp(cg);
            if (base_ty->tag == TY_PRIM && str_eq_c(base_ty->name, "string")) {
                w_line(&cg->w, "YisVal %s = stdr_str_at(%s, yis_as_int(%s));", t, at.tmp, it.tmp);
            } else if (base_ty->tag == TY_DICT) {
                w_line(&cg->w, "YisVal %s = yis_dict_get((YisDict*)%s.as.p, %s);", t, at.tmp, it.tmp);
            } else {
                w_line(&cg->w, "YisVal %s = YV_NULLV;", t);
                w_line(&cg->w, "if (%s.tag == EVT_ARR) %s = yis_arr_get((YisArr*)%s.as.p, yis_as_int(%s));", at.tmp, t, at.tmp, it.tmp);
                w_line(&cg->w, "else if (%s.tag == EVT_DICT) %s = yis_dict_get((YisDict*)%s.as.p, %s);", at.tmp, t, at.tmp, it.tmp);
            }
            w_line(&cg->w, "yis_release_val(%s);", at.tmp);
            w_line(&cg->w, "yis_release_val(%s);", it.tmp);
            gen_expr_release_except(cg, &at, at.tmp);
            gen_expr_release_except(cg, &it, it.tmp);
            gen_expr_free(&at);
            gen_expr_free(&it);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_TERNARY: {
            GenExpr ct;
            if (!gen_expr(cg, path, e->as.ternary.cond, &ct, err)) return false;
            char *t = codegen_new_tmp(cg);
            w_line(&cg->w, "YisVal %s = YV_NULLV;", t);
            w_line(&cg->w, "if (yis_as_bool(%s)) {", ct.tmp);
            cg->w.indent++;
            GenExpr at;
            if (!gen_expr(cg, path, e->as.ternary.then_expr, &at, err)) { gen_expr_free(&ct); return false; }
            w_line(&cg->w, "yis_move_into(&%s, %s);", t, at.tmp);
            gen_expr_release_except(cg, &at, at.tmp);
            gen_expr_free(&at);
            cg->w.indent--;
            w_line(&cg->w, "} else {");
            cg->w.indent++;
            GenExpr bt;
            if (!gen_expr(cg, path, e->as.ternary.else_expr, &bt, err)) { gen_expr_free(&ct); return false; }
            w_line(&cg->w, "yis_move_into(&%s, %s);", t, bt.tmp);
            gen_expr_release_except(cg, &bt, bt.tmp);
            gen_expr_free(&bt);
            cg->w.indent--;
            w_line(&cg->w, "}");
            w_line(&cg->w, "yis_release_val(%s);", ct.tmp);
            gen_expr_release_except(cg, &ct, ct.tmp);
            gen_expr_free(&ct);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_ASSIGN: {
            TokKind assign_op = is_assign_op(e->as.assign.op) ? e->as.assign.op : TOK_EQ;
            bool is_compound = assign_op != TOK_EQ;
            GenExpr vt;
            if (!gen_expr(cg, path, e->as.assign.value, &vt, err)) return false;
            char *tret = codegen_new_tmp(cg);
            if (!tret) {
                gen_expr_free(&vt);
                return cg_set_err(err, path, "out of memory");
            }
            if (!is_compound) {
                w_line(&cg->w, "YisVal %s = %s; yis_retain_val(%s);", tret, vt.tmp, tret);
                if (e->as.assign.target->kind == EXPR_IDENT) {
                    char *slot = codegen_cname_of(cg, e->as.assign.target->as.ident.name);
                    if (!slot) {
                        gen_expr_free(&vt);
                        return cg_set_err(err, path, "unknown assignment target");
                    }
                    w_line(&cg->w, "yis_move_into(&%s, %s);", slot, vt.tmp);
                } else if (e->as.assign.target->kind == EXPR_INDEX) {
                    GenExpr at, it;
                    if (!gen_expr(cg, path, e->as.assign.target->as.index.a, &at, err)) {
                        gen_expr_free(&vt);
                        return false;
                    }
                    if (!gen_expr(cg, path, e->as.assign.target->as.index.i, &it, err)) {
                        gen_expr_free(&at);
                        gen_expr_free(&vt);
                        return false;
                    }
                    Ty *base_ty_idx = cg_tc_expr(cg, path, e->as.assign.target->as.index.a, err);
                    if (base_ty_idx && base_ty_idx->tag == TY_DICT) {
                        w_line(&cg->w, "yis_dict_set((YisDict*)%s.as.p, %s, %s);", at.tmp, it.tmp, vt.tmp);
                    } else if (base_ty_idx && base_ty_idx->tag == TY_ARRAY) {
                        w_line(&cg->w, "yis_arr_set((YisArr*)%s.as.p, yis_as_int(%s), %s);", at.tmp, it.tmp, vt.tmp);
                    } else {
                        w_line(&cg->w, "if (%s.tag == EVT_DICT) yis_dict_set((YisDict*)%s.as.p, %s, %s);", at.tmp, at.tmp, it.tmp, vt.tmp);
                        w_line(&cg->w, "else if (%s.tag == EVT_ARR) yis_arr_set((YisArr*)%s.as.p, yis_as_int(%s), %s);", at.tmp, at.tmp, it.tmp, vt.tmp);
                        w_line(&cg->w, "else yis_trap(\"index assignment expects array or dict\");");
                    }
                    w_line(&cg->w, "yis_release_val(%s);", at.tmp);
                    w_line(&cg->w, "yis_release_val(%s);", it.tmp);
                    gen_expr_release_except(cg, &at, at.tmp);
                    gen_expr_release_except(cg, &it, it.tmp);
                    gen_expr_free(&at);
                    gen_expr_free(&it);
                } else if (e->as.assign.target->kind == EXPR_MEMBER) {
                    Ty *base_ty = cg_tc_expr(cg, path, e->as.assign.target->as.member.a, err);
                    if (!base_ty || base_ty->tag != TY_CLASS) {
                        gen_expr_free(&vt);
                        return cg_set_err(err, path, "unsupported member assignment");
                    }
                    GenExpr at;
                    if (!gen_expr(cg, path, e->as.assign.target->as.member.a, &at, err)) {
                        gen_expr_free(&vt);
                        return false;
                    }
                    char *cname = codegen_c_class_name(cg, base_ty->name);
                    char *field = codegen_c_field_name(cg, e->as.assign.target->as.member.name);
                    w_line(&cg->w, "yis_move_into(&((%s*)%s.as.p)->%s, %s);", cname, at.tmp, field, vt.tmp);
                    w_line(&cg->w, "yis_release_val(%s);", at.tmp);
                    gen_expr_release_except(cg, &at, at.tmp);
                    gen_expr_free(&at);
                } else {
                    gen_expr_free(&vt);
                    return cg_set_err(err, path, "unsupported assignment target");
                }
            } else {
                const char *opfn = compound_assign_opfn(assign_op);
                if (!opfn) {
                    gen_expr_free(&vt);
                    return cg_set_err(err, path, "unsupported compound assignment op");
                }
                if (e->as.assign.target->kind == EXPR_IDENT) {
                    char *slot = codegen_cname_of(cg, e->as.assign.target->as.ident.name);
                    if (!slot) {
                        gen_expr_free(&vt);
                        return cg_set_err(err, path, "unknown assignment target");
                    }
                    w_line(&cg->w, "YisVal %s = %s(%s, %s);", tret, opfn, slot, vt.tmp);
                    w_line(&cg->w, "yis_retain_val(%s);", tret);
                    w_line(&cg->w, "yis_move_into(&%s, %s);", slot, tret);
                } else if (e->as.assign.target->kind == EXPR_INDEX) {
                    Ty *base_ty = cg_tc_expr(cg, path, e->as.assign.target->as.index.a, err);
                    if (!base_ty) {
                        gen_expr_free(&vt);
                        return false;
                    }
                    if (base_ty->tag == TY_PRIM && str_eq_c(base_ty->name, "string")) {
                        gen_expr_free(&vt);
                        return cg_set_err(err, path, "unsupported string index assignment");
                    }
                    GenExpr at, it;
                    if (!gen_expr(cg, path, e->as.assign.target->as.index.a, &at, err)) {
                        gen_expr_free(&vt);
                        return false;
                    }
                    if (!gen_expr(cg, path, e->as.assign.target->as.index.i, &it, err)) {
                        gen_expr_free(&at);
                        gen_expr_free(&vt);
                        return false;
                    }
                    char *cur = codegen_new_tmp(cg);
                    if (!cur) {
                        gen_expr_free(&at);
                        gen_expr_free(&it);
                        gen_expr_free(&vt);
                        return cg_set_err(err, path, "out of memory");
                    }
                    if (base_ty->tag == TY_DICT) {
                        w_line(&cg->w, "YisVal %s = yis_dict_get((YisDict*)%s.as.p, %s);", cur, at.tmp, it.tmp);
                        w_line(&cg->w, "YisVal %s = %s(%s, %s);", tret, opfn, cur, vt.tmp);
                        w_line(&cg->w, "yis_retain_val(%s);", tret);
                        w_line(&cg->w, "yis_dict_set((YisDict*)%s.as.p, %s, %s);", at.tmp, it.tmp, tret);
                    } else if (base_ty->tag == TY_ARRAY) {
                        w_line(&cg->w, "YisVal %s = yis_arr_get((YisArr*)%s.as.p, yis_as_int(%s));", cur, at.tmp, it.tmp);
                        w_line(&cg->w, "YisVal %s = %s(%s, %s);", tret, opfn, cur, vt.tmp);
                        w_line(&cg->w, "yis_retain_val(%s);", tret);
                        w_line(&cg->w, "yis_arr_set((YisArr*)%s.as.p, yis_as_int(%s), %s);", at.tmp, it.tmp, tret);
                    } else {
                        w_line(&cg->w, "YisVal %s = YV_NULLV;", cur);
                        w_line(&cg->w, "if (%s.tag == EVT_DICT) %s = yis_dict_get((YisDict*)%s.as.p, %s);", at.tmp, cur, at.tmp, it.tmp);
                        w_line(&cg->w, "else if (%s.tag == EVT_ARR) %s = yis_arr_get((YisArr*)%s.as.p, yis_as_int(%s));", at.tmp, cur, at.tmp, it.tmp);
                        w_line(&cg->w, "else yis_trap(\"compound index assignment expects array or dict\");");
                        w_line(&cg->w, "YisVal %s = %s(%s, %s);", tret, opfn, cur, vt.tmp);
                        w_line(&cg->w, "yis_retain_val(%s);", tret);
                        w_line(&cg->w, "if (%s.tag == EVT_DICT) yis_dict_set((YisDict*)%s.as.p, %s, %s);", at.tmp, at.tmp, it.tmp, tret);
                        w_line(&cg->w, "else yis_arr_set((YisArr*)%s.as.p, yis_as_int(%s), %s);", at.tmp, it.tmp, tret);
                    }
                    w_line(&cg->w, "yis_release_val(%s);", cur);
                    w_line(&cg->w, "yis_release_val(%s);", at.tmp);
                    w_line(&cg->w, "yis_release_val(%s);", it.tmp);
                    gen_expr_release_except(cg, &at, at.tmp);
                    gen_expr_release_except(cg, &it, it.tmp);
                    gen_expr_free(&at);
                    gen_expr_free(&it);
                } else if (e->as.assign.target->kind == EXPR_MEMBER) {
                    Ty *base_ty = cg_tc_expr(cg, path, e->as.assign.target->as.member.a, err);
                    if (!base_ty || base_ty->tag != TY_CLASS) {
                        gen_expr_free(&vt);
                        return cg_set_err(err, path, "unsupported member assignment");
                    }
                    GenExpr at;
                    if (!gen_expr(cg, path, e->as.assign.target->as.member.a, &at, err)) {
                        gen_expr_free(&vt);
                        return false;
                    }
                    char *cname = codegen_c_class_name(cg, base_ty->name);
                    char *field = codegen_c_field_name(cg, e->as.assign.target->as.member.name);
                    w_line(&cg->w, "YisVal %s = %s(((%s*)%s.as.p)->%s, %s);", tret, opfn, cname, at.tmp, field, vt.tmp);
                    w_line(&cg->w, "yis_retain_val(%s);", tret);
                    w_line(&cg->w, "yis_move_into(&((%s*)%s.as.p)->%s, %s);", cname, at.tmp, field, tret);
                    w_line(&cg->w, "yis_release_val(%s);", at.tmp);
                    gen_expr_release_except(cg, &at, at.tmp);
                    gen_expr_free(&at);
                } else {
                    gen_expr_free(&vt);
                    return cg_set_err(err, path, "unsupported assignment target");
                }
                w_line(&cg->w, "yis_release_val(%s);", vt.tmp);
            }
            gen_expr_release_except(cg, &vt, vt.tmp);
            gen_expr_free(&vt);
            gen_expr_add(out, tret);
            out->tmp = tret;
            return true;
        }
        case EXPR_CALL: {
            Expr *fn = e->as.call.fn;
            // cask-qualified calls
            if (fn && fn->kind == EXPR_MEMBER && fn->as.member.a && fn->as.member.a->kind == EXPR_IDENT) {
                Str mod = fn->as.member.a->as.ident.name;
                if (codegen_cask_in_scope(cg, mod)) {
                    Str name = fn->as.member.name;
                    /* Emit stdr.slice(s, start, end) as stdr_slice directly so arg order is guaranteed (avoids "expected int (got string)" from wrong wrapper). */
                    if (str_eq_c(mod, "stdr") && str_eq_c(name, "slice") && e->as.call.args_len == 3) {
                        GenExpr sv, startv, endv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &sv, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &startv, err)) { gen_expr_free(&sv); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &endv, err)) { gen_expr_free(&sv); gen_expr_free(&startv); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_slice(%s, yis_as_int(%s), yis_as_int(%s));", t, sv.tmp, startv.tmp, endv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", sv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", startv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", endv.tmp);
                        gen_expr_release_except(cg, &sv, sv.tmp);
                        gen_expr_release_except(cg, &startv, startv.tmp);
                        gen_expr_release_except(cg, &endv, endv.tmp);
                        gen_expr_free(&sv);
                        gen_expr_free(&startv);
                        gen_expr_free(&endv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    FunSig *sig = codegen_fun_sig(cg, mod, name);
                    if (!sig) {
                        return cg_set_errf(err, path, e->line, e->col, "unknown %.*s.%.*s", (int)mod.len, mod.data, (int)name.len, name.data);
                    }
                    VEC(char *) arg_ts = VEC_INIT;
                    for (size_t i = 0; i < e->as.call.args_len; i++) {
                        GenExpr ge;
                        if (!gen_expr(cg, path, e->as.call.args[i], &ge, err)) { VEC_FREE(arg_ts); return false; }
                        VEC_PUSH(arg_ts, ge.tmp);
                        gen_expr_release_except(cg, &ge, ge.tmp);
                        gen_expr_free(&ge);
                    }
                    bool ret_void = sig->ret && sig->ret->tag == TY_VOID;
                    bool direct_extern_stub = is_extern_stub_sig(sig);
                    if (ret_void) {
                        StrBuf line; sb_init(&line);
                        if (direct_extern_stub) {
                            sb_append_n(&line, name.data, name.len);
                            sb_append(&line, "(");
                        } else {
                            char *mangled = mangle_global(cg->arena, mod, name);
                            sb_appendf(&line, "%s(", mangled);
                        }
                        for (size_t i = 0; i < arg_ts.len; i++) {
                            if (i) sb_append(&line, ", ");
                            sb_append(&line, arg_ts.data[i]);
                        }
                        sb_append(&line, ");");
                        w_line(&cg->w, "%s", line.data ? line.data : "");
                        sb_free(&line);
                        for (size_t i = 0; i < arg_ts.len; i++) {
                            w_line(&cg->w, "yis_release_val(%s);", arg_ts.data[i]);
                        }
                        VEC_FREE(arg_ts);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = YV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    char *t = codegen_new_tmp(cg);
                    StrBuf line; sb_init(&line);
                    if (direct_extern_stub) {
                        sb_appendf(&line, "YisVal %s = ", t);
                        sb_append_n(&line, name.data, name.len);
                        sb_append(&line, "(");
                    } else {
                        char *mangled = mangle_global(cg->arena, mod, name);
                        sb_appendf(&line, "YisVal %s = %s(", t, mangled);
                    }
                    for (size_t i = 0; i < arg_ts.len; i++) {
                        if (i) sb_append(&line, ", ");
                        sb_append(&line, arg_ts.data[i]);
                    }
                    sb_append(&line, ");");
                    w_line(&cg->w, "%s", line.data ? line.data : "");
                    sb_free(&line);
                    for (size_t i = 0; i < arg_ts.len; i++) {
                        w_line(&cg->w, "yis_release_val(%s);", arg_ts.data[i]);
                    }
                    VEC_FREE(arg_ts);
                    gen_expr_add(out, t);
                    out->tmp = t;
                    return true;
                }
            }

            // method calls
            if (fn && fn->kind == EXPR_MEMBER) {
                Expr *base = fn->as.member.a;
                Str mname = fn->as.member.name;
                Ty *base_ty = cg_tc_expr(cg, path, base, err);
                if (!base_ty) return false;

                if (str_eq_c(mname, "to_string") && e->as.call.args_len == 0) {
                    GenExpr bt;
                    if (!gen_expr(cg, path, base, &bt, err)) return false;
                    char *t = codegen_new_tmp(cg);
                    w_line(&cg->w, "YisVal %s = YV_STR(stdr_to_string(%s));", t, bt.tmp);
                    w_line(&cg->w, "yis_release_val(%s);", bt.tmp);
                    gen_expr_release_except(cg, &bt, bt.tmp);
                    gen_expr_free(&bt);
                    gen_expr_add(out, t);
                    out->tmp = t;
                    return true;
                }

                if (base_ty->tag == TY_ARRAY && str_eq_c(mname, "add") && e->as.call.args_len == 1) {
                    GenExpr at, vt;
                    if (!gen_expr(cg, path, base, &at, err)) return false;
                    if (!gen_expr(cg, path, e->as.call.args[0], &vt, err)) { gen_expr_free(&at); return false; }
                    w_line(&cg->w, "yis_arr_add((YisArr*)%s.as.p, %s);", at.tmp, vt.tmp);
                    w_line(&cg->w, "yis_release_val(%s);", at.tmp);
                    gen_expr_release_except(cg, &at, at.tmp);
                    gen_expr_release_except(cg, &vt, vt.tmp);
                    gen_expr_free(&at);
                    gen_expr_free(&vt);
                    char *t = codegen_new_tmp(cg);
                    w_line(&cg->w, "YisVal %s = YV_NULLV;", t);
                    gen_expr_add(out, t);
                    out->tmp = t;
                    return true;
                }

                if (base_ty->tag == TY_ARRAY && str_eq_c(mname, "remove") && e->as.call.args_len == 1) {
                    GenExpr at, it;
                    if (!gen_expr(cg, path, base, &at, err)) return false;
                    if (!gen_expr(cg, path, e->as.call.args[0], &it, err)) { gen_expr_free(&at); return false; }
                    char *t = codegen_new_tmp(cg);
                    w_line(&cg->w, "YisVal %s = yis_arr_remove((YisArr*)%s.as.p, yis_as_int(%s));", t, at.tmp, it.tmp);
                    w_line(&cg->w, "yis_release_val(%s);", at.tmp);
                    w_line(&cg->w, "yis_release_val(%s);", it.tmp);
                    gen_expr_release_except(cg, &at, at.tmp);
                    gen_expr_release_except(cg, &it, it.tmp);
                    gen_expr_free(&at);
                    gen_expr_free(&it);
                    gen_expr_add(out, t);
                    out->tmp = t;
                    return true;
                }
                if (base_ty->tag == TY_CLASS) {
                    ClassInfo *ci = codegen_class_info(cg, base_ty->name);
                    if (!ci) return cg_set_err(err, path, "unknown class method");
                    FunSig *sig = NULL;
                    for (size_t i = 0; i < ci->methods_len; i++) {
                        if (str_eq(ci->methods[i].name, mname)) {
                            sig = ci->methods[i].sig;
                            break;
                        }
                    }
                    if (!sig) {
                        return cg_set_errf(err, path, e->line, e->col, "unknown method '%.*s'", (int)mname.len, mname.data);
                    }
                    Str mod, cls_short;
                    split_qname(ci->qname, &mod, &cls_short);
                    GenExpr bt;
                    if (!gen_expr(cg, path, base, &bt, err)) return false;
                    VEC(char *) arg_ts = VEC_INIT;
                    for (size_t i = 0; i < e->as.call.args_len; i++) {
                        GenExpr ge;
                        if (!gen_expr(cg, path, e->as.call.args[i], &ge, err)) { gen_expr_free(&bt); VEC_FREE(arg_ts); return false; }
                        VEC_PUSH(arg_ts, ge.tmp);
                        gen_expr_release_except(cg, &ge, ge.tmp);
                        gen_expr_free(&ge);
                    }
                    bool ret_void = sig->ret && sig->ret->tag == TY_VOID;
                    char *mangled = mangle_method(cg->arena, mod, cls_short, mname);
                    if (ret_void) {
                        StrBuf line; sb_init(&line);
                        sb_appendf(&line, "%s(%s", mangled, bt.tmp);
                        for (size_t i = 0; i < arg_ts.len; i++) {
                            sb_append(&line, ", ");
                            sb_append(&line, arg_ts.data[i]);
                        }
                        sb_append(&line, ");");
                        w_line(&cg->w, "%s", line.data ? line.data : "");
                        sb_free(&line);
                        w_line(&cg->w, "yis_release_val(%s);", bt.tmp);
                        gen_expr_release_except(cg, &bt, bt.tmp);
                        gen_expr_free(&bt);
                        for (size_t i = 0; i < arg_ts.len; i++) {
                            w_line(&cg->w, "yis_release_val(%s);", arg_ts.data[i]);
                        }
                        VEC_FREE(arg_ts);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = YV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    char *t = codegen_new_tmp(cg);
                    StrBuf line; sb_init(&line);
                    sb_appendf(&line, "YisVal %s = %s(%s", t, mangled, bt.tmp);
                    for (size_t i = 0; i < arg_ts.len; i++) {
                        sb_append(&line, ", ");
                        sb_append(&line, arg_ts.data[i]);
                    }
                    sb_append(&line, ");");
                    w_line(&cg->w, "%s", line.data ? line.data : "");
                    sb_free(&line);
                    w_line(&cg->w, "yis_release_val(%s);", bt.tmp);
                    gen_expr_release_except(cg, &bt, bt.tmp);
                    gen_expr_free(&bt);
                    for (size_t i = 0; i < arg_ts.len; i++) {
                        w_line(&cg->w, "yis_release_val(%s);", arg_ts.data[i]);
                    }
                    VEC_FREE(arg_ts);
                    gen_expr_add(out, t);
                    out->tmp = t;
                    return true;
                }

                return cg_set_err(err, path, "unknown member call");
            }

            // global prelude calls
            if (fn && fn->kind == EXPR_IDENT) {
                Str fname = fn->as.ident.name;
                if (!locals_lookup(&cg->ty_loc, fname)) {
                    if (str_eq_c(fname, "str")) {
                        if (e->as.call.args_len != 1) return cg_set_err(err, path, "str expects 1 arg");
                        GenExpr at;
                        if (!gen_expr(cg, path, e->as.call.args[0], &at, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = YV_STR(stdr_to_string(%s));", t, at.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", at.tmp);
                        gen_expr_release_except(cg, &at, at.tmp);
                        gen_expr_free(&at);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__len")) {
                        GenExpr at;
                        if (!gen_expr(cg, path, e->as.call.args[0], &at, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = YV_INT(stdr_len(%s));", t, at.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", at.tmp);
                        gen_expr_release_except(cg, &at, at.tmp);
                        gen_expr_free(&at);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__num")) {
                        GenExpr at;
                        if (!gen_expr(cg, path, e->as.call.args[0], &at, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = YV_INT(stdr_num(%s));", t, at.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", at.tmp);
                        gen_expr_release_except(cg, &at, at.tmp);
                        gen_expr_free(&at);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__write")) {
                        GenExpr arg;
                        if (!gen_expr(cg, path, e->as.call.args[0], &arg, err)) return false;
                        w_line(&cg->w, "stdr_write(%s);", arg.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", arg.tmp);
                        gen_expr_release_except(cg, &arg, arg.tmp);
                        gen_expr_free(&arg);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = YV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__writef")) {
                        GenExpr fmt;
                        GenExpr args;
                        if (!gen_expr(cg, path, e->as.call.args[0], &fmt, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &args, err)) { gen_expr_free(&fmt); return false; }
                        w_line(&cg->w, "stdr_writef_args(%s, %s);", fmt.tmp, args.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", fmt.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", args.tmp);
                        gen_expr_release_except(cg, &fmt, fmt.tmp);
                        gen_expr_release_except(cg, &args, args.tmp);
                        gen_expr_free(&fmt);
                        gen_expr_free(&args);
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = YV_NULLV;", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__read_line")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = YV_STR(stdr_read_line());", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__args")) {
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_args();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__run_command")) {
                        if (e->as.call.args_len != 1) return cg_set_err(err, path, "__run_command expects 1 arg");
                        GenExpr cmdv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &cmdv, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_run_command(%s);", t, cmdv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", cmdv.tmp);
                        gen_expr_release_except(cg, &cmdv, cmdv.tmp);
                        gen_expr_free(&cmdv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__file_exists")) {
                        if (e->as.call.args_len != 1) return cg_set_err(err, path, "__file_exists expects 1 arg");
                        GenExpr pathv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &pathv, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_file_exists(%s);", t, pathv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", pathv.tmp);
                        gen_expr_release_except(cg, &pathv, pathv.tmp);
                        gen_expr_free(&pathv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__file_mtime")) {
                        if (e->as.call.args_len != 1) return cg_set_err(err, path, "__file_mtime expects 1 arg");
                        GenExpr pathv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &pathv, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_file_mtime(%s);", t, pathv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", pathv.tmp);
                        gen_expr_release_except(cg, &pathv, pathv.tmp);
                        gen_expr_free(&pathv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__getcwd")) {
                        if (e->as.call.args_len != 0) return cg_set_err(err, path, "__getcwd expects 0 args");
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_getcwd();", t);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__read_text_file")) {
                        if (e->as.call.args_len != 1) return cg_set_err(err, path, "__read_text_file expects 1 arg");
                        GenExpr pathv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &pathv, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_read_text_file(%s);", t, pathv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", pathv.tmp);
                        gen_expr_release_except(cg, &pathv, pathv.tmp);
                        gen_expr_free(&pathv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__slice")) {
                        if (e->as.call.args_len != 3) return cg_set_err(err, path, "__slice expects 3 args");
                        GenExpr sv, startv, endv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &sv, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &startv, err)) { gen_expr_free(&sv); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &endv, err)) { gen_expr_free(&sv); gen_expr_free(&startv); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_slice(%s, yis_as_int(%s), yis_as_int(%s));", t, sv.tmp, startv.tmp, endv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", sv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", startv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", endv.tmp);
                        gen_expr_release_except(cg, &sv, sv.tmp);
                        gen_expr_release_except(cg, &startv, startv.tmp);
                        gen_expr_release_except(cg, &endv, endv.tmp);
                        gen_expr_free(&sv);
                        gen_expr_free(&startv);
                        gen_expr_free(&endv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__concat")) {
                        if (e->as.call.args_len != 2) return cg_set_err(err, path, "__concat expects 2 args");
                        GenExpr av, bv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &av, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &bv, err)) { gen_expr_free(&av); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_array_concat(%s, %s);", t, av.tmp, bv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", av.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", bv.tmp);
                        gen_expr_release_except(cg, &av, av.tmp);
                        gen_expr_release_except(cg, &bv, bv.tmp);
                        gen_expr_free(&av);
                        gen_expr_free(&bv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__str_concat")) {
                        if (e->as.call.args_len != 2) return cg_set_err(err, path, "__str_concat expects 2 args");
                        GenExpr av, bv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &av, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &bv, err)) { gen_expr_free(&av); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_str_concat(%s, %s);", t, av.tmp, bv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", av.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", bv.tmp);
                        gen_expr_release_except(cg, &av, av.tmp);
                        gen_expr_release_except(cg, &bv, bv.tmp);
                        gen_expr_free(&av);
                        gen_expr_free(&bv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__char_code")) {
                        if (e->as.call.args_len != 1) return cg_set_err(err, path, "__char_code expects 1 arg");
                        GenExpr cv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &cv, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = YV_INT(stdr_char_code(%s));", t, cv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", cv.tmp);
                        gen_expr_release_except(cg, &cv, cv.tmp);
                        gen_expr_free(&cv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__parse_hex")) {
                        if (e->as.call.args_len != 1) return cg_set_err(err, path, "__parse_hex expects 1 arg");
                        GenExpr sv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &sv, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_parse_hex(%s);", t, sv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", sv.tmp);
                        gen_expr_release_except(cg, &sv, sv.tmp);
                        gen_expr_free(&sv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__char_from_code")) {
                        if (e->as.call.args_len != 1) return cg_set_err(err, path, "__char_from_code expects 1 arg");
                        GenExpr cv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &cv, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_char_from_code(%s);", t, cv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", cv.tmp);
                        gen_expr_release_except(cg, &cv, cv.tmp);
                        gen_expr_free(&cv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__floor")) {
                        if (e->as.call.args_len != 1) return cg_set_err(err, path, "__floor expects 1 arg");
                        GenExpr av;
                        if (!gen_expr(cg, path, e->as.call.args[0], &av, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_floor(%s);", t, av.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", av.tmp);
                        gen_expr_release_except(cg, &av, av.tmp);
                        gen_expr_free(&av);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__ceil")) {
                        if (e->as.call.args_len != 1) return cg_set_err(err, path, "__ceil expects 1 arg");
                        GenExpr av;
                        if (!gen_expr(cg, path, e->as.call.args[0], &av, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_ceil(%s);", t, av.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", av.tmp);
                        gen_expr_release_except(cg, &av, av.tmp);
                        gen_expr_free(&av);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__keys")) {
                        if (e->as.call.args_len != 1) return cg_set_err(err, path, "__keys expects 1 arg");
                        GenExpr dv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &dv, err)) return false;
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_keys(%s);", t, dv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", dv.tmp);
                        gen_expr_release_except(cg, &dv, dv.tmp);
                        gen_expr_free(&dv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__replace")) {
                        if (e->as.call.args_len != 3) return cg_set_err(err, path, "__replace expects 3 args");
                        GenExpr tv, fv, rv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &tv, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &fv, err)) { gen_expr_free(&tv); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &rv, err)) { gen_expr_free(&tv); gen_expr_free(&fv); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_replace(%s, %s, %s);", t, tv.tmp, fv.tmp, rv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", tv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", fv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", rv.tmp);
                        gen_expr_release_except(cg, &tv, tv.tmp);
                        gen_expr_release_except(cg, &fv, fv.tmp);
                        gen_expr_release_except(cg, &rv, rv.tmp);
                        gen_expr_free(&tv);
                        gen_expr_free(&fv);
                        gen_expr_free(&rv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__write_text_file")) {
                        if (e->as.call.args_len != 2) return cg_set_err(err, path, "__write_text_file expects 2 args");
                        GenExpr pathv, textv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &pathv, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &textv, err)) { gen_expr_free(&pathv); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_write_text_file(%s, %s);", t, pathv.tmp, textv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", pathv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", textv.tmp);
                        gen_expr_release_except(cg, &pathv, pathv.tmp);
                        gen_expr_release_except(cg, &textv, textv.tmp);
                        gen_expr_free(&pathv);
                        gen_expr_free(&textv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__open_file_dialog")) {
                        if (e->as.call.args_len != 2) return cg_set_err(err, path, "__open_file_dialog expects 2 args");
                        GenExpr promptv, extv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &promptv, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &extv, err)) { gen_expr_free(&promptv); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_open_file_dialog(%s, %s);", t, promptv.tmp, extv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", promptv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", extv.tmp);
                        gen_expr_release_except(cg, &promptv, promptv.tmp);
                        gen_expr_release_except(cg, &extv, extv.tmp);
                        gen_expr_free(&promptv);
                        gen_expr_free(&extv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__save_file_dialog")) {
                        if (e->as.call.args_len != 3) return cg_set_err(err, path, "__save_file_dialog expects 3 args");
                        GenExpr promptv, defaultv, extv;
                        if (!gen_expr(cg, path, e->as.call.args[0], &promptv, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &defaultv, err)) { gen_expr_free(&promptv); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &extv, err)) { gen_expr_free(&promptv); gen_expr_free(&defaultv); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_save_file_dialog(%s, %s, %s);", t, promptv.tmp, defaultv.tmp, extv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", promptv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", defaultv.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", extv.tmp);
                        gen_expr_release_except(cg, &promptv, promptv.tmp);
                        gen_expr_release_except(cg, &defaultv, defaultv.tmp);
                        gen_expr_release_except(cg, &extv, extv.tmp);
                        gen_expr_free(&promptv);
                        gen_expr_free(&defaultv);
                        gen_expr_free(&extv);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    if (str_eq_c(fname, "__readf_parse")) {
                        GenExpr fmt, line, args;
                        if (!gen_expr(cg, path, e->as.call.args[0], &fmt, err)) return false;
                        if (!gen_expr(cg, path, e->as.call.args[1], &line, err)) { gen_expr_free(&fmt); return false; }
                        if (!gen_expr(cg, path, e->as.call.args[2], &args, err)) { gen_expr_free(&fmt); gen_expr_free(&line); return false; }
                        char *t = codegen_new_tmp(cg);
                        w_line(&cg->w, "YisVal %s = stdr_readf_parse(%s, %s, %s);", t, fmt.tmp, line.tmp, args.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", fmt.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", line.tmp);
                        w_line(&cg->w, "yis_release_val(%s);", args.tmp);
                        gen_expr_release_except(cg, &fmt, fmt.tmp);
                        gen_expr_release_except(cg, &line, line.tmp);
                        gen_expr_release_except(cg, &args, args.tmp);
                        gen_expr_free(&fmt);
                        gen_expr_free(&line);
                        gen_expr_free(&args);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                    FunSig *sig = codegen_fun_sig(cg, cg->current_cask, fname);
                    if (!sig && is_stdr_prelude(fname)) {
                        bool allow = str_eq_c(cg->current_cask, "stdr");
                        for (size_t i = 0; i < cg->current_imports_len && !allow; i++) {
                            if (str_eq_c(cg->current_imports[i], "stdr")) allow = true;
                        }
                        if (allow) sig = codegen_fun_sig(cg, str_from_c("stdr"), fname);
                    }
                    if (sig) {
                        VEC(char *) arg_ts = VEC_INIT;
                        for (size_t i = 0; i < e->as.call.args_len; i++) {
                            GenExpr ge;
                            if (!gen_expr(cg, path, e->as.call.args[i], &ge, err)) { VEC_FREE(arg_ts); return false; }
                            VEC_PUSH(arg_ts, ge.tmp);
                            gen_expr_release_except(cg, &ge, ge.tmp);
                            gen_expr_free(&ge);
                        }
                        bool ret_void = sig->ret && sig->ret->tag == TY_VOID;
                        bool direct_extern_stub = is_extern_stub_sig(sig);
                        if (ret_void) {
                            StrBuf line; sb_init(&line);
                            if (direct_extern_stub) {
                                sb_append_n(&line, fname.data, fname.len);
                                sb_append(&line, "(");
                            } else {
                                char *mangled = mangle_global(cg->arena, sig->cask, fname);
                                sb_appendf(&line, "%s(", mangled);
                            }
                            for (size_t i = 0; i < arg_ts.len; i++) {
                                if (i) sb_append(&line, ", ");
                                sb_append(&line, arg_ts.data[i]);
                            }
                            sb_append(&line, ");");
                            w_line(&cg->w, "%s", line.data ? line.data : "");
                            sb_free(&line);
                            for (size_t i = 0; i < arg_ts.len; i++) {
                                w_line(&cg->w, "yis_release_val(%s);", arg_ts.data[i]);
                            }
                            VEC_FREE(arg_ts);
                            char *t = codegen_new_tmp(cg);
                            w_line(&cg->w, "YisVal %s = YV_NULLV;", t);
                            gen_expr_add(out, t);
                            out->tmp = t;
                            return true;
                        }
                        char *t = codegen_new_tmp(cg);
                        StrBuf line; sb_init(&line);
                        if (direct_extern_stub) {
                            sb_appendf(&line, "YisVal %s = ", t);
                            sb_append_n(&line, fname.data, fname.len);
                            sb_append(&line, "(");
                        } else {
                            char *mangled = mangle_global(cg->arena, sig->cask, fname);
                            sb_appendf(&line, "YisVal %s = %s(", t, mangled);
                        }
                        for (size_t i = 0; i < arg_ts.len; i++) {
                            if (i) sb_append(&line, ", ");
                            sb_append(&line, arg_ts.data[i]);
                        }
                        sb_append(&line, ");");
                        w_line(&cg->w, "%s", line.data ? line.data : "");
                        sb_free(&line);
                        for (size_t i = 0; i < arg_ts.len; i++) {
                            w_line(&cg->w, "yis_release_val(%s);", arg_ts.data[i]);
                        }
                        VEC_FREE(arg_ts);
                        gen_expr_add(out, t);
                        out->tmp = t;
                        return true;
                    }
                }
            }

            // function value call
            GenExpr ft;
            if (!gen_expr(cg, path, fn, &ft, err)) return false;
            VEC(char *) arg_ts = VEC_INIT;
            for (size_t i = 0; i < e->as.call.args_len; i++) {
                GenExpr ge;
                if (!gen_expr(cg, path, e->as.call.args[i], &ge, err)) { gen_expr_free(&ft); VEC_FREE(arg_ts); return false; }
                VEC_PUSH(arg_ts, ge.tmp);
                gen_expr_release_except(cg, &ge, ge.tmp);
                gen_expr_free(&ge);
            }
            char *t = codegen_new_tmp(cg);
            w_line(&cg->w, "YisVal %s = YV_NULLV;", t);
            if (arg_ts.len > 0) {
                char *argv_name = codegen_new_sym(cg, "argv");
                StrBuf line; sb_init(&line);
                sb_appendf(&line, "YisVal %s[%zu] = { ", argv_name, arg_ts.len);
                for (size_t i = 0; i < arg_ts.len; i++) {
                    if (i) sb_append(&line, ", ");
                    sb_append(&line, arg_ts.data[i]);
                }
                sb_append(&line, " };");
                w_line(&cg->w, "{");
                cg->w.indent++;
                w_line(&cg->w, "%s", line.data ? line.data : "");
                w_line(&cg->w, "%s = yis_call(%s, %zu, %s);", t, ft.tmp, arg_ts.len, argv_name);
                cg->w.indent--;
                w_line(&cg->w, "}");
                sb_free(&line);
            } else {
                w_line(&cg->w, "%s = yis_call(%s, 0, NULL);", t, ft.tmp);
            }
            w_line(&cg->w, "yis_release_val(%s);", ft.tmp);
            gen_expr_release_except(cg, &ft, ft.tmp);
            gen_expr_free(&ft);
            for (size_t i = 0; i < arg_ts.len; i++) {
                w_line(&cg->w, "yis_release_val(%s);", arg_ts.data[i]);
            }
            VEC_FREE(arg_ts);
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_BLOCK: {
            char *t = codegen_new_tmp(cg);
            if (!t) return cg_set_err(err, path, "out of memory");
            w_line(&cg->w, "YisVal %s = YV_NULLV;", t);
            if (!gen_stmt(cg, path, e->as.block_expr.block, false, err)) return false;
            gen_expr_add(out, t);
            out->tmp = t;
            return true;
        }
        case EXPR_PAREN:
            return gen_expr(cg, path, e->as.paren.x, out, err);
        default:
            break;
    }
    return cg_set_err(err, path, "unhandled expr in codegen");
}

// -----------------
// Statement generation
// -----------------

static bool gen_block(Codegen *cg, Str path, Stmt *b, bool ret_void, Diag *err);

static bool gen_if_chain(Codegen *cg, Str path, IfArm **arms, size_t idx, size_t arms_len, bool ret_void, Diag *err) {
    if (idx >= arms_len) return true;
    IfArm *arm = arms[idx];
    if (!arm->cond) {
        if (arm->body->kind == STMT_BLOCK) {
            return gen_block(cg, path, arm->body, ret_void, err);
        }
        return gen_block(cg, path, arm->body, ret_void, err);
    }
    GenExpr cond;
    if (!gen_expr(cg, path, arm->cond, &cond, err)) return false;
    cg->var_id++;
    char *bname = arena_printf(cg->arena, "__b%d", cg->var_id);
    w_line(&cg->w, "bool %s = yis_as_bool(%s);", bname, cond.tmp);
    w_line(&cg->w, "yis_release_val(%s);", cond.tmp);
    gen_expr_release_except(cg, &cond, cond.tmp);
    gen_expr_free(&cond);

    w_line(&cg->w, "if (%s) {", bname);
    cg->w.indent++;
    codegen_push_scope(cg);
    if (arm->body->kind == STMT_BLOCK) {
        if (!gen_block(cg, path, arm->body, ret_void, err)) return false;
    } else {
        if (!gen_block(cg, path, arm->body, ret_void, err)) return false;
    }
    LocalList locals = codegen_pop_scope(cg);
    codegen_release_scope(cg, locals);
    cg->w.indent--;

    if (idx + 1 < arms_len) {
        w_line(&cg->w, "} else {");
        cg->w.indent++;
        if (!gen_if_chain(cg, path, arms, idx + 1, arms_len, ret_void, err)) return false;
        cg->w.indent--;
        w_line(&cg->w, "}");
    } else {
        w_line(&cg->w, "}");
    }
    return true;
}

// Emit a #line directive if the source location changed.
static void emit_line_directive(Codegen *cg, Str path, int line) {
    if (line <= 0 || !path.data || !path.len) return;
    if (line == cg->last_line_num && str_eq(path, cg->last_line_file)) return;
    cg->last_line_num = line;
    cg->last_line_file = path;
    // Emit at indent 0 (preprocessor directives must start at column 1)
    int saved = cg->w.indent;
    cg->w.indent = 0;
    w_line(&cg->w, "#line %d \"%.*s\"", line, (int)path.len, path.data);
    cg->w.indent = saved;
}

static bool gen_block(Codegen *cg, Str path, Stmt *b, bool ret_void, Diag *err) {
    if (!b) return true;
    if (b->kind != STMT_BLOCK) {
        if (b->line > 0) emit_line_directive(cg, path, b->line);
        return gen_stmt(cg, path, b, ret_void, err);
    }
    for (size_t i = 0; i < b->as.block_s.stmts_len; i++) {
        Stmt *s = b->as.block_s.stmts[i];
        if (s && s->line > 0) emit_line_directive(cg, path, s->line);
        if (!gen_stmt(cg, path, s, ret_void, err)) return false;
    }
    return true;
}

static bool gen_stmt(Codegen *cg, Str path, Stmt *s, bool ret_void, Diag *err) {
    if (!s) return true;
    switch (s->kind) {
        case STMT_LET: {
            Ty *ty = cg_tc_expr(cg, path, s->as.let_s.expr, err);
            if (!ty) return false;
            char *cvar = codegen_define_local(cg, s->as.let_s.name, ty, s->as.let_s.is_mut, false);
            if (!cvar) return cg_set_err(err, path, "out of memory");
            w_line(&cg->w, "YisVal %s = YV_NULLV;", cvar);
            GenExpr ge;
            if (!gen_expr(cg, path, s->as.let_s.expr, &ge, err)) return false;
            w_line(&cg->w, "yis_move_into(&%s, %s);", cvar, ge.tmp);
            gen_expr_release_except(cg, &ge, ge.tmp);
            gen_expr_free(&ge);
            return true;
        }
        case STMT_CONST: {
            Ty *ty = cg_tc_expr(cg, path, s->as.const_s.expr, err);
            if (!ty) return false;
            char *cvar = codegen_define_local(cg, s->as.const_s.name, ty, false, true);
            if (!cvar) return cg_set_err(err, path, "out of memory");
            w_line(&cg->w, "YisVal %s = YV_NULLV;", cvar);
            GenExpr ge;
            if (!gen_expr(cg, path, s->as.const_s.expr, &ge, err)) return false;
            w_line(&cg->w, "yis_move_into(&%s, %s);", cvar, ge.tmp);
            gen_expr_release_except(cg, &ge, ge.tmp);
            gen_expr_free(&ge);
            return true;
        }
        case STMT_EXPR: {
            GenExpr ge;
            if (!gen_expr(cg, path, s->as.expr_s.expr, &ge, err)) return false;
            w_line(&cg->w, "yis_release_val(%s);", ge.tmp);
            gen_expr_release_except(cg, &ge, ge.tmp);
            gen_expr_free(&ge);
            return true;
        }
        case STMT_RETURN: {
            if (ret_void) {
                if (s->as.ret_s.expr) {
                    GenExpr ge;
                    if (!gen_expr(cg, path, s->as.ret_s.expr, &ge, err)) return false;
                    w_line(&cg->w, "yis_release_val(%s);", ge.tmp);
                    gen_expr_release_except(cg, &ge, ge.tmp);
                    gen_expr_free(&ge);
                }
                w_line(&cg->w, "return;");
            } else {
                if (!s->as.ret_s.expr) {
                    w_line(&cg->w, "__ret = YV_NULLV;");
                } else {
                    GenExpr ge;
                    if (!gen_expr(cg, path, s->as.ret_s.expr, &ge, err)) return false;
                    w_line(&cg->w, "yis_move_into(&__ret, %s);", ge.tmp);
                    gen_expr_release_except(cg, &ge, ge.tmp);
                    gen_expr_free(&ge);
                }
                w_line(&cg->w, "return __ret;");
            }
            return true;
        }
        case STMT_IF: {
            return gen_if_chain(cg, path, s->as.if_s.arms, 0, s->as.if_s.arms_len, ret_void, err);
        }
        case STMT_BREAK: {
            w_line(&cg->w, "break;");
            return true;
        }
        case STMT_CONTINUE: {
            LoopCtx *lc = codegen_loop_current(cg);
            if (lc && lc->continue_label) {
                w_line(&cg->w, "goto %s;", lc->continue_label);
            } else {
                w_line(&cg->w, "continue;");
            }
            return true;
        }
        case STMT_FOR: {
            if (s->as.for_s.init) {
                if (!gen_stmt(cg, path, s->as.for_s.init, ret_void, err)) return false;
            }
            w_line(&cg->w, "for (;;) {");
            cg->w.indent++;
            char *cont_label = codegen_new_sym(cg, "for_continue");
            if (!codegen_loop_push(cg, cont_label)) {
                return cg_set_err(err, path, "out of memory");
            }
            if (s->as.for_s.cond) {
                GenExpr ct;
                if (!gen_expr(cg, path, s->as.for_s.cond, &ct, err)) return false;
                cg->var_id++;
                char *bname = arena_printf(cg->arena, "__b%d", cg->var_id);
                w_line(&cg->w, "bool %s = yis_as_bool(%s);", bname, ct.tmp);
                w_line(&cg->w, "yis_release_val(%s);", ct.tmp);
                gen_expr_release_except(cg, &ct, ct.tmp);
                gen_expr_free(&ct);
                w_line(&cg->w, "if (!%s) {", bname);
                cg->w.indent++;
                w_line(&cg->w, "break;");
                cg->w.indent--;
                w_line(&cg->w, "}");
            }
            codegen_push_scope(cg);
            if (s->as.for_s.body->kind == STMT_BLOCK) {
                if (!gen_block(cg, path, s->as.for_s.body, ret_void, err)) return false;
            } else {
                if (!gen_stmt(cg, path, s->as.for_s.body, ret_void, err)) return false;
            }
            LocalList locals = codegen_pop_scope(cg);
            codegen_release_scope(cg, locals);
            w_line(&cg->w, "%s: ;", cont_label);
            if (s->as.for_s.step) {
                GenExpr st;
                if (!gen_expr(cg, path, s->as.for_s.step, &st, err)) return false;
                w_line(&cg->w, "yis_release_val(%s);", st.tmp);
                gen_expr_release_except(cg, &st, st.tmp);
                gen_expr_free(&st);
            }
            codegen_loop_pop(cg);
            cg->w.indent--;
            w_line(&cg->w, "}");
            return true;
        }
        case STMT_FOREACH: {
            GenExpr it;
            if (!gen_expr(cg, path, s->as.foreach_s.expr, &it, err)) return false;
            char *idx_name = codegen_new_sym(cg, "i");
            char *len_name = codegen_new_sym(cg, "len");

            Ty *elem_ty = cg_tc_expr(cg, path, s->as.foreach_s.expr, err);
            if (!elem_ty) return false;
            Ty *ety = NULL;
            if (elem_ty->tag == TY_ARRAY && elem_ty->elem) {
                ety = elem_ty->elem;
            } else if (elem_ty->tag == TY_PRIM && str_eq_c(elem_ty->name, "string")) {
                ety = elem_ty; // string
            } else {
                return cg_set_err(err, path, "foreach expects array or string");
            }

            codegen_push_scope(cg);
            char *cvar = codegen_define_local(cg, s->as.foreach_s.name, ety, false, false);
            w_line(&cg->w, "YisVal %s = YV_NULLV;", cvar);

            w_line(&cg->w, "int %s = stdr_len(%s);", len_name, it.tmp);
            w_line(&cg->w, "for (int %s = 0; %s < %s; %s++) {", idx_name, idx_name, len_name, idx_name);
            cg->w.indent++;
            if (!codegen_loop_push(cg, NULL)) {
                return cg_set_err(err, path, "out of memory");
            }
            codegen_push_scope(cg);

            if (elem_ty->tag == TY_ARRAY) {
                w_line(&cg->w, "YisVal __e = yis_arr_get((YisArr*)%s.as.p, %s);", it.tmp, idx_name);
            } else {
                w_line(&cg->w, "YisVal __e = stdr_str_at(%s, %s);", it.tmp, idx_name);
            }
            w_line(&cg->w, "yis_move_into(&%s, __e);", cvar);

            if (s->as.foreach_s.body->kind == STMT_BLOCK) {
                if (!gen_block(cg, path, s->as.foreach_s.body, ret_void, err)) return false;
            } else {
                if (!gen_stmt(cg, path, s->as.foreach_s.body, ret_void, err)) return false;
            }

            LocalList inner_locals = codegen_pop_scope(cg);
            codegen_release_scope(cg, inner_locals);
            codegen_loop_pop(cg);
            cg->w.indent--;
            w_line(&cg->w, "}");

            LocalList outer_locals = codegen_pop_scope(cg);
            codegen_release_scope(cg, outer_locals);
            w_line(&cg->w, "yis_release_val(%s);", it.tmp);
            gen_expr_release_except(cg, &it, it.tmp);
            gen_expr_free(&it);
            return true;
        }
        case STMT_BLOCK: {
            w_line(&cg->w, "{");
            cg->w.indent++;
            codegen_push_scope(cg);
            if (!gen_block(cg, path, s, ret_void, err)) return false;
            LocalList locals = codegen_pop_scope(cg);
            codegen_release_scope(cg, locals);
            cg->w.indent--;
            w_line(&cg->w, "}");
            return true;
        }
        default:
            return cg_set_err(err, path, "unhandled stmt in codegen");
    }
}

// -----------------
// Function generation
// -----------------

static bool gen_class_defs(Codegen *cg, Diag *err) {
    (void)err;
    for (size_t i = 0; i < cg->class_decls_len; i++) {
        ClassDeclEntry *ce = &cg->class_decls[i];
        Str mod, name;
        split_qname(ce->qname, &mod, &name);
        char *cname = codegen_c_class_name(cg, ce->qname);
        char *drop_sym = mod.len ? arena_printf(cg->arena, "yis_drop_%s_%.*s", mangle_mod(cg->arena, mod), (int)name.len, name.data)
                                  : arena_printf(cg->arena, "yis_drop_%.*s", (int)name.len, name.data);
        w_line(&cg->w, "typedef struct %s {", cname);
        cg->w.indent++;
        w_line(&cg->w, "YisObj base;");
        for (size_t f = 0; f < ce->decl->fields_len; f++) {
            FieldDecl *fd = ce->decl->fields[f];
            w_line(&cg->w, "YisVal %s;", codegen_c_field_name(cg, fd->name));
        }
        cg->w.indent--;
        w_line(&cg->w, "} %s;", cname);
        w_line(&cg->w, "static void %s(YisObj* o);", drop_sym);
        w_line(&cg->w, "");
    }
    for (size_t i = 0; i < cg->class_decls_len; i++) {
        ClassDeclEntry *ce = &cg->class_decls[i];
        Str mod, name;
        split_qname(ce->qname, &mod, &name);
        char *cname = codegen_c_class_name(cg, ce->qname);
        char *drop_sym = mod.len ? arena_printf(cg->arena, "yis_drop_%s_%.*s", mangle_mod(cg->arena, mod), (int)name.len, name.data)
                                  : arena_printf(cg->arena, "yis_drop_%.*s", (int)name.len, name.data);
        w_line(&cg->w, "static void %s(YisObj* o) {", drop_sym);
        cg->w.indent++;
        w_line(&cg->w, "%s* self = (%s*)o;", cname, cname);
        for (size_t f = 0; f < ce->decl->fields_len; f++) {
            FieldDecl *fd = ce->decl->fields[f];
            w_line(&cg->w, "yis_release_val(self->%s);", codegen_c_field_name(cg, fd->name));
        }
        cg->w.indent--;
        w_line(&cg->w, "}");
        w_line(&cg->w, "");
    }
    return true;
}

static char *c_params(size_t count, bool leading_comma) {
    if (count == 0) return dup_cstr(leading_comma ? "" : "void");
    StrBuf b; sb_init(&b);
    for (size_t i = 0; i < count; i++) {
        if (i) sb_append(&b, ", ");
        sb_appendf(&b, "YisVal a%zu", i);
    }
    char *out = b.data ? dup_cstr(b.data) : NULL;
    sb_free(&b);
    if (!out) return dup_cstr(leading_comma ? "" : "void");
    if (leading_comma) {
        StrBuf b2; sb_init(&b2);
        sb_appendf(&b2, ", %s", out);
        free(out);
        out = b2.data ? dup_cstr(b2.data) : NULL;
        sb_free(&b2);
        return out ? out : dup_cstr("");
    }
    return out;
}

static bool gen_method(Codegen *cg, Str path, ClassDecl *cls, FunDecl *fn, Diag *err) {
    cg->scopes_len = 0;
    cg->scope_locals_len = 0;
    cg->loop_stack_len = 0;
    locals_free(&cg->ty_loc);
    locals_init(&cg->ty_loc);
    codegen_push_scope(cg);

    Str qname = cls->name;
    if (cg->current_cask.len) {
        size_t len = cg->current_cask.len + 1 + cls->name.len;
        char *buf = (char *)arena_alloc(cg->arena, len + 1);
        if (!buf) return cg_set_err(err, path, "out of memory");
        memcpy(buf, cg->current_cask.data, cg->current_cask.len);
        buf[cg->current_cask.len] = '.';
        memcpy(buf + cg->current_cask.len + 1, cls->name.data, cls->name.len);
        buf[len] = '\0';
        qname.data = buf;
        qname.len = len;
    }
    cg->current_class = qname;
    cg->has_current_class = true;

    // receiver
    if (fn->params_len > 0) {
        codegen_add_name(cg, fn->params[0]->name, "self");
        Ty *self_ty = (Ty *)arena_alloc(cg->arena, sizeof(Ty));
        if (!self_ty) return cg_set_err(err, path, "out of memory");
        memset(self_ty, 0, sizeof(Ty));
        self_ty->tag = TY_CLASS;
        self_ty->name = qname;
        Binding b = { self_ty, fn->params[0]->is_mut, false, false };
        locals_define(&cg->ty_loc, fn->params[0]->name, b);
    }

    ClassInfo *ci = codegen_class_info(cg, qname);
    FunSig *sig = NULL;
    if (ci) {
        for (size_t i = 0; i < ci->methods_len; i++) {
            if (str_eq(ci->methods[i].name, fn->name)) {
                sig = ci->methods[i].sig;
                break;
            }
        }
    }

    // params after this
    for (size_t i = 1; i < fn->params_len; i++) {
        Param *p = fn->params[i];
        char *cname = arena_printf(cg->arena, "a%zu", i - 1);
        codegen_add_name(cg, p->name, cname);
        Ty *pty = sig ? sig->params[i - 1] : NULL;
        Binding b = { pty, p->is_mut, false, false };
        locals_define(&cg->ty_loc, p->name, b);
    }

    bool ret_void = fn->ret.is_void;
    const char *ret_ty = ret_void ? "void" : "YisVal";
    char *params = c_params(fn->params_len > 0 ? fn->params_len - 1 : 0, true);
    char *mangled = mangle_method(cg->arena, cg->current_cask, cls->name, fn->name);
    w_line(&cg->w, "static %s %s(YisVal self%s) {", ret_ty, mangled, params ? params : "");
    cg->w.indent++;
    if (!ret_void) {
        w_line(&cg->w, "YisVal __ret = YV_NULLV;");
    }

    if (!gen_block(cg, path, fn->body, ret_void, err)) return false;
    {
        LocalList locals = codegen_pop_scope(cg);
        codegen_release_scope(cg, locals);
    }

    if (!ret_void) {
        w_line(&cg->w, "return __ret;");
    }
    cg->w.indent--;
    w_line(&cg->w, "}");
    w_line(&cg->w, "");
    if (params) free(params);
    cg->has_current_class = false;
    return true;
}

static bool gen_fun(Codegen *cg, Str path, FunDecl *fn, Diag *err) {
    cg->scopes_len = 0;
    cg->scope_locals_len = 0;
    cg->loop_stack_len = 0;
    locals_free(&cg->ty_loc);
    locals_init(&cg->ty_loc);
    codegen_push_scope(cg);
    cg->has_current_class = false;

    FunSig *sig = codegen_fun_sig(cg, cg->current_cask, fn->name);

    for (size_t i = 0; i < fn->params_len; i++) {
        Param *p = fn->params[i];
        char *cname = arena_printf(cg->arena, "a%zu", i);
        codegen_add_name(cg, p->name, cname);
        Ty *pty = sig ? sig->params[i] : NULL;
        Binding b = { pty, p->is_mut, false, false };
        locals_define(&cg->ty_loc, p->name, b);
    }

    bool ret_void = fn->ret.is_void;
    const char *ret_ty = ret_void ? "void" : "YisVal";
    char *params = c_params(fn->params_len, false);
    char *mangled = mangle_global(cg->arena, cg->current_cask, fn->name);
    w_line(&cg->w, "static %s %s(%s) {", ret_ty, mangled, params ? params : "void");
    cg->w.indent++;
    if (!ret_void) {
        w_line(&cg->w, "YisVal __ret = YV_NULLV;");
    }

    if (!gen_block(cg, path, fn->body, ret_void, err)) return false;
    {
        LocalList locals = codegen_pop_scope(cg);
        codegen_release_scope(cg, locals);
    }

    if (!ret_void) {
        w_line(&cg->w, "return __ret;");
    }
    cg->w.indent--;
    w_line(&cg->w, "}");
    w_line(&cg->w, "");
    if (params) free(params);
    return true;
}

static bool gen_entry(Codegen *cg, Diag *err) {
    EntryDecl *entry_decl = NULL;
    Str entry_path = {NULL, 0};
    for (size_t i = 0; i < cg->prog->mods_len; i++) {
        Module *m = cg->prog->mods[i];
        for (size_t j = 0; j < m->decls_len; j++) {
            if (m->decls[j]->kind == DECL_ENTRY) {
                entry_decl = &m->decls[j]->as.entry;
                entry_path = m->path;
            }
        }
    }
    if (!entry_decl) {
        return cg_set_err(err, (Str){NULL, 0}, "missing entry()");
    }
    
    // Store entry path for use in main()
    cg->entry_path = entry_path;

    cg->scopes_len = 0;
    cg->scope_locals_len = 0;
    cg->loop_stack_len = 0;
    locals_free(&cg->ty_loc);
    locals_init(&cg->ty_loc);
    codegen_push_scope(cg);

    if (entry_path.data) {
        Str mod_name = codegen_cask_name(cg, entry_path);
        cg->current_cask = mod_name;
        ModuleImport *mi = codegen_cask_imports(cg, mod_name);
        cg->current_imports = mi ? mi->imports : NULL;
        cg->current_imports_len = mi ? mi->imports_len : 0;
    }

    if (entry_decl->body && entry_decl->body->line > 0)
        emit_line_directive(cg, entry_path, entry_decl->body->line);
    w_line(&cg->w, "static void yis_entry(void) {");
    cg->w.indent++;
    for (size_t i = 0; i < cg->prog->mods_len; i++) {
        Module *m = cg->prog->mods[i];
        Str mod_name = codegen_cask_name(cg, m->path);
        ModuleGlobals *mg = codegen_cask_globals(cg, mod_name);
        if (mg && mg->len > 0) {
            char *init_name = mangle_global_init(cg->arena, mod_name);
            w_line(&cg->w, "%s();", init_name);
        }
    }
    if (!gen_block(cg, entry_path, entry_decl->body, true, err)) return false;
    {
        LocalList locals = codegen_pop_scope(cg);
        codegen_release_scope(cg, locals);
    }
    cg->w.indent--;
    w_line(&cg->w, "}");
    w_line(&cg->w, "");
    return true;
}

// -----------------
// Codegen top-level
// -----------------

static bool codegen_init(Codegen *cg, Program *prog, Arena *arena, Diag *err) {
    memset(cg, 0, sizeof(*cg));
    cg->prog = prog;
    cg->arena = arena;
    sb_init(&cg->out);
    cg->w.buf = &cg->out;
    cg->w.indent = 0;

    cg->env = build_global_env(prog, arena, err);
    if (!cg->env) {
        return false;
    }

    // Build hash map caches for env lookups
    if (cg->env->classes_len > 0) {
        strmap_init(&cg->class_map, arena, cg->env->classes_len);
        for (size_t i = 0; i < cg->env->classes_len; i++) {
            strmap_put(&cg->class_map, cg->env->classes[i].qname, i);
        }
    }
    if (cg->env->funs_len > 0) {
        strmap_init(&cg->fun_map, arena, cg->env->funs_len);
        for (size_t i = 0; i < cg->env->funs_len; i++) {
            // Build composite key "cask\0name"
            FunSig *fs = &cg->env->funs[i];
            size_t klen = fs->cask.len + 1 + fs->name.len;
            char *key = (char *)arena_alloc(arena, klen);
            if (key) {
                memcpy(key, fs->cask.data, fs->cask.len);
                key[fs->cask.len] = '\0';
                memcpy(key + fs->cask.len + 1, fs->name.data, fs->name.len);
                strmap_put(&cg->fun_map, (Str){key, klen}, i);
            }
        }
    }
    if (cg->env->cask_globals_len > 0) {
        strmap_init(&cg->cask_globals_map, arena, cg->env->cask_globals_len);
        for (size_t i = 0; i < cg->env->cask_globals_len; i++) {
            strmap_put(&cg->cask_globals_map, cg->env->cask_globals[i].cask, i);
        }
    }
    if (cg->env->cask_consts_len > 0) {
        strmap_init(&cg->cask_consts_map, arena, cg->env->cask_consts_len);
        for (size_t i = 0; i < cg->env->cask_consts_len; i++) {
            strmap_put(&cg->cask_consts_map, cg->env->cask_consts[i].cask, i);
        }
    }
    if (cg->env->cask_imports_len > 0) {
        strmap_init(&cg->cask_imports_map, arena, cg->env->cask_imports_len);
        for (size_t i = 0; i < cg->env->cask_imports_len; i++) {
            strmap_put(&cg->cask_imports_map, cg->env->cask_imports[i].cask, i);
        }
    }

    // build class decl map
    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        Str mod_name = codegen_cask_name(cg, m->path);
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind == DECL_CLASS) {
                Str qname = {NULL, 0};
                char *buf = arena_printf(arena, "%.*s.%.*s", (int)mod_name.len, mod_name.data, (int)d->as.class_decl.name.len, d->as.class_decl.name.data);
                if (buf) {
                    qname.data = buf;
                    qname.len = mod_name.len + 1 + d->as.class_decl.name.len;
                }
                if (cg->class_decls_len + 1 > cg->class_decls_cap) {
                    size_t next = cg->class_decls_cap ? cg->class_decls_cap * 2 : 8;
                    ClassDeclEntry *arr = (ClassDeclEntry *)realloc(cg->class_decls, next * sizeof(ClassDeclEntry));
                    if (!arr) return cg_set_err(err, m->path, "out of memory");
                    cg->class_decls = arr;
                    cg->class_decls_cap = next;
                }
                cg->class_decls[cg->class_decls_len].qname = qname;
                cg->class_decls[cg->class_decls_len].decl = &d->as.class_decl;
                cg->class_decls_len++;
            }
        }
    }

    locals_init(&cg->ty_loc);
    codegen_push_scope(cg);

    return true;
}

static void codegen_free(Codegen *cg) {
    for (size_t i = 0; i < cg->scopes_len; i++) {
        free(cg->scopes[i].items);
    }
    free(cg->scopes);
    for (size_t i = 0; i < cg->scope_locals_len; i++) {
        free(cg->scope_locals[i].items);
    }
    free(cg->scope_locals);
    free(cg->loop_stack);
    free(cg->lambdas);
    free(cg->funvals);
    free(cg->class_decls);
    locals_free(&cg->ty_loc);
    sb_free(&cg->out);
}

static bool codegen_gen(Codegen *cg, const char *ext_module_name, const char *ext_bindings_path, Diag *err) {
    bool has_ext_module = ext_module_name && ext_module_name[0];
    codegen_collect_lambdas(cg);

    const char *runtime_override = getenv("YIS_RUNTIME");
    bool runtime_forced = runtime_override && runtime_override[0];
    const char *runtime_path = runtime_forced ? runtime_override : NULL;
    char *exe_runtime_path = NULL;  // heap-allocated, freed below
    if (!runtime_path) {
        const char *runtime_candidates[] = {
            "src/bootstrap/runtime.inc",
            "src/runtime.inc",
            "yis/src/bootstrap/runtime.inc",
            "yis/src/runtime.inc",
            "../src/bootstrap/runtime.inc",
            "../src/runtime.inc",
            "../yis/src/bootstrap/runtime.inc",
            "../yis/src/runtime.inc",
            "../../yis/src/bootstrap/runtime.inc",
            "../../yis/src/runtime.inc",
        };
        runtime_path = NULL;
        for (size_t i = 0; i < sizeof(runtime_candidates) / sizeof(runtime_candidates[0]); i++) {
            if (path_is_file(runtime_candidates[i])) {
                runtime_path = runtime_candidates[i];
                break;
            }
        }
        // If not found via cwd-relative paths, try relative to the executable.
        // The binary is typically at <project>/yis/build/yis, so runtime is at
        // <exe_dir>/../src/bootstrap/runtime.inc.
        if (!runtime_path) {
            char *exe_dir = yis_exe_dir();
            if (exe_dir) {
                const char *exe_rel[] = {
                    "../src/bootstrap/runtime.inc",
                    "../src/runtime.inc",
                    "../../yis/src/bootstrap/runtime.inc",
                    "../../yis/src/runtime.inc",
                };
                for (size_t i = 0; i < sizeof(exe_rel) / sizeof(exe_rel[0]); i++) {
                    char *candidate = path_join(exe_dir, exe_rel[i]);
                    if (candidate && path_is_file(candidate)) {
                        exe_runtime_path = candidate;
                        runtime_path = exe_runtime_path;
                        break;
                    }
                    free(candidate);
                }
                free(exe_dir);
            }
        }
    }
    Arena tmp_arena;
    arena_init(&tmp_arena);
    size_t runtime_len = 0;
    const char *runtime_src = NULL;
    if (runtime_path && runtime_path[0]) {
        char *runtime_file_src = read_file_with_includes(runtime_path, "// @include", &tmp_arena, &runtime_len, NULL);
        if (runtime_file_src) {
            runtime_src = runtime_file_src;
        } else if (runtime_forced) {
            arena_free(&tmp_arena);
            free(exe_runtime_path);
            return cg_set_err(err, (Str){runtime_path, strlen(runtime_path)}, "failed to read runtime.inc");
        }
    }
    if (!runtime_src) {
        runtime_src = (const char *)yis_runtime_embedded;
        runtime_len = (size_t)yis_runtime_embedded_len;
    }

    size_t module_bindings_len = 0;
    const char *module_bindings_src = NULL;
    if (ext_bindings_path && ext_bindings_path[0]) {
        char *bindings_src =
            read_file_with_includes(ext_bindings_path, "// @include", &tmp_arena, &module_bindings_len, NULL);
        if (bindings_src) {
            module_bindings_src = bindings_src;
        } else {
            arena_free(&tmp_arena);
            free(exe_runtime_path);
            return cg_set_err(err, (Str){ext_bindings_path, strlen(ext_bindings_path)},
                              "failed to read module bindings file");
        }
    }

    sb_append_n(&cg->out, runtime_src, runtime_len);
    if (runtime_len == 0 || runtime_src[runtime_len - 1] != '\n') {
        sb_append_char(&cg->out, '\n');
    }
    if (has_ext_module) {
        if (!module_bindings_src || module_bindings_len == 0) {
            arena_free(&tmp_arena);
            free(exe_runtime_path);
            return cg_set_err(err, (Str){0}, "program imports external module but bindings file was not found");
        }
        sb_append_n(&cg->out, module_bindings_src, module_bindings_len);
        if (module_bindings_src[module_bindings_len - 1] != '\n') {
            sb_append_char(&cg->out, '\n');
        }
    }
    arena_free(&tmp_arena);
    free(exe_runtime_path);
    exe_runtime_path = NULL;

    w_line(&cg->w, "// ---- cask globals ----");
    for (size_t i = 0; i < cg->prog->mods_len; i++) {
        Module *m = cg->prog->mods[i];
        Str mod_name = codegen_cask_name(cg, m->path);
        ModuleGlobals *mg = codegen_cask_globals(cg, mod_name);
        if (!mg || mg->len == 0) continue;
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind != DECL_DEF) continue;
            char *gname = mangle_global_var(cg->arena, mod_name, d->as.def_decl.name);
            w_line(&cg->w, "static YisVal %s = YV_NULLV;", gname);
        }
    }
    w_line(&cg->w, "");

    w_line(&cg->w, "// ---- class definitions ----");
    if (!gen_class_defs(cg, err)) return false;
    w_line(&cg->w, "");

    if (cg->lambdas_len > 0) {
        w_line(&cg->w, "// ---- lambda forward decls ----");
        for (size_t i = 0; i < cg->lambdas_len; i++) {
            w_line(&cg->w, "static YisVal %s(void* env, int argc, YisVal* argv);", cg->lambdas[i].name);
        }
        w_line(&cg->w, "");
    }

    if (cg->funvals_len > 0) {
        w_line(&cg->w, "// ---- function value forward decls ----");
        for (size_t i = 0; i < cg->funvals_len; i++) {
            w_line(&cg->w, "static YisVal %s(void* env, int argc, YisVal* argv);", cg->funvals[i].wrapper);
        }
        w_line(&cg->w, "");
    }

    w_line(&cg->w, "// ---- forward decls ----");
    for (size_t i = 0; i < cg->prog->mods_len; i++) {
        Module *m = cg->prog->mods[i];
        Str mod_name = codegen_cask_name(cg, m->path);
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind == DECL_CLASS) {
                for (size_t k = 0; k < d->as.class_decl.methods_len; k++) {
                    FunDecl *md = d->as.class_decl.methods[k];
                    const char *ret_ty = md->ret.is_void ? "void" : "YisVal";
                    char *params = c_params(md->params_len > 0 ? md->params_len - 1 : 0, true);
                    char *mangled = mangle_method(cg->arena, mod_name, d->as.class_decl.name, md->name);
                    w_line(&cg->w, "static %s %s(YisVal self%s);", ret_ty, mangled, params ? params : "");
                    if (params) free(params);
                }
            }
            if (d->kind == DECL_FUN) {
                const char *ret_ty = d->as.fun.ret.is_void ? "void" : "YisVal";
                char *params = c_params(d->as.fun.params_len, false);
                char *mangled = mangle_global(cg->arena, mod_name, d->as.fun.name);
                w_line(&cg->w, "static %s %s(%s);", ret_ty, mangled, params ? params : "void");
                if (params) free(params);
            }
        }
        ModuleGlobals *mg = codegen_cask_globals(cg, mod_name);
        if (mg && mg->len > 0) {
            char *init_name = mangle_global_init(cg->arena, mod_name);
            w_line(&cg->w, "static void %s(void);", init_name);
        }
    }
    w_line(&cg->w, "static void yis_entry(void);");
    w_line(&cg->w, "");

    if (cg->funvals_len > 0) {
        w_line(&cg->w, "// ---- function value defs ----");
        for (size_t i = 0; i < cg->funvals_len; i++) {
            FunValInfo *fi = &cg->funvals[i];
            FunSig *sig = codegen_fun_sig(cg, fi->cask, fi->name);
            if (!sig) continue;
            w_line(&cg->w, "static YisVal %s(void* env, int argc, YisVal* argv) {", fi->wrapper);
            cg->w.indent++;
            w_line(&cg->w, "(void)env;");
            w_line(&cg->w, "if (argc != %zu) yis_trap(\"fn arity mismatch\");", sig->params_len);
            for (size_t p = 0; p < sig->params_len; p++) {
                w_line(&cg->w, "YisVal arg%zu = argv[%zu];", p, p);
            }
            bool ret_void = sig->ret && sig->ret->tag == TY_VOID;
            bool direct_extern_stub = is_extern_stub_sig(sig);
            if (ret_void) {
                StrBuf line; sb_init(&line);
                if (direct_extern_stub) {
                    sb_append_n(&line, sig->name.data, sig->name.len);
                    sb_append(&line, "(");
                } else {
                    char *mangled = mangle_global(cg->arena, sig->cask, sig->name);
                    sb_appendf(&line, "%s(", mangled);
                }
                for (size_t p = 0; p < sig->params_len; p++) {
                    if (p) sb_append(&line, ", ");
                    sb_appendf(&line, "arg%zu", p);
                }
                sb_append(&line, ");");
                w_line(&cg->w, "%s", line.data ? line.data : "");
                sb_free(&line);
                w_line(&cg->w, "return YV_NULLV;");
            } else {
                StrBuf line; sb_init(&line);
                if (direct_extern_stub) {
                    sb_append(&line, "return ");
                    sb_append_n(&line, sig->name.data, sig->name.len);
                    sb_append(&line, "(");
                } else {
                    char *mangled = mangle_global(cg->arena, sig->cask, sig->name);
                    sb_appendf(&line, "return %s(", mangled);
                }
                for (size_t p = 0; p < sig->params_len; p++) {
                    if (p) sb_append(&line, ", ");
                    sb_appendf(&line, "arg%zu", p);
                }
                sb_append(&line, ");");
                w_line(&cg->w, "%s", line.data ? line.data : "");
                sb_free(&line);
            }
            cg->w.indent--;
            w_line(&cg->w, "}");
        }
        w_line(&cg->w, "");
    }

    w_line(&cg->w, "// ---- cask global init ----");
    for (size_t i = 0; i < cg->prog->mods_len; i++) {
        Module *m = cg->prog->mods[i];
        Str mod_name = codegen_cask_name(cg, m->path);
        ModuleGlobals *mg = codegen_cask_globals(cg, mod_name);
        if (!mg || mg->len == 0) continue;
        char *init_name = mangle_global_init(cg->arena, mod_name);
        w_line(&cg->w, "static void %s(void) {", init_name);
        cg->w.indent++;
        Str saved_mod = cg->current_cask;
        Str *saved_imports = cg->current_imports;
        size_t saved_imports_len = cg->current_imports_len;
        cg->current_cask = mod_name;
        ModuleImport *mi = codegen_cask_imports(cg, mod_name);
        cg->current_imports = mi ? mi->imports : NULL;
        cg->current_imports_len = mi ? mi->imports_len : 0;

        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind != DECL_DEF) continue;
            if (d->line > 0) emit_line_directive(cg, m->path, d->line);
            GenExpr ge;
            if (!gen_expr(cg, m->path, d->as.def_decl.expr, &ge, err)) return false;
            char *gname = mangle_global_var(cg->arena, mod_name, d->as.def_decl.name);
            w_line(&cg->w, "yis_move_into(&%s, %s);", gname, ge.tmp);
            gen_expr_release_except(cg, &ge, ge.tmp);
            gen_expr_free(&ge);
        }

        cg->current_cask = saved_mod;
        cg->current_imports = saved_imports;
        cg->current_imports_len = saved_imports_len;
        cg->w.indent--;
        w_line(&cg->w, "}");
        w_line(&cg->w, "");
    }

    w_line(&cg->w, "// ---- compiled functions ----");
    for (size_t i = 0; i < cg->prog->mods_len; i++) {
        Module *m = cg->prog->mods[i];
        Str mod_name = codegen_cask_name(cg, m->path);
        cg->current_cask = mod_name;
        ModuleImport *mi = codegen_cask_imports(cg, mod_name);
        cg->current_imports = mi ? mi->imports : NULL;
        cg->current_imports_len = mi ? mi->imports_len : 0;
        cg->has_current_class = false;
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (d->kind == DECL_CLASS) {
                for (size_t k = 0; k < d->as.class_decl.methods_len; k++) {
                    if (!gen_method(cg, m->path, &d->as.class_decl, d->as.class_decl.methods[k], err)) return false;
                }
            }
            if (d->kind == DECL_FUN) {
                emit_line_directive(cg, m->path, d->line);
                if (!gen_fun(cg, m->path, &d->as.fun, err)) return false;
            }
        }
    }

    w_line(&cg->w, "// ---- entry ----");
    if (!gen_entry(cg, err)) return false;

    // ---- deferred lambda bodies ----
    // Generated AFTER functions/methods/entry so captures are populated
    w_line(&cg->w, "// ---- lambda defs ----");
    for (size_t i = 0; i < cg->lambdas_len; i++) {
        LambdaInfo *li = &cg->lambdas[i];
        // save state
        NameScope *saved_scopes = cg->scopes;
        size_t saved_scopes_len = cg->scopes_len;
        size_t saved_scopes_cap = cg->scopes_cap;
        LocalList *saved_locals = cg->scope_locals;
        size_t saved_locals_len = cg->scope_locals_len;
        size_t saved_locals_cap = cg->scope_locals_cap;
        Locals saved_ty = cg->ty_loc;
        Str saved_mod = cg->current_cask;
        Str *saved_imports = cg->current_imports;
        size_t saved_imports_len = cg->current_imports_len;
        Str saved_class = cg->current_class;
        bool saved_has_class = cg->has_current_class;
        int saved_indent = cg->w.indent;

        cg->scopes = NULL;
        cg->scopes_len = 0;
        cg->scopes_cap = 0;
        cg->scope_locals = NULL;
        cg->scope_locals_len = 0;
        cg->scope_locals_cap = 0;
        locals_init(&cg->ty_loc);
        codegen_push_scope(cg);
        cg->w.indent = 0;

        // set cask context
        Str mod_name = codegen_cask_name(cg, li->path);
        cg->current_cask = mod_name;
        ModuleImport *mi = codegen_cask_imports(cg, mod_name);
        cg->current_imports = mi ? mi->imports : NULL;
        cg->current_imports_len = mi ? mi->imports_len : 0;
        cg->has_current_class = false;

        // emit lambda
        w_line(&cg->w, "static YisVal %s(void* env, int argc, YisVal* argv) {", li->name);
        cg->w.indent++;

        // Unpack captured variables from env
        if (li->lam->as.lambda.captures_len > 0) {
            w_line(&cg->w, "YisVal* __caps = (YisVal*)env;");
            for (size_t c = 0; c < li->lam->as.lambda.captures_len; c++) {
                Capture *cap = li->lam->as.lambda.captures[c];
                char *cname = arena_printf(cg->arena, "__cap%zu", c);
                w_line(&cg->w, "YisVal %s = __caps[%zu];", cname, c);
                codegen_add_name(cg, cap->name, cname);
                Binding b = { (Ty*)cap->ty, false, false, false };
                locals_define(&cg->ty_loc, cap->name, b);
            }
        } else {
            w_line(&cg->w, "(void)env;");
        }

        w_line(&cg->w, "if (argc != %zu) yis_trap(\"lambda arity mismatch\");", li->lam->as.lambda.params_len);
        for (size_t p = 0; p < li->lam->as.lambda.params_len; p++) {
            Param *param = li->lam->as.lambda.params[p];
            char *cname = arena_printf(cg->arena, "arg%zu", p);
            w_line(&cg->w, "YisVal %s = argv[%zu];", cname, p);
            codegen_add_name(cg, param->name, cname);
            Ty *pty = NULL;
            if (param->typ) {
                pty = cg_ty_from_type_ref(cg, param->typ, cg->current_cask, cg->current_imports, cg->current_imports_len, err);
            } else {
                pty = cg_ty_gen(cg, param->name);
            }
            Binding b = { pty, param->is_mut, false, false };
            locals_define(&cg->ty_loc, param->name, b);
        }
        w_line(&cg->w, "YisVal __ret = YV_NULLV;");
        GenExpr ge;
        if (!gen_expr(cg, li->path, li->lam->as.lambda.body, &ge, err)) return false;
        w_line(&cg->w, "yis_move_into(&__ret, %s);", ge.tmp);
        gen_expr_release_except(cg, &ge, ge.tmp);
        gen_expr_free(&ge);
        {
            LocalList locals = codegen_pop_scope(cg);
            codegen_release_scope(cg, locals);
        }
        w_line(&cg->w, "return __ret;");
        cg->w.indent--;
        w_line(&cg->w, "}");
        w_line(&cg->w, "");

        // cleanup lambda state
        for (size_t si = 0; si < cg->scopes_len; si++) {
            free(cg->scopes[si].items);
        }
        free(cg->scopes);
        for (size_t lii = 0; lii < cg->scope_locals_len; lii++) {
            free(cg->scope_locals[lii].items);
        }
        free(cg->scope_locals);
        locals_free(&cg->ty_loc);

        // restore state
        cg->scopes = saved_scopes;
        cg->scopes_len = saved_scopes_len;
        cg->scopes_cap = saved_scopes_cap;
        cg->scope_locals = saved_locals;
        cg->scope_locals_len = saved_locals_len;
        cg->scope_locals_cap = saved_locals_cap;
        cg->ty_loc = saved_ty;
        cg->current_cask = saved_mod;
        cg->current_imports = saved_imports;
        cg->current_imports_len = saved_imports_len;
        cg->current_class = saved_class;
        cg->has_current_class = saved_has_class;
        cg->w.indent = saved_indent;
    }
    w_line(&cg->w, "");

    w_line(&cg->w, "int main(int argc, char **argv) {");
    cg->w.indent++;
    w_line(&cg->w, "yis_set_args(argc, argv);");
    w_line(&cg->w, "#ifdef __OBJC__");
    w_line(&cg->w, "@autoreleasepool {");
    cg->w.indent++;
    w_line(&cg->w, "yis_runtime_init();");
    w_line(&cg->w, "yis_entry();");
    cg->w.indent--;
    w_line(&cg->w, "}");
    w_line(&cg->w, "#else");
    w_line(&cg->w, "yis_runtime_init();");
    w_line(&cg->w, "yis_entry();");
    w_line(&cg->w, "#endif");
    w_line(&cg->w, "return 0;");
    cg->w.indent--;
    w_line(&cg->w, "}");
    return true;
}
bool emit_c(Program *prog, const char *out_path,
            const char *ext_module_name, const char *ext_bindings_path,
            Diag *err) {
    if (!prog || !out_path) {
        return cg_set_err(err, (Str){NULL, 0}, "emit_c: missing program or output path");
    }
    Arena arena;
    arena_init(&arena);
    Codegen cg;
    if (!codegen_init(&cg, prog, &arena, err)) {
        arena_free(&arena);
        return false;
    }
    if (!codegen_gen(&cg, ext_module_name, ext_bindings_path, err)) {
        codegen_free(&cg);
        arena_free(&arena);
        return false;
    }
    FILE *f = fopen(out_path, "wb");
    if (!f) {
        codegen_free(&cg);
        arena_free(&arena);
        return cg_set_err(err, (Str){out_path, strlen(out_path)}, "failed to open output file");
    }
    fwrite(cg.out.data, 1, cg.out.len, f);
    fclose(f);
    codegen_free(&cg);
    arena_free(&arena);
    return true;
}
