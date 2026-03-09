#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <ctype.h>

#if defined(_WIN32)
#include <direct.h>
#define yis_getcwd _getcwd
#define yis_mkdir(path) _mkdir(path)
#else
#include <unistd.h>
#include <sys/stat.h>
#define yis_getcwd getcwd
#define yis_mkdir(path) mkdir((path), 0755)
#endif

#include "arena.h"
#include "codegen.h"
#include "diag.h"
#include "external_module.h"
#include "file.h"
#include "platform.h"
#include "project.h"
#include "sum_validate.h"
#include "str.h"
#include "typecheck.h"

#define YIS_CACHE_VERSION __DATE__ " " __TIME__

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

static char *dup_cstr(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    memcpy(out, s, len + 1);
    return out;
}

static bool ensure_dir(const char *path) {
    if (!path || !path[0]) {
        return false;
    }
    if (yis_mkdir(path) == 0) {
        return true;
    }
    return errno == EEXIST;
}

static char *cache_base_dir(void) {
    const char *env = getenv("YIS_CACHE_DIR");
    if (env && env[0]) {
        return dup_cstr(env);
    }
    char buf[4096];
    if (!yis_getcwd(buf, sizeof(buf))) {
        return NULL;
    }
    return path_join(buf, ".yi-cache");
}

/* Run binary with optional arguments (e.g. for self-hosted compiler). */
static int run_binary_with_args(const char *path, int argc, char **argv) {
    if (!path) return 1;
    char cmd[8192];
    int n = snprintf(cmd, sizeof(cmd), "\"%s\"", path);
    if (n < 0 || (size_t)n >= sizeof(cmd)) return 1;
    for (int i = 0; i < argc && argv && argv[i]; i++) {
        size_t cur = (size_t)n;
        n += snprintf(cmd + cur, sizeof(cmd) - cur, " \"%s\"", argv[i]);
        if (n < 0 || (size_t)n >= sizeof(cmd)) return 1;
    }
    return system(cmd);
}

#if defined(__APPLE__) || defined(__linux__)
static bool run_module_packager_hook(const char *packager_path, const char *entry_path,
                                     const char *out_path, const char *app_name) {
    if (!packager_path || !packager_path[0]) return true;
    if (!entry_path || !out_path || !app_name) return false;
    char cmd[16384];
    int n = snprintf(cmd, sizeof(cmd),
                     "sh \"%s\" \"%s\" \"%s\" \"%s\"",
                     packager_path, entry_path, out_path, app_name);
    if (n < 0 || (size_t)n >= sizeof(cmd)) return false;
    return system(cmd) == 0;
}
#endif

#if defined(__APPLE__)
static bool write_text_file(const char *path, const char *text) {
    if (!path || !text) return false;
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    size_t n = strlen(text);
    bool ok = fwrite(text, 1, n, f) == n;
    fclose(f);
    return ok;
}
static bool prepare_macos_app_bundle(const char *exe_path, const char *bundle_id, const char *bundle_name) {
    if (!exe_path || !bundle_id || !bundle_id[0] || !bundle_name || !bundle_name[0]) return false;

    const char *marker = "/Contents/MacOS/";
    const char *m = strstr(exe_path, marker);
    if (!m) return false;
    const char *exec_name = m + strlen(marker);
    if (!exec_name[0] || strchr(exec_name, '/')) return false;

    size_t bundle_root_len = (size_t)(m - exe_path);
    char *bundle_root = (char *)malloc(bundle_root_len + 1);
    if (!bundle_root) return false;
    memcpy(bundle_root, exe_path, bundle_root_len);
    bundle_root[bundle_root_len] = '\0';

    char *contents_dir = path_join(bundle_root, "Contents");
    char *macos_dir = contents_dir ? path_join(contents_dir, "MacOS") : NULL;
    char *resources_dir = contents_dir ? path_join(contents_dir, "Resources") : NULL;
    char *plist_path = contents_dir ? path_join(contents_dir, "Info.plist") : NULL;

    bool ok = contents_dir && macos_dir && resources_dir && plist_path;
    if (ok) {
        ok = ensure_dir(bundle_root) &&
             ensure_dir(contents_dir) &&
             ensure_dir(macos_dir) &&
             ensure_dir(resources_dir);
    }

    if (ok) {
        char plist[4096];
        int n = snprintf(
            plist, sizeof(plist),
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
            "<plist version=\"1.0\">\n"
            "<dict>\n"
            "  <key>CFBundleDevelopmentRegion</key><string>en</string>\n"
            "  <key>CFBundleExecutable</key><string>%s</string>\n"
            "  <key>CFBundleIdentifier</key><string>%s</string>\n"
            "  <key>CFBundleInfoDictionaryVersion</key><string>6.0</string>\n"
            "  <key>CFBundleName</key><string>%s</string>\n"
            "  <key>CFBundleDisplayName</key><string>%s</string>\n"
            "  <key>CFBundlePackageType</key><string>APPL</string>\n"
            "  <key>CFBundleShortVersionString</key><string>1.0</string>\n"
            "  <key>CFBundleVersion</key><string>1</string>\n"
            "  <key>NSHighResolutionCapable</key><true/>\n"
            "</dict>\n"
            "</plist>\n",
            exec_name, bundle_id, bundle_name, bundle_name
        );
        ok = n > 0 && (size_t)n < sizeof(plist) && write_text_file(plist_path, plist);
    }

    free(bundle_root);
    free(contents_dir);
    free(macos_dir);
    free(resources_dir);
    free(plist_path);
    return ok;
}
#endif

