#ifndef YIS_PLATFORM_H
#define YIS_PLATFORM_H

#include <stdbool.h>

bool yis_stdout_isatty(void);
void yis_set_stdout_buffered(void);

// Returns the full path of the running executable (heap-allocated).
// Caller must free(). Returns NULL on failure.
char *yis_exe_path(void);

// Returns the directory containing the running executable (heap-allocated).
// Caller must free(). Returns NULL on failure.
char *yis_exe_dir(void);

#endif
