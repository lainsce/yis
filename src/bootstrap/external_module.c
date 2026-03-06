#include "external_module.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "file.h"
#include "platform.h"

// --- helpers ---

// Build the upper-cased env var name: YIS_{NAME}_{suffix}
// e.g. name="cogito", suffix="PATH" => "YIS_COGITO_PATH"
static const char *module_env(const char *name, const char *suffix) {
    static char env_buf[256];
    size_t nlen = strlen(name);
    if (nlen + strlen(suffix) + 5 >= sizeof(env_buf)) return NULL;
    memcpy(env_buf, "YIS_", 4);
    for (size_t i = 0; i < nlen; i++) {
        env_buf[4 + i] = (char)toupper((unsigned char)name[i]);
    }
    env_buf[4 + nlen] = '_';
    size_t slen = strlen(suffix);
    memcpy(env_buf + 5 + nlen, suffix, slen);
    env_buf[5 + nlen + slen] = '\0';
    return env_buf;
}

static const char *get_module_env(const char *name, const char *suffix) {
    const char *var = module_env(name, suffix);
    if (!var) return NULL;
    const char *val = getenv(var);
    return (val && val[0]) ? val : NULL;
}

// Build "{name}.yi"
static char *module_yi_name(const char *name) {
    size_t nlen = strlen(name);
    char *buf = (char *)malloc(nlen + 4);
    if (!buf) return NULL;
    memcpy(buf, name, nlen);
    memcpy(buf + nlen, ".yi", 4);
    return buf;
}

// --- resolve_external_module ---

char *resolve_external_module(const char *name, const char *stdlib_dir) {
    if (!name || !name[0]) return NULL;
    char *yi_name = module_yi_name(name);
    if (!yi_name) return NULL;

    char *result = NULL;

    // 1) YIS_{NAME}_PATH env var — direct file or directory
    // Also accept legacy YIS_{NAME}_STDLIB for backward compatibility
    const char *env_path = get_module_env(name, "PATH");
    if (!env_path) env_path = get_module_env(name, "STDLIB");
    if (env_path) {
        if (path_is_file(env_path)) {
            free(yi_name);
            return strdup(env_path);
        }
        result = path_join(env_path, yi_name);
        if (result && path_is_file(result)) {
            free(yi_name);
            return result;
        }
        free(result);
        result = NULL;
    }

    // 2) cwd-relative development paths
    {
        char pat[512];
        const char *dev_patterns[] = {
            "%s/%s",              // {name}/{name}.yi
            "%s/src/%s",          // {name}/src/{name}.yi
            "%s/_build/%s",
            "%s/build/%s",
        };
        const char *prefixes[] = { "", "../", "../../" };
        for (size_t p = 0; p < sizeof(prefixes) / sizeof(prefixes[0]); p++) {
            for (size_t d = 0; d < sizeof(dev_patterns) / sizeof(dev_patterns[0]); d++) {
                int n = snprintf(pat, sizeof(pat), "%s", prefixes[p]);
                n += snprintf(pat + n, sizeof(pat) - (size_t)n, dev_patterns[d], name, yi_name);
                (void)n;
                if (path_is_file(pat)) {
                    free(yi_name);
                    return strdup(pat);
                }
            }
        }
    }

    // 3) Installed stdlib
    if (stdlib_dir && stdlib_dir[0]) {
        result = path_join(stdlib_dir, yi_name);
        if (result && path_is_file(result)) {
            free(yi_name);
            return result;
        }
        free(result);
        result = NULL;
    }
    {
        const char *sys_dirs[] = {
#if defined(__APPLE__)
            "/opt/homebrew/share/yis/stdlib",
            "/usr/local/share/yis/stdlib",
#endif
            "/usr/share/yis/stdlib",
        };
        for (size_t i = 0; i < sizeof(sys_dirs) / sizeof(sys_dirs[0]); i++) {
            result = path_join(sys_dirs[i], yi_name);
            if (result && path_is_file(result)) {
                free(yi_name);
                return result;
            }
            free(result);
            result = NULL;
        }
    }

    // 4) Relative to executable
    {
        char *exe_dir = yis_exe_dir();
        if (exe_dir) {
            char rel[512];
            const char *exe_prefixes[] = {
                "../../%s/src/%s",
                "../../%s/%s",
                "../../%s/_build/%s",
                "../../%s/build/%s",
                "../share/yis/stdlib/%s",
            };
            for (size_t i = 0; i < sizeof(exe_prefixes) / sizeof(exe_prefixes[0]); i++) {
                if (i < 4) {
                    snprintf(rel, sizeof(rel), exe_prefixes[i], name, yi_name);
                } else {
                    // For stdlib path: only sub {name}.yi
                    snprintf(rel, sizeof(rel), exe_prefixes[i], yi_name);
                }
                result = path_join(exe_dir, rel);
                if (result && path_is_file(result)) {
                    free(yi_name);
                    free(exe_dir);
                    return result;
                }
                free(result);
                result = NULL;
            }
            free(exe_dir);
        }
    }

    free(yi_name);
    return NULL;
}