static bool sanitize_filename_component(const char *src, size_t src_len, char *out, size_t out_cap) {
    if (!src || !out || out_cap == 0) return false;
    size_t n = 0;
    for (size_t i = 0; i < src_len; i++) {
        unsigned char c = (unsigned char)src[i];
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


#define YIS_VERSION "0.1.0"

static bool verbose_mode = false;

static void print_usage(FILE *out) {
    fprintf(out, "Usage: yis [OPTIONS] <source.yi>\n");
    fprintf(out, "       yis run [OPTIONS] <source.yi>\n");
    fprintf(out, "       yis lint [--mode warn|strict] <source.yi>\n");
    fprintf(out, "       yis sum validate [--mode off|warn|strict] <path>\n");
    fprintf(out, "\n");
    fprintf(out, "Options:\n");
    fprintf(out, "  -h, --help       Show this help message\n");
    fprintf(out, "  -v, --version    Show version information\n");
    fprintf(out, "  --verbose        Enable verbose error output with more context\n");
    fprintf(out, "\n");
    fprintf(out, "Examples:\n");
    fprintf(out, "  yis init.yi              # Compile and check init.yi\n");
    fprintf(out, "  yis run init.yi          # Compile and run init.yi\n");
    fprintf(out, "  yis lint --mode strict init.yi\n");
    fprintf(out, "  yis sum validate theme.sum # Validate one SUM file\n");
    fprintf(out, "  yis --help                 # Show this help\n");
    fprintf(out, "\n");
    fprintf(out, "Environment Variables:\n");
    fprintf(out, "  YIS_STDLIB      Path to standard library (default: auto-detected, fallback: yis/src/stdlib)\n");
    fprintf(out, "  YIS_CACHE_DIR   Cache directory for compiled binaries\n");
    fprintf(out, "  YIS_NO_CACHE    Set to 1 to disable caching\n");
    fprintf(out, "  YIS_KEEP_C      Set to 1 to keep generated C files\n");
    fprintf(out, "  CC               C compiler to use (default: cc)\n");
    fprintf(out, "  YIS_CC_FLAGS    Additional C compiler flags\n");
    fprintf(out, "  NO_COLOR         Set to disable colored output\n");
    fprintf(out, "\n");
    fprintf(out, "External Module Environment Variables (replace <NAME> with module name, e.g. COGITO):\n");
    fprintf(out, "    YIS_<NAME>_PATH     Directory or file path for module .yi resolution\n");
    fprintf(out, "    YIS_<NAME>_BINDINGS Path to module bindings .inc file\n");
    fprintf(out, "    YIS_<NAME>_CFLAGS   Additional C flags for module compilation\n");
    fprintf(out, "    YIS_<NAME>_FLAGS    Additional linker flags for module\n");
    fprintf(out, "    YIS_RAYLIB_CFLAGS   C flags for raylib (auto-detected on macOS/Linux)\n");
    fprintf(out, "    YIS_RAYLIB_FLAGS    Linker flags for raylib (auto-detected on macOS/Linux)\n");
}

static void print_version(void) {
    printf("yis version %s\n", YIS_VERSION);
    printf("Copyright (c) 2026 Yis Contributors\n");
}

static int is_flag(const char *arg, const char *flag) {
    return arg && flag && strcmp(arg, flag) == 0;
}

static const char *cc_path(void) {
    const char *cc = getenv("CC");
    return cc && cc[0] ? cc : "cc";
}

static const char *cc_flags(void) {
    const char *flags = getenv("YIS_CC_FLAGS");
    return flags && flags[0] ? flags : "-O3 -std=c11 -pipe";
}

static const char *join_flags(char *buf, size_t cap, const char *a, const char *b) {
    if (!(a && a[0])) {
        snprintf(buf, cap, "%s", b && b[0] ? b : "");
        return buf;
    }
    if (!(b && b[0])) {
        snprintf(buf, cap, "%s", a);
        return buf;
    }
    snprintf(buf, cap, "%s %s", a, b);
    return buf;
}

// Get an env var for an external module: YIS_{NAME}_{suffix}
static const char *get_module_env_val(const char *name, const char *suffix) {
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
    const char *val = getenv(env_buf);
    return (val && val[0]) ? val : NULL;
}

#if defined(__APPLE__) || defined(__linux__)
static const char *raylib_default_cflags(void) {
#if defined(__APPLE__)
    if (path_is_file("/opt/homebrew/include/raylib.h")) {
        return "-I/opt/homebrew/include";
    }
    if (path_is_file("/usr/local/include/raylib.h")) {
        return "-I/usr/local/include";
    }
#elif defined(__linux__)
    if (path_is_file("/usr/include/raylib.h")) {
        return "-I/usr/include";
    }
    if (path_is_file("/usr/local/include/raylib.h")) {
        return "-I/usr/local/include";
    }
#endif
    return "";
}

static const char *raylib_default_ldflags(void) {
#if defined(_WIN32)
    return "-lraylib -lopengl32 -lgdi32 -lwinmm";
#elif defined(__APPLE__)
    static char buf[512];
    if (path_is_file("/opt/homebrew/lib/libraylib.dylib")) {
        snprintf(buf, sizeof(buf),
                 "-L/opt/homebrew/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo");
        return buf;
    }
    if (path_is_file("/usr/local/lib/libraylib.dylib")) {
        snprintf(buf, sizeof(buf),
                 "-L/usr/local/lib -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo");
        return buf;
    }
    return "-lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo";
#else
    if (path_is_file("/usr/local/lib/libraylib.so")) {
        return "-L/usr/local/lib -lraylib -lm -lpthread -ldl -lrt -lX11";
    }
    return "-lraylib -lm -lpthread -ldl -lrt -lX11";
#endif
}
#endif


int main(int argc, char **argv) {
    yis_set_stdout_buffered();

    if (argc < 2) {
        print_usage(stderr);
        return 2;
    }

    // Handle global flags
    if (is_flag(argv[1], "--help") || is_flag(argv[1], "-h")) {
        print_usage(stdout);
        return 0;
    }

    if (is_flag(argv[1], "--version") || is_flag(argv[1], "-v")) {
        print_version();
        return 0;
    }

    if (is_flag(argv[1], "--verbose")) {
        verbose_mode = true;
        // Shift arguments and continue
        if (argc < 3) {
            print_usage(stderr);
            return 2;
        }
        argv++;
        argc--;
    }

    if (is_flag(argv[1], "--emit-c")) {
        fprintf(stderr, "error: --emit-c is not supported in the C compiler\n");
        return 2;
    }

    if (is_flag(argv[1], "sum")) {
        return sum_validate_cli(argc, argv);
    }

    if (is_flag(argv[1], "lint")) {
        YisLintMode lint_mode = YIS_LINT_WARN;
        const char *entry = NULL;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--mode") == 0) {
                if (i + 1 >= argc) {
                    fprintf(stderr, "error: --mode requires one of warn|strict\n");
                    return 2;
                }
                i++;
                if (strcmp(argv[i], "warn") == 0) {
                    lint_mode = YIS_LINT_WARN;
                } else if (strcmp(argv[i], "strict") == 0) {
                    lint_mode = YIS_LINT_STRICT;
                } else {
                    fprintf(stderr, "error: unknown lint mode '%s'\n", argv[i]);
                    return 2;
                }
                continue;
            }
            if (argv[i][0] == '-') {
                fprintf(stderr, "error: unknown option %s\n", argv[i]);
                return 2;
            }
            if (entry) {
                fprintf(stderr, "error: multiple source paths provided\n");
                return 2;
            }
            entry = argv[i];
        }
        if (!entry) {
            fprintf(stderr, "error: lint needs a source path\n");
            return 2;
        }
        Arena arena;
        arena_init(&arena);
        Diag err = {0};
        Program *prog = NULL;
        if (!load_project(entry, &arena, &prog, NULL, &err)) {
            diag_print_enhanced(&err, verbose_mode);
            arena_free(&arena);
            return 1;
        }
        prog = lower_program(prog, &arena, &err);
        if (!prog || err.message) {
            diag_print_enhanced(&err, verbose_mode);
            arena_free(&arena);
            return 1;
        }
        if (!typecheck_program(prog, &arena, &err)) {
            diag_print_enhanced(&err, verbose_mode);
            arena_free(&arena);
            return 1;
        }
        int warnings = 0;
        int errors = 0;
        bool ok = lint_program(prog, &arena, lint_mode, &warnings, &errors);
        fprintf(stderr, "lint summary: %d warning(s), %d error(s)\n", warnings, errors);
        arena_free(&arena);
        return ok ? 0 : 1;
    }

    if (is_flag(argv[1], "run")) {
        const char *entry = NULL;
        int run_argc = 0;
        char **run_argv = NULL;
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--") == 0) {
                run_argc = argc - i - 1;
                run_argv = run_argc > 0 ? &argv[i + 1] : NULL;
                break;
            }
            if (argv[i][0] == '-') {
                fprintf(stderr, "error: unknown option %s\n", argv[i]);
                return 2;
            }
            if (entry) {
                fprintf(stderr, "error: multiple source paths provided\n");
                return 2;
            }
            entry = argv[i];
        }
        if (!entry) {
            fprintf(stderr, "error: run needs a source path\n");
            return 2;
        }
        Arena arena;
        arena_init(&arena);
        Diag err = {0};
        Program *prog = NULL;
        uint64_t proj_hash = 0;
    if (!load_project(entry, &arena, &prog, &proj_hash, &err)) {
        diag_print_enhanced(&err, verbose_mode);
        arena_free(&arena);
        return 1;
    }


        // Detect the first external module and resolve its build artifacts.
        const char *ext_module_name = NULL;
        char ext_module_name_buf[256];
        const char *ext_bindings_path_str = NULL;
        char *ext_bindings_alloc = NULL;
        char *ext_packager_alloc = NULL;
        ext_module_name_buf[0] = '\0';
        {
            if (program_find_first_external_module(prog, NULL, ext_module_name_buf,
                                                   sizeof(ext_module_name_buf))) {
                ext_module_name = ext_module_name_buf;
                char *ext_module_yi = resolve_external_module(ext_module_name, NULL);
                ext_bindings_alloc = find_module_bindings(ext_module_name, ext_module_yi);
                ext_bindings_path_str = ext_bindings_alloc;
                ext_packager_alloc = find_module_packager(ext_module_name, ext_module_yi);
                free(ext_module_yi);
            }
        }
        bool uses_ext_module = (ext_module_name != NULL);

        const char *extra_cflags = "";
        const char *extra_ldflags = "";
        char extra_cflags_buf[2048] = {0};
        char extra_ldflags_buf[2048] = {0};
        if (uses_ext_module) {
            // Module-specific cflags: YIS_<NAME>_CFLAGS or auto-detected
            const char *mod_cflags = get_module_env_val(ext_module_name, "CFLAGS");
            if (!mod_cflags) mod_cflags = module_default_cflags(ext_module_name);

            const char *ray_cflags = getenv("YIS_RAYLIB_CFLAGS");
            if (!(ray_cflags && ray_cflags[0])) {
#if defined(__APPLE__) || defined(__linux__)
                ray_cflags = raylib_default_cflags();
#endif
            }
            extra_cflags = join_flags(extra_cflags_buf, sizeof(extra_cflags_buf), ray_cflags, mod_cflags);

            // Module-specific ldflags: YIS_<NAME>_FLAGS or auto-detected
            const char *ray_flags = getenv("YIS_RAYLIB_FLAGS");
            const char *mod_flags = get_module_env_val(ext_module_name, "FLAGS");
            if (!(ray_flags && ray_flags[0])) {
                ray_flags = raylib_default_ldflags();
            }
            if (!mod_flags) {
                mod_flags = module_default_ldflags(ext_module_name);
            }
            extra_ldflags = join_flags(extra_ldflags_buf, sizeof(extra_ldflags_buf), mod_flags, ray_flags);
        }

        // Generate unique binary name from app id (if statically set) or entry file basename.
        char unique_bin_name[1024];
        const char *entry_basename = strrchr(entry, '/');
        if (!entry_basename) {
            entry_basename = strrchr(entry, '\\');
        }
        entry_basename = entry_basename ? entry_basename + 1 : entry;
        size_t entry_len = strlen(entry);
        bool is_self_host_entry =
            strcmp(entry, "src/init.yi") == 0 ||
            (entry_len >= strlen("/src/init.yi") &&
             strcmp(entry + (entry_len - strlen("/src/init.yi")), "/src/init.yi") == 0) ||
            (entry_len >= strlen("\\src\\init.yi") &&
             strcmp(entry + (entry_len - strlen("\\src\\init.yi")), "\\src\\init.yi") == 0);

        // Remove .yi extension if present
        char name_source[256];
        snprintf(name_source, sizeof(name_source), "%s", entry_basename);
        char *dot = strrchr(name_source, '.');
        if (dot && strcmp(dot, ".yi") == 0) {
            *dot = '\0';
        }
        char name_without_ext[256];
        if (!sanitize_filename_component(name_source, strlen(name_source), name_without_ext, sizeof(name_without_ext))) {
            snprintf(name_without_ext, sizeof(name_without_ext), "main");
        }
        if (uses_ext_module) {
            char appid_name[256];
            if (program_find_appid_name(prog, appid_name, sizeof(appid_name))) {
                snprintf(name_without_ext, sizeof(name_without_ext), "%s", appid_name);
            }
        }
