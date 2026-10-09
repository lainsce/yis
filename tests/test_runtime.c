/* Native runtime regressions, compiled against both runtime snapshots. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 700
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int allocations_left = -1;
static int short_write = 0;
static int files_closed = 0;

static int allocation_fails(void) {
  if (allocations_left < 0) return 0;
  if (allocations_left == 0) return 1;
  allocations_left--;
  return 0;
}

static void* test_malloc(size_t size) {
  return allocation_fails() ? NULL : malloc(size);
}

static void* test_realloc(void* ptr, size_t size) {
  return allocation_fails() ? NULL : realloc(ptr, size);
}

static size_t test_fwrite(const void* ptr, size_t size, size_t count, FILE* f) {
  if (short_write) return 0;
  return fwrite(ptr, size, count, f);
}

static int test_fclose(FILE* f) {
  files_closed++;
  return fclose(f);
}

#define malloc test_malloc
#define realloc test_realloc
#define fwrite test_fwrite
#define fclose test_fclose
#include YIS_RUNTIME_HEADER
#undef malloc
#undef realloc
#undef fwrite
#undef fclose

static int objects_dropped = 0;
static void drop_object(YisObj* obj) { (void)obj; objects_dropped++; }
static YisVal noop(void* env, int argc, YisVal* args) {
  (void)env; (void)argc; (void)args;
  return YV_NULLV;
}

static void test_self_assignment(void) {
  YisArr* a = stdr_arr_new(1);
  yis_arr_add(a, YV_STR(stdr_str_lit("array owns this string")));
  yis_arr_add(a, YV_ARR(stdr_arr_new(1)));
  yis_arr_add(a, YV_DICT(stdr_dict_new()));
  yis_arr_add(a, YV_OBJ(yis_obj_new(sizeof(YisObj), drop_object)));
  yis_arr_add(a, YV_FN(yi_fn_new(noop, 0)));
  for (size_t i = 0; i < a->len; i++) {
    /* Pass a borrowed value so the array is its sole owner at the call. */
    yis_arr_set(a, (int64_t)i, a->items[i]);
  }
  assert(strcmp(((YisStr*)a->items[0].as.p)->data, "array owns this string") == 0);
  yis_arr_set(a, 10, a->items[0]);
  assert(((YisStr*)a->items[0].as.p)->ref == 2);
  yis_release_val(YV_ARR(a));
  assert(objects_dropped == 1);
}

static void test_array_operations(void) {
  YisArr* a = stdr_arr_new(1);
  for (int i = 0; i < 20; i++) yis_arr_add(a, YV_INT(i));
  YisVal value = YV_STR(stdr_str_lit("sparse array entry"));
  yis_arr_set(a, 35, value);
  yis_release_val(value);
  assert(a->len == 36);
  for (size_t i = 20; i < 35; i++) assert(a->items[i].tag == EVT_NULL);
  value = yis_arr_get(a, 35);
  assert(strcmp(((YisStr*)value.as.p)->data, "sparse array entry") == 0);
  yis_release_val(value);
  yis_arr_set(a, -1, YV_INT(99));
  assert(a->len == 36);
  assert(yis_arr_get(a, -1).tag == EVT_NULL);
  assert(yis_arr_get(a, 36).tag == EVT_NULL);
  value = yis_arr_remove(a, 35);
  assert(a->len == 35);
  yis_release_val(value);
  value = yis_arr_remove(a, 2);
  assert(value.as.i == 2 && a->items[2].as.i == 3);
  yis_release_val(YV_ARR(a));
}

static void fill_dict(YisDict* d, int count) {
  for (int i = 0; i < count; i++) {
    char buf[24];
    snprintf(buf, sizeof(buf), "key%d", i);
    YisVal key = YV_STR(stdr_str_lit(buf));
    YisVal value = YV_STR(stdr_str_lit("dictionary entry"));
    yis_dict_set(d, key, value);
    yis_release_val(key);
    yis_release_val(value);
  }
}

static void test_dict_operations(void) {
  YisDict* d = stdr_dict_new();
  fill_dict(d, 20);
  for (size_t i = 0; i < d->len; i++)
    yis_dict_set(d, YV_STR(d->entries[i].key), d->entries[i].val);
  assert(d->len == 20);
  YisVal keys = stdr_keys(YV_DICT(d));
  assert(((YisArr*)keys.as.p)->len == 20);
  yis_release_val(keys);
  YisVal key = YV_STR(stdr_str_lit("key7"));
  YisVal value = yis_dict_get(d, key);
  assert(strcmp(((YisStr*)value.as.p)->data, "dictionary entry") == 0);
  yis_release_val(key);
  yis_release_val(value);
  yis_release_val(YV_DICT(d));
}

