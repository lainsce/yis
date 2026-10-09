"""Exercise bootstrap scope cleanup through real generated C programs."""

import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest


BOOTSTRAP = Path(sys.argv.pop(1)).resolve()
ROOT = Path(sys.argv.pop(1)).resolve()

# Count allocations in the generated program only. This detects skipped cleanup
# without requiring an optional sanitizer or measuring process memory usage.
ALLOCATION_TRACKER = r"""
#include <stdio.h>
#include <stdlib.h>
static long yis_test_allocations;
static void *yis_test_malloc(size_t size) {
  void *result = malloc(size);
  if (result) yis_test_allocations++;
  return result;
}
static void *yis_test_realloc(void *ptr, size_t size) {
  int new_allocation = ptr == NULL;
  void *result = realloc(ptr, size);
  if (new_allocation && result) yis_test_allocations++;
  return result;
}
static void yis_test_free(void *ptr) {
  if (ptr) yis_test_allocations--;
  free(ptr);
}
static void yis_test_check_allocations(void) {
  if (yis_test_allocations != 0) {
    fprintf(stderr, "unreleased allocations: %ld\n", yis_test_allocations);
    fflush(NULL);
    _Exit(99);
  }
}
#define malloc yis_test_malloc
#define realloc yis_test_realloc
#define free yis_test_free
"""


class BootstrapTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="yis-bootstrap-regression-")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        (self.work / "src").mkdir()
        self.entry = self.work / "src/init.yi"
        self.bootstrap = BOOTSTRAP
        self.env = os.environ.copy()
        self.env["YIS_STDLIB"] = str(ROOT / "src/stdlib")
        self.env["YIS_NO_CACHE"] = "1"
        self.env["YIS_KEEP_C"] = "1"
        self.env["YIS_CC_FLAGS"] = "-O0 -std=c11 -pipe"
        self.env["PATH"] = os.pathsep.join(
            path for path in self.env["PATH"].split(os.pathsep)
            if not path.endswith(".yis-tools/compat")
        )
        runtime = (ROOT / "src/bootstrap/runtime.inc").read_text()
        init = "static void yis_runtime_init(void) {"
        self.assertEqual(runtime.count(init), 1)
        marker = "static int yis_stdout_isatty = 0;"
        self.assertEqual(runtime.count(marker), 1)
        runtime = runtime.replace(marker, ALLOCATION_TRACKER + "\n" + marker, 1)
        runtime = runtime.replace(
            init, init + "\n  atexit(yis_test_check_allocations);", 1
        )
        runtime_path = self.work / "runtime.inc"
        runtime_path.write_text(runtime)
        self.env["YIS_RUNTIME"] = str(runtime_path)

    def run_body(self, body, expected="", declarations=""):
        self.entry.write_text("cask init\nbring stdr\n" + declarations + "\n-> ()\n" +
                              textwrap.indent(textwrap.dedent(body).strip(), "  ") + "\n;\n")
        result = self.invoke_bootstrap()
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual(result.stdout, expected)

    def invoke_bootstrap(self):
        return subprocess.run(
            [str(self.bootstrap), "run", str(self.entry), "--", str(self.entry)],
            cwd=self.work, env=self.env, text=True,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20,
        )

    def test_else_scope_restores_outer_shadowed_name(self):
        self.run_body("""
            let value = "outer"
            if false
              let value = "then"
              write(value)
            else
              let value = "else"
              write(value)
            write(value)
        """, "elseouter")

    def test_else_if_locals_are_released_within_each_branch(self):
        self.run_body("""
            if false
              let value = [1, 2]
              write(value[0])
            elif false
              let value = [3, 4]
              write(value[0])
            else
              let value = [5, 6]
              write(value[0])
        """, "5")

    def test_for_initializer_restores_outer_shadowed_name(self):
        self.run_body("""
            let value = "outer"
            for (let ?value = 0; value < 1; value += 1)
              write(value)
            write(value)
        """, "0outer")

    def test_break_releases_all_exited_loop_body_scopes(self):
        self.run_body("""
            for (let ?i = 0; i < 3; i += 1)
              let body = [i]
              if true
                let inner = [body[0], i]
                write(inner[0])
                break
            write("done")
        """, "0done")

    def test_continue_releases_body_scopes_and_runs_step(self):
        self.run_body("""
            for (let ?i = 0; i < 3; i += 1)
              let body = [i]
              if true
                let inner = [body[0], i]
                write(inner[0])
                continue
            write("done")
        """, "012done")

    def test_nested_loop_break_preserves_outer_loop_locals(self):
        self.run_body("""
            for (let ?i = 0; i < 2; i += 1)
              let outer = [i]
              for (let ?j = 0; j < 2; j += 1)
                let inner = [j]
                if true
                  let nested = [inner[0]]
                  break
              write(outer[0])
        """, "01")

    def test_foreach_binding_and_loop_cleanup(self):
        self.run_body("""
            let values = [1, 2, 3]
            for (value in values)
              let inner = [value]
              if value == 2
                continue
              write(inner[0])
              if value == 3
                break
        """, "13")

    def test_foreach_early_return_releases_iterable(self):
        self.run_body("write(first())", "one", declarations=textwrap.dedent("""
            : first() (( string ))
              for (value in ["one", "two"])
                let inner = [value]
                <- inner[0]
              <- "missing"
            ;
        """))

    def test_percent_literals_and_modulo_are_preserved(self):
        self.run_body("""
            write("% %% %s 100%")
            let value = 7
            write(value % 3)
        """, "% %% %s 100%1")

    def test_assignment_and_conditional_values_release_temporaries(self):
        self.run_body("""
            let ?text = "original"
            text = "changed"
            text = text
            text += "-text"
            let ?array = ["original"]
            array[0] = "changed-array"
            let ?dict = ["key" => "original"]
            dict["key"] = "changed-dict"
            let first = "left" ?? "unused"
            let second = null ?? "right"
            let branch = if true { first } else { "unused" }
            write(text)
            write(array[0])
            write(dict["key"])
            write(branch)
            write(second)
        """, "changed-textchanged-arraychanged-dictleftright")

    def test_constructor_fields_and_returned_values_release_temporaries(self):
        self.run_body("""
            let ?box = new Holder(text: "original")
            box.text = "changed"
            box.text += "-field"
            write(box.get())
        """, "changed-field", declarations=textwrap.dedent("""
            ,: Holder
              pub text = string
              :: get(this) (( string ))
                let local = this.text
                <- local
              ;
            ;
        """))

    def test_c_compilation_failure_exits_unsuccessfully(self):
        self.entry.write_text('cask init\nbring stdr\n-> ()\n  write("not run")\n;\n')
        runtime = Path(self.env["YIS_RUNTIME"])
        runtime.write_text(runtime.read_text() + '\n#error bootstrap_failure_test\n')
        result = self.invoke_bootstrap()
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("Compilation FAIL!", result.stdout)
        self.assertFalse((self.work / ".yi_run_out").exists())

    def enable_cache(self):
        self.env["YIS_NO_CACHE"] = "0"
        self.env["YIS_CACHE_DIR"] = str(self.work / "cache")

    def cached_executables(self):
        return list((self.work / "cache").glob("*/.yi_run_out"))

    def test_runtime_content_change_invalidates_cache_with_unchanged_timestamp(self):
        self.enable_cache()
        self.entry.write_text('cask init\nbring stdr\n-> ()\n  write("original")\n;\n')
        first = self.invoke_bootstrap()
        self.assertEqual((first.returncode, first.stdout), (0, "original"))
        self.assertEqual(len(self.cached_executables()), 1)
        original_binary = self.cached_executables()[0]
        compiled_at = original_binary.stat().st_mtime_ns
        cached = self.invoke_bootstrap()
        self.assertEqual((cached.returncode, cached.stdout), (0, "original"))
        self.assertEqual(original_binary.stat().st_mtime_ns, compiled_at)

        runtime = Path(self.env["YIS_RUNTIME"])
        original_stat = runtime.stat()
        original = runtime.read_text()
        function = "static YisStr* stdr_str_lit(const char* s) {"
        self.assertEqual(original.count(function), 1)
        runtime.write_text(original.replace(
            function, function + '\n  if (strcmp(s, "original") == 0) s = "modified";', 1
        ))
        os.utime(runtime, ns=(original_stat.st_atime_ns, original_stat.st_mtime_ns))
        changed = self.invoke_bootstrap()
        self.assertEqual((changed.returncode, changed.stdout), (0, "modified"))
        self.assertEqual(len(self.cached_executables()), 2)

    @unittest.skipUnless(sys.platform == "linux", "ELF permits harmless trailing marker data")
    def test_bootstrap_binary_content_change_invalidates_cache(self):
        self.enable_cache()
        self.bootstrap = self.work / "yis-bootstrap"
        shutil.copy2(BOOTSTRAP, self.bootstrap)
        with self.bootstrap.open("ab") as binary:
            binary.write(b"cache-marker-A")
        self.entry.write_text('cask init\nbring stdr\n-> ()\n  write("ok")\n;\n')
        first = self.invoke_bootstrap()
        self.assertEqual((first.returncode, first.stdout), (0, "ok"))
        self.assertEqual(len(self.cached_executables()), 1)
        original_stat = self.bootstrap.stat()
        with self.bootstrap.open("r+b") as binary:
            binary.seek(-1, os.SEEK_END)
            binary.write(b"B")
        os.utime(self.bootstrap, ns=(original_stat.st_atime_ns, original_stat.st_mtime_ns))
        changed = self.invoke_bootstrap()
        self.assertEqual((changed.returncode, changed.stdout), (0, "ok"))
        self.assertEqual(len(self.cached_executables()), 2)


if __name__ == "__main__":
    unittest.main()