#if defined(_WIN32)
        snprintf(unique_bin_name, sizeof(unique_bin_name), "%s.exe", name_without_ext);
#else
        snprintf(unique_bin_name, sizeof(unique_bin_name), "%s", name_without_ext);
#endif

    if (is_self_host_entry) {
#if defined(_WIN32)
        snprintf(unique_bin_name, sizeof(unique_bin_name), ".yi_run_out.exe");
#else
        snprintf(unique_bin_name, sizeof(unique_bin_name), ".yi_run_out");
#endif
    }

#if defined(__APPLE__)
        bool macos_bundle_mode = uses_ext_module;
        char macos_bundle_id[256];
        macos_bundle_id[0] = '\0';
        if (macos_bundle_mode) {
            if (strchr(name_without_ext, '.')) {
                snprintf(macos_bundle_id, sizeof(macos_bundle_id), "%s", name_without_ext);
            } else {
                snprintf(macos_bundle_id, sizeof(macos_bundle_id), "org.yi.%s", name_without_ext);
            }
            snprintf(unique_bin_name, sizeof(unique_bin_name), "%s.app/Contents/MacOS/%s", name_without_ext, name_without_ext);
        }
#endif

        uint64_t build_hash = proj_hash;
        build_hash = hash_cstr(build_hash, cc_path());
        build_hash = hash_cstr(build_hash, cc_flags());
        build_hash = hash_cstr(build_hash, extra_cflags);
        build_hash = hash_cstr(build_hash, extra_ldflags);
        build_hash = hash_cstr(build_hash, YIS_CACHE_VERSION);

        const char *no_cache_env = getenv("YIS_NO_CACHE");
        bool cache_enabled = false;
        if (no_cache_env && no_cache_env[0]) {
            cache_enabled = (no_cache_env[0] == '0');
        }

        char *cache_base = NULL;
        char *cache_dir = NULL;
        char *cache_c = NULL;
        char *cache_bin = NULL;
        if (cache_enabled) {
            cache_base = cache_base_dir();
            if (cache_base && ensure_dir(cache_base)) {
                char hex[17];
                snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)build_hash);
                cache_dir = path_join(cache_base, hex);
                if (cache_dir && ensure_dir(cache_dir)) {
                    char cache_c_name[512];
                    snprintf(cache_c_name, sizeof(cache_c_name), "%s.c", name_without_ext);
                    cache_c = path_join(cache_dir, cache_c_name);
                    cache_bin = path_join(cache_dir, unique_bin_name);
                }
            }
        }

        if (cache_enabled) {
            if (cache_bin && path_is_file(cache_bin)) {
                remove(cache_bin);
            }
            if (cache_c && path_is_file(cache_c)) {
                remove(cache_c);
            }
        }

        if (cache_enabled && cache_bin && path_is_file(cache_bin)) {
            int rc = run_binary_with_args(cache_bin, run_argc, run_argv);
            free(ext_bindings_alloc);
            free(ext_packager_alloc);
            free(cache_base);
            free(cache_dir);
            free(cache_c);
            free(cache_bin);
            arena_free(&arena);
            return rc == 0 ? 0 : 1;
        }

        // When not using cache, check if binary is up-to-date.
        if (!cache_enabled && !is_self_host_entry && path_is_file(unique_bin_name)) {
            long long bin_mtime = path_mtime(unique_bin_name);
            long long src_mtime = path_mtime(entry);
            if (bin_mtime >= 0 && src_mtime >= 0 && bin_mtime >= src_mtime) {
                // Binary is newer than source, just run it
                char run_cmd_buf[512];
#if defined(_WIN32)
                snprintf(run_cmd_buf, sizeof(run_cmd_buf), ".\\%s", unique_bin_name);
#else
                snprintf(run_cmd_buf, sizeof(run_cmd_buf), "./%s", unique_bin_name);
#endif
                int rc = run_binary_with_args(run_cmd_buf, run_argc, run_argv);
                free(ext_bindings_alloc);
                free(ext_packager_alloc);
                arena_free(&arena);
                return rc == 0 ? 0 : 1;
            }
        }

        prog = lower_program(prog, &arena, &err);
        if (!prog || err.message) {
            diag_print_enhanced(&err, verbose_mode);
            free(ext_bindings_alloc);
            free(ext_packager_alloc);
            free(cache_base);
            free(cache_dir);
            free(cache_c);
            free(cache_bin);
            arena_free(&arena);
            return 1;
        }
        if (!typecheck_program(prog, &arena, &err)) {
            diag_print_enhanced(&err, verbose_mode);
            free(ext_bindings_alloc);
            free(ext_packager_alloc);
            free(cache_base);
            free(cache_dir);
            free(cache_c);
            free(cache_bin);
            arena_free(&arena);
            return 1;
        }
        const char *c_path = cache_c ? cache_c : (is_self_host_entry ? ".yi_run_compile_out.c" : ".yi_run.c");
        const char *bin_path = cache_bin ? cache_bin : unique_bin_name;

        char run_cmd_buf[1024];