// --- find_module_bindings ---

char *find_module_bindings(const char *name, const char *module_yi_path) {
    if (!name || !name[0]) return NULL;

    // Build "{name}_bindings.inc"
    size_t nlen = strlen(name);
    char inc_name[256];
    if (nlen + 14 >= sizeof(inc_name)) return NULL;
    snprintf(inc_name, sizeof(inc_name), "%s_bindings.inc", name);

    char *result = NULL;

    // 1) YIS_{NAME}_BINDINGS env var
    const char *env = get_module_env(name, "BINDINGS");
    if (env) {
        if (path_is_file(env)) return strdup(env);
        // not found at env path, continue searching
    }

    // 2) Sibling yis/{name}_bindings.inc relative to module .yi directory
    if (module_yi_path && module_yi_path[0]) {
        char *mod_dir = path_dirname(module_yi_path);
        if (mod_dir) {
            // Try {mod_dir}/yis/{name}_bindings.inc
            char *yis_dir = path_join(mod_dir, "yis");
            if (yis_dir) {
                result = path_join(yis_dir, inc_name);
                free(yis_dir);
                if (result && path_is_file(result)) {
                    free(mod_dir);
                    return result;
                }
                free(result);
                result = NULL;
            }
            // Try {mod_dir}/{name}_bindings.inc
            result = path_join(mod_dir, inc_name);
            if (result && path_is_file(result)) {
                free(mod_dir);
                return result;
            }
            free(result);
            result = NULL;
            free(mod_dir);
        }
    }

    // 3) Installed: {datadir}/yis/{name}/{name}_bindings.inc
    {
        char sys_path[512];
        const char *sys_dirs[] = {
#if defined(__APPLE__)
            "/opt/homebrew/share/yis",
            "/usr/local/share/yis",
#endif
            "/usr/share/yis",
        };
        for (size_t i = 0; i < sizeof(sys_dirs) / sizeof(sys_dirs[0]); i++) {
            snprintf(sys_path, sizeof(sys_path), "%s/%s/%s", sys_dirs[i], name, inc_name);
            if (path_is_file(sys_path)) {
                return strdup(sys_path);
            }
        }
    }

    // 4) cwd-relative dev paths
    {
        char pat[512];
        const char *patterns[] = {
            "src/yis/%s",
            "../src/yis/%s",
            "../../src/yis/%s",
            "%s/src/yis/%s",
            "%s/yis/%s",
            "../%s/src/yis/%s",
            "../%s/yis/%s",
            "../../%s/src/yis/%s",
            "../../%s/yis/%s",
        };
        for (size_t i = 0; i < sizeof(patterns) / sizeof(patterns[0]); i++) {
            if (i < 3) {
                snprintf(pat, sizeof(pat), patterns[i], inc_name);
            } else {
                snprintf(pat, sizeof(pat), patterns[i], name, inc_name);
            }
            if (path_is_file(pat)) {
                return strdup(pat);
            }
        }
    }

    // 5) Relative to executable
    {
        char *exe_dir = yis_exe_dir();
        if (exe_dir) {
            char rel[512];
            snprintf(rel, sizeof(rel), "../share/yis/%s/%s", name, inc_name);
            result = path_join(exe_dir, rel);
            if (result && path_is_file(result)) {
                free(exe_dir);
                return result;
            }
            free(result);
            result = NULL;

            snprintf(rel, sizeof(rel), "../../share/yis/%s/%s", name, inc_name);
            result = path_join(exe_dir, rel);
            if (result && path_is_file(result)) {
                free(exe_dir);
                return result;
            }
            free(result);
            result = NULL;

            snprintf(rel, sizeof(rel), "../Resources/share/yis/%s/%s", name, inc_name);
            result = path_join(exe_dir, rel);
            if (result && path_is_file(result)) {
                free(exe_dir);
                return result;
            }
            free(result);
            result = NULL;

            free(exe_dir);
        }
    }

    return NULL;
}

