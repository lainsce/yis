#ifndef YIS_CODEGEN_H
#define YIS_CODEGEN_H

#include <stdbool.h>

#include "ast.h"
#include "diag.h"

// emit_c generates a C source file from the given program.
// ext_module_name: name of the external module (e.g. "cogito"), or NULL for none.
// ext_bindings_path: path to the module's bindings .inc file, or NULL.
bool emit_c(Program *prog, const char *out_path,
            const char *ext_module_name, const char *ext_bindings_path,
            Diag *err);

#endif