#if defined(_WIN32)
        snprintf(run_cmd_buf, sizeof(run_cmd_buf), ".\\%s", unique_bin_name);
#else
        snprintf(run_cmd_buf, sizeof(run_cmd_buf), "./%s", unique_bin_name);
#endif
        const char *run_cmd = cache_bin ? cache_bin : run_cmd_buf;
        if (!emit_c(prog, c_path, ext_module_name, ext_bindings_path_str, &err)) {
            diag_print_enhanced(&err, verbose_mode);
            free(ext_bindings_alloc);
            free(cache_base);
            free(cache_dir);
            free(cache_c);
            free(cache_bin);
            arena_free(&arena);
            return 1;
        }
#if defined(__APPLE__)
        if (macos_bundle_mode) {
            if (!prepare_macos_app_bundle(bin_path, macos_bundle_id, name_without_ext)) {
                fprintf(stderr, "error: failed to prepare macOS app bundle (%s)\n", name_without_ext);
                free(ext_bindings_alloc);
                free(ext_packager_alloc);
                free(cache_base);
                free(cache_dir);
                free(cache_c);
                free(cache_bin);
                arena_free(&arena);
                return 1;
            }
        }
#endif
        char cmd[4096];
        int n = snprintf(cmd, sizeof(cmd), "%s %s %s %s -o %s %s",
                         cc_path(), cc_flags(), extra_cflags, c_path, bin_path, extra_ldflags);
        if (n < 0 || (size_t)n >= sizeof(cmd)) {
            fprintf(stderr, "error: compile command too long\n");
            free(ext_bindings_alloc);
            free(ext_packager_alloc);
            free(cache_base);
            free(cache_dir);
            free(cache_c);
            free(cache_bin);
            arena_free(&arena);
            return 1;
        }
        int rc = system(cmd);
        if (rc != 0) {
            fprintf(stderr, "error: C compiler failed (code %d)\n", rc);
            free(ext_bindings_alloc);
            free(ext_packager_alloc);
            free(cache_base);
            free(cache_dir);
            free(cache_c);
            free(cache_bin);
            arena_free(&arena);
            return rc;
        }
