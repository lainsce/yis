"""Generated-program ownership and evaluation order, checked with sanitizers on Linux."""

import os
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest

COMPILER = Path(sys.argv.pop(1)).resolve()
ROOT = Path(sys.argv.pop(1)).resolve()


class CodegenMemoryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="yis-ownership-")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.env = os.environ.copy()
        self.env["YIS_STDLIB"] = str(ROOT / "src/stdlib")
        self.env["YIS_KEEP_C"] = "1"
        self.env["YIS_NO_CACHE"] = "1"
        self.env["YIS_DISABLE_BOOTSTRAP_FALLBACK"] = "1"
        self.env.pop("YIS_CC_FLAGS", None)
        self.env.pop("YIS_CACHE_DIR", None)

    def check_program(self, text, output, modules=None):
        for name, contents in (modules or {}).items():
            (self.work / name).write_text(contents)
        source = self.work / "program.yi"
        source.write_text("cask program\nbring stdr\n" + text)
        compile_result = subprocess.run(
            [str(COMPILER), str(source)], cwd=self.work, env=self.env,
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30,
        )
        self.assertEqual(compile_result.returncode, 0, compile_result.stdout)
        binary = self.work / "program"
        if sys.platform == "linux":
            # Compile emitted C directly: the compiler process itself is not the
            # subject of the sanitizer check. Non-PIE avoids ASan shadow collisions.
            candidates = list(self.work.glob(".yis-c-*")) + list(self.work.glob("*.c"))
            self.assertEqual(len(candidates), 1, list(self.work.iterdir()))
            binary = self.work / "program-sanitized"
            result = subprocess.run(
                [*shlex.split(self.env.get("CC", "cc")), "-x", "c", "-std=gnu11", "-g", "-O1",
                 "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                 "-fno-pie", "-no-pie", str(candidates[0]), "-lm", "-o", str(binary)],
                text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30,
            )
            self.assertEqual(result.returncode, 0, result.stdout)
        self.env["ASAN_OPTIONS"] = "detect_leaks=1:abort_on_error=1"
        self.env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
        result = subprocess.run([str(binary)], cwd=self.work, env=self.env,
                                text=True, stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE, timeout=10)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual(result.stdout, output)
        self.assertEqual(result.stderr, "")

    def test_function_locals_returns_and_parameter_rebinding(self):
        self.check_program('''
: borrow(value = string) (( string ))
  let local = value
  <- local
;
: fresh() (( string ))
  let scratch = ["temporary"]
  <- "fresh"
;
: rebind(value = string) (( string ))
  value = "changed"
  <- value
;
: fallthrough() (( -- ))
  let local = ["cleaned"]
  if true
    let nested = ["nested"]
  else
    let unused = "other"
;
-> ()
  let value = "original"
  fallthrough()
  write(borrow(value))
  write(rebind(value))
  write(value)
  write(fresh())
  write("\\n")
;
''', "originalchangedoriginalfresh\n")

    def test_assignment_and_nested_string_expressions(self):
        self.check_program('''
-> ()
  let ?text = ""
  for (let ?i = 0; i < 300; i += 1)
    text = stdr.str_concat(text, "abc")
  let combined = ("left" + "-") + "right"
  text = text
  write(len(text))
  write(combined)
  write("\\n")
;
''', "900left-right\n")

    def test_container_getters_and_assignment(self):
        self.check_program('''
-> ()
  let ?array = ["first", "second"]
  let ?dictionary = ["name" => "value"]
  let item = array[0]
  let field = dictionary.name
  array[1] = "replacement"
  dictionary.name = "updated"
  dictionary["other"] = "indexed"
  write(item)
  write(field)
  write(array[1])
  write(dictionary.name)
  write(dictionary["other"])
  write(["temporary"][0])
  write("\\n")
;
''', "firstvaluereplacementupdatedindexedtemporary\n")

    def test_loop_scopes_break_continue_and_return(self):
        self.check_program('''
: search() (( string ))
  for (item in ["one", "two", "three"])
    let nested = [item]
    if item == "one" { continue }
    <- item
  <- "missing"
;
-> ()
  for (let ?i = 0; i < 4; i += 1)
    let local = ["iteration"]
    if i == 1 { continue }
    if i == 2 { break }
    write(i)
  for (item in ["a", "b", "c"])
    let local = [item]
    if item == "a" { continue }
    write(item)
    break
  write(search())
  write("\\n")
;
''', "0btwo\n")

    def test_short_circuit_and_single_coalesce_evaluation(self):
        self.check_program('''
: mark(value = string) (( string ))
  write(value)
  <- value
;
: flag() (( bool ))
  write("BAD")
  <- true
;
-> ()
  write(false && flag())
  write(true || flag())
  let left = mark("L") ?? mark("BAD")
  let right = null ?? mark("R")
  let branch = if true { left } else { mark("BAD") }
  write(branch)
  write(right)
  write("\\n")
;
''', "falsetrueLRLR\n")

    def test_loop_condition_rechecks_fresh_call_arguments(self):
        self.check_program('''
: next(counter = any, suffix = string) (( bool ))
  counter[0] = counter[0] + 1
  <- counter[0] < 4
;
-> ()
  let counter = [0]
  for (; next(counter, stdr.str_concat("a", "b")); )
    let local = ["value"]
    write(counter[0])
  write("\\n")
;
''', "123\n")

    def test_match_interpolation_lambda_and_shadowed_locals(self):
        self.check_program('''
: select(value = string) (( string ))
  <- match value { "first" => value; item => item }
;
-> ()
  let value = "first"
  let callback = (suffix = string) => value + suffix
  if true
    let value = value + "-shadow"
    write(value)
  write(select(value))
  write(select("second"))
  write(callback("!"))
  write("[$$stdr.str_concat(value, \"!\")$$]\\n")
;
''', "first-shadowfirstsecondfirst![first!]\n")

    def test_class_field_ownership_and_method_rebinding(self):
        self.check_program('''
,: Box
  value = string
  :: get(this) (( string ))
    let local = this.value
    <- local
  ;
  :: change(this, value = string) (( -- ))
    this.value = value + "!"
  ;
;
-> ()
  let box = Box("original")
  box.change("changed")
  let field = box.value
  write(box.get())
  write(field)
  write(Box("temporary").value)
  write("\\n")
;
''', "changed!changed!temporary\n")

    def test_global_ownership_transitive_initialization_and_exit_order(self):
        self.check_program('''
bring middle
def ?global = middle.message()
<- ()
  write(global)
  write("\\n")
;
-> ()
  global = global + "!"
  write(global)
  write("\\n")
;
''', "leaf!\nleaf!\n", modules={
            "middle.yi": 'cask middle\nbring inner\n'
                         ':: message() (( string ))\n  <- inner.message()\n;\n',
            "inner.yi": 'cask inner\nbring leaf\n'
                        ':: message() (( string ))\n  <- leaf.message()\n;\n',
            "leaf.yi": 'cask leaf\ndef value = "leaf"\n'
                       ':: message() (( string ))\n  <- value\n;\n',
        })


if __name__ == "__main__":
    unittest.main()
