"""Compare real optimized and unoptimized programs against language results."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


COMPILER = Path(sys.argv.pop(1)).resolve()
ROOT = Path(sys.argv.pop(1)).resolve()


class OptimizerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="yis-optimizer-")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.env = os.environ.copy()
        self.env["YIS_STDLIB"] = str(ROOT / "src/stdlib")
        self.env["YIS_NO_CACHE"] = "1"
        self.env.pop("YIS_CC_FLAGS", None)
        self.env.pop("YIS_CACHE_DIR", None)

    def program(self, body, declarations=""):
        # Exercise a regular value-returning function as well as the entry.
        return ("cask program\nbring stdr\n" + declarations
                + ": exercise() (( num ))\n" + body + "  <- 0\n;\n"
                + "-> ()\n  exercise()\n;\n")

    def execute(self, source, mode):
        # Never let compile mode reuse a binary produced by run mode.
        work = self.work / mode
        work.mkdir()
        path = work / "program.yi"
        path.write_text(source)
        args = [str(COMPILER)]
        if mode == "run":
            args.append("run")
        args.append(str(path))
        compiled = subprocess.run(args, cwd=work, env=self.env, text=True,
                                  stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                  timeout=30)
        if mode == "run" or compiled.returncode:
            return compiled
        return subprocess.run([str(work / "program")], cwd=work, env=self.env,
                              text=True, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, timeout=5)

    def assert_program(self, source, output):
        for mode in ("run", "compile"):
            with self.subTest(mode=mode):
                result = self.execute(source, mode)
                self.assertEqual(result.returncode, 0, result.stdout)
                self.assertEqual(result.stdout, output)

    def assert_folded_main_unit(self):
        kept = list((self.work / "compile").glob(".yis-c-*"))
        self.assertEqual(len(kept), 1, kept)
        generated = kept[0].read_text()
        begin = generated.find("/* begin main unit */")
        end = generated.find("/* end main unit */", begin)
        self.assertTrue(begin >= 0 and end > begin, "generated main unit is missing")
        main = generated[begin:end]
        self.assertTrue("YV_INT(42)" in main, "folded integer value is missing")
        self.assertFalse("yis_add(" in main or "yis_mul(" in main,
                         "literal arithmetic was left for runtime evaluation")
        return main

    def test_multiply_by_zero_keeps_function_call(self):
        self.assert_program(self.program(
            '  write(bump() * 0)\n  write("\\n")\n',
            ': bump() (( num ))\n  write("called\\n")\n  <- 3\n;\n'),
            "called\n0\n")

    def test_identities_preserve_dynamic_value_types(self):
        self.assert_program(self.program(
            '  write("a" + 0)\n  write(0 + "a")\n  write("\\n")\n'
            '  write((true - 0) == 1)\n'
            '  write((1 * "12") == 12)\n'
            '  write((0 * 2.5) == 0.0)\n  write("\\n")\n'),
            "a00a\ntruetruetrue\n")

    def test_logical_identities_still_produce_booleans(self):
        self.assert_program(self.program(
            '  write(true && "yes")\n  write("yes" && true)\n'
            '  write(false || "yes")\n  write("yes" || false)\n'
            '  write(true && 7)\n  write("\\n")\n'),
            "truetruetruetruetrue\n")

    def test_logical_right_constant_preserves_left_effect(self):
        self.assert_program(self.program(
            '  write(mark() && false)\n  write("\\n")\n'
            '  write(mark() || true)\n  write("\\n")\n',
            ': mark() (( bool ))\n  write("called\\n")\n  <- true\n;\n'),
            "called\nfalse\ncalled\ntrue\n")

    def test_literal_container_queries_keep_element_calls(self):
        self.assert_program(self.program(
            '  write(stdr.len([bump()]))\n  write("\\n")\n'
            '  write(#[bump()])\n  write("\\n")\n'
            '  write(stdr.is_null([bump()]))\n  write("\\n")\n'
            '  write(stdr.is_null(["key" => bump()]))\n  write("\\n")\n',
            ': bump() (( num ))\n  write("called\\n")\n  <- 3\n;\n'),
            "called\n1\ncalled\n1\ncalled\nfalse\ncalled\nfalse\n")

    def test_foreach_variable_shadows_outer_constant(self):
        self.assert_program(self.program(
            '  let item = 9\n'
            '  for (item in [1, 2]) { write(item) }\n'
            '  write(item)\n  write("\\n")\n'), "129\n")

    def test_for_initializer_shadows_outer_constant(self):
        self.assert_program(self.program(
            '  let i = 9\n'
            '  for (let ?i = 0; i < 2; i += 1) { write(i) }\n'
            '  write(i)\n  write("\\n")\n'), "019\n")

    def test_constant_branches_keep_local_scope(self):
        self.assert_program(self.program(
            '  let x = 1\n'
            '  if true { let x = 2; write(x) }\n'
            '  if false { write("bad") } else { let x = 3; write(x) }\n'
            '  write(x)\n  write("\\n")\n'), "231\n")

    def test_false_loop_keeps_initializer_scope_and_effect(self):
        self.assert_program(self.program(
            '  let x = 1\n'
            '  for (let x = bump(); false; ) { write("bad") }\n'
            '  write(x)\n  write("\\n")\n',
            ': bump() (( num ))\n  write("called\\n")\n  <- 3\n;\n'),
            "called\n1\n")

    def test_assignment_invalidates_scalar_propagation(self):
        self.assert_program(self.program(
            '  let x = 1\n  x = 2\n  write(x)\n'
            '  if true { x = 3 }\n  write(x)\n'
            '  for (let ?i = 0; i < 2; i += 1) { x += 1 }\n'
            '  write(x)\n  write("\\n")\n'), "235\n")

    def test_statement_interpolation_keeps_calls(self):
        self.assert_program(self.program(
            '  "$$bump()$$"\n',
            ': bump() (( num ))\n  write("called\\n")\n  <- 3\n;\n'),
            "called\n")

    def test_statement_match_keeps_calls(self):
        self.assert_program(self.program(
            '  match 1 { 1 => bump(); _ => 0 }\n',
            ': bump() (( num ))\n  write("called\\n")\n  <- 3\n;\n'),
            "called\n")

    def test_statement_arithmetic_keeps_type_errors(self):
        source = self.program('  [] * 0\n')
        for mode in ("run", "compile"):
            with self.subTest(mode=mode):
                result = self.execute(source, mode)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("type mismatch", result.stdout)

    def test_dead_expression_does_not_hide_unknown_name(self):
        source = self.program('  unknown_variable\n')
        for mode in ("run", "compile"):
            with self.subTest(mode=mode):
                result = self.execute(source, mode)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("unknown_variable", result.stdout)

    def test_large_integer_folding_is_exact(self):
        self.assert_program(self.program(
            '  let low = 9007199254740992\n  let high = 9007199254740993\n'
            '  write(low < high)\n  write(high > low)\n'
            '  write(high <= low)\n  write(low >= high)\n  write("\\n")\n'
            '  write(9223372036854775806 + 1)\n  write("\\n")\n'
            '  write((-9223372036854775807 - 1) + 1)\n  write("\\n")\n'
            '  write(-9223372036854775807 - 1)\n  write("\\n")\n'
            '  write(-3 * -7)\n  write("\\n")\n'),
            "truetruefalsefalse\n9223372036854775807\n-9223372036854775807\n"
            "-9223372036854775808\n21\n")

    def test_safe_literal_folding_remains_enabled(self):
        self.env["YIS_KEEP_C"] = "1"
        source = self.program(
            '  let answer = 20 + 22\n  write(answer)\n  write("\\n")\n'
            '  if false { write("unreachable optimizer marker") }\n')
        result = self.execute(source, "compile")
        self.assertEqual((result.returncode, result.stdout), (0, "42\n"))
        main = self.assert_folded_main_unit()
        self.assertFalse("unreachable optimizer marker" in main,
                         "dead constant branch was retained")

    def test_entry_without_explicit_return_is_optimized(self):
        self.env["YIS_KEEP_C"] = "1"
        source = ('cask program\nbring stdr\n-> ()\n'
                  '  let answer = 20 + 22\n  write(answer)\n  write("\\n")\n;\n')
        result = self.execute(source, "compile")
        self.assertEqual((result.returncode, result.stdout), (0, "42\n"))
        self.assert_folded_main_unit()

    def test_macro_preserves_implicit_final_return(self):
        self.assert_program(self.program(
            '  write((0 !answer false))\n  write("\\n")\n'
            '  write((0 !answer true))\n  write("\\n")\n',
            '!: answer(flag = bool) (( num ))\n'
            '  if flag { <- 9 }\n  42\n;\n'), "42\n9\n")

    def test_void_macro_keeps_statement_body(self):
        self.assert_program(self.program(
            '  0 !show\n',
            '!: show() (( -- ))\n  write("called\\n")\n;\n'), "called\n")

    def assert_runtime_overflow(self, expression):
        source = self.program('  write(' + expression + ')\n')
        for mode in ("run", "compile"):
            with self.subTest(mode=mode):
                result = self.execute(source, mode)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("integer overflow", result.stdout)

    def test_add_overflow_is_a_runtime_error_in_both_modes(self):
        self.assert_runtime_overflow("9223372036854775807 + 1")

    def test_sub_overflow_is_a_runtime_error_in_both_modes(self):
        self.assert_runtime_overflow("(-9223372036854775807 - 1) - 1")

    def test_mul_overflow_is_a_runtime_error_in_both_modes(self):
        self.assert_runtime_overflow("9223372036854775807 * 2")

    def test_neg_overflow_is_a_runtime_error_in_both_modes(self):
        self.assert_runtime_overflow("-(-9223372036854775807 - 1)")


if __name__ == "__main__":
    unittest.main()
