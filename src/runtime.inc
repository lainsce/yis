// ---- Yis runtime (minimal) ----
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#if !defined(_WIN32) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#endif
#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
#define _DARWIN_C_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <time.h>
#include <sys/stat.h>
#include <stddef.h>
#include <errno.h>
#include <fcntl.h>

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#include <direct.h>
#endif
#if !defined(_WIN32)
#include <dirent.h>
#include <sys/wait.h>
#endif
#include <unistd.h>
#if defined(__APPLE__)
#include <dlfcn.h>
typedef void *yis_objc_id;
typedef void *yis_objc_sel;
typedef void *yis_objc_cls;
typedef signed char yis_objc_bool;
typedef yis_objc_id  (*yis_objc_msgSend_t)(void);
typedef yis_objc_cls (*yis_objc_getClass_t)(const char*);
typedef yis_objc_sel (*yis_objc_selRegister_t)(const char*);
static yis_objc_msgSend_t     yis_objc_msgSend_fn;
static yis_objc_getClass_t    yis_objc_getClass_fn;
static yis_objc_selRegister_t yis_objc_selRegister_fn;
static int yis_objc_loaded = 0;
static int yis_objc_load(void) {
  if (yis_objc_loaded) return yis_objc_msgSend_fn != NULL;
  yis_objc_loaded = 1;
  void *lib = dlopen("/usr/lib/libobjc.A.dylib", RTLD_LAZY);
  if (!lib) return 0;
  yis_objc_msgSend_fn     = (yis_objc_msgSend_t)dlsym(lib, "objc_msgSend");
  yis_objc_getClass_fn    = (yis_objc_getClass_t)dlsym(lib, "objc_getClass");
  yis_objc_selRegister_fn = (yis_objc_selRegister_t)dlsym(lib, "sel_registerName");
  return yis_objc_msgSend_fn && yis_objc_getClass_fn && yis_objc_selRegister_fn;
}
#define OBJC_CLS(name)       yis_objc_getClass_fn(name)
#define OBJC_SEL(name)       yis_objc_selRegister_fn(name)
#define OBJC_SEND(ret, ...) ((ret(*)(__VA_ARGS__))yis_objc_msgSend_fn)
#endif

static int yis_stdout_isatty = 0;

static int yis_argc = 0;
static char **yis_argv = NULL;

void yis_set_args(int argc, char **argv) {
  yis_argc = argc;
  yis_argv = argv;
}

static void yis_runtime_init(void) {
#if defined(_WIN32)
  yis_stdout_isatty = _isatty(_fileno(stdout));
#else
  yis_stdout_isatty = isatty(fileno(stdout));
#endif
  if (!yis_stdout_isatty) {
    setvbuf(stdout, NULL, _IOFBF, 1 << 16);
  }
}

typedef enum {
  EVT_NULL,
  EVT_INT,
  EVT_FLOAT,
  EVT_BOOL,
  EVT_STR,
  EVT_ARR,
  EVT_DICT,
  EVT_OBJ,
  EVT_FN
} YisTag;

typedef struct YisVal YisVal;

typedef struct YisStr {
  int ref;
  size_t len;
  char* data;
} YisStr;

typedef struct YisArr {
  int ref;
  size_t len;
  size_t cap;
  YisVal* items;
} YisArr;

struct YisVal {
  YisTag tag;
  union {
    int64_t i;
    double f;
    bool b;
    void* p;
  } as;
};

typedef struct YisDictEnt {
  YisStr* key;
  YisVal val;
} YisDictEnt;

typedef struct YisDict {
  int ref;
  size_t len;
  size_t cap;
  YisDictEnt* entries;
} YisDict;

typedef struct YisObj {
  int ref;
  void (*drop)(struct YisObj*);
} YisObj;

typedef struct YisFn {
  int ref;
  int arity;
  YisVal (*fn)(void* env, int argc, YisVal* argv);
  void* env;
  int env_size;
} YisFn;

typedef struct YisRef {
  int ref;
  YisVal val;
} YisRef;

static int stdr_len(YisVal v);
static int64_t stdr_num(YisVal v);

#define YV_NULLV ((YisVal){.tag=EVT_NULL})
#define YV_INT(x) ((YisVal){.tag=EVT_INT, .as.i=(int64_t)(x)})
#define YV_FLOAT(x) ((YisVal){.tag=EVT_FLOAT, .as.f=(double)(x)})
#define YV_BOOL(x) ((YisVal){.tag=EVT_BOOL, .as.b=(x)?true:false})
#define YV_STR(x) ((YisVal){.tag=EVT_STR, .as.p=(x)})
#define YV_ARR(x) ((YisVal){.tag=EVT_ARR, .as.p=(x)})
#define YV_DICT(x) ((YisVal){.tag=EVT_DICT, .as.p=(x)})
#define YV_OBJ(x) ((YisVal){.tag=EVT_OBJ, .as.p=(x)})
#define YV_FN(x) ((YisVal){.tag=EVT_FN, .as.p=(x)})

static void yis_trap(const char* msg) {
  fprintf(stderr, "runtime error: %s\n", msg ? msg : "unknown error");
  fprintf(stderr, "  (run with debugger for stack trace)\n");
  abort();
}

// Keep allocation arithmetic checked before touching storage. Runtime failures
// abort deliberately instead of continuing with a null pointer or wrapped size.
static size_t yis_size_add(size_t a, size_t b) {
  if (b > SIZE_MAX - a) yis_trap("allocation size overflow");
  return a + b;
}

static size_t yis_size_mul(size_t count, size_t size) {
  if (size != 0 && count > SIZE_MAX / size) yis_trap("allocation size overflow");
  return count * size;
}

static void* yis_alloc(size_t size) {
  void* out = malloc(size);
  if (!out) yis_trap("out of memory");
  return out;
}

static void* yis_realloc(void* ptr, size_t size) {
  void* out = realloc(ptr, size);
  if (!out) yis_trap("out of memory");
  return out;
}

static size_t yis_grow_capacity(size_t cap, size_t needed, size_t item_size) {
  size_t limit = SIZE_MAX / item_size;
  if (needed > limit) yis_trap("allocation size overflow");
  if (cap == 0) cap = needed < 4 ? 4 : needed;
  if (cap > limit) cap = needed;
  while (cap < needed) {
    if (cap > limit / 2) {
      cap = needed;
      break;
    }
    cap *= 2;
  }
  return cap;
}

static void yis_retain_heap(YisVal v);
static void yis_release_heap(YisVal v);

// Primitive values have no ownership work. Keep this dispatch at the call site
// so the C compiler can remove it when a value's scalar tag is already known.
static inline __attribute__((always_inline)) void yis_retain_val(YisVal v) {
  if (v.tag >= EVT_STR) yis_retain_heap(v);
}

static inline __attribute__((always_inline)) void yis_release_val(YisVal v) {
  if (v.tag >= EVT_STR) yis_release_heap(v);
}
static int64_t yis_as_int(YisVal v);
static YisRef* yis_ref_new(void);
static void yis_ref_retain(YisRef* r);
static void yis_ref_release(YisRef* r);

// Static constant strings (ref=INT32_MAX means never freed)
static YisStr yis_static_empty    = { INT32_MAX, 0, "" };
static YisStr yis_static_null     = { INT32_MAX, 4, "null" };
static YisStr yis_static_true     = { INT32_MAX, 4, "true" };
static YisStr yis_static_false    = { INT32_MAX, 5, "false" };
static YisStr yis_static_array    = { INT32_MAX, 7, "[array]" };
static YisStr yis_static_dict     = { INT32_MAX, 6, "[dict]" };
static YisStr yis_static_object   = { INT32_MAX, 8, "[object]" };
static YisStr yis_static_function = { INT32_MAX, 10, "[function]" };
static YisStr yis_static_unknown  = { INT32_MAX, 3, "<?>" };

static bool yis_static_ascii_init = false;
static YisStr yis_static_ascii[256];
static char yis_static_ascii_data[256][2];

static void yis_init_static_ascii(void) {
  if (yis_static_ascii_init) return;
  for (int i = 0; i < 256; i++) {
    yis_static_ascii_data[i][0] = (char)i;
    yis_static_ascii_data[i][1] = 0;
    yis_static_ascii[i].ref = INT32_MAX;
    yis_static_ascii[i].len = 1;
    yis_static_ascii[i].data = yis_static_ascii_data[i];
  }
  yis_static_ascii_init = true;
}

static YisStr* yis_static_char(unsigned char c) {
  if (!yis_static_ascii_init) yis_init_static_ascii();
  return &yis_static_ascii[c];
}

static YisStr* stdr_str_lit(const char* s) {
  size_t n = strlen(s);
  if (n == 0) return &yis_static_empty;
  if (n == 1) return yis_static_char((unsigned char)s[0]);
  YisStr* st = (YisStr*)yis_alloc(yis_size_add(sizeof(YisStr), yis_size_add(n, 1)));
  st->ref = 1;
  st->len = n;
  st->data = (char*)(st + 1);
  memcpy(st->data, s, n + 1);
  return st;
}

static YisStr* stdr_str_from_parts(int n, YisVal* parts);
static YisStr* stdr_to_string(YisVal v);
static YisStr* stdr_str_from_slice(const char* s, size_t len);
static YisArr* stdr_arr_new(int n);
static void yis_arr_add(YisArr* a, YisVal v);
static YisVal yis_arr_get(YisArr* a, int64_t idx);
static void yis_arr_set(YisArr* a, int64_t idx, YisVal v);
static YisVal yis_arr_remove(YisArr* a, int64_t idx);

static YisDict* stdr_dict_new(void);
static void yis_dict_set(YisDict* d, YisVal key, YisVal val);
static YisVal yis_dict_get(YisDict* d, YisVal key);
static int yis_dict_len(YisDict* d);

static YisVal stdr_str_at(YisVal v, int64_t idx) {
  if (v.tag != EVT_STR) yis_trap("str_at expects string");
  YisStr* s = (YisStr*)v.as.p;
  if (idx < 0 || (size_t)idx >= s->len) return YV_STR(&yis_static_empty);
  return YV_STR(yis_static_char((unsigned char)s->data[idx]));
}