// --- program_uses_module ---

static bool import_name_matches(Str sname, const char *name) {
    if (str_eq_c(sname, name)) return true;
    size_t nlen = strlen(name);
    if (sname.len >= nlen && memcmp(sname.data, name, nlen) == 0) {
        if (sname.len == nlen) return true;
        char next = sname.data[nlen];
        if (next == '.' || next == '/' || next == '\\') return true;
    }
    return false;
}

static bool module_path_matches(Str path, const char *name) {
    const char *p = path.data;
    size_t len = path.len;
    size_t start = 0;
    for (size_t i = 0; i < len; i++) {
        if (p[i] == '/' || p[i] == '\\') start = i + 1;
    }
    size_t n = len - start;
    size_t nlen = strlen(name);
    // Strip .yi extension
    if (n >= 3 && memcmp(p + start + n - 3, ".yi", 3) == 0) {
        n -= 3;
    }
    return n == nlen && memcmp(p + start, name, nlen) == 0;
}

bool program_uses_module(Program *prog, const char *name) {
    if (!prog || !name) return false;
    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        if (!m) continue;
        if (m->has_declared_name && import_name_matches(m->declared_name, name))
            return true;
        if (module_path_matches(m->path, name))
            return true;
        for (size_t j = 0; j < m->imports_len; j++) {
            if (m->imports[j] && import_name_matches(m->imports[j]->name, name))
                return true;
        }
    }
    return false;
}

// --- program_find_appid_name ---

static bool expr_string_literal_as_filename_ext(Expr *e, char *out, size_t out_cap);
static void find_appid_stmt(Stmt *s, char *out, size_t out_cap, bool *found);

static void find_appid_expr(Expr *e, char *out, size_t out_cap, bool *found) {
    if (!e) return;
    switch (e->kind) {
        case EXPR_CALL: {
            Expr *fn = e->as.call.fn;
            bool matches = false;
            int arg_idx = 0;
            if (fn && fn->kind == EXPR_MEMBER && str_eq_c(fn->as.member.name, "set_appid")) {
                matches = true;
                arg_idx = 0;
            } else if (fn && fn->kind == EXPR_IDENT) {
                // Match __<module>_app_set_appid patterns
                const char *id = fn->as.ident.name.data;
                size_t id_len = fn->as.ident.name.len;
                if (id_len > 14 && memcmp(id + id_len - 14, "_app_set_appid", 14) == 0) {
                    matches = true;
                    arg_idx = 1;
                }
            }
            if (matches && e->as.call.args_len > (size_t)arg_idx) {
                char candidate[256];
                if (expr_string_literal_as_filename_ext(e->as.call.args[arg_idx], candidate, sizeof(candidate))) {
                    snprintf(out, out_cap, "%s", candidate);
                    *found = true;
                }
            }
            find_appid_expr(fn, out, out_cap, found);
            for (size_t i = 0; i < e->as.call.args_len; i++)
                find_appid_expr(e->as.call.args[i], out, out_cap, found);
            break;
        }
        case EXPR_UNARY:
            find_appid_expr(e->as.unary.x, out, out_cap, found);
            break;
        case EXPR_BINARY:
            find_appid_expr(e->as.binary.a, out, out_cap, found);
            find_appid_expr(e->as.binary.b, out, out_cap, found);
            break;
        case EXPR_ASSIGN:
            find_appid_expr(e->as.assign.target, out, out_cap, found);
            find_appid_expr(e->as.assign.value, out, out_cap, found);
            break;
        case EXPR_INDEX:
            find_appid_expr(e->as.index.a, out, out_cap, found);
            find_appid_expr(e->as.index.i, out, out_cap, found);
            break;
        case EXPR_MEMBER:
            find_appid_expr(e->as.member.a, out, out_cap, found);
            break;
        case EXPR_PAREN:
            find_appid_expr(e->as.paren.x, out, out_cap, found);
            break;
        case EXPR_MATCH:
            find_appid_expr(e->as.match_expr.scrut, out, out_cap, found);
            for (size_t i = 0; i < e->as.match_expr.arms_len; i++) {
                MatchArm *arm = e->as.match_expr.arms[i];
                if (arm) find_appid_expr(arm->expr, out, out_cap, found);
            }
            break;
        case EXPR_LAMBDA:
            find_appid_expr(e->as.lambda.body, out, out_cap, found);
            break;
        case EXPR_BLOCK:
            find_appid_stmt(e->as.block_expr.block, out, out_cap, found);
            break;
        case EXPR_NEW:
            for (size_t i = 0; i < e->as.new_expr.args_len; i++)
                find_appid_expr(e->as.new_expr.args[i], out, out_cap, found);
            break;
        case EXPR_IF:
            for (size_t i = 0; i < e->as.if_expr.arms_len; i++) {
                ExprIfArm *arm = e->as.if_expr.arms[i];
                if (!arm) continue;
                find_appid_expr(arm->cond, out, out_cap, found);
                find_appid_expr(arm->value, out, out_cap, found);
            }
            break;
        case EXPR_TERNARY:
            find_appid_expr(e->as.ternary.cond, out, out_cap, found);
            find_appid_expr(e->as.ternary.then_expr, out, out_cap, found);
            find_appid_expr(e->as.ternary.else_expr, out, out_cap, found);
            break;
        case EXPR_MOVE:
            find_appid_expr(e->as.move.x, out, out_cap, found);
            break;
        case EXPR_TUPLE:
            for (size_t i = 0; i < e->as.tuple_lit.items_len; i++)
                find_appid_expr(e->as.tuple_lit.items[i], out, out_cap, found);
            break;
        case EXPR_ARRAY:
            for (size_t i = 0; i < e->as.array_lit.items_len; i++)
                find_appid_expr(e->as.array_lit.items[i], out, out_cap, found);
            break;
        case EXPR_DICT:
            for (size_t i = 0; i < e->as.dict_lit.pairs_len; i++) {
                find_appid_expr(e->as.dict_lit.keys[i], out, out_cap, found);
                find_appid_expr(e->as.dict_lit.vals[i], out, out_cap, found);
            }
            break;
        default:
            break;
    }
}