static void test_strings(void) {
  YisVal parts[3] = {YV_STR(stdr_str_lit("hello")), YV_INT(123), YV_BOOL(true)};
  YisStr* joined = stdr_str_from_parts(3, parts);
  assert(strcmp(joined->data, "hello123true") == 0);
  YisVal from = YV_STR(stdr_str_lit("123"));
  YisVal to = YV_STR(stdr_str_lit(" replacement "));
  YisVal replaced = stdr_replace(YV_STR(joined), from, to);
  assert(strcmp(((YisStr*)replaced.as.p)->data, "hello replacement true") == 0);
  yis_release_val(parts[0]);
  yis_release_val(YV_STR(joined));
  yis_release_val(from);
  yis_release_val(to);
  yis_release_val(replaced);
}

static void test_arithmetic(void) {
  assert(yis_add(YV_INT(INT64_MAX - 1), YV_INT(1)).as.i == INT64_MAX);
  assert(yis_add(YV_INT(INT64_MIN + 1), YV_INT(-1)).as.i == INT64_MIN);
  assert(yis_sub(YV_INT(INT64_MAX - 1), YV_INT(-1)).as.i == INT64_MAX);
  assert(yis_sub(YV_INT(INT64_MIN + 1), YV_INT(1)).as.i == INT64_MIN);
  assert(yis_mul(YV_INT(INT64_MAX), YV_INT(1)).as.i == INT64_MAX);
  assert(yis_mul(YV_INT(INT64_MIN), YV_INT(1)).as.i == INT64_MIN);
  assert(yis_mul(YV_INT(INT64_MIN / 2), YV_INT(2)).as.i == INT64_MIN);
  assert(yis_div(YV_INT(INT64_MIN), YV_INT(1)).as.i == INT64_MIN);
  assert(yis_div(YV_INT(-7), YV_INT(3)).as.i == -2);
  assert(yis_mod(YV_INT(-7), YV_INT(3)).as.i == -1);
  assert(yis_mod(YV_INT(INT64_MIN), YV_INT(1)).as.i == 0);
  assert(yis_neg(YV_INT(INT64_MIN + 1)).as.i == INT64_MAX);
  assert(yis_add(YV_FLOAT(1.25), YV_FLOAT(2.5)).as.f == 3.75);
  assert(isinf(yis_div(YV_FLOAT(1.0), YV_FLOAT(0.0)).as.f));
}

static void test_float_integer_conversion(void) {
  assert(yis_as_int(YV_FLOAT(1.9)) == 1);
  assert(yis_as_int(YV_FLOAT(-1.9)) == -1);
  assert(yis_as_int(YV_FLOAT(0.99)) == 0);
  assert(yis_as_int(YV_FLOAT(-0.99)) == 0);
  assert(yis_as_int(YV_FLOAT(-0.0)) == 0);
  assert(stdr_num(YV_FLOAT(-1.9)) == -1);
  assert(yis_as_int(YV_FLOAT(-0x1p63)) == INT64_MIN);
  double largest = nextafter(0x1p63, 0.0);
  assert(yis_as_int(YV_FLOAT(largest)) == INT64_MAX - 1023);
  assert(stdr_floor(YV_FLOAT(largest)).as.i == INT64_MAX - 1023);
  assert(stdr_ceil(YV_FLOAT(largest)).as.i == INT64_MAX - 1023);
  assert(stdr_floor(YV_FLOAT(-0x1p63)).as.i == INT64_MIN);
  assert(stdr_ceil(YV_FLOAT(-0x1p63)).as.i == INT64_MIN);
  assert(stdr_floor(YV_FLOAT(1.9)).as.i == 1);
  assert(stdr_floor(YV_FLOAT(-1.9)).as.i == -2);
  assert(stdr_ceil(YV_FLOAT(1.9)).as.i == 2);
  assert(stdr_ceil(YV_FLOAT(-1.9)).as.i == -1);
  int64_t integers[] = {INT64_MIN, INT64_MAX, INT64_C(9007199254740993)};
  for (int i = 0; i < 3; i++) {
    assert(stdr_floor(YV_INT(integers[i])).as.i == integers[i]);
    assert(stdr_ceil(YV_INT(integers[i])).as.i == integers[i]);
  }
}

static void test_invalid_float_conversion(const char* test) {
  const char* kind = strchr(test, '-');
  assert(kind != NULL);
  kind++;
  double value;
  if (strcmp(kind, "upper") == 0) value = 0x1p63;
  else if (strcmp(kind, "lower") == 0) value = nextafter(-0x1p63, -INFINITY);
  else if (strcmp(kind, "nan") == 0) value = NAN;
  else if (strcmp(kind, "positive-inf") == 0) value = INFINITY;
  else if (strcmp(kind, "negative-inf") == 0) value = -INFINITY;
  else { assert(0); return; }
  if (strncmp(test, "convert-", 8) == 0) yis_as_int(YV_FLOAT(value));
  else if (strncmp(test, "floor-", 6) == 0) stdr_floor(YV_FLOAT(value));
  else if (strncmp(test, "ceil-", 5) == 0) stdr_ceil(YV_FLOAT(value));
  else assert(0);
}