static YisVal stdr_slice(YisVal sv, int64_t start, int64_t end) {
  if (sv.tag != EVT_STR) yis_trap("slice expects string");
  YisStr* s = (YisStr*)sv.as.p;
  size_t len = s->len;
  if (start < 0) start = 0;
  if ((size_t)start > len) start = (int64_t)len;
  if (end < start) end = start;
  if ((size_t)end > len) end = (int64_t)len;
  size_t n = (size_t)(end - start);
  if (n == 0) return YV_STR(&yis_static_empty);
  if (n == 1) return YV_STR(yis_static_char((unsigned char)s->data[start]));
  return YV_STR(stdr_str_from_slice(s->data + start, n));
}

static YisVal stdr_str_concat(YisVal a, YisVal b) {
  YisVal parts[2] = { a, b };
  return YV_STR(stdr_str_from_parts(2, parts));
}

static int64_t stdr_char_code(YisVal cv) {
  if (cv.tag != EVT_STR) yis_trap("char_code expects string");
  YisStr* s = (YisStr*)cv.as.p;
  if (s->len == 0) return 0;
  return (unsigned char)s->data[0];
}

static int stdr_len(YisVal v) {
  if (v.tag == EVT_STR) return (int)((YisStr*)v.as.p)->len;
  if (v.tag == EVT_ARR) return (int)((YisArr*)v.as.p)->len;
  if (v.tag == EVT_DICT) return (int)((YisDict*)v.as.p)->len;
  return 0;
}

static int64_t stdr_num(YisVal v) {
  return yis_as_int(v);
}

static bool stdr_is_null(YisVal v) { return v.tag == EVT_NULL; }

static void stdr_write(YisVal v) {
  YisStr* s = stdr_to_string(v);
  fwrite(s->data, 1, s->len, stdout);
  fflush(stdout);
  yis_release_val(YV_STR(s));
}

static void writef(YisVal fmt, int argc, YisVal* argv) {
  if (fmt.tag != EVT_STR) yis_trap("writef expects string");
  YisStr* s = (YisStr*)fmt.as.p;
  size_t i = 0;
  size_t seg = 0;
  int argi = 0;
  while (i < s->len) {
    if (i + 1 < s->len && s->data[i] == '{' && s->data[i + 1] == '}') {
      if (i > seg) fwrite(s->data + seg, 1, i - seg, stdout);
      if (argi < argc) {
        YisStr* ps = stdr_to_string(argv[argi++]);
        fwrite(ps->data, 1, ps->len, stdout);
        yis_release_val(YV_STR(ps));
      }
      i += 2;
      seg = i;
      continue;
    }
    i++;
  }
  if (i > seg) fwrite(s->data + seg, 1, i - seg, stdout);
  if (yis_stdout_isatty) fflush(stdout);
}

static void stdr_writef_args(YisVal fmt, YisVal args) {
  if (args.tag != EVT_ARR) yis_trap("writef expects args tuple");
  YisArr* a = (YisArr*)args.as.p;
  writef(fmt, (int)a->len, a->items);
}

static YisStr* stdr_read_line(void) {
  size_t cap = 128;
  size_t len = 0;
  char* buf = (char*)malloc(cap);
  if (!buf) yis_trap("out of memory");
  int c;
  while ((c = fgetc(stdin)) != EOF) {
    if (c == '\n') break;
    if (len + 1 >= cap) {
      cap = yis_grow_capacity(cap, yis_size_add(len, 2), 1);
      buf = (char*)realloc(buf, cap);
      if (!buf) yis_trap("out of memory");
    }
    buf[len++] = (char)c;
  }
  if (len > 0 && buf[len - 1] == '\r') len--;
  YisStr* s = (YisStr*)yis_alloc(yis_size_add(sizeof(YisStr), yis_size_add(len, 1)));
  if (!s) yis_trap("out of memory");
  s->ref = 1;
  s->len = len;
  s->data = (char*)(s + 1);
  memcpy(s->data, buf, len);
  s->data[len] = 0;
  free(buf);
  return s;
}

static YisVal stdr_read_text_file(YisVal pathv) {
  if (pathv.tag != EVT_STR) yis_trap("read_text_file expects string path");
  YisStr* path = (YisStr*)pathv.as.p;
  FILE* f = fopen(path->data, "rb");
  if (!f) return YV_NULLV;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return YV_NULLV;
  }
  long sz = ftell(f);
  if (sz < 0) {
    fclose(f);
    return YV_NULLV;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return YV_NULLV;
  }
  size_t len = (size_t)sz;
  YisStr* out = (YisStr*)yis_alloc(yis_size_add(sizeof(YisStr), yis_size_add(len, 1)));
  if (!out) {
    fclose(f);
    yis_trap("out of memory");
  }
  out->data = (char*)(out + 1);
  size_t n = 0;
  if (len > 0) n = fread(out->data, 1, len, f);
  fclose(f);
  if (n != len) {
    free(out);
    return YV_NULLV;
  }
  out->ref = 1;
  out->len = len;
  out->data[len] = 0;
  return YV_STR(out);
}

static YisVal stdr_write_text_file(YisVal pathv, YisVal textv) {
  if (pathv.tag != EVT_STR) yis_trap("write_text_file expects string path");
  if (textv.tag != EVT_STR) yis_trap("write_text_file expects string text");
  YisStr* path = (YisStr*)pathv.as.p;
  YisStr* text = (YisStr*)textv.as.p;
  FILE* f = fopen(path->data, "wb");
  if (!f) return YV_BOOL(false);
  size_t n = 0;
  if (text->len > 0) n = fwrite(text->data, 1, text->len, f);
  // fclose must run even after a short write to release the descriptor.
  int closed = fclose(f);
  return YV_BOOL(n == text->len && closed == 0);
}

static bool stdr_is_dir_path(const char* path) {
  if (!path || !path[0]) return false;
  struct stat st;
  if (stat(path, &st) != 0) return false;
  return S_ISDIR(st.st_mode);
}

static int stdr_mkdir_single(const char* path) {
#if defined(_WIN32)
  if (_mkdir(path) == 0) return 0;
#else
  if (mkdir(path, 0755) == 0) return 0;
#endif
  if (errno == EEXIST && stdr_is_dir_path(path)) return 0;
  return -1;
}

static YisVal stdr_ensure_dir(YisVal pathv) {
  if (pathv.tag != EVT_STR) yis_trap("ensure_dir expects string path");
  YisStr* path = (YisStr*)pathv.as.p;
  if (!path || path->len == 0) return YV_BOOL(false);

  size_t len = path->len;
  char* buf = (char*)malloc(yis_size_add(len, 2));
  if (!buf) yis_trap("out of memory");
  memcpy(buf, path->data, len);
  buf[len] = 0;

  while (len > 1 && (buf[len - 1] == '/' || buf[len - 1] == '\\')) {
    buf[len - 1] = 0;
    len--;
  }
  if (len == 0) {
    free(buf);
    return YV_BOOL(false);
  }

  size_t start = 0;
#if defined(_WIN32)
  if (len >= 2 &&
      (((buf[0] >= 'A' && buf[0] <= 'Z') || (buf[0] >= 'a' && buf[0] <= 'z')) &&
       buf[1] == ':')) {
    start = 2;
  }
#endif
  if (buf[0] == '/' || buf[0] == '\\') start = 1;

  for (size_t i = start; i <= len; i++) {
    char ch = buf[i];
    if (ch != '/' && ch != '\\' && ch != 0) continue;
    buf[i] = 0;
    if (buf[0] != 0) {
      if (stdr_mkdir_single(buf) != 0) {
        free(buf);
        return YV_BOOL(false);
      }
    }
    buf[i] = ch;
  }

  free(buf);
  return YV_BOOL(true);
}

static YisVal stdr_remove_file(YisVal pathv) {
  if (pathv.tag != EVT_STR) yis_trap("remove_file expects string path");
  YisStr* path = (YisStr*)pathv.as.p;
  if (!path || path->len == 0) return YV_BOOL(false);
#if defined(_WIN32)
  if (_unlink(path->data) == 0) return YV_BOOL(true);
#else
  if (unlink(path->data) == 0) return YV_BOOL(true);
#endif
  if (errno == ENOENT) return YV_BOOL(true);
  return YV_BOOL(false);
}

// Canonical identity for existing files, including symlink resolution on POSIX.
static YisVal stdr_realpath(YisVal pathv) {
  if (pathv.tag != EVT_STR) yis_trap("realpath expects string path");
  YisStr* path = (YisStr*)pathv.as.p;
  if (path->len == 0 || memchr(path->data, 0, path->len)) return YV_NULLV;
#if defined(_WIN32)
  struct stat st;
  if (stat(path->data, &st) != 0) return YV_NULLV;
  char* full = _fullpath(NULL, path->data, 0);
#else
  char* full = realpath(path->data, NULL);
#endif
  if (!full) return YV_NULLV;
  YisVal out = YV_STR(stdr_str_from_slice(full, strlen(full)));
  free(full);
  return out;
}

// Environment values are copied so callers own the resulting string.
static YisVal stdr_getenv(YisVal namev) {
  if (namev.tag != EVT_STR) yis_trap("getenv expects string name");
  YisStr* name = (YisStr*)namev.as.p;
  if (name->len == 0 || memchr(name->data, 0, name->len) ||
      memchr(name->data, '=', name->len)) return YV_NULLV;
  const char* value = getenv(name->data);
  return value ? YV_STR(stdr_str_from_slice(value, strlen(value))) : YV_NULLV;
}

// Stable byte-based FNV-1a hash for cache identities; unsigned overflow is defined.
static YisVal stdr_hash_text(YisVal textv) {
  if (textv.tag != EVT_STR) yis_trap("hash_text expects string");
  YisStr* text = (YisStr*)textv.as.p;
  uint64_t hash = UINT64_C(14695981039346656037);
  for (size_t i = 0; i < text->len; i++) {
    hash ^= (unsigned char)text->data[i];
    hash *= UINT64_C(1099511628211);
  }
  char hex[17];
  snprintf(hex, sizeof(hex), "%016llx", (unsigned long long)hash);
  return YV_STR(stdr_str_from_slice(hex, 16));
}