static void find_appid_stmt(Stmt *s, char *out, size_t out_cap, bool *found) {
    if (!s) return;
    switch (s->kind) {
        case STMT_LET:
            find_appid_expr(s->as.let_s.expr, out, out_cap, found);
            break;
        case STMT_CONST:
            find_appid_expr(s->as.const_s.expr, out, out_cap, found);
            break;
        case STMT_IF:
            for (size_t i = 0; i < s->as.if_s.arms_len; i++) {
                IfArm *arm = s->as.if_s.arms[i];
                if (!arm) continue;
                find_appid_expr(arm->cond, out, out_cap, found);
                find_appid_stmt(arm->body, out, out_cap, found);
            }
            break;
        case STMT_FOR:
            find_appid_stmt(s->as.for_s.init, out, out_cap, found);
            find_appid_expr(s->as.for_s.cond, out, out_cap, found);
            find_appid_expr(s->as.for_s.step, out, out_cap, found);
            find_appid_stmt(s->as.for_s.body, out, out_cap, found);
            break;
        case STMT_FOREACH:
            find_appid_expr(s->as.foreach_s.expr, out, out_cap, found);
            find_appid_stmt(s->as.foreach_s.body, out, out_cap, found);
            break;
        case STMT_RETURN:
            find_appid_expr(s->as.ret_s.expr, out, out_cap, found);
            break;
        case STMT_BREAK:
        case STMT_CONTINUE:
            break;
        case STMT_EXPR:
            find_appid_expr(s->as.expr_s.expr, out, out_cap, found);
            break;
        case STMT_BLOCK:
            for (size_t i = 0; i < s->as.block_s.stmts_len; i++)
                find_appid_stmt(s->as.block_s.stmts[i], out, out_cap, found);
            break;
    }
}

