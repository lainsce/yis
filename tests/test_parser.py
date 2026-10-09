"""Malformed source must fail before producing or running a program."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


COMPILER = Path(sys.argv.pop(1)).resolve()
ROOT = Path(sys.argv.pop(1)).resolve()


class ParserTests(unittest.TestCase):
    def test_rejects_invalid_declarations_and_delimiters(self):
        cases = {
            "unknown_declaration": 'cask sample\nunsupported statement\n',
            "extra_close": 'cask sample\n)\n',
            "mismatched_close": 'cask sample\nbring stdr\n-> ()\n  write([1, 2))\n;\n',
            "unclosed_call": 'cask sample\nbring stdr\n-> ()\n  write(("must not run")\n;\n',
            "unclosed_array": 'cask sample\nbring stdr\n-> ()\n  let value = [1, 2\n;\n',
            "unclosed_block": 'cask sample\nbring stdr\n-> () { write("must not run")\n',
        }
        for name, source in cases.items():
            with self.subTest(case=name), tempfile.TemporaryDirectory(prefix="yis-parser-") as directory:
                work = Path(directory)
                path = work / "sample.yi"
                path.write_text(source)
                environment = os.environ.copy()
                environment["YIS_STDLIB"] = str(ROOT / "src/stdlib")
                environment["YIS_CACHE_DIR"] = str(work / "cache")
                environment["YIS_NO_CACHE"] = "1"
                result = subprocess.run([str(COMPILER), "run", str(path)], cwd=work,
                                        env=environment, text=True, capture_output=True, timeout=10)
                self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertIn("error: line", result.stdout + result.stderr)
                self.assertNotIn("must not run", result.stdout + result.stderr)
                self.assertFalse((work / "sample").exists())


class LiteralTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="yis-literals-")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.invocations = 0
        self.env = os.environ.copy()
        self.env["YIS_STDLIB"] = str(ROOT / "src/stdlib")
        self.env["YIS_NO_CACHE"] = "1"
        self.env.pop("YIS_CC_FLAGS", None)
        self.env.pop("YIS_CACHE_DIR", None)

    def invoke(self, source, mode, modules=None):
        directory = self.work / f"{mode}-{self.invocations}"
        self.invocations += 1
        directory.mkdir()
        path = directory / "sample.yi"
        path.write_text(source)
        for name, contents in (modules or {}).items():
            (directory / name).write_text(contents)
        args = [str(COMPILER)]
        if mode == "run":
            args.append("run")
        args.append(str(path))
        result = subprocess.run(args, cwd=directory, env=self.env, text=True,
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                timeout=30)
        if mode == "compile" and result.returncode == 0:
            result = subprocess.run([str(directory / "sample")], cwd=directory,
                                    env=self.env, text=True, stdout=subprocess.PIPE,
                                    stderr=subprocess.STDOUT, timeout=5)
        return result, directory

    def program(self, expression):
        return ('cask sample\nbring stdr\n-> ()\n  write(' + expression
                + ')\n  write("\\n")\n;\n')

    def assert_outputs(self, source, expected):
        for mode in ("run", "compile"):
            with self.subTest(mode=mode):
                result, _ = self.invoke(source, mode)
                self.assertEqual(result.returncode, 0, result.stdout)
                self.assertEqual(result.stdout, expected)

    def assert_invalid_literal(self, literal):
        for mode in ("run", "compile"):
            with self.subTest(mode=mode):
                result, directory = self.invoke(self.program(literal), mode)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("error: line 4, column", result.stdout)
                self.assertIn("integer literal", result.stdout)
                self.assertFalse((directory / "sample").exists())

    def test_exact_signed_integer_limits_and_leading_zeroes(self):
        source = ('cask sample\nbring stdr\n-> ()\n'
                  '  write(-9223372036854775808)\n  write("\\n")\n'
                  '  write(9223372036854775807)\n  write("\\n")\n'
                  '  write(-0x8000000000000000)\n  write("\\n")\n'
                  '  write(0X7fffffffffffffff)\n  write("\\n")\n'
                  '  write(010)\n  write("\\n")\n'
                  '  write(00009223372036854775807)\n  write("\\n")\n;\n')
        self.assert_outputs(source, "-9223372036854775808\n9223372036854775807\n"
                            "-9223372036854775808\n9223372036854775807\n"
                            "10\n9223372036854775807\n")

    def test_positive_minimum_magnitude_is_rejected(self):
        self.assert_invalid_literal("9223372036854775808")

    def test_large_positive_decimal_is_rejected(self):
        self.assert_invalid_literal("9999999999999999999999999999999999999999")

    def test_below_integer_minimum_is_rejected(self):
        self.assert_invalid_literal("-9223372036854775809")

    def test_positive_hexadecimal_minimum_magnitude_is_rejected(self):
        self.assert_invalid_literal("0x8000000000000000")

    def test_hexadecimal_overflow_is_rejected(self):
        self.assert_invalid_literal("0xffffffffffffffff")

    def test_hexadecimal_digits_are_required_and_valid(self):
        for literal in ("0x", "0x1g", "-0X"):
            for mode in ("run", "compile"):
                with self.subTest(literal=literal, mode=mode):
                    result, directory = self.invoke(self.program(literal), mode)
                    self.assertNotEqual(result.returncode, 0, result.stdout)
                    self.assertIn("error: line 4, column", result.stdout)
                    self.assertIn("hexadecimal literal", result.stdout)
                    self.assertFalse((directory / "sample").exists())

    def test_binary_subtraction_cannot_hide_out_of_range_operand(self):
        self.assert_invalid_literal("0 - 9223372036854775808")

    def test_negative_integer_match_patterns(self):
        self.assert_outputs(self.program(
            'match -9223372036854775808 { '
            '-9223372036854775808 => "minimum"; _ => "wrong" }'), "minimum\n")

    def test_out_of_range_import_prevents_compilation(self):
        source = ('cask sample\nbring stdr\nbring helper\n-> ()\n'
                  '  write("must not run\\n")\n;\n')
        module = ('cask helper\n:: value() (( num ))\n'
                  '  <- 9223372036854775808\n;\n')
        for mode in ("run", "compile"):
            with self.subTest(mode=mode):
                result, directory = self.invoke(source, mode, {"helper.yi": module})
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("error: line 3, column 6", result.stdout)
                self.assertIn("integer literal", result.stdout)
                self.assertNotIn("must not run", result.stdout)
                self.assertFalse((directory / "sample").exists())

    def test_interpolation_text_is_not_parsed_as_an_integer(self):
        digits = "9999999999999999999999999999999999999999"
        self.assert_outputs(self.program('"' + digits + ' $$1$$"'), digits + " 1\n")

    def test_interpolated_out_of_range_literal_is_rejected(self):
        source = self.program('"$$9223372036854775808$$"')
        for mode in ("run", "compile"):
            with self.subTest(mode=mode):
                result, directory = self.invoke(source, mode)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("error: line", result.stdout)
                self.assertIn("integer literal", result.stdout)
                self.assertFalse((directory / "sample").exists())

    def test_nonempty_dictionary_and_nested_values(self):
        source = ('cask sample\nbring stdr\n-> ()\n'
                  '  let data = ["name" => "value", "items" => [1, 2],]\n'
                  '  write(stdr.len(data))\n  write("\\n")\n'
                  '  write(data["name"])\n  write("\\n")\n'
                  '  write(data["items"][1])\n  write("\\n")\n'
                  '  write(stdr.len([1, 2,]))\n  write("\\n")\n;\n')
        self.assert_outputs(source, "2\nvalue\n2\n2\n")

    def test_mixed_array_dictionary_literals_are_rejected(self):
        for literal in ('[1, "key" => 2]', '["key" => 2, 1]'):
            for mode in ("run", "compile"):
                with self.subTest(literal=literal, mode=mode):
                    result, directory = self.invoke(self.program(literal), mode)
                    self.assertNotEqual(result.returncode, 0, result.stdout)
                    self.assertIn("cannot mix", result.stdout)
                    self.assertFalse((directory / "sample").exists())

    def test_dictionary_value_and_array_separator_are_required(self):
        for literal in ('["key" => ]', '[1 2]'):
            for mode in ("run", "compile"):
                with self.subTest(literal=literal, mode=mode):
                    result, directory = self.invoke(self.program(literal), mode)
                    self.assertNotEqual(result.returncode, 0, result.stdout)
                    self.assertIn("error: line 4, column", result.stdout)
                    self.assertFalse((directory / "sample").exists())


if __name__ == "__main__":
    unittest.main()