// Create the file exclusively before returning its path. The caller can replace
// its contents or atomically rename it without choosing a shared temporary name.
static YisVal stdr_temp_file(YisVal dirv, YisVal prefixv) {
  if (dirv.tag != EVT_STR || prefixv.tag != EVT_STR)
    yis_trap("temp_file expects directory and prefix strings");
  YisStr* dir = (YisStr*)dirv.as.p;
  YisStr* prefix = (YisStr*)prefixv.as.p;
  if (dir->len == 0 || memchr(dir->data, 0, dir->len) ||
      memchr(prefix->data, 0, prefix->len) ||
      memchr(prefix->data, '/', prefix->len) ||
      memchr(prefix->data, '\\', prefix->len)) return YV_NULLV;
  bool sep = dir->data[dir->len - 1] != '/' && dir->data[dir->len - 1] != '\\';
  size_t base = yis_size_add(yis_size_add(dir->len, sep ? 1 : 0), prefix->len);
  char* path = (char*)yis_alloc(yis_size_add(base, 33));
  memcpy(path, dir->data, dir->len);
  if (sep) path[dir->len] = '/';
  memcpy(path + dir->len + (sep ? 1 : 0), prefix->data, prefix->len);
  int fd = -1;
#if defined(_WIN32)
  static unsigned long serial = 0;
  for (int attempt = 0; attempt < 128; attempt++) {
    snprintf(path + base, 33, "%08lx%08lx%08lx", (unsigned long)GetCurrentProcessId(),
             (unsigned long)GetTickCount(), ++serial);
    fd = _open(path, _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY, _S_IREAD | _S_IWRITE);
    if (fd >= 0 || errno != EEXIST) break;
  }
  int closed = fd >= 0 ? _close(fd) : -1;
#else
  memcpy(path + base, "XXXXXX", 7);
  fd = mkstemp(path);
  int closed = fd >= 0 ? close(fd) : -1;
#endif
  if (fd < 0 || closed != 0) {
    if (fd >= 0) remove(path);
    free(path);
    return YV_NULLV;
  }
  YisStr* result = stdr_str_from_slice(path, strlen(path));
  free(path);
  return YV_STR(result);
}

static YisVal stdr_move_file(YisVal srcv, YisVal dstv) {
  if (srcv.tag != EVT_STR) yis_trap("move_file expects source path string");
  if (dstv.tag != EVT_STR) yis_trap("move_file expects destination path string");
  YisStr* src = (YisStr*)srcv.as.p;
  YisStr* dst = (YisStr*)dstv.as.p;
  if (!src || src->len == 0 || !dst || dst->len == 0) return YV_BOOL(false);
#if defined(_WIN32)
  _unlink(dst->data);
#endif
  if (rename(src->data, dst->data) == 0) return YV_BOOL(true);
  return YV_BOOL(false);
}

static bool stdr_ends_with_ci(const char* text, const char* suffix) {
  if (!text || !suffix) return false;
  size_t tn = strlen(text);
  size_t sn = strlen(suffix);
  if (sn == 0 || sn > tn) return false;
  const char* t = text + (tn - sn);
  for (size_t i = 0; i < sn; i++) {
    unsigned char a = (unsigned char)t[i];
    unsigned char b = (unsigned char)suffix[i];
    if ((unsigned char)tolower(a) != (unsigned char)tolower(b)) return false;
  }
  return true;
}

static bool stdr_name_matches_exts(const char* name, YisArr* exts) {
  if (!name || !exts) return false;
  for (size_t i = 0; i < exts->len; i++) {
    YisVal ev = exts->items[i];
    if (ev.tag != EVT_STR) continue;
    YisStr* ext = (YisStr*)ev.as.p;
    if (!ext || ext->len == 0) continue;
    if (stdr_ends_with_ci(name, ext->data)) return true;
  }
  return false;
}

static char* stdr_join_path_c(const char* dir, const char* name) {
  if (!dir || !name) return NULL;
  size_t dl = strlen(dir);
  size_t nl = strlen(name);
  bool need_sep = (dl > 0 && dir[dl - 1] != '/');
  size_t total = yis_size_add(yis_size_add(dl, nl), need_sep ? 2 : 1);
  char* out = (char*)malloc(total);
  if (!out) return NULL;
  memcpy(out, dir, dl);
  size_t p = dl;
  if (need_sep) out[p++] = '/';
  memcpy(out + p, name, nl);
  out[p + nl] = 0;
  return out;
}

static int stdr_cmp_paths(const void* a, const void* b) {
  const YisVal* va = (const YisVal*)a;
  const YisVal* vb = (const YisVal*)b;
  if (va->tag != EVT_STR || vb->tag != EVT_STR) return 0;
  YisStr* sa = (YisStr*)va->as.p;
  YisStr* sb = (YisStr*)vb->as.p;
  const char* pa = (sa && sa->data) ? sa->data : "";
  const char* pb = (sb && sb->data) ? sb->data : "";
  return strcmp(pa, pb);
}

#if !defined(_WIN32)
static int64_t stdr_stat_mtime_secs(const struct stat* st) {
#if defined(__APPLE__)
  return (int64_t)st->st_mtimespec.tv_sec;
#elif defined(_WIN32)
  return (int64_t)st->st_mtime;
#else
  return (int64_t)st->st_mtim.tv_sec;
#endif
}

static void stdr_find_files_walk(const char* dir, YisArr* exts, YisArr* out) {
  DIR* dp = opendir(dir);
  if (!dp) return;
  struct dirent* ent = NULL;
  while ((ent = readdir(dp)) != NULL) {
    const char* name = ent->d_name;
    if (!name) continue;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

    char* full = stdr_join_path_c(dir, name);
    if (!full) continue;

    struct stat st;
    if (lstat(full, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        stdr_find_files_walk(full, exts, out);
      } else if (S_ISREG(st.st_mode) && stdr_name_matches_exts(name, exts)) {
        yis_arr_add(out, YV_STR(stdr_str_from_slice(full, strlen(full))));
      }
    }
    free(full);
  }
  closedir(dp);
}

static void stdr_prune_old_files_walk(const char* dir, int64_t cutoff, int64_t* removed) {
  DIR* dp = opendir(dir);
  if (!dp) return;
  struct dirent* ent = NULL;
  while ((ent = readdir(dp)) != NULL) {
    const char* name = ent->d_name;
    if (!name) continue;
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) continue;

    char* full = stdr_join_path_c(dir, name);
    if (!full) continue;

    struct stat st;
    if (lstat(full, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        stdr_prune_old_files_walk(full, cutoff, removed);
      } else if (S_ISREG(st.st_mode)) {
        int64_t mt = stdr_stat_mtime_secs(&st);
        if (mt <= cutoff && unlink(full) == 0) {
          if (removed) (*removed)++;
        }
      }
    }
    free(full);
  }
  closedir(dp);
}
#endif

static YisVal stdr_find_files(YisVal rootv, YisVal extsv) {
  if (rootv.tag != EVT_STR) yis_trap("find_files expects root path string");
  if (extsv.tag != EVT_ARR) yis_trap("find_files expects extensions array");
  YisStr* root = (YisStr*)rootv.as.p;
  YisArr* exts = (YisArr*)extsv.as.p;
  YisArr* out = stdr_arr_new(8);
  if (!root || root->len == 0 || !exts) return YV_ARR(out);

#if !defined(_WIN32)
  struct stat st;
  if (stat(root->data, &st) == 0 && S_ISDIR(st.st_mode)) {
    stdr_find_files_walk(root->data, exts, out);
  }
#else
  (void)root;
  (void)exts;
#endif

  if (out->len > 1) {
    qsort(out->items, out->len, sizeof(YisVal), stdr_cmp_paths);
  }
  return YV_ARR(out);
}

static YisVal stdr_prune_files_older_than(YisVal dirv, YisVal daysv) {
  if (dirv.tag != EVT_STR) yis_trap("prune_files_older_than expects directory path string");
  YisStr* dir = (YisStr*)dirv.as.p;
  int64_t days = stdr_num(daysv);
  if (!dir || dir->len == 0 || days <= 0) return YV_INT(0);

  time_t now = time(NULL);
  if (now == (time_t)-1) return YV_INT(0);
  int64_t seconds;
  int64_t cutoff;
  if (__builtin_mul_overflow(days, INT64_C(86400), &seconds) ||
      __builtin_sub_overflow((int64_t)now, seconds, &cutoff))
    yis_trap("integer overflow");
  int64_t removed = 0;

#if !defined(_WIN32)
  struct stat st;
  if (stat(dir->data, &st) == 0 && S_ISDIR(st.st_mode)) {
    stdr_prune_old_files_walk(dir->data, cutoff, &removed);
  }
#else
  (void)dir;
  (void)cutoff;
#endif

  return YV_INT(removed);
}

static void stdr_exit_process(YisVal codev) {
  exit((int)stdr_num(codev));
}

