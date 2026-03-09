#ifndef YIS_EXTERNAL_MODULE_H
#define YIS_EXTERNAL_MODULE_H

#include <stdbool.h>
#include <stddef.h>

#include "ast.h"
#include "str.h"

// --- External module resolution ---
// Discovers external modules by convention:
//   Module interface:  {name}.yi  found via env, dev paths, stdlib, or system install
//   Module bindings:   yis/{name}_bindings.inc  found relative to module .yi file or install path
//   Module build info: cflags/ldflags discovered via pkg-config or well-known lib paths

// Resolve an external module .yi file. Returns heap-allocated path or NULL.
// Search order:
//   1. YIS_{NAME}_PATH env var (direct file or directory containing {name}.yi)
//   2. cwd-relative dev paths: {name}/src/{name}.yi, ../{name}/src/{name}.yi, etc.
//   3. Installed stdlib: {stdlib_dir}/{name}.yi, system paths
//   4. Relative to executable
char *resolve_external_module(const char *name, const char *stdlib_dir);

// Find the bindings .inc file for an external module.
// module_yi_path is the path returned by resolve_external_module().
// Returns heap-allocated path or NULL.
// Search order:
//   1. YIS_{NAME}_BINDINGS env var
//   2. Sibling yis/{name}_bindings.inc (relative to module .yi directory)
//   3. Installed: {datadir}/yis/{name}/{name}_bindings.inc
//   4. cwd-relative dev paths
//   5. Relative to executable
char *find_module_bindings(const char *name, const char *module_yi_path);

// Find an optional post-build packager hook for an external module.
// module_yi_path is the path returned by resolve_external_module().
// Returns heap-allocated path or NULL.
// Search order:
//   1. YIS_{NAME}_PACKAGER env var
//   2. Sibling yis/{name}_packager.sh (relative to module .yi file)
//   3. Installed: {datadir}/yis/{name}/{name}_packager.sh
//   4. cwd-relative dev paths
//   5. Relative to executable
char *find_module_packager(const char *name, const char *module_yi_path);

// Detect whether a loaded Program uses a given external module.
bool program_uses_module(Program *prog, const char *name);

// Find the first non-stdlib external module imported by a Program.
// Returns true if found, writing the module name into out.
bool program_find_first_external_module(Program *prog, const char *stdlib_dir,
									   char *out, size_t out_cap);

// Search for set_appid() in a Program's AST and extract the app ID string.
// This is a generic facility: any module can use set_appid().
// Returns true if found, writing the sanitized name into out.
bool program_find_appid_name(Program *prog, char *out, size_t out_cap);

// Return default -I flags for an external module's header.
// Searches well-known paths for {name}/{name}.h or include/{name}/{name}.h.
// Returns static string (empty string if not found).
const char *module_default_cflags(const char *name);

// Return default -L/-l flags for an external module's shared library.
// Searches well-known lib paths for lib{name}.dylib/.so.
// Returns static string ("-l{name}" as fallback).
const char *module_default_ldflags(const char *name);

#endif