bool program_find_appid_name(Program *prog, char *out, size_t out_cap) {
    if (!prog || !out || out_cap == 0) return false;
    bool found = false;
    for (size_t i = 0; i < prog->mods_len; i++) {
        Module *m = prog->mods[i];
        if (!m) continue;
        for (size_t j = 0; j < m->decls_len; j++) {
            Decl *d = m->decls[j];
            if (!d) continue;
            switch (d->kind) {
                case DECL_ENTRY:
                    find_appid_stmt(d->as.entry.body, out, out_cap, &found);
                    break;
                case DECL_FUN:
                    find_appid_stmt(d->as.fun.body, out, out_cap, &found);
                    break;
                case DECL_MACRO:
                    find_appid_stmt(d->as.macro.body, out, out_cap, &found);
                    break;
                case DECL_CONST:
                    find_appid_expr(d->as.const_decl.expr, out, out_cap, &found);
                    break;
                case DECL_DEF:
                    find_appid_expr(d->as.def_decl.expr, out, out_cap, &found);
                    break;
                case DECL_CLASS:
                    for (size_t k = 0; k < d->as.class_decl.methods_len; k++) {
                        FunDecl *meth = d->as.class_decl.methods[k];
                        if (meth) find_appid_stmt(meth->body, out, out_cap, &found);
                    }
                    break;
            }
        }
    }
    return found;
}

// --- module_default_cflags ---

const char *module_default_cflags(const char *name) {
    static char cflag_buf[512];
    char header_path[512];

    // Check install layout: include/{name}/{name}.h
    {
        char rel[256];
        snprintf(rel, sizeof(rel), "%s/%s.h", name, name);
        const char *inc_dirs[] = {
            // cwd-relative development locations
            "%s/include/%s",
            "../%s/include/%s",
            "../../%s/include/%s",
            "%s/src/%s",
            "%s/src/src/%s",
            "../%s/src/%s",
            "../%s/src/src/%s",
            "../../%s/src/%s",
            "../../%s/src/src/%s",
        };
        for (size_t i = 0; i < sizeof(inc_dirs) / sizeof(inc_dirs[0]); i++) {
            snprintf(header_path, sizeof(header_path), inc_dirs[i], name, rel);
            if (path_is_file(header_path)) {
                char *dir = path_dirname(header_path);
                if (dir) {
                    snprintf(cflag_buf, sizeof(cflag_buf), "-I%s", dir);
                    free(dir);
                    return cflag_buf;
                }
            }
        }
    }

    // System install locations
    {
        const char *sys_inc[] = {
#if defined(__APPLE__)
            "/opt/homebrew/include",
            "/usr/local/include",
#endif
            "/usr/include",
        };
        for (size_t i = 0; i < sizeof(sys_inc) / sizeof(sys_inc[0]); i++) {
            snprintf(header_path, sizeof(header_path), "%s/%s/%s.h", sys_inc[i], name, name);
            if (path_is_file(header_path)) {
                snprintf(cflag_buf, sizeof(cflag_buf), "-I%s/%s", sys_inc[i], name);
                return cflag_buf;
            }
        }
    }

    // Relative to executable
    {
        char *exe_dir = yis_exe_dir();
        if (exe_dir) {
            const char *exe_rel[] = {
                "../../%s/src/%s.h",
                "../../%s/src/src/%s.h",
                "../../%s/include/%s/%s.h",
            };
            for (size_t i = 0; i < sizeof(exe_rel) / sizeof(exe_rel[0]); i++) {
                char rel[512];
                if (i < 2) {
                    snprintf(rel, sizeof(rel), exe_rel[i], name, name);
                } else {
                    snprintf(rel, sizeof(rel), exe_rel[i], name, name, name);
                }
                char *p = path_join(exe_dir, rel);
                if (p && path_is_file(p)) {
                    char *dir = path_dirname(p);
                    if (dir) {
                        snprintf(cflag_buf, sizeof(cflag_buf), "-I%s", dir);
                        free(dir);
                        free(p);
                        free(exe_dir);
                        return cflag_buf;
                    }
                    free(dir);
                }
                free(p);
            }
            free(exe_dir);
        }
    }

    return "";
}

// --- module_default_ldflags ---