static YisVal stdr_run_command(YisVal cmdv) {
  if (cmdv.tag != EVT_STR) yis_trap("run_command expects string");
  YisStr* cmd = (YisStr*)cmdv.as.p;
#if defined(_WIN32)
  FILE* p = _popen(cmd->data, "r");
#else
  FILE* p = popen(cmd->data, "r");
#endif
  if (!p) {
    YisArr* out = stdr_arr_new(2);
    yis_arr_add(out, YV_INT(-1));
    yis_arr_add(out, YV_STR(stdr_str_lit("")));
    return YV_ARR(out);
  }
  size_t cap = 4096;
  size_t len = 0;
  char* buf = (char*)malloc(cap);
  if (!buf) {
#if defined(_WIN32)
    _pclose(p);
#else
    pclose(p);
#endif
    yis_trap("out of memory");
  }
  int c;
  while ((c = fgetc(p)) != EOF) {
    if (len + 1 >= cap) {
      cap = yis_grow_capacity(cap, yis_size_add(len, 2), 1);
      char* n = (char*)realloc(buf, cap);
      if (!n) {
        free(buf);
#if defined(_WIN32)
        _pclose(p);
#else
        pclose(p);
#endif
        yis_trap("out of memory");
      }
      buf = n;
    }
    buf[len++] = (char)c;
  }
  buf[len] = '\0';
#if defined(_WIN32)
  int status = _pclose(p);
#else
  int status = pclose(p);
  if (status != -1) {
    if (WIFEXITED(status)) status = WEXITSTATUS(status);
    else if (WIFSIGNALED(status)) status = 128 + WTERMSIG(status);
    else status = -1;
  }
#endif
  YisStr* out_str = stdr_str_from_slice(buf, len);
  free(buf);
  YisArr* out = stdr_arr_new(2);
  yis_arr_add(out, YV_INT(status));
  yis_arr_add(out, YV_STR(out_str));
  YisVal rv; rv.tag = EVT_ARR; rv.as.p = out;
  return rv;
}

static YisVal stdr_file_exists(YisVal pathv) {
  if (pathv.tag != EVT_STR) yis_trap("file_exists expects string");
  YisStr* s = (YisStr*)pathv.as.p;
  struct stat st;
  return YV_BOOL(stat(s->data, &st) == 0 && S_ISREG(st.st_mode));
}

static YisVal stdr_file_mtime(YisVal pathv) {
  if (pathv.tag != EVT_STR) yis_trap("file_mtime expects string");
  YisStr* s = (YisStr*)pathv.as.p;
  struct stat st;
  if (stat(s->data, &st) != 0) return YV_INT(-1);
#if defined(__APPLE__)
  return YV_INT((int64_t)st.st_mtimespec.tv_sec);
#elif defined(_WIN32)
  return YV_INT((int64_t)st.st_mtime);
#else
  return YV_INT((int64_t)st.st_mtim.tv_sec);
#endif
}

static YisVal stdr_file_mtime_ns(YisVal pathv) {
  if (pathv.tag != EVT_STR) yis_trap("file_mtime_ns expects string");
  YisStr* s = (YisStr*)pathv.as.p;
  struct stat st;
  if (stat(s->data, &st) != 0) return YV_INT(-1);
#if defined(__APPLE__)
  return YV_INT((int64_t)st.st_mtimespec.tv_sec * INT64_C(1000000000) + st.st_mtimespec.tv_nsec);
#elif defined(_WIN32)
  return YV_INT((int64_t)st.st_mtime * INT64_C(1000000000));
#else
  return YV_INT((int64_t)st.st_mtim.tv_sec * INT64_C(1000000000) + st.st_mtim.tv_nsec);
#endif
}

static YisVal stdr_getcwd(void) {
  char buf[4096];
  if (getcwd(buf, sizeof(buf))) return YV_STR(stdr_str_from_slice(buf, strlen(buf)));
  return YV_STR(stdr_str_from_slice("", 0));
}

static YisVal stdr_home_dir(void) {
  const char* home = getenv("HOME");
#if defined(_WIN32)
  if (!home || !home[0]) home = getenv("USERPROFILE");
#endif
  if (!home) home = "";
  return YV_STR(stdr_str_from_slice(home, strlen(home)));
}

static bool stdr_localtime_safe(time_t ts, struct tm* out_tm) {
  if (!out_tm) return false;
#if defined(_WIN32)
  return localtime_s(out_tm, &ts) == 0;
#else
  return localtime_r(&ts, out_tm) != NULL;
#endif
}

static YisVal stdr_unix_time(void) {
  time_t now = time(NULL);
  if (now == (time_t)-1) return YV_INT(-1);
  return YV_INT((int64_t)now);
}

static YisVal stdr_current_year(void) {
  time_t now = time(NULL);
  struct tm tmv;
  if (now == (time_t)-1 || !stdr_localtime_safe(now, &tmv)) return YV_INT(0);
  return YV_INT((int64_t)(tmv.tm_year + 1900));
}

static YisVal stdr_current_month(void) {
  time_t now = time(NULL);
  struct tm tmv;
  if (now == (time_t)-1 || !stdr_localtime_safe(now, &tmv)) return YV_INT(0);
  return YV_INT((int64_t)(tmv.tm_mon + 1));
}

static YisVal stdr_current_day(void) {
  time_t now = time(NULL);
  struct tm tmv;
  if (now == (time_t)-1 || !stdr_localtime_safe(now, &tmv)) return YV_INT(0);
  return YV_INT((int64_t)tmv.tm_mday);
}

static YisVal stdr_weekday(YisVal yearv, YisVal monthv, YisVal dayv) {
  int y = (int)stdr_num(yearv);
  int m = (int)stdr_num(monthv);
  int d = (int)stdr_num(dayv);
  if (y < 1 || m < 1 || m > 12 || d < 1 || d > 31) return YV_INT(0);

  struct tm tmv;
  memset(&tmv, 0, sizeof(tmv));
  tmv.tm_year = y - 1900;
  tmv.tm_mon = m - 1;
  tmv.tm_mday = d;
  tmv.tm_hour = 12;
  tmv.tm_isdst = -1;

  time_t ts = mktime(&tmv);
  if (ts == (time_t)-1) return YV_INT(0);
  struct tm out_tm;
  if (!stdr_localtime_safe(ts, &out_tm)) return YV_INT(0);
  return YV_INT((int64_t)out_tm.tm_wday);
}

static bool stdr_parse_iso_ymdhm(const char* s, int* y, int* m, int* d, int* hh, int* mm) {
  if (!s || !y || !m || !d || !hh || !mm) return false;
  if (strlen(s) < 16) return false;
  if (s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':') return false;

  for (int i = 0; i < 16; i++) {
    if (i == 4 || i == 7 || i == 10 || i == 13) continue;
    if (!isdigit((unsigned char)s[i])) return false;
  }

  *y = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
  *m = (s[5] - '0') * 10 + (s[6] - '0');
  *d = (s[8] - '0') * 10 + (s[9] - '0');
  *hh = (s[11] - '0') * 10 + (s[12] - '0');
  *mm = (s[14] - '0') * 10 + (s[15] - '0');

  if (*m < 1 || *m > 12 || *d < 1 || *d > 31 || *hh < 0 || *hh > 23 || *mm < 0 || *mm > 59) {
    return false;
  }
  return true;
}

static int64_t stdr_mktime_with_tz(struct tm* tmv, const char* tz_name) {
  if (!tmv) return -1;
#if defined(_WIN32)
  if (tz_name && tz_name[0]) {
    _putenv_s("TZ", tz_name);
  } else {
    _putenv_s("TZ", "");
  }
  _tzset();
  time_t ts = mktime(tmv);
  _putenv_s("TZ", "");
  _tzset();
  if (ts == (time_t)-1) return -1;
  return (int64_t)ts;
#else
  char* old_tz = getenv("TZ");
  char* old_copy = old_tz ? strdup(old_tz) : NULL;
  if (tz_name && tz_name[0]) {
    setenv("TZ", tz_name, 1);
  } else {
    unsetenv("TZ");
  }
  tzset();

  time_t ts = mktime(tmv);

  if (old_copy) {
    setenv("TZ", old_copy, 1);
    free(old_copy);
  } else {
    unsetenv("TZ");
  }
  tzset();

  if (ts == (time_t)-1) return -1;
  return (int64_t)ts;
#endif
}

static YisVal stdr_iso_to_epoch(YisVal isov, YisVal tzv) {
  if (isov.tag != EVT_STR) yis_trap("iso_to_epoch expects iso string");
  if (tzv.tag != EVT_STR && tzv.tag != EVT_NULL) yis_trap("iso_to_epoch expects timezone string");

  YisStr* iso = (YisStr*)isov.as.p;
  if (!iso || iso->len == 0) return YV_INT(-1);

  int y = 0, m = 0, d = 0, hh = 0, mm = 0;
  if (!stdr_parse_iso_ymdhm(iso->data, &y, &m, &d, &hh, &mm)) return YV_INT(-1);

  struct tm tmv;
  memset(&tmv, 0, sizeof(tmv));
  tmv.tm_year = y - 1900;
  tmv.tm_mon = m - 1;
  tmv.tm_mday = d;
  tmv.tm_hour = hh;
  tmv.tm_min = mm;
  tmv.tm_sec = 0;
  tmv.tm_isdst = -1;

  const char* tz_name = "";
  if (tzv.tag == EVT_STR) {
    YisStr* tz = (YisStr*)tzv.as.p;
    if (tz && tz->len > 0) tz_name = tz->data;
  }

  return YV_INT(stdr_mktime_with_tz(&tmv, tz_name));
}

static YisVal stdr_capture_shell_first_line(const char* cmd) {
  if (!cmd || !cmd[0]) return YV_NULLV;
#if defined(_WIN32)
  FILE* p = _popen(cmd, "r");
#else
  FILE* p = popen(cmd, "r");
#endif
  if (!p) return YV_NULLV;
  char buf[4096];
  if (!fgets(buf, sizeof(buf), p)) {
#if defined(_WIN32)
    _pclose(p);
#else
    pclose(p);
#endif
    return YV_NULLV;
  }
#if defined(_WIN32)
  _pclose(p);
#else
  pclose(p);
#endif
  size_t len = strlen(buf);
  while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) len--;
  if (len == 0) return YV_NULLV;
  return YV_STR(stdr_str_from_slice(buf, len));
}

