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
#else
int isatty(int);
int fileno(FILE*);
#endif

static int yis_stdout_isatty = 0;

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

struct YisVal {
  YisTag tag;
  union {
    int64_t i;
    double f;
    bool b;
    void* p;
  } as;
};

#define YV_NULLV ((YisVal){.tag=EVT_NULL})
#define YV_INT(x) ((YisVal){.tag=EVT_INT, .as.i=(int64_t)(x)})
#define YV_FLOAT(x) ((YisVal){.tag=EVT_FLOAT, .as.f=(double)(x)})
#define YV_BOOL(x) ((YisVal){.tag=EVT_BOOL, .as.b=(x)?true:false})
#define YV_STR(x) ((YisVal){.tag=EVT_STR, .as.p=(x)})
#define YV_ARR(x) ((YisVal){.tag=EVT_ARR, .as.p=(x)})
#define YV_OBJ(x) ((YisVal){.tag=EVT_OBJ, .as.p=(x)})
#define YV_FN(x) ((YisVal){.tag=EVT_FN, .as.p=(x)})

static void yis_trap(const char* msg) {
  fprintf(stderr, "runtime error: %s\n", msg ? msg : "unknown error");
  fprintf(stderr, "  (run with debugger for stack trace)\n");
  abort();
}

static void yis_retain_val(YisVal v);
static void yis_release_val(YisVal v);

static YisStr* stdr_str_lit(const char* s) {
  size_t n = strlen(s);
  YisStr* st = (YisStr*)malloc(sizeof(YisStr));
  st->ref = 1;
  st->len = n;
  st->data = (char*)malloc(n + 1);
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

static YisVal stdr_str_at(YisVal v, int64_t idx) {
  if (v.tag != EVT_STR) yis_trap("str_at expects string");
  YisStr* s = (YisStr*)v.as.p;
  if (idx < 0 || (size_t)idx >= s->len) return YV_STR(stdr_str_lit(""));
  return YV_STR(stdr_str_from_slice(s->data + idx, 1));
}

static int stdr_len(YisVal v) {
  if (v.tag == EVT_STR) return (int)((YisStr*)v.as.p)->len;
  if (v.tag == EVT_ARR) return (int)((YisArr*)v.as.p)->len;
  return 0;
}

static bool stdr_is_null(YisVal v) { return v.tag == EVT_NULL; }

static void stdr_write(YisVal v) {
  YisStr* s = stdr_to_string(v);
  fwrite(s->data, 1, s->len, stdout);
  if (yis_stdout_isatty) fflush(stdout);
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
  buf[len] = 0;
  YisStr* s = (YisStr*)malloc(sizeof(YisStr));
  if (!s) yis_trap("out of memory");
  s->ref = 1;
  s->len = len;
  s->data = buf;
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
  char* buf = (char*)malloc(len + 1);
  if (!buf) yis_trap("out of memory");
  size_t n = 0;
  if (len > 0) n = fread(buf, 1, len, f);
  fclose(f);
  if (n != len) {
    free(buf);
    return YV_NULLV;
  }
  buf[len] = 0;
  YisStr* out = stdr_str_from_slice(buf, len);
  free(buf);
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
  YisStr* st = (YisStr*)malloc(sizeof(YisStr));
  if (!st) yis_trap("out of memory");
  st->ref = 1;
  st->len = len;
  st->data = (char*)malloc(len + 1);
  if (!st->data) yis_trap("out of memory");
  if (len > 0) memcpy(st->data, s, len);
  st->data[len] = 0;
  return st;
}

static int64_t stdr_parse_int_slice(const char* s, size_t len) {
  if (len == 0) return 0;
  char* tmp = (char*)malloc(len + 1);
  if (!tmp) yis_trap("out of memory");
  memcpy(tmp, s, len);
  tmp[len] = 0;
  char* end = NULL;
  long long v = strtoll(tmp, &end, 10);
  free(tmp);
  if (end == tmp) return 0;
  return (int64_t)v;
}

static double stdr_parse_float_slice(const char* s, size_t len) {
  if (len == 0) return 0.0;
  char* tmp = (char*)malloc(len + 1);
  if (!tmp) yis_trap("out of memory");
  memcpy(tmp, s, len);
  tmp[len] = 0;
  char* end = NULL;
  double v = strtod(tmp, &end);
  free(tmp);
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

  const char** seg_ptrs = (const char**)malloc(sizeof(char*) * segs);
  size_t* seg_lens = (size_t*)malloc(sizeof(size_t) * segs);
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

  free(seg_ptrs);
  free(seg_lens);

  return YV_ARR(out);
}

static YisStr* stdr_to_string(YisVal v) {
  char buf[64];
  if (v.tag == EVT_NULL) return stdr_str_lit("null");
  if (v.tag == EVT_BOOL) return stdr_str_lit(v.as.b ? "true" : "false");
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
  if (v.tag == EVT_ARR) return stdr_str_lit("[array]");
  if (v.tag == EVT_OBJ) return stdr_str_lit("[object]");
  if (v.tag == EVT_FN) return stdr_str_lit("[function]");
  return stdr_str_lit("<?>");
}

static YisStr* stdr_str_from_parts(int n, YisVal* parts) {
  size_t total = 0;
  YisStr** strs = (YisStr**)malloc(sizeof(YisStr*) * (size_t)n);
  for (int i = 0; i < n; i++) {
    strs[i] = stdr_to_string(parts[i]);
    total += strs[i]->len;
  }
  YisStr* out = (YisStr*)malloc(sizeof(YisStr));
  out->ref = 1;
  out->len = total;
  out->data = (char*)malloc(total + 1);
  size_t off = 0;
  for (int i = 0; i < n; i++) {
    memcpy(out->data + off, strs[i]->data, strs[i]->len);
    off += strs[i]->len;
    yis_release_val(YV_STR(strs[i]));
  }
  out->data[total] = 0;
  free(strs);
  return out;
}

static void yis_retain_val(YisVal v) {
  if (v.tag == EVT_STR) ((YisStr*)v.as.p)->ref++;
  else if (v.tag == EVT_ARR) ((YisArr*)v.as.p)->ref++;
  else if (v.tag == EVT_OBJ) ((YisObj*)v.as.p)->ref++;
  else if (v.tag == EVT_FN) ((YisFn*)v.as.p)->ref++;
}

static void yis_release_val(YisVal v) {
  if (v.tag == EVT_STR) {
    YisStr* s = (YisStr*)v.as.p;
    if (--s->ref == 0) {
      free(s->data);
      free(s);
    }
  } else if (v.tag == EVT_ARR) {
    YisArr* a = (YisArr*)v.as.p;
    if (--a->ref == 0) {
      for (size_t i = 0; i < a->len; i++) yis_release_val(a->items[i]);
      free(a->items);
      free(a);
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
  return true;
}

static YisVal yis_add(YisVal a, YisVal b) {
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

static YisVal yis_arr_get(YisArr* a, int64_t idx) {
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
    for (size_t i = a->len; i < uidx; i++) a->items[i] = YV_NULLV;
    a->len = uidx + 1;
  }
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
