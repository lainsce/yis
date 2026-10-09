"""End-to-end regressions for the self-hosted compiler, using real C compilation."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


COMPILER = Path(sys.argv.pop(1)).resolve()
ROOT = Path(sys.argv.pop(1)).resolve()


class CompilerTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="yis-regression-")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.env = os.environ.copy()
        self.env["YIS_STDLIB"] = str(ROOT / "src/stdlib")
        self.env["YIS_CACHE_DIR"] = str(self.work / "cache")
        # These tests must exercise the implementation, without onboarding workarounds.
        self.env.pop("YIS_CC_FLAGS", None)
        self.env.pop("YIS_DISABLE_BOOTSTRAP_FALLBACK", None)
        self.env["PATH"] = os.pathsep.join(
            p for p in self.env["PATH"].split(os.pathsep)
            if not p.endswith(".yis-tools/compat")
        )

    def source(self, text, name="program.yi"):
        path = self.work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        return path

    def invoke(self, *args):
        return subprocess.run(
            [str(COMPILER), *map(str, args)], cwd=self.work, env=self.env,
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=20,
        )

    def cached_binary(self):
        binaries = list((self.work / "cache").rglob("program"))
        self.assertTrue(binaries, "successful run should produce a cached executable")
        return max(binaries, key=lambda path: path.stat().st_mtime_ns)

    def assert_success(self, result, output=None):
        self.assertEqual(result.returncode, 0, result.stdout)
        if output is not None:
            self.assertEqual(result.stdout, output)

    def test_help_does_not_compile_a_file(self):
        result = self.invoke("--help")
        self.assert_success(result)
        self.assertIn("Usage:", result.stdout)
        self.assertEqual(list(self.work.iterdir()), [])

    def test_missing_input_fails(self):
        for args in [(), ("run",), ("missing.yi",), ("run", "missing.yi")]:
            with self.subTest(args=args):
                result = self.invoke(*args)
                self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertEqual(list(self.work.iterdir()), [])

    def test_compile_and_run_examples(self):
        expected = {
            "hello": "Hello, Yis World!\n",
            "json_parse_demo": "name=Yis ok=true nums_len=3 lang=yi\nBee\n",
            "class_test": "Point(3, 4)\nPoint(10, 20)\nPoint(13, 24)\nPoint(42, 99)\n",
            "new_syntax_test": "Hello, World!\nHello, Yis!\nHi, Friend!\nVec2(4, 6)\ni = 0\ni = 1\ni = 2\n",
        }
        for name, output in expected.items():
            with self.subTest(example=name):
                path = self.source((ROOT / f"examples/{name}.yi").read_text(), f"{name}.yi")
                self.assert_success(self.invoke("run", path), output)
                self.assert_success(self.invoke("run", path), output)

    def test_compile_mode_uses_self_hosted_compiler(self):
        path = self.source((ROOT / "examples/hello.yi").read_text())
        self.assert_success(self.invoke(path), "")
        result = subprocess.run([str(self.work / "program")], cwd=self.work,
                                text=True, capture_output=True, timeout=10)
        self.assertEqual((result.returncode, result.stdout), (0, "Hello, Yis World!\n"))

    def test_run_arguments_are_literal_including_cached_runs(self):
        path = self.source('cask program\nbring stdr\n-> ()\n'
                           '  let a = stdr.args()\n'
                           '  for (let ?i = 1; i < len(a); i += 1)\n'
                           '    write(a[i])\n    write("\\n")\n;\n')
        args = ["", "two words", 'a"b', "a'b", r"back\slash", "$HOME",
                "$(touch unexpected)", "`touch unexpected`"]
        output = "\n".join(args) + "\n"
        compiled_at = None
        for _ in range(2):
            self.assert_success(self.invoke("run", path, "--", *args), output)
            self.assertFalse((self.work / "unexpected").exists())
            stamp = self.cached_binary().stat().st_mtime_ns
            if compiled_at is not None:
                self.assertEqual(stamp, compiled_at, "unchanged program should use the cache")
            compiled_at = stamp

    def test_run_from_directory_with_shell_metacharacters(self):
        self.work = self.work / "space ' quote $literal `literal`"
        self.work.mkdir()
        path = self.source((ROOT / "examples/hello.yi").read_text())
        self.assert_success(self.invoke("run", path), "Hello, Yis World!\n")

    def test_no_cache_rebuilds_even_when_inputs_are_unchanged(self):
        path = self.source((ROOT / "examples/hello.yi").read_text())
        self.assert_success(self.invoke("run", path), "Hello, Yis World!\n")
        binary = self.cached_binary()
        compiled_at = binary.stat().st_mtime_ns
        self.env["YIS_NO_CACHE"] = "1"
        self.assert_success(self.invoke("run", path), "Hello, Yis World!\n")
        self.assertGreater(binary.stat().st_mtime_ns, compiled_at)

    def test_run_preserves_program_exit_status(self):
        path = self.source('cask program\nbring stdr\n-> ()\n'
                           '  stdr.exit_process(7)\n;\n')
        for _ in range(2):
            result = self.invoke("run", path)
            self.assertEqual(result.returncode, 7, result.stdout)

    def test_run_command_returns_exit_code(self):
        path = self.source('cask program\nbring stdr\n-> ()\n'
                           '  let r = stdr.run_command("exit 7")\n'
                           '  write(r[0])\n  write("\\n")\n;\n')
        self.assert_success(self.invoke("run", path), "7\n")

    def test_compilation_failure_does_not_run_or_cache(self):
        path = self.source('cask program\nbring stdr\n-> ()\n'
                           '  unknown_function()\n;\n')
        result = self.invoke("run", path)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertFalse((self.work / "program").exists())
        self.assertFalse((self.work / ".program.yis_manifest").exists())
        self.assertEqual(list((self.work / "cache").rglob("program")), [])

    def test_cache_invalidates_changed_transitive_import(self):
        leaf = self.source('cask leaf\n:: value() (( string ))\n  <- "first"\n;\n', "leaf.yi")
        self.source('cask helper\nbring leaf\n:: value() (( string ))\n'
                    '  <- leaf.value()\n;\n', "helper.yi")
        path = self.source('cask program\nbring stdr\nbring helper\n-> ()\n'
                           '  write(helper.value())\n  write("\\n")\n;\n')
        self.assert_success(self.invoke("run", path), "first\n")
        leaf.write_text(leaf.read_text().replace("first", "second"))
        # Set a later nanosecond timestamp without a test sleep.
        stamp = self.cached_binary().stat().st_mtime_ns + 1
        os.utime(leaf, ns=(stamp, stamp))
        self.assert_success(self.invoke("run", path), "second\n")

    def test_large_integer_ordering_is_exact(self):
        path = self.source('cask program\nbring stdr\n-> ()\n'
                           '  let low = 9007199254740992\n'
                           '  let high = 9007199254740993\n'
                           '  write(low < high)\n  write(high > low)\n'
                           '  write(high <= low)\n  write(low >= high)\n'
                           '  write("\\n")\n;\n')
        self.assert_success(self.invoke("run", path), "truetruefalsefalse\n")

    def test_cache_distinguishes_same_basename_in_different_directories(self):
        first = self.source('cask program\nbring stdr\n-> ()\n  write("first\\n")\n;\n',
                            "first/program.yi")
        second = self.source(first.read_text().replace("first", "second"), "second/program.yi")
        self.assert_success(self.invoke("run", first), "first\n")
        self.assert_success(self.invoke("run", second), "second\n")
        self.assert_success(self.invoke("run", first), "first\n")

    def test_invalid_lexical_input_fails(self):
        for invalid in ['write("unterminated)', '-| unterminated comment', 'write(@)',
                        'write("$$unterminated")']:
            with self.subTest(source=invalid):
                path = self.source('cask program\nbring stdr\n-> ()\n  ' + invalid + '\n;\n')
                result = self.invoke("run", path)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertIn("error", result.stdout.lower())
                self.assertFalse((self.work / "program").exists())

    def test_missing_or_invalid_import_fails(self):
        path = self.source('cask program\nbring stdr\nbring helper\n-> ()\n'
                           '  write("must not run\\n")\n;\n')
        result = self.invoke("run", path)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertFalse((self.work / "program").exists())
        self.source('cask helper\n-| unterminated\n', "helper.yi")
        result = self.invoke("run", path)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertFalse((self.work / "program").exists())

    def test_block_comments_preserve_line_comment_markers_and_quotes(self):
        path = self.source('cask program\nbring stdr\n'
                           '-| outer -- text -| nested " quote |- |-\n'
                           '-> ()\n  write("accepted\\n") -- trailing " quote\n;\n')
        self.assert_success(self.invoke("run", path), "accepted\n")


if __name__ == "__main__":
    unittest.main()
