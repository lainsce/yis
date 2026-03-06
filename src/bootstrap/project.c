#include "project.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "external_module.h"
#include "file.h"
#include "lexer.h"
#include "parser.h"
#include "platform.h"
#include "str.h"

typedef struct {
    char *path;
    Module *mod;
} ModEntry;

typedef struct {
    ModEntry *data;
    size_t len;
    size_t cap;
} ModVec;

static bool modvec_push(ModVec *v, ModEntry entry) {
    if (v->len + 1 > v->cap) {
        size_t next = v->cap ? v->cap * 2 : 8;
        while (next < v->len + 1) {
            next *= 2;
        }
        ModEntry *data = (ModEntry *)realloc(v->data, next * sizeof(ModEntry));
        if (!data) {
            return false;
        }
        v->data = data;
        v->cap = next;
    }
    v->data[v->len++] = entry;
    return true;
}

static Module *modvec_find(ModVec *v, const char *path) {
    for (size_t i = 0; i < v->len; i++) {
        if (strcmp(v->data[i].path, path) == 0) {
            return v->data[i].mod;
        }
    }
    return NULL;
}

static void set_err(Diag *err, const char *path, const char *msg) {
    if (!err) return;
    err->path = path;
    err->line = 0;
    err->col = 0;
    err->message = msg;
}

