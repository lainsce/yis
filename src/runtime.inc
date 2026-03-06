// ---- Yis runtime (minimal) ----
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
#if defined(__APPLE__)
#include <CoreGraphics/CoreGraphics.h>
#endif

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#include <unistd.h>
#endif

static int yis_stdout_isatty = 0;

static int yis_argc = 0;
static char **yis_argv = NULL;

void yis_set_args(int argc, char **argv) {
  yis_argc = argc;
  yis_argv = argv;
}

#define COGITO_DEFINED_COGITO_DEBUG_ENABLED
static bool cogito_debug_enabled(void) { return false; }
static const char* cogito_font_path_active = NULL;

static void yis_runtime_init(void) {
#if defined(__APPLE__)
  if (cogito_debug_enabled()) {
    fprintf(stderr, "cogito: runtime_init\n");
    fflush(stderr);
  }
#endif
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

static void yis_retain_val(YisVal v);
static void yis_release_val(YisVal v);
static int64_t yis_as_int(YisVal v);

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

static YisStr* stdr_str_lit(const char* s) {
  size_t n = strlen(s);
  YisStr* st = (YisStr*)malloc(sizeof(YisStr) + n + 1);
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
  return YV_STR(stdr_str_from_slice(s->data + idx, 1));
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
      cap *= 2;
      buf = (char*)realloc(buf, cap);
      if (!buf) yis_trap("out of memory");
    }
    buf[len++] = (char)c;
  }
  if (len > 0 && buf[len - 1] == '\r') len--;
  YisStr* s = (YisStr*)malloc(sizeof(YisStr) + len + 1);
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
  YisStr* out = (YisStr*)malloc(sizeof(YisStr) + len + 1);
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
  bool ok = (n == text->len) && (fclose(f) == 0);
  return YV_BOOL(ok);
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
      cap *= 2;
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
#endif
  YisStr* out_str = stdr_str_from_slice(buf, len);
  free(buf);
  YisArr* out = stdr_arr_new(2);
  yis_arr_add(out, YV_INT(status));
  yis_arr_add(out, YV_STR(out_str));
  return YV_ARR(out);
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

static YisVal stdr_getcwd(void) {
  char buf[4096];
  if (getcwd(buf, sizeof(buf))) return YV_STR(stdr_str_from_slice(buf, strlen(buf)));
  return YV_STR(stdr_str_from_slice("", 0));
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
  char cmd[8192];
  snprintf(cmd, sizeof(cmd),
           "osascript -e 'set _p to POSIX path of (choose file of type {\"%s\"} with prompt \"%s\")' -e 'return _p' 2>/dev/null",
           ext ? ext->data : "", prompt ? prompt->data : "");
  return stdr_capture_shell_first_line(cmd);
#else
  (void)prompt;
  (void)ext;
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
  char cmd[8192];
  snprintf(cmd, sizeof(cmd),
           "osascript -e 'set _p to POSIX path of (choose file name with prompt \"%s\" default name \"%s\")' -e 'return _p' 2>/dev/null",
           prompt ? prompt->data : "", def ? def->data : "");
  YisVal out = stdr_capture_shell_first_line(cmd);
  (void)ext;
  return out;
#else
  (void)prompt;
  (void)def;
  (void)ext;
  return YV_NULLV;
#endif
}

static size_t stdr_find_sub(const char* s, size_t slen, const char* sub, size_t sublen, size_t start) {
  if (sublen == 0) return start;
  if (start > slen) return (size_t)-1;
  for (size_t i = start; i + sublen <= slen; i++) {
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
  YisStr* st = (YisStr*)malloc(sizeof(YisStr) + len + 1);
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
  char* tmp = (len < sizeof(stack)) ? stack : (char*)malloc(len + 1);
  if (!tmp) yis_trap("out of memory");
  memcpy(tmp, s, len);
  tmp[len] = 0;
  char* end = NULL;
  long long v = strtoll(tmp, &end, 10);
  if (tmp != stack) free(tmp);
  if (end == tmp) return 0;
  return (int64_t)v;
}

static double stdr_parse_float_slice(const char* s, size_t len) {
  if (len == 0) return 0.0;
  char stack[64];
  char* tmp = (len < sizeof(stack)) ? stack : (char*)malloc(len + 1);
  if (!tmp) yis_trap("out of memory");
  memcpy(tmp, s, len);
  tmp[len] = 0;
  char* end = NULL;
  double v = strtod(tmp, &end);
  if (tmp != stack) free(tmp);
  if (end == tmp) return 0.0;
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
      segs++;
      i++;
    }
  }

  const char* stack_ptrs[16];
  size_t stack_lens[16];
  const char** seg_ptrs = (segs <= 16) ? stack_ptrs : (const char**)malloc(sizeof(char*) * segs);
  size_t* seg_lens = (segs <= 16) ? stack_lens : (size_t*)malloc(sizeof(size_t) * segs);
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
  YisStr** strs = (n <= 16) ? stack_strs : (YisStr**)malloc(sizeof(YisStr*) * (size_t)n);
  for (int i = 0; i < n; i++) {
    strs[i] = stdr_to_string(parts[i]);
    total += strs[i]->len;
  }
  YisStr* out = (YisStr*)malloc(sizeof(YisStr) + total + 1);
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