static void test_environment_and_hash(void) {
  YisVal name = YV_STR(stdr_str_lit("YIS_RUNTIME_TEST_VALUE"));
  YisVal value = stdr_getenv(name);
  assert(value.tag == EVT_STR);
  assert(strcmp(((YisStr*)value.as.p)->data, "test value with spaces") == 0);
  yis_release_val(name);
  yis_release_val(value);
  name = YV_STR(stdr_str_lit("YIS_RUNTIME_TEST_UNSET"));
  assert(stdr_getenv(name).tag == EVT_NULL);
  yis_release_val(name);
  const char* texts[] = {"", "a", "hello"};
  const char* hashes[] = {"cbf29ce484222325", "af63dc4c8601ec8c", "a430d84680aabd0b"};
  for (int i = 0; i < 3; i++) {
    value = YV_STR(stdr_str_lit(texts[i]));
    YisVal hashed = stdr_hash_text(value);
    assert(strcmp(((YisStr*)hashed.as.p)->data, hashes[i]) == 0);
    yis_release_val(value);
    yis_release_val(hashed);
  }
  value = YV_STR(stdr_str_from_slice("\0", 1));
  YisVal hashed = stdr_hash_text(value);
  assert(strcmp(((YisStr*)hashed.as.p)->data, "af63bd4c8601b7df") == 0);
  yis_release_val(value);
  yis_release_val(hashed);
}

static void test_temporary_files(const char* dir) {
  YisVal dirv = YV_STR(stdr_str_lit(dir));
  YisVal prefix = YV_STR(stdr_str_lit(".native-test-"));
  YisVal first = stdr_temp_file(dirv, prefix);
  YisVal second = stdr_temp_file(dirv, prefix);
  assert(first.tag == EVT_STR && second.tag == EVT_STR);
  assert(strcmp(((YisStr*)first.as.p)->data, ((YisStr*)second.as.p)->data) != 0);
  struct stat st;
  assert(stat(((YisStr*)first.as.p)->data, &st) == 0 && st.st_size == 0);
#if !defined(_WIN32)
  assert((st.st_mode & 0777) == 0600);
#endif
  assert(stdr_remove_file(first).as.b && stdr_remove_file(second).as.b);
  yis_release_val(prefix);
  prefix = YV_STR(stdr_str_lit("../escape"));
  assert(stdr_temp_file(dirv, prefix).tag == EVT_NULL);
  yis_release_val(prefix);
  yis_release_val(dirv);
  yis_release_val(first);
  yis_release_val(second);
}

static void test_canonical_paths(const char* dir) {
  YisVal dirv = YV_STR(stdr_str_lit(dir));
  YisVal canonical = stdr_realpath(dirv);
  assert(canonical.tag == EVT_STR);
  char* dot = stdr_join_path_c(dir, ".");
  YisVal dotv = YV_STR(stdr_str_lit(dot));
  YisVal same = stdr_realpath(dotv);
  assert(yis_eq(canonical, same).as.b);
  free(dot);
  yis_release_val(dotv);
  yis_release_val(same);
  char* alias = stdr_join_path_c(dir, "canonical-alias");
  YisVal aliasv = YV_STR(stdr_str_lit(alias));
#if !defined(_WIN32)
  assert(symlink(dir, alias) == 0);
  same = stdr_realpath(aliasv);
  assert(yis_eq(canonical, same).as.b);
  yis_release_val(same);
  assert(unlink(alias) == 0);
#endif
  assert(stdr_realpath(aliasv).tag == EVT_NULL);
  free(alias);
  yis_release_val(aliasv);
  yis_release_val(dirv);
  yis_release_val(canonical);
}

static void test_short_write_closes_file(const char* dir) {
  YisVal dirv = YV_STR(stdr_str_lit(dir));
  YisVal prefix = YV_STR(stdr_str_lit("short-write-"));
  YisVal path = stdr_temp_file(dirv, prefix);
  YisVal text = YV_STR(stdr_str_lit("file contents"));
  assert(path.tag == EVT_STR);
  files_closed = 0;
  short_write = 1;
  assert(!stdr_write_text_file(path, text).as.b);
  assert(files_closed == 1);
  short_write = 0;
  assert(stdr_remove_file(path).as.b);
  yis_release_val(dirv);
  yis_release_val(prefix);
  yis_release_val(path);
  yis_release_val(text);
}