static uint64_t hash_update(uint64_t h, const void *data, size_t len) {
    const unsigned char *p = (const unsigned char *)data;
    for (size_t i = 0; i < len; i++) {
        h ^= (uint64_t)p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static uint64_t hash_cstr(uint64_t h, const char *s) {
    if (!s) {
        return hash_update(h, "", 0);
    }
    return hash_update(h, s, strlen(s));
}

static char *str_to_c(Str s) {
    char *buf = (char *)malloc(s.len + 1);
    if (!buf) return NULL;
    memcpy(buf, s.data, s.len);
    buf[s.len] = '\0';
    return buf;
}

static bool str_ends_with(Str s, const char *suffix) {
    size_t slen = s.len;
    size_t suf = strlen(suffix);
    if (slen < suf) return false;
    return memcmp(s.data + (slen - suf), suffix, suf) == 0;
}

static Str arena_copy_str(Arena *arena, const char *s) {
    size_t len = strlen(s);
    char *buf = (char *)arena_alloc(arena, len + 1);
    if (!buf) {
        Str out = {"", 0};
        return out;
    }
    memcpy(buf, s, len + 1);
    Str out;
    out.data = buf;
    out.len = len;
    return out;
}

static const char *stdlib_dir_default(void) {
    const char *env = getenv("YIS_STDLIB");
    if (env && env[0]) {
        return env;
    }

    // Check development locations across repo-root and subdir invocations.
    const struct {
        const char *dir;
        const char *marker;
    } dev_locations[] = {
        {"src/stdlib", "src/stdlib/stdr.yi"},
        {"yis/src/stdlib", "yis/src/stdlib/stdr.yi"},
        {"../src/stdlib", "../src/stdlib/stdr.yi"},
        {"../yis/src/stdlib", "../yis/src/stdlib/stdr.yi"},
        {"../../yis/src/stdlib", "../../yis/src/stdlib/stdr.yi"},
    };
    for (size_t i = 0; i < sizeof(dev_locations) / sizeof(dev_locations[0]); i++) {
        if (path_is_file(dev_locations[i].marker)) {
            return dev_locations[i].dir;
        }
    }

    // Try relative to the executable (binary at <project>/yis/build/yis)
    {
        char *exe_dir = yis_exe_dir();
        if (exe_dir) {
            static char stdlib_buf[512];
            const char *exe_rel[] = {
                "../src/stdlib",
                "../../yis/src/stdlib",
            };
            for (size_t i = 0; i < sizeof(exe_rel) / sizeof(exe_rel[0]); i++) {
                char *dir = path_join(exe_dir, exe_rel[i]);
                if (dir) {
                    char *marker = path_join(dir, "stdr.yi");
                    if (marker && path_is_file(marker)) {
                        snprintf(stdlib_buf, sizeof(stdlib_buf), "%s", dir);
                        free(marker);
                        free(dir);
                        free(exe_dir);
                        return stdlib_buf;
                    }
                    free(marker);
                    free(dir);
                }
            }
            free(exe_dir);
        }
    }

    // Check installed locations
    if (path_is_file("/usr/local/share/yis/stdlib/stdr.yi")) {
        return "/usr/local/share/yis/stdlib";
    }
    if (path_is_file("/opt/homebrew/share/yis/stdlib/stdr.yi")) {
        return "/opt/homebrew/share/yis/stdlib";
    }
    if (path_is_file("/usr/share/yis/stdlib/stdr.yi")) {
        return "/usr/share/yis/stdlib";
    }
    return "yis/src/stdlib";
}

static Module *load_file(const char *path,
                         const char *root_dir,
                         const char *stdlib_dir,
                         Arena *arena,
                         ModVec *visited,
                         uint64_t *hash,
                         Diag *err) {
    char *abs_path = path_abs(path);
    if (!abs_path) {
        set_err(err, path, "failed to resolve path");
        return NULL;
    }
    Module *existing = modvec_find(visited, abs_path);
    if (existing) {
        free(abs_path);
        return existing;
    }

    size_t len = 0;
    char *src = read_file_with_includes(abs_path, "-- @include", arena, &len, err);
    if (!src) {
        // err->path may point into abs_path which we are about to free.
        // Copy it into the arena so the diagnostic survives.
        if (err && err->path) {
            Str saved = arena_copy_str(arena, err->path);
            if (saved.data) err->path = saved.data;
        }
        free(abs_path);
        return NULL;
    }
    if (hash) {
        uint64_t h = *hash;
        h = hash_cstr(h, abs_path);
        h = hash_update(h, "\0", 1);
        h = hash_update(h, src, len);
        h = hash_update(h, "\0", 1);
        *hash = h;
    }

    TokVec toks = {0};
    if (!lex_source(abs_path, src, len, arena, &toks, err)) {
        free(toks.data);
        free(abs_path);
        return NULL;
    }

    Module *mod = parse_cask(toks.data, toks.len, abs_path, arena, err);
    free(toks.data);
    if (!mod) {
        free(abs_path);
        return NULL;
    }

    mod->path = arena_copy_str(arena, abs_path);

    ModEntry entry;
    entry.path = abs_path;
    entry.mod = mod;
    if (!modvec_push(visited, entry)) {
        free(abs_path);
        set_err(err, path, "out of memory");
        return NULL;
    }

    bool is_stdlib = path_has_prefix(abs_path, stdlib_dir);
    if (!is_stdlib) {
        bool has_stdr = false;
        for (size_t i = 0; i < mod->imports_len; i++) {
            if (str_eq_c(mod->imports[i]->name, "stdr")) {
                has_stdr = true;
                break;
            }
        }
        if (!has_stdr) {
            set_err(err, abs_path, "missing required `bring stdr;`");
            return NULL;
        }
    }

    for (size_t i = 0; i < mod->imports_len; i++) {
        Import *imp = mod->imports[i];
        if (str_eq_c(imp->name, "stdr")) {
            char *p = path_join(stdlib_dir, "stdr.yi");
            if (!p || !path_is_file(p)) {
                set_err(err, abs_path, "stdr.yi not found in stdlib");
                free(p);
                return NULL;
            }
            if (!load_file(p, root_dir, stdlib_dir, arena, visited, hash, err)) {
                free(p);
                return NULL;
            }
            free(p);
            continue;
        }
        if (str_eq_c(imp->name, "math")) {
            char *p = path_join(stdlib_dir, "math.yi");
            if (!p || !path_is_file(p)) {
                set_err(err, abs_path, "math.yi not found in stdlib");
                free(p);
                return NULL;
            }
            if (!load_file(p, root_dir, stdlib_dir, arena, visited, hash, err)) {
                free(p);
                return NULL;
            }
            free(p);
            continue;
        }
        if (str_eq_c(imp->name, "net")) {
            char *p = path_join(stdlib_dir, "net.yi");
            if (!p || !path_is_file(p)) {
                set_err(err, abs_path, "net.yi not found in stdlib");
                free(p);
                return NULL;
            }
            if (!load_file(p, root_dir, stdlib_dir, arena, visited, hash, err)) {
                free(p);
                return NULL;
            }
            free(p);
            continue;
        }
        if (str_eq_c(imp->name, "json")) {
            char *p = path_join(stdlib_dir, "json.yi");
            if (!p || !path_is_file(p)) {
                set_err(err, abs_path, "json.yi not found in stdlib");
                free(p);
                return NULL;
            }
            if (!load_file(p, root_dir, stdlib_dir, arena, visited, hash, err)) {
                free(p);
                return NULL;
            }
            free(p);
            continue;
        }
        // Try generic external module resolution
        {
            char *imp_name_c = str_to_c(imp->name);
            if (!imp_name_c) {
                set_err(err, abs_path, "out of memory");
                return NULL;
            }
            char *ext_path = resolve_external_module(imp_name_c, stdlib_dir);
            if (ext_path) {
                if (!load_file(ext_path, root_dir, stdlib_dir, arena, visited, hash, err)) {
                    free(ext_path);
                    free(imp_name_c);
                    return NULL;
                }
                free(ext_path);
                free(imp_name_c);
                continue;
            }
            free(imp_name_c);
        }
        // Fall back to user cask (relative .yi file)
        {
            char *name = str_to_c(imp->name);
            if (!name) {
                set_err(err, abs_path, "out of memory");
                return NULL;
            }
            if (str_ends_with(imp->name, ".e")) {
                free(name);
                set_err(err, abs_path, "'.e' files are no longer supported; use .yi");
                return NULL;
            }
            if (!str_ends_with(imp->name, ".yi")) {
                size_t nlen = strlen(name);
                char *with_ext = (char *)malloc(nlen + 4);
                if (!with_ext) {
                    free(name);
                    set_err(err, abs_path, "out of memory");
                    return NULL;
                }
                memcpy(with_ext, name, nlen);
                memcpy(with_ext + nlen, ".yi", 3);
                free(name);
                name = with_ext;
            }
            char *child = path_join(root_dir, name);
            free(name);
            if (!child || !path_is_file(child)) {
                set_err(err, abs_path, "bring expects a stdlib module, external module, or valid user cask (file)");
                free(child);
                return NULL;
            }
            if (!load_file(child, root_dir, stdlib_dir, arena, visited, hash, err)) {
                free(child);
                return NULL;
            }
            free(child);
        }
    }

    return mod;
}

bool load_project(const char *entry_path, Arena *arena, Program **out_prog, uint64_t *out_hash, Diag *err) {
    if (!entry_path || !arena || !out_prog) {
        set_err(err, entry_path ? entry_path : "unknown", "internal error: invalid arguments to load_project");
        return false;
    }
    uint64_t hash = 1469598103934665603ULL;
    uint64_t *hash_ptr = out_hash ? &hash : NULL;
    char *entry_abs = path_abs(entry_path);
    if (!entry_abs) {
        set_err(err, entry_path, "failed to resolve entry path");
        return false;
    }
    char *root_dir = path_dirname(entry_abs);
    const char *stdlib_dir = stdlib_dir_default();
    char *stdlib_abs = path_abs(stdlib_dir);
    if (stdlib_abs) {
        stdlib_dir = stdlib_abs;
    }
    ModVec visited = {0};

    Module *init_mod = load_file(entry_abs, root_dir, stdlib_dir, arena, &visited, hash_ptr, err);
    if (!init_mod) {
        free(entry_abs);
        free(root_dir);
        free(stdlib_abs);
        for (size_t i = 0; i < visited.len; i++) {
            free(visited.data[i].path);
        }
        free(visited.data);
        return false;
    }

    size_t entry_count = 0;
    for (size_t i = 0; i < init_mod->decls_len; i++) {
        if (init_mod->decls[i]->kind == DECL_ENTRY) {
            entry_count++;
        }
    }
    if (entry_count != 1) {
        // Use cask path which is already in arena
        const char *err_path = init_mod->path.data ? init_mod->path.data : "entry file";
        set_err(err, err_path, "init.yi must contain exactly one entry() decl");
        free(entry_abs);
        free(root_dir);
        free(stdlib_abs);
        for (size_t i = 0; i < visited.len; i++) {
            free(visited.data[i].path);
        }
        free(visited.data);
        return false;
    }

    for (size_t i = 0; i < visited.len; i++) {
        Module *mod = visited.data[i].mod;
        if (mod == init_mod) {
            continue;
        }
        for (size_t j = 0; j < mod->decls_len; j++) {
            if (mod->decls[j]->kind == DECL_ENTRY) {
                // Use cask path which is already in arena
                const char *err_path = mod->path.data ? mod->path.data : "cask file";
                set_err(err, err_path, "entry() is only allowed in init.yi");
                free(entry_abs);
                free(root_dir);
                free(stdlib_abs);
                for (size_t k = 0; k < visited.len; k++) {
                    free(visited.data[k].path);
                }
                free(visited.data);
                return false;
            }
        }
    }

    Program *prog = (Program *)ast_alloc(arena, sizeof(Program));
    if (!prog) {
        set_err(err, entry_abs, "out of memory");
        free(entry_abs);
        free(root_dir);
        free(stdlib_abs);
        for (size_t i = 0; i < visited.len; i++) {
            free(visited.data[i].path);
        }
        free(visited.data);
        return false;
    }
    prog->mods_len = visited.len;
    prog->mods = (Module **)arena_alloc(arena, sizeof(Module *) * visited.len);
    if (!prog->mods) {
        set_err(err, entry_abs, "out of memory");
        free(entry_abs);
        free(root_dir);
        free(stdlib_abs);
        for (size_t i = 0; i < visited.len; i++) {
            free(visited.data[i].path);
        }
        free(visited.data);
        return false;
    }
    for (size_t i = 0; i < visited.len; i++) {
        prog->mods[i] = visited.data[i].mod;
    }

    for (size_t i = 0; i < visited.len; i++) {
        free(visited.data[i].path);
    }
    free(visited.data);
    free(entry_abs);
    free(root_dir);
    free(stdlib_abs);
    *out_prog = prog;
    if (out_hash) {
        *out_hash = hash;
    }
    return true;
}