static void yis_retain_val(YisVal v) {
  if (v.tag == EVT_STR) { int* r = &((YisStr*)v.as.p)->ref; if (*r != INT32_MAX) (*r)++; }
  else if (v.tag == EVT_ARR) ((YisArr*)v.as.p)->ref++;
  else if (v.tag == EVT_DICT) ((YisDict*)v.as.p)->ref++;
  else if (v.tag == EVT_OBJ) ((YisObj*)v.as.p)->ref++;
  else if (v.tag == EVT_FN) ((YisFn*)v.as.p)->ref++;
}

static void yis_release_val(YisVal v) {
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

static YisVal yis_move(YisVal* slot) {
  YisVal v = *slot;
  *slot = YV_NULLV;
  return v;
}

static void yis_move_into(YisVal* slot, YisVal v) {
  yis_release_val(*slot);
  *slot = v;
}

static int64_t yis_as_int(YisVal v) {
  if (v.tag == EVT_INT) return v.as.i;
  if (v.tag == EVT_BOOL) return v.as.b ? 1 : 0;
  if (v.tag == EVT_FLOAT) return (int64_t)v.as.f;
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
  yis_trap("type mismatch: expected float");
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
  return YV_INT(yis_as_int(a) + yis_as_int(b));
}

static YisVal yis_sub(YisVal a, YisVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(yis_as_float(a) - yis_as_float(b));
  return YV_INT(yis_as_int(a) - yis_as_int(b));
}

static YisVal yis_mul(YisVal a, YisVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(yis_as_float(a) * yis_as_float(b));
  return YV_INT(yis_as_int(a) * yis_as_int(b));
}

static YisVal yis_div(YisVal a, YisVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) return YV_FLOAT(yis_as_float(a) / yis_as_float(b));
  return YV_INT(yis_as_int(a) / yis_as_int(b));
}

static YisVal yis_mod(YisVal a, YisVal b) {
  if (a.tag == EVT_FLOAT || b.tag == EVT_FLOAT) yis_trap("% expects integer");
  return YV_INT(yis_as_int(a) % yis_as_int(b));
}

static YisVal yis_neg(YisVal a) {
  if (a.tag == EVT_FLOAT) return YV_FLOAT(-a.as.f);
  return YV_INT(-yis_as_int(a));
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

static YisVal yis_lt(YisVal a, YisVal b) { return YV_BOOL(yis_as_float(a) < yis_as_float(b)); }
static YisVal yis_le(YisVal a, YisVal b) { return YV_BOOL(yis_as_float(a) <= yis_as_float(b)); }
static YisVal yis_gt(YisVal a, YisVal b) { return YV_BOOL(yis_as_float(a) > yis_as_float(b)); }
static YisVal yis_ge(YisVal a, YisVal b) { return YV_BOOL(yis_as_float(a) >= yis_as_float(b)); }

static YisArr* stdr_arr_new(int n) {
  YisArr* a = (YisArr*)malloc(sizeof(YisArr));
  a->ref = 1;
  a->len = 0;
  a->cap = (n > 0) ? (size_t)n : 4;
  a->items = (YisVal*)malloc(sizeof(YisVal) * a->cap);
  return a;
}

static void yis_arr_add(YisArr* a, YisVal v) {
  if (a->len >= a->cap) {
    a->cap *= 2;
    a->items = (YisVal*)realloc(a->items, sizeof(YisVal) * a->cap);
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
  if (idx < 0) return;
  size_t uidx = (size_t)idx;
  if (uidx >= a->len) {
    if (uidx >= a->cap) {
      size_t new_cap = a->cap ? a->cap : 4;
      while (new_cap <= uidx) new_cap *= 2;
      a->items = (YisVal*)realloc(a->items, sizeof(YisVal) * new_cap);
      a->cap = new_cap;
    }
    for (size_t i = a->len; i <= uidx; i++) a->items[i] = YV_NULLV;
    a->len = uidx + 1;
  } else {
    yis_release_val(a->items[uidx]);
  }
  yis_retain_val(v);
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
  YisDict* d = (YisDict*)malloc(sizeof(YisDict));
  d->ref = 1;
  d->len = 0;
  d->cap = 8;
  d->entries = (YisDictEnt*)malloc(sizeof(YisDictEnt) * d->cap);
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
    d->cap *= 2;
    d->entries = (YisDictEnt*)realloc(d->entries, sizeof(YisDictEnt) * d->cap);
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
  YisObj* o = (YisObj*)malloc(size);
  o->ref = 1;
  o->drop = drop;
  return o;
}

static YisFn* yi_fn_new(YisVal (*fn)(void* env, int argc, YisVal* argv), int arity) {
  YisFn* f = (YisFn*)malloc(sizeof(YisFn));
  f->ref = 1;
  f->arity = arity;
  f->fn = fn;
  f->env = NULL;
  f->env_size = 0;
  return f;
}

static YisFn* yi_fn_new_with_env(YisVal (*fn)(void* env, int argc, YisVal* argv), int arity, void* env, int env_size) {
  YisFn* f = (YisFn*)malloc(sizeof(YisFn));
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

// ---- Cogito GUI (shared library bindings) ----
// Injected by codegen when the program imports `cogito`.