int main(int argc, char** argv) {
  assert(argc >= 2);
  const char* test = argv[1];
  if (strcmp(test, "self-assignment") == 0) test_self_assignment();
  else if (strcmp(test, "array") == 0) test_array_operations();
  else if (strcmp(test, "dict") == 0) test_dict_operations();
  else if (strcmp(test, "strings") == 0) test_strings();
  else if (strcmp(test, "arithmetic") == 0) test_arithmetic();
  else if (strcmp(test, "float-conversion") == 0) test_float_integer_conversion();
  else if (strncmp(test, "convert-", 8) == 0 || strncmp(test, "floor-", 6) == 0 ||
           strncmp(test, "ceil-", 5) == 0) test_invalid_float_conversion(test);
  else if (strcmp(test, "environment-hash") == 0) test_environment_and_hash();
  else if (strcmp(test, "temp-file") == 0) { assert(argc == 3); test_temporary_files(argv[2]); }
  else if (strcmp(test, "canonical-path") == 0) { assert(argc == 3); test_canonical_paths(argv[2]); }
  else if (strcmp(test, "short-write") == 0) { assert(argc == 3); test_short_write_closes_file(argv[2]); }
  else if (strcmp(test, "array-overflow") == 0) yis_arr_set(stdr_arr_new(1), INT64_MAX, YV_INT(7));
  else if (strcmp(test, "string-overflow") == 0) stdr_str_from_slice("", SIZE_MAX);
  else if (strcmp(test, "add-positive-overflow") == 0) yis_add(YV_INT(INT64_MAX), YV_INT(1));
  else if (strcmp(test, "add-negative-overflow") == 0) yis_add(YV_INT(INT64_MIN), YV_INT(-1));
  else if (strcmp(test, "sub-positive-overflow") == 0) yis_sub(YV_INT(INT64_MAX), YV_INT(-1));
  else if (strcmp(test, "sub-negative-overflow") == 0) yis_sub(YV_INT(INT64_MIN), YV_INT(1));
  else if (strcmp(test, "mul-positive-overflow") == 0) yis_mul(YV_INT(INT64_MAX), YV_INT(2));
  else if (strcmp(test, "mul-negative-overflow") == 0) yis_mul(YV_INT(INT64_MIN), YV_INT(2));
  else if (strcmp(test, "div-overflow") == 0) yis_div(YV_INT(INT64_MIN), YV_INT(-1));
  else if (strcmp(test, "mod-overflow") == 0) yis_mod(YV_INT(INT64_MIN), YV_INT(-1));
  else if (strcmp(test, "neg-overflow") == 0) yis_neg(YV_INT(INT64_MIN));
  else if (strcmp(test, "div-zero") == 0) yis_div(YV_INT(1), YV_INT(0));
  else if (strcmp(test, "mod-zero") == 0) yis_mod(YV_INT(1), YV_INT(0));
  else if (strcmp(test, "prune-overflow") == 0) {
    assert(argc == 3);
    stdr_prune_files_older_than(YV_STR(stdr_str_lit(argv[2])), YV_INT(INT64_MAX));
  }
  else if (strcmp(test, "oom-array") == 0) { allocations_left = 0; stdr_arr_new(1); }
  else if (strcmp(test, "oom-array-items") == 0) { allocations_left = 1; stdr_arr_new(1); }
  else if (strcmp(test, "oom-string") == 0) { allocations_left = 0; stdr_str_lit("allocated string"); }
  else if (strcmp(test, "oom-concat") == 0) {
    YisVal parts[2] = {YV_INT(1), YV_INT(2)};
    allocations_left = 0;
    stdr_str_from_parts(2, parts);
  }
  else if (strcmp(test, "oom-dict") == 0) { allocations_left = 0; stdr_dict_new(); }
  else if (strcmp(test, "oom-function") == 0) { allocations_left = 0; yi_fn_new(noop, 0); }
  else if (strcmp(test, "oom-object") == 0) { allocations_left = 0; yis_obj_new(sizeof(YisObj), drop_object); }
  else if (strcmp(test, "oom-array-growth") == 0) {
    YisArr* a = stdr_arr_new(1);
    yis_arr_add(a, YV_INT(1));
    allocations_left = 0;
    yis_arr_add(a, YV_INT(2));
  }
  else if (strcmp(test, "oom-dict-growth") == 0) {
    YisDict* d = stdr_dict_new();
    fill_dict(d, 8);
    YisVal key = YV_STR(stdr_str_lit("new key"));
    allocations_left = 0;
    yis_dict_set(d, key, YV_INT(1));
  }
  else return 2;
  return 0;
}