static YisVal stdr_open_file_dialog(YisVal promptv, YisVal extv) {
  if (promptv.tag != EVT_STR) yis_trap("open_file_dialog expects prompt string");
  if (extv.tag != EVT_STR) yis_trap("open_file_dialog expects extension string");
  YisStr* prompt = (YisStr*)promptv.as.p;
  YisStr* ext = (YisStr*)extv.as.p;
#if defined(__APPLE__)
  if (!yis_objc_load()) return YV_NULLV;
  yis_objc_id cls = (yis_objc_id)OBJC_CLS("NSOpenPanel");
  if (!cls) return YV_NULLV;
  yis_objc_id panel = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(cls, OBJC_SEL("openPanel"));
  if (!panel) return YV_NULLV;
  if (prompt && prompt->data && prompt->data[0]) {
    yis_objc_id ns = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, const char*)(
        (yis_objc_id)OBJC_CLS("NSString"), OBJC_SEL("stringWithUTF8String:"), prompt->data);
    OBJC_SEND(void, yis_objc_id, yis_objc_sel, yis_objc_id)(panel, OBJC_SEL("setMessage:"), ns);
  }
  if (ext && ext->data && ext->data[0]) {
    yis_objc_id es = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, const char*)(
        (yis_objc_id)OBJC_CLS("NSString"), OBJC_SEL("stringWithUTF8String:"), ext->data);
    yis_objc_id UTType = (yis_objc_id)OBJC_CLS("UTType");
    if (UTType) {
      yis_objc_id ut = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, yis_objc_id)(UTType, OBJC_SEL("typeWithFilenameExtension:"), es);
      if (ut) {
        yis_objc_id arr = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, yis_objc_id)((yis_objc_id)OBJC_CLS("NSArray"), OBJC_SEL("arrayWithObject:"), ut);
        OBJC_SEND(void, yis_objc_id, yis_objc_sel, yis_objc_id)(panel, OBJC_SEL("setAllowedContentTypes:"), arr);
      }
    }
  }
  long result = OBJC_SEND(long, yis_objc_id, yis_objc_sel)(panel, OBJC_SEL("runModal"));
  if (result != 1) return YV_NULLV;
  yis_objc_id urls = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(panel, OBJC_SEL("URLs"));
  yis_objc_id url = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(urls, OBJC_SEL("firstObject"));
  if (!url) return YV_NULLV;
  yis_objc_id path = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(url, OBJC_SEL("path"));
  if (!path) return YV_NULLV;
  const char *cp = OBJC_SEND(const char*, yis_objc_id, yis_objc_sel)(path, OBJC_SEL("UTF8String"));
  if (!cp || !cp[0]) return YV_NULLV;
  return YV_STR(stdr_str_from_slice(cp, strlen(cp)));
#elif defined(__linux__)
  char cmd[8192];
  if (ext && ext->data && ext->data[0]) {
    snprintf(cmd, sizeof(cmd),
             "zenity --file-selection --title=\"%s\" --file-filter=\"*.%s\" 2>/dev/null",
             prompt ? prompt->data : "Open", ext->data);
  } else {
    snprintf(cmd, sizeof(cmd),
             "zenity --file-selection --title=\"%s\" 2>/dev/null",
             prompt ? prompt->data : "Open");
  }
  return stdr_capture_shell_first_line(cmd);
#else
  (void)prompt;
  (void)ext;
  return YV_NULLV;
#endif
}

static YisVal stdr_open_folder_dialog(YisVal promptv) {
  if (promptv.tag != EVT_STR) yis_trap("open_folder_dialog expects prompt string");
  YisStr* prompt = (YisStr*)promptv.as.p;
#if defined(__APPLE__)
  if (!yis_objc_load()) return YV_NULLV;
  yis_objc_id cls = (yis_objc_id)OBJC_CLS("NSOpenPanel");
  if (!cls) return YV_NULLV;
  yis_objc_id panel = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(cls, OBJC_SEL("openPanel"));
  if (!panel) return YV_NULLV;
  OBJC_SEND(void, yis_objc_id, yis_objc_sel, yis_objc_bool)(panel, OBJC_SEL("setCanChooseDirectories:"), (yis_objc_bool)1);
  OBJC_SEND(void, yis_objc_id, yis_objc_sel, yis_objc_bool)(panel, OBJC_SEL("setCanChooseFiles:"), (yis_objc_bool)0);
  if (prompt && prompt->data && prompt->data[0]) {
    yis_objc_id ns = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, const char*)(
        (yis_objc_id)OBJC_CLS("NSString"), OBJC_SEL("stringWithUTF8String:"), prompt->data);
    OBJC_SEND(void, yis_objc_id, yis_objc_sel, yis_objc_id)(panel, OBJC_SEL("setMessage:"), ns);
  }
  long result = OBJC_SEND(long, yis_objc_id, yis_objc_sel)(panel, OBJC_SEL("runModal"));
  if (result != 1) return YV_NULLV;
  yis_objc_id urls = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(panel, OBJC_SEL("URLs"));
  yis_objc_id url = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(urls, OBJC_SEL("firstObject"));
  if (!url) return YV_NULLV;
  yis_objc_id path = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(url, OBJC_SEL("path"));
  if (!path) return YV_NULLV;
  const char *cp = OBJC_SEND(const char*, yis_objc_id, yis_objc_sel)(path, OBJC_SEL("UTF8String"));
  if (!cp || !cp[0]) return YV_NULLV;
  return YV_STR(stdr_str_from_slice(cp, strlen(cp)));
#elif defined(__linux__)
  char cmd[8192];
  snprintf(cmd, sizeof(cmd),
           "zenity --file-selection --directory --title=\"%s\" 2>/dev/null",
           prompt ? prompt->data : "Open Folder");
  return stdr_capture_shell_first_line(cmd);
#else
  (void)prompt;
  return YV_NULLV;
#endif
}

static YisVal stdr_save_file_dialog(YisVal promptv, YisVal default_namev, YisVal extv) {
  if (promptv.tag != EVT_STR) yis_trap("save_file_dialog expects prompt string");
  if (default_namev.tag != EVT_STR) yis_trap("save_file_dialog expects default_name string");
  if (extv.tag != EVT_STR) yis_trap("save_file_dialog expects extension string");
  YisStr* prompt = (YisStr*)promptv.as.p;
  YisStr* def = (YisStr*)default_namev.as.p;
  YisStr* ext = (YisStr*)extv.as.p;
#if defined(__APPLE__)
  if (!yis_objc_load()) return YV_NULLV;
  yis_objc_id cls = (yis_objc_id)OBJC_CLS("NSSavePanel");
  if (!cls) return YV_NULLV;
  yis_objc_id panel = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(cls, OBJC_SEL("savePanel"));
  if (!panel) return YV_NULLV;
  if (prompt && prompt->data && prompt->data[0]) {
    yis_objc_id ns = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, const char*)(
        (yis_objc_id)OBJC_CLS("NSString"), OBJC_SEL("stringWithUTF8String:"), prompt->data);
    OBJC_SEND(void, yis_objc_id, yis_objc_sel, yis_objc_id)(panel, OBJC_SEL("setMessage:"), ns);
  }
  if (def && def->data && def->data[0]) {
    yis_objc_id ns = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, const char*)(
        (yis_objc_id)OBJC_CLS("NSString"), OBJC_SEL("stringWithUTF8String:"), def->data);
    OBJC_SEND(void, yis_objc_id, yis_objc_sel, yis_objc_id)(panel, OBJC_SEL("setNameFieldStringValue:"), ns);
  }
  if (ext && ext->data && ext->data[0]) {
    yis_objc_id es = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, const char*)(
        (yis_objc_id)OBJC_CLS("NSString"), OBJC_SEL("stringWithUTF8String:"), ext->data);
    yis_objc_id UTType = (yis_objc_id)OBJC_CLS("UTType");
    if (UTType) {
      yis_objc_id ut = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, yis_objc_id)(UTType, OBJC_SEL("typeWithFilenameExtension:"), es);
      if (ut) {
        yis_objc_id arr = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel, yis_objc_id)((yis_objc_id)OBJC_CLS("NSArray"), OBJC_SEL("arrayWithObject:"), ut);
        OBJC_SEND(void, yis_objc_id, yis_objc_sel, yis_objc_id)(panel, OBJC_SEL("setAllowedContentTypes:"), arr);
      }
    }
  }
  long result = OBJC_SEND(long, yis_objc_id, yis_objc_sel)(panel, OBJC_SEL("runModal"));
  if (result != 1) return YV_NULLV;
  yis_objc_id url = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(panel, OBJC_SEL("URL"));
  if (!url) return YV_NULLV;
  yis_objc_id path = OBJC_SEND(yis_objc_id, yis_objc_id, yis_objc_sel)(url, OBJC_SEL("path"));
  if (!path) return YV_NULLV;
  const char *cp = OBJC_SEND(const char*, yis_objc_id, yis_objc_sel)(path, OBJC_SEL("UTF8String"));
  if (!cp || !cp[0]) return YV_NULLV;
  return YV_STR(stdr_str_from_slice(cp, strlen(cp)));
#elif defined(__linux__)
  char cmd[8192];
  snprintf(cmd, sizeof(cmd),
           "zenity --file-selection --save --confirm-overwrite --title=\"%s\" --filename=\"%s\" 2>/dev/null",
           prompt ? prompt->data : "Save", def ? def->data : "");
  (void)ext;
  return stdr_capture_shell_first_line(cmd);
#else
  (void)prompt;
  (void)def;
  (void)ext;
  return YV_NULLV;
#endif
}

static size_t stdr_find_sub(const char* s, size_t slen, const char* sub, size_t sublen, size_t start) {
  if (start > slen || sublen > slen - start) return (size_t)-1;
  if (sublen == 0) return start;
  for (size_t i = start; i <= slen - sublen; i++) {
    if (memcmp(s + i, sub, sublen) == 0) return i;
  }
  return (size_t)-1;
}

static void stdr_trim_span(const char* s, size_t len, size_t* out_start, size_t* out_len) {
  size_t a = 0;
  while (a < len && (s[a] == ' ' || s[a] == '\t')) a++;
  size_t b = len;
  while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) b--;
  *out_start = a;
  *out_len = b - a;
}

static YisStr* stdr_str_from_slice(const char* s, size_t len) {
  YisStr* st = (YisStr*)yis_alloc(yis_size_add(sizeof(YisStr), yis_size_add(len, 1)));
  if (!st) yis_trap("out of memory");
  st->ref = 1;
  st->len = len;
  st->data = (char*)(st + 1);
  if (len > 0) memcpy(st->data, s, len);
  st->data[len] = 0;
  return st;
}