const char *module_default_ldflags(const char *name) {
    static char buf[1024];

#if defined(__APPLE__)
    const char *ext = "dylib";
#elif defined(_WIN32)
    const char *ext = "dll";
#else
    const char *ext = "so";
#endif

    char libname[256];
#if defined(_WIN32)
    snprintf(libname, sizeof(libname), "%s.%s", name, ext);
#else
    snprintf(libname, sizeof(libname), "lib%s.%s", name, ext);
#endif

    // 1) System install paths
    {
        const char *sys_dirs[] = {
#if defined(__APPLE__)
            "/opt/homebrew/lib",
            "/usr/local/lib",
#endif
            "/usr/lib",
        };
        for (size_t i = 0; i < sizeof(sys_dirs) / sizeof(sys_dirs[0]); i++) {
            char path[512];
            snprintf(path, sizeof(path), "%s/%s", sys_dirs[i], libname);
            if (path_is_file(path)) {
#if defined(__APPLE__) || defined(__linux__)
                snprintf(buf, sizeof(buf), "-L%s -l%s -Wl,-rpath,%s", sys_dirs[i], name, sys_dirs[i]);
#else
                snprintf(buf, sizeof(buf), "-L%s -l%s", sys_dirs[i], name);
#endif
                return buf;
            }
        }
    }

    // 2) Development build directories
    {
        const char *dev_dirs[] = {
            "%s/_build",
            "%s/build",
            "../%s/_build",
            "../%s/build",
            "../../%s/_build",
            "../../%s/build",
        };
        for (size_t i = 0; i < sizeof(dev_dirs) / sizeof(dev_dirs[0]); i++) {
            char dir[512], path[512];
            snprintf(dir, sizeof(dir), dev_dirs[i], name);
            snprintf(path, sizeof(path), "%s/%s", dir, libname);
            if (path_is_file(path)) {
#if defined(__APPLE__) || defined(__linux__)
                char *abs_dir = path_abs(dir);
                if (abs_dir) {
                    snprintf(buf, sizeof(buf), "-L%s -l%s -Wl,-rpath,%s", abs_dir, name, abs_dir);
                    free(abs_dir);
                } else {
                    snprintf(buf, sizeof(buf), "-L%s -l%s -Wl,-rpath,%s", dir, name, dir);
                }
#else
                snprintf(buf, sizeof(buf), "-L%s -l%s", dir, name);
#endif
                return buf;
            }
        }
    }

    // 3) Relative to executable
    {
        char *exe_dir = yis_exe_dir();
        if (exe_dir) {
            const char *rel_dirs[] = {
                "../../%s/_build",
                "../../%s/build",
            };
            for (size_t i = 0; i < sizeof(rel_dirs) / sizeof(rel_dirs[0]); i++) {
                char rel[512];
                snprintf(rel, sizeof(rel), rel_dirs[i], name);
                char *dir = path_join(exe_dir, rel);
                if (dir) {
                    char path[512];
                    snprintf(path, sizeof(path), "%s/%s", dir, libname);
                    if (path_is_file(path)) {
#if defined(__APPLE__) || defined(__linux__)
                        char *abs_dir = path_abs(dir);
                        const char *rpath_dir = abs_dir ? abs_dir : dir;
                        snprintf(buf, sizeof(buf), "-L%s -l%s -Wl,-rpath,%s", rpath_dir, name, rpath_dir);
                        free(abs_dir);
#else
                        snprintf(buf, sizeof(buf), "-L%s -l%s", dir, name);
#endif
                        free(dir);
                        free(exe_dir);
                        return buf;
                    }
                    free(dir);
                }
            }
            free(exe_dir);
        }
    }

    // Fallback
    snprintf(buf, sizeof(buf), "-l%s", name);
    return buf;
}

// --- expr_string_literal_as_filename_ext ---
// (Copied from main.c's local helper so external_module.c is self-contained for appid)

static bool expr_string_literal_as_filename_ext(Expr *e, char *out, size_t out_cap) {
    if (!e || e->kind != EXPR_STR || !out || out_cap == 0) return false;
    StrParts *parts = e->as.str_lit.parts;
    if (!parts || parts->len == 0) return false;

    size_t n = 0;
    for (size_t i = 0; i < parts->len; i++) {
        StrPart *part = &parts->parts[i];
        if (part->kind != STR_PART_TEXT) return false;
        for (size_t j = 0; j < part->as.text.len; j++) {
            unsigned char c = (unsigned char)part->as.text.data[j];
            char mapped;
            if (isalnum(c) || c == '.' || c == '_' || c == '-') {
                mapped = (char)c;
            } else if (c == ' ' || c == '\t') {
                mapped = '-';
            } else {
                mapped = '_';
            }
            if (n + 1 < out_cap) {
                out[n++] = mapped;
            }
        }
    }
    while (n > 0 && (out[n - 1] == '.' || out[n - 1] == ' ')) {
        n--;
    }
    if (n == 0) {
        out[0] = '\0';
        return false;
    }
    out[n] = '\0';
    return true;
}
