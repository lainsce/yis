#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <io.h>
#include <windows.h>
#define yis_isatty _isatty
#define yis_fileno _fileno
#elif defined(__APPLE__)
#include <unistd.h>
#include <mach-o/dyld.h>
#include <limits.h>
#define yis_isatty isatty
#define yis_fileno fileno
#else
#include <unistd.h>
#include <limits.h>
#define yis_isatty isatty
#define yis_fileno fileno
#endif

bool yis_stdout_isatty(void) {
    return yis_isatty(yis_fileno(stdout)) != 0;
}

void yis_set_stdout_buffered(void) {
    if (!yis_stdout_isatty()) {
        setvbuf(stdout, NULL, _IOFBF, 1 << 16);
    }
}

char *yis_exe_path(void) {
    char *path = NULL;
#if defined(__APPLE__)
    uint32_t sz = 0;
    _NSGetExecutablePath(NULL, &sz);
    if (sz > 0) {
        char *buf = (char *)malloc(sz);
        if (buf && _NSGetExecutablePath(buf, &sz) == 0) {
            // Resolve symlinks
            char *resolved = realpath(buf, NULL);
            free(buf);
            path = resolved;
        } else {
            free(buf);
        }
    }
#elif defined(_WIN32)
    char buf[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, buf, MAX_PATH);
    if (n > 0 && n < MAX_PATH) {
        path = _strdup(buf);
    }
#else
    // Linux: read /proc/self/exe
    char buf[PATH_MAX];
    ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        path = strdup(buf);
    }
#endif
    return path;
}

char *yis_exe_dir(void) {
    char *path = yis_exe_path();
    if (!path) return NULL;
    // Strip the filename to get the directory
    char *last_sep = strrchr(path, '/');
#if defined(_WIN32)
    char *last_back = strrchr(path, '\\');
    if (!last_sep || (last_back && last_back > last_sep)) last_sep = last_back;
#endif
    if (last_sep) {
        *last_sep = '\0';
    } else {
        free(path);
        return NULL;
    }
    return path;
}