static YisVal stdr_args(void) {
  YisArr* a = stdr_arr_new(yis_argc > 0 ? yis_argc : 1);
  for (int i = 0; i < yis_argc; i++) {
    const char* s = yis_argv && yis_argv[i] ? yis_argv[i] : "";
    size_t len = strlen(s);
    yis_arr_add(a, YV_STR(stdr_str_from_slice(s, len)));
  }
  return YV_ARR(a);
}

static int64_t stdr_parse_int_slice(const char* s, size_t len) {
  if (len == 0) return 0;
  char stack[64];
  char* tmp = (len < sizeof(stack)) ? stack : (char*)malloc(yis_size_add(len, 1));
  if (!tmp) yis_trap("out of memory");
  memcpy(tmp, s, len);
  tmp[len] = 0;
  char* end = NULL;
  long long v = strtoll(tmp, &end, 10);
  bool parsed = end != tmp;
  if (tmp != stack) free(tmp);
  if (!parsed) return 0;
  return (int64_t)v;
}

static double stdr_parse_float_slice(const char* s, size_t len) {
  if (len == 0) return 0.0;
  char stack[64];
  char* tmp = (len < sizeof(stack)) ? stack : (char*)malloc(yis_size_add(len, 1));
  if (!tmp) yis_trap("out of memory");
  memcpy(tmp, s, len);
  tmp[len] = 0;
  char* end = NULL;
  double v = strtod(tmp, &end);
  bool parsed = end != tmp;
  if (tmp != stack) free(tmp);
  if (!parsed) return 0.0;
  return v;
}

static bool stdr_parse_bool_slice(const char* s, size_t len) {
  if (len == 1) {
    if (s[0] == '1') return true;
    if (s[0] == '0') return false;
  }
  if (len == 4) {
    return ((s[0] == 't' || s[0] == 'T') &&
            (s[1] == 'r' || s[1] == 'R') &&
            (s[2] == 'u' || s[2] == 'U') &&
            (s[3] == 'e' || s[3] == 'E'));
  }
  return false;
}

static YisVal stdr_readf_parse(YisVal fmt, YisVal line, YisVal args) {
  if (fmt.tag != EVT_STR) yis_trap("readf expects string format");
  if (line.tag != EVT_STR) yis_trap("readf expects string input");
  if (args.tag != EVT_ARR) yis_trap("readf expects args tuple");

  YisStr* fs = (YisStr*)fmt.as.p;
  YisStr* ls = (YisStr*)line.as.p;
  YisArr* a = (YisArr*)args.as.p;

  const char* f = fs->data;
  size_t flen = fs->len;
  const char* s = ls->data;
  size_t slen = ls->len;

  int segs = 1;
  for (size_t i = 0; i + 1 < flen; i++) {
    if (f[i] == '{' && f[i + 1] == '}') {
      if (segs == INT_MAX) yis_trap("allocation size overflow");
      segs++;
      i++;
    }
  }

  const char* stack_ptrs[16];
  size_t stack_lens[16];
  const char** seg_ptrs = (segs <= 16) ? stack_ptrs : (const char**)malloc(yis_size_mul(sizeof(char*), (size_t)segs));
  size_t* seg_lens = (segs <= 16) ? stack_lens : (size_t*)malloc(yis_size_mul(sizeof(size_t), (size_t)segs));
  if (!seg_ptrs || !seg_lens) yis_trap("out of memory");

  size_t seg_start = 0;
  int seg_idx = 0;
  for (size_t i = 0; i + 1 < flen; i++) {
    if (f[i] == '{' && f[i + 1] == '}') {
      seg_ptrs[seg_idx] = f + seg_start;
      seg_lens[seg_idx] = i - seg_start;
      seg_idx++;
      i++;
      seg_start = i + 1;
    }
  }
  seg_ptrs[seg_idx] = f + seg_start;
  seg_lens[seg_idx] = flen - seg_start;

  int placeholders = segs - 1;

  size_t spos = 0;
  if (seg_lens[0] > 0) {
    size_t found = stdr_find_sub(s, slen, seg_ptrs[0], seg_lens[0], 0);
    if (found != (size_t)-1) spos = found + seg_lens[0];
  }

  YisArr* out = stdr_arr_new((int)a->len);

  for (size_t i = 0; i < a->len; i++) {
    size_t cap_start = spos;
    size_t cap_len = 0;
    if ((int)i < placeholders) {
      size_t found = stdr_find_sub(s, slen, seg_ptrs[i + 1], seg_lens[i + 1], spos);
      if (found == (size_t)-1) {
        cap_len = slen - spos;
        spos = slen;
      } else {
        cap_len = found - spos;
        spos = found + seg_lens[i + 1];
      }
    }

    size_t trim_start = 0;
    size_t trim_len = cap_len;
    stdr_trim_span(s + cap_start, cap_len, &trim_start, &trim_len);
    const char* cap = (cap_len > 0) ? (s + cap_start + trim_start) : "";

    YisVal hint = a->items[i];
    YisVal v;
    if (hint.tag == EVT_INT) {
      v = YV_INT(stdr_parse_int_slice(cap, trim_len));
    } else if (hint.tag == EVT_FLOAT) {
      v = YV_FLOAT(stdr_parse_float_slice(cap, trim_len));
    } else if (hint.tag == EVT_BOOL) {
      v = YV_BOOL(stdr_parse_bool_slice(cap, trim_len));
    } else if (hint.tag == EVT_STR) {
      v = YV_STR(stdr_str_from_slice(cap, trim_len));
    } else {
      v = YV_STR(stdr_str_from_slice(cap, trim_len));
    }
    yis_arr_add(out, v);
  }

  if (seg_ptrs != stack_ptrs) free(seg_ptrs);
  if (seg_lens != stack_lens) free(seg_lens);

  return YV_ARR(out);
}

static YisStr* stdr_to_string(YisVal v) {
  char buf[64];
  if (v.tag == EVT_NULL) return &yis_static_null;
  if (v.tag == EVT_BOOL) return v.as.b ? &yis_static_true : &yis_static_false;
  if (v.tag == EVT_INT) {
    snprintf(buf, sizeof(buf), "%lld", (long long)v.as.i);
    return stdr_str_lit(buf);
  }
  if (v.tag == EVT_FLOAT) {
    snprintf(buf, sizeof(buf), "%.6f", v.as.f);
    return stdr_str_lit(buf);
  }
  if (v.tag == EVT_STR) {
    yis_retain_val(v);
    return (YisStr*)v.as.p;
  }
  if (v.tag == EVT_ARR) return &yis_static_array;
  if (v.tag == EVT_DICT) return &yis_static_dict;
  if (v.tag == EVT_OBJ) return &yis_static_object;
  if (v.tag == EVT_FN) return &yis_static_function;
  return &yis_static_unknown;
}

static YisStr* stdr_str_from_parts(int n, YisVal* parts) {
  size_t total = 0;
  YisStr* stack_strs[16];
  YisStr** strs = (n <= 16) ? stack_strs : (YisStr**)yis_alloc(yis_size_mul(sizeof(YisStr*), (size_t)n));
  for (int i = 0; i < n; i++) {
    strs[i] = stdr_to_string(parts[i]);
    total = yis_size_add(total, strs[i]->len);
  }
  YisStr* out = (YisStr*)yis_alloc(yis_size_add(sizeof(YisStr), yis_size_add(total, 1)));
  out->ref = 1;
  out->len = total;
  out->data = (char*)(out + 1);
  size_t off = 0;
  for (int i = 0; i < n; i++) {
    memcpy(out->data + off, strs[i]->data, strs[i]->len);
    off += strs[i]->len;
    yis_release_val(YV_STR(strs[i]));
  }
  out->data[total] = 0;
  if (strs != stack_strs) free(strs);
  return out;
}

static void yis_retain_heap(YisVal v) {
  if (v.tag == EVT_STR) { int* r = &((YisStr*)v.as.p)->ref; if (*r != INT32_MAX) (*r)++; }
  else if (v.tag == EVT_ARR) ((YisArr*)v.as.p)->ref++;
  else if (v.tag == EVT_DICT) ((YisDict*)v.as.p)->ref++;
  else if (v.tag == EVT_OBJ) ((YisObj*)v.as.p)->ref++;
  else if (v.tag == EVT_FN) ((YisFn*)v.as.p)->ref++;
}

static void yis_release_heap(YisVal v) {
  if (v.tag == EVT_STR) {
    YisStr* s = (YisStr*)v.as.p;
    if (s->ref == INT32_MAX) return;
    if (--s->ref == 0) {
      if (s->data != (char*)(s + 1)) free(s->data);
      free(s);
    }
  } else if (v.tag == EVT_ARR) {
    YisArr* a = (YisArr*)v.as.p;
    if (--a->ref == 0) {
      for (size_t i = 0; i < a->len; i++) yis_release_val(a->items[i]);
      free(a->items);
      free(a);
    }
  } else if (v.tag == EVT_DICT) {
    YisDict* d = (YisDict*)v.as.p;
    if (--d->ref == 0) {
      for (size_t i = 0; i < d->len; i++) {
        yis_release_val(YV_STR(d->entries[i].key));
        yis_release_val(d->entries[i].val);
      }
      free(d->entries);
      free(d);
    }
  } else if (v.tag == EVT_OBJ) {
    YisObj* o = (YisObj*)v.as.p;
    if (--o->ref == 0) {
      if (o->drop) o->drop(o);
      free(o);
    }
  } else if (v.tag == EVT_FN) {
    YisFn* f = (YisFn*)v.as.p;
    if (--f->ref == 0) {
      if (f->env && f->env_size > 0) {
        YisVal* caps = (YisVal*)f->env;
        for (int i = 0; i < f->env_size; i++) yis_release_val(caps[i]);
        free(f->env);
      }
      free(f);
    }
  }
}

static void yis_release_slot(YisVal* slot) {
  yis_release_val(*slot);
}

static YisVal yis_move(YisVal* slot) {
  YisVal v = *slot;
  *slot = YV_NULLV;
  return v;
}