#if defined(__APPLE__) || defined(__linux__)
        if (ext_packager_alloc && ext_packager_alloc[0]) {
            if (!run_module_packager_hook(ext_packager_alloc, entry, bin_path, name_without_ext)) {
                fprintf(stderr, "warning: external module packager failed for %s\n", ext_module_name);
            }
        }
#endif
        const char *emit_c_to = getenv("YIS_EMIT_C_TO");
        if (emit_c_to && emit_c_to[0]) {
            if (!path_is_file(c_path)) {
                fprintf(stderr, "error: C file was not written\n");
                free(ext_bindings_alloc);
                free(ext_packager_alloc);
                free(cache_base);
                free(cache_dir);
                free(cache_c);
                free(cache_bin);
                arena_free(&arena);
                return 1;
            }
            // Copy to user-requested path for inspection (e.g. debugging codegen)
            FILE *src = fopen(c_path, "rb");
            FILE *dst = fopen(emit_c_to, "wb");
            if (!src || !dst) {
                if (src) fclose(src);
                if (dst) fclose(dst);
                fprintf(stderr, "error: cannot copy C to %s\n", emit_c_to);
                free(ext_bindings_alloc);
                free(ext_packager_alloc);
                free(cache_base);
                free(cache_dir);
                free(cache_c);
                free(cache_bin);
                arena_free(&arena);
                return 1;
            }
            char buf[4096];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
                fwrite(buf, 1, n, dst);
            fclose(src);
            fclose(dst);
            (void)remove(c_path);
            free(ext_bindings_alloc);
            free(ext_packager_alloc);
            arena_free(&arena);
            free(cache_base);
            free(cache_dir);
            free(cache_c);
            free(cache_bin);
            fprintf(stderr, "Emitted C to %s (skip run)\n", emit_c_to);
            return 0;
        }
        const char *keep_c = getenv("YIS_KEEP_C");
        if (!(keep_c && keep_c[0] && keep_c[0] != '0')) {
            (void)remove(c_path);
        }
        // Compile-time AST/type data is no longer needed after codegen/compile.
        // Release it before running user code to reduce peak RSS.
        arena_free(&arena);
        rc = run_binary_with_args(run_cmd, run_argc, run_argv);
        free(ext_bindings_alloc);
        free(ext_packager_alloc);
        free(cache_base);
        free(cache_dir);
        free(cache_c);
        free(cache_bin);
        return rc == 0 ? 0 : 1;
    }

    if (argv[1][0] == '-') {
        fprintf(stderr, "error: unknown option %s\n", argv[1]);
        return 2;
    }

    if (argc > 2) {
        fprintf(stderr, "error: unexpected extra arguments\n");
        return 2;
    }

    Arena arena;
    arena_init(&arena);
    Diag err = {0};
    Program *prog = NULL;
    if (!load_project(argv[1], &arena, &prog, NULL, &err)) {
        diag_print_enhanced(&err, verbose_mode);
        arena_free(&arena);
        return 1;
    }
    prog = lower_program(prog, &arena, &err);
    if (!prog || err.message) {
        diag_print_enhanced(&err, verbose_mode);
        arena_free(&arena);
        return 1;
    }
    if (!typecheck_program(prog, &arena, &err)) {
        diag_print_enhanced(&err, verbose_mode);
        arena_free(&arena);
        return 1;
    }
    arena_free(&arena);
    return 0;
}