static void yis_move_into(YisVal* slot, YisVal v) {
  yis_retain_val(v);
  yis_release_val(*slot);
  *slot = v;
}

// The nearest double to INT64_MAX is +2^63, which is already out of range.
// Check exact powers of two before converting; C float-to-int overflow is UB.
static int64_t yis_float_to_int(double n) {
  if (!isfinite(n) || n < -0x1p63 || n >= 0x1p63)
    yis_trap("integer conversion out of range");
  return (int64_t)n;
}

static int64_t yis_as_int(YisVal v) {
  if (v.tag == EVT_INT) return v.as.i;
  if (v.tag == EVT_BOOL) return v.as.b ? 1 : 0;
  if (v.tag == EVT_FLOAT) return yis_float_to_int(v.as.f);
  if (v.tag == EVT_NULL) return 0;
  if (v.tag == EVT_STR) {
    /* Coerce string to int when possible; otherwise 0 (avoids trap until codegen is fully audited) */
    YisStr *s = (YisStr*)v.as.p;
    if (s && s->len > 0 && s->len < 32) {
      char buf[32];
      memcpy(buf, s->data, s->len);
      buf[s->len] = '\0';
      char *end = NULL;
      long x = strtol(buf, &end, 10);
      if (end && end == buf + (ptrdiff_t)s->len)
        return (int64_t)x;
    }
    return 0;
  }
  {
    const char *tag_name = "?";
    switch (v.tag) {
      case EVT_NULL: tag_name = "null"; break;
      case EVT_STR: tag_name = "string"; break;
      case EVT_ARR: tag_name = "array"; break;
      case EVT_DICT: tag_name = "dict"; break;
      case EVT_OBJ: tag_name = "object"; break;
      case EVT_FN: tag_name = "function"; break;
      default: tag_name = "unknown"; break;
    }
    char buf[80];
    snprintf(buf, sizeof(buf), "type mismatch: expected int (got %s)", tag_name);
    yis_trap(buf);
  }
  return 0;
}

static double yis_as_float(YisVal v) {
  if (v.tag == EVT_FLOAT) return v.as.f;
  if (v.tag == EVT_INT) return (double)v.as.i;
  if (v.tag == EVT_NULL) return 0.0;
  if (v.tag == EVT_BOOL) return v.as.b ? 1.0 : 0.0;
  if (v.tag == EVT_STR) {
    YisStr *s = (YisStr*)v.as.p;
    if (s && s->len > 0 && s->len < 32) {
      char buf[32];
      memcpy(buf, s->data, s->len);
      buf[s->len] = '\0';
      char *end = NULL;
      double x = strtod(buf, &end);
      if (end && end == buf + (ptrdiff_t)s->len)
        return x;
    }
    return 0.0;
  }
  {
    const char *tag_name = "?";
    switch (v.tag) {
      case EVT_NULL: tag_name = "null"; break;
      case EVT_ARR: tag_name = "array"; break;
      case EVT_DICT: tag_name = "dict"; break;
      case EVT_OBJ: tag_name = "object"; break;
      case EVT_FN: tag_name = "function"; break;
      default: tag_name = "unknown"; break;
    }
    char buf[80];
    snprintf(buf, sizeof(buf), "type mismatch: expected float (got %s)", tag_name);
    yis_trap(buf);
  }
  return 0.0;
}

static bool yis_as_bool(YisVal v) {
  if (v.tag == EVT_BOOL) return v.as.b;
  if (v.tag == EVT_NULL) return false;
  if (v.tag == EVT_INT) return v.as.i != 0;
  if (v.tag == EVT_FLOAT) return v.as.f != 0.0;
  if (v.tag == EVT_STR) return ((YisStr*)v.as.p)->len != 0;
  if (v.tag == EVT_ARR) return ((YisArr*)v.as.p)->len != 0;
  if (v.tag == EVT_DICT) return ((YisDict*)v.as.p)->len != 0;
  return true;
}

static YisVal yis_add(YisVal a, YisVal b) {
  if (a.tag == EVT_STR || b.tag == EVT_STR) return stdr_str_concat(a, b);
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(yis_as_float(a) + yis_as_float(b));
  int64_t out;
  if (__builtin_add_overflow(yis_as_int(a), yis_as_int(b), &out))
    yis_trap("integer overflow");
  return YV_INT(out);
}

static YisVal yis_sub(YisVal a, YisVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(yis_as_float(a) - yis_as_float(b));
  int64_t out;
  if (__builtin_sub_overflow(yis_as_int(a), yis_as_int(b), &out))
    yis_trap("integer overflow");
  return YV_INT(out);
}

static YisVal yis_mul(YisVal a, YisVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(yis_as_float(a) * yis_as_float(b));
  int64_t out;
  if (__builtin_mul_overflow(yis_as_int(a), yis_as_int(b), &out))
    yis_trap("integer overflow");
  return YV_INT(out);
}

static YisVal yis_div(YisVal a, YisVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(yis_as_float(a) / yis_as_float(b));
  int64_t lhs = yis_as_int(a);
  int64_t rhs = yis_as_int(b);
  if (rhs == 0) yis_trap("division by zero");
  if (lhs == INT64_MIN && rhs == -1) yis_trap("integer overflow");
  return YV_INT(lhs / rhs);
}

static YisVal yis_mod(YisVal a, YisVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) yis_trap("% expects integer");
  int64_t lhs = yis_as_int(a);
  int64_t rhs = yis_as_int(b);
  if (rhs == 0) yis_trap("division by zero");
  if (lhs == INT64_MIN && rhs == -1) yis_trap("integer overflow");
  return YV_INT(lhs % rhs);
}

static YisVal yis_neg(YisVal a) {
  if (a.tag == EVT_FLOAT) return YV_FLOAT(-a.as.f);
  int64_t n = yis_as_int(a);
  if (n == INT64_MIN) yis_trap("integer overflow");
  return YV_INT(-n);
}

static YisVal yis_eq(YisVal a, YisVal b) {
  if (a.tag != b.tag) return YV_BOOL(false);
  switch (a.tag) {
    case EVT_NULL: return YV_BOOL(true);
    case EVT_BOOL: return YV_BOOL(a.as.b == b.as.b);
    case EVT_INT: return YV_BOOL(a.as.i == b.as.i);
    case EVT_FLOAT: return YV_BOOL(a.as.f == b.as.f);
    case EVT_STR: {
      YisStr* sa = (YisStr*)a.as.p;
      YisStr* sb = (YisStr*)b.as.p;
      if (sa->len != sb->len) return YV_BOOL(false);
      return YV_BOOL(memcmp(sa->data, sb->data, sa->len) == 0);
    }
    default: return YV_BOOL(a.as.p == b.as.p);
  }
}

static YisVal yis_ne(YisVal a, YisVal b) {
  YisVal v = yis_eq(a, b);
  return YV_BOOL(!v.as.b);
}

static YisVal yis_lt(YisVal a, YisVal b) {
  if (a.tag == EVT_INT && b.tag == EVT_INT) return YV_BOOL(a.as.i < b.as.i);
  return YV_BOOL(yis_as_float(a) < yis_as_float(b));
}

static YisVal yis_le(YisVal a, YisVal b) {
  if (a.tag == EVT_INT && b.tag == EVT_INT) return YV_BOOL(a.as.i <= b.as.i);
  return YV_BOOL(yis_as_float(a) <= yis_as_float(b));
}

static YisVal yis_gt(YisVal a, YisVal b) {
  if (a.tag == EVT_INT && b.tag == EVT_INT) return YV_BOOL(a.as.i > b.as.i);
  return YV_BOOL(yis_as_float(a) > yis_as_float(b));
}

static YisVal yis_ge(YisVal a, YisVal b) {
  if (a.tag == EVT_INT && b.tag == EVT_INT) return YV_BOOL(a.as.i >= b.as.i);
  return YV_BOOL(yis_as_float(a) >= yis_as_float(b));
}

static YisArr* stdr_arr_new(int n) {
  YisArr* a = (YisArr*)yis_alloc(sizeof(YisArr));
  a->ref = 1;
  a->len = 0;
  a->cap = (n > 0) ? (size_t)n : 4;
  a->items = (YisVal*)yis_alloc(yis_size_mul(sizeof(YisVal), a->cap));
  return a;
}

static void yis_arr_add(YisArr* a, YisVal v) {
  if (a->len >= a->cap) {
    size_t cap = yis_grow_capacity(a->cap, yis_size_add(a->len, 1), sizeof(YisVal));
    a->items = (YisVal*)yis_realloc(a->items, yis_size_mul(sizeof(YisVal), cap));
    a->cap = cap;
  }
  a->items[a->len++] = v;
}

static void stdr_push(YisVal av, YisVal val) {
  if (av.tag != EVT_ARR) yis_trap("push expects array");
  YisArr* a = (YisArr*)av.as.p;
  yis_retain_val(val);
  yis_arr_add(a, val);
}

static YisVal stdr_join(YisVal av) {
  if (av.tag != EVT_ARR) yis_trap("join expects array");
  YisArr* a = (YisArr*)av.as.p;
  return YV_STR(stdr_str_from_parts((int)a->len, a->items));
}

static YisVal stdr_array_concat(YisVal av, YisVal bv) {
  if (av.tag != EVT_ARR || bv.tag != EVT_ARR) yis_trap("concat expects two arrays");
  YisArr* a = (YisArr*)av.as.p;
  YisArr* b = (YisArr*)bv.as.p;
  YisArr* out = stdr_arr_new((int)(a->len + b->len));
  for (size_t i = 0; i < a->len; i++) {
    yis_retain_val(a->items[i]);
    yis_arr_add(out, a->items[i]);
  }
  for (size_t i = 0; i < b->len; i++) {
    yis_retain_val(b->items[i]);
    yis_arr_add(out, b->items[i]);
  }
  return YV_ARR(out);
}

static YisVal yis_arr_get(YisArr* a, int64_t idx) {
  if (!a) yis_trap("array index on null");
  if (idx < 0 || (size_t)idx >= a->len) return YV_NULLV;
  YisVal v = a->items[idx];
  yis_retain_val(v);
  return v;
}

static void yis_arr_set(YisArr* a, int64_t idx, YisVal v) {
  if (!a) yis_trap("array index on null");
  if (idx < 0) return;
  if ((uint64_t)idx > SIZE_MAX - 1) yis_trap("allocation size overflow");
  size_t uidx = (size_t)idx;
  if (uidx >= a->len) {
    if (uidx >= a->cap) {
      size_t new_cap = yis_grow_capacity(a->cap, yis_size_add(uidx, 1), sizeof(YisVal));
      a->items = (YisVal*)yis_realloc(a->items, yis_size_mul(sizeof(YisVal), new_cap));
      a->cap = new_cap;
    }
    for (size_t i = a->len; i <= uidx; i++) a->items[i] = YV_NULLV;
    a->len = uidx + 1;
  }
  // The incoming value may be borrowed from the slot being overwritten.
  yis_retain_val(v);
  yis_release_val(a->items[uidx]);
  a->items[uidx] = v;
}

static YisVal yis_arr_remove(YisArr* a, int64_t idx) {
  if (idx < 0 || (size_t)idx >= a->len) return YV_NULLV;
  YisVal v = a->items[idx];
  for (size_t i = (size_t)idx; i + 1 < a->len; i++) {
    a->items[i] = a->items[i + 1];
  }
  a->len--;
  return v;
}

static int yis_str_cmp(YisStr* a, YisStr* b) {
  if (a->len != b->len) return (a->len > b->len) ? 1 : -1;
  return memcmp(a->data, b->data, a->len);
}

static YisDict* stdr_dict_new(void) {
  YisDict* d = (YisDict*)yis_alloc(sizeof(YisDict));
  d->ref = 1;
  d->len = 0;
  d->cap = 8;
  d->entries = (YisDictEnt*)yis_alloc(yis_size_mul(sizeof(YisDictEnt), d->cap));
  return d;
}

static void yis_dict_set(YisDict* d, YisVal key, YisVal val) {
  if (key.tag != EVT_STR) yis_trap("dict key must be string");
  YisStr* k = (YisStr*)key.as.p;
  for (size_t i = 0; i < d->len; i++) {
    if (yis_str_cmp(d->entries[i].key, k) == 0) {
      yis_retain_val(val);
      yis_release_val(d->entries[i].val);
      d->entries[i].val = val;
      return;
    }
  }
  if (d->len >= d->cap) {
    size_t cap = yis_grow_capacity(d->cap, yis_size_add(d->len, 1), sizeof(YisDictEnt));
    d->entries = (YisDictEnt*)yis_realloc(d->entries, yis_size_mul(sizeof(YisDictEnt), cap));
    d->cap = cap;
  }
  yis_retain_val(key);
  yis_retain_val(val);
  d->entries[d->len].key = k;
  d->entries[d->len].val = val;
  d->len++;
}

static YisVal yis_dict_get(YisDict* d, YisVal key) {
  if (!d) return YV_NULLV;
  if ((uintptr_t)d < 4096u) return YV_NULLV;
  if (key.tag != EVT_STR) return YV_NULLV;
  YisStr* k = (YisStr*)key.as.p;
  if (!k) return YV_NULLV;
  if ((uintptr_t)k < 4096u) return YV_NULLV;
  for (size_t i = 0; i < d->len; i++) {
    if (yis_str_cmp(d->entries[i].key, k) == 0) {
      YisVal v = d->entries[i].val;
      yis_retain_val(v);
      return v;
    }
  }
  return YV_NULLV;
}

static int yis_dict_len(YisDict* d) {
  return (int)d->len;
}

static YisObj* yis_obj_new(size_t size, void (*drop)(YisObj*)) {
  YisObj* o = (YisObj*)yis_alloc(size);
  o->ref = 1;
  o->drop = drop;
  return o;
}

static YisRef* yis_ref_new(void) {
  YisRef* r = (YisRef*)yis_alloc(sizeof(YisRef));
  if (!r) yis_trap("out of memory");
  r->ref = 1;
  r->val = YV_NULLV;
  return r;
}

static void yis_ref_retain(YisRef* r) {
  if (r) r->ref++;
}

static void yis_ref_release(YisRef* r) {
  if (!r) return;
  if (--r->ref == 0) {
    yis_release_val(r->val);
    free(r);
  }
}

static YisFn* yi_fn_new(YisVal (*fn)(void* env, int argc, YisVal* argv), int arity) {
  YisFn* f = (YisFn*)yis_alloc(sizeof(YisFn));
  f->ref = 1;
  f->arity = arity;
  f->fn = fn;
  f->env = NULL;
  f->env_size = 0;
  return f;
}

static YisFn* yi_fn_new_with_env(YisVal (*fn)(void* env, int argc, YisVal* argv), int arity, void* env, int env_size) {
  YisFn* f = (YisFn*)yis_alloc(sizeof(YisFn));
  f->ref = 1;
  f->arity = arity;
  f->fn = fn;
  f->env = env;
  f->env_size = env_size;
  return f;
}

static YisVal yis_call(YisVal fval, int argc, YisVal* argv) {
  if (fval.tag != EVT_FN) yis_trap("call expects function");
  YisFn* f = (YisFn*)fval.as.p;
  if (f->arity >= 0 && f->arity != argc) yis_trap("arity mismatch");
  return f->fn(f->env, argc, argv);
}

static YisVal stdr_parse_hex(YisVal sv) {
  if (sv.tag != EVT_STR) yis_trap("parse_hex expects string");
  YisStr* s = (YisStr*)sv.as.p;
  if (s->len == 0) return YV_INT(0);
  char* end = NULL;
  long long v = strtoll(s->data, &end, 16);
  if (end == s->data) return YV_INT(0);
  return YV_INT((int64_t)v);
}

static YisVal stdr_char_from_code(YisVal cv) {
  int64_t code = yis_as_int(cv);
  if (code < 0 || code > 0x10FFFF) return YV_STR(&yis_static_empty);
  char buf[4];
  int len = 0;
  if (code <= 0x7F) {
    buf[0] = (char)code; len = 1;
  } else if (code <= 0x7FF) {
    buf[0] = (char)(0xC0 | ((code >> 6) & 0x1F));
    buf[1] = (char)(0x80 | (code & 0x3F));
    len = 2;
  } else if (code <= 0xFFFF) {
    buf[0] = (char)(0xE0 | ((code >> 12) & 0x0F));
    buf[1] = (char)(0x80 | ((code >> 6) & 0x3F));
    buf[2] = (char)(0x80 | (code & 0x3F));
    len = 3;
  } else {
    buf[0] = (char)(0xF0 | ((code >> 18) & 0x07));
    buf[1] = (char)(0x80 | ((code >> 12) & 0x3F));
    buf[2] = (char)(0x80 | ((code >> 6) & 0x3F));
    buf[3] = (char)(0x80 | (code & 0x3F));
    len = 4;
  }
  return YV_STR(stdr_str_from_slice(buf, (size_t)len));
}

static YisVal stdr_floor(YisVal v) {
  if (v.tag != EVT_FLOAT) return YV_INT(yis_as_int(v));
  return YV_INT(yis_float_to_int(floor(v.as.f)));
}

static YisVal stdr_ceil(YisVal v) {
  if (v.tag != EVT_FLOAT) return YV_INT(yis_as_int(v));
  return YV_INT(yis_float_to_int(ceil(v.as.f)));
}

static YisVal stdr_keys(YisVal dv) {
  if (dv.tag != EVT_DICT) yis_trap("keys expects dict");
  YisDict* d = (YisDict*)dv.as.p;
  YisArr* out = stdr_arr_new((int)d->len);
  for (size_t i = 0; i < d->len; i++) {
    YisVal k = YV_STR(d->entries[i].key);
    yis_retain_val(k);
    yis_arr_add(out, k);
  }
  return YV_ARR(out);
}

static YisVal stdr_replace(YisVal textv, YisVal fromv, YisVal tov) {
  if (textv.tag != EVT_STR) yis_trap("replace expects string");
  if (fromv.tag != EVT_STR) yis_trap("replace expects string");
  if (tov.tag != EVT_STR) yis_trap("replace expects string");
  YisStr* text = (YisStr*)textv.as.p;
  YisStr* from = (YisStr*)fromv.as.p;
  YisStr* to = (YisStr*)tov.as.p;
  if (from->len == 0) { yis_retain_val(textv); return textv; }
  size_t cap = yis_size_add(text->len, 64);
  char* buf = (char*)malloc(cap);
  if (!buf) yis_trap("out of memory");
  size_t out_len = 0;
  size_t i = 0;
  while (from->len <= text->len - i) {
    if (memcmp(text->data + i, from->data, from->len) == 0) {
      size_t need = yis_size_add(out_len, to->len);
      if (need >= cap) {
        cap = yis_grow_capacity(cap, yis_size_add(need, 1), 1);
        buf = (char*)realloc(buf, cap);
        if (!buf) yis_trap("out of memory");
      }
      memcpy(buf + out_len, to->data, to->len);
      out_len += to->len;
      i += from->len;
    } else {
      if (yis_size_add(out_len, 1) >= cap) {
        cap = yis_grow_capacity(cap, yis_size_add(out_len, 2), 1);
        buf = (char*)realloc(buf, cap);
        if (!buf) yis_trap("out of memory");
      }
      buf[out_len++] = text->data[i++];
    }
  }
  size_t tail = text->len - i;
  if (tail > 0) {
    size_t need = yis_size_add(out_len, tail);
    if (need >= cap) {
      cap = yis_size_add(need, 1);
      buf = (char*)realloc(buf, cap);
      if (!buf) yis_trap("out of memory");
    }
    memcpy(buf + out_len, text->data + i, tail);
    out_len += tail;
  }
  buf[out_len] = 0;
  YisStr* result = stdr_str_from_slice(buf, out_len);
  free(buf);
  return YV_STR(result);
}

// ---- External module bindings ----
// Injected by codegen when the program imports an external module.
