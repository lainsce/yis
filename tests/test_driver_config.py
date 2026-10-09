"""End-to-end driver settings and cache correctness regressions."""

import json
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import sys
import tempfile
import unittest


COMPILER = Path(sys.argv.pop(1)).resolve()
ROOT = Path(sys.argv.pop(1)).resolve()
REAL_CC = shlex.split(os.environ.get("CC", "cc"))


class DriverConfigTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="yis-driver-")
        self.addCleanup(self.temp.cleanup)
        self.work = Path(self.temp.name)
        self.cache = self.work / "cache ' $literal `literal`"
        self.env = os.environ.copy()
        for name in ["CC", "YIS_CC_FLAGS", "YIS_NO_CACHE", "YIS_KEEP_C"]:
            self.env.pop(name, None)
        self.env["YIS_STDLIB"] = str(ROOT / "src/stdlib")
        self.env["YIS_CACHE_DIR"] = str(self.cache)
        self.env["PATH"] = os.pathsep.join(
            p for p in self.env["PATH"].split(os.pathsep)
            if not p.endswith(".yis-tools/compat")
        )
        self.log = self.work / "cc.jsonl"
        self.env["YIS_TEST_CC_LOG"] = str(self.log)

    def source(self, text=None, name="program.yi"):
        path = self.work / name
        path.parent.mkdir(parents=True, exist_ok=True)
        if text is None:
            text = 'cask program\nbring stdr\n-> ()\n  write("ready\\n")\n;\n'
        path.write_text(text)
        return path

    def invoke(self, *args):
        return subprocess.run(
            [str(COMPILER), *map(str, args)], cwd=self.work, env=self.env,
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30,
        )

    def run_source(self, path):
        result = self.invoke("run", path)
        self.assertEqual(result.returncode, 0, result.stdout)
        return result.stdout

    def wrapper(self, name="compiler ' with spaces"):
        path = self.work / name
        path.write_text(
            f"#!{sys.executable}\n"
            "import json, os, subprocess, sys\n"
            "arguments = sys.argv[1:]\n"
            "if arguments and arguments[0] == '--wrapper-mode': arguments.pop(0)\n"
            "with open(os.environ['YIS_TEST_CC_LOG'], 'a') as log:\n"
            "    log.write(json.dumps(arguments) + '\\n')\n"
            f"sys.exit(subprocess.run([*{REAL_CC!r}, *arguments]).returncode)\n"
        )
        path.chmod(0o755)
        self.env["CC"] = str(path)
        return path

    def commands(self):
        if not self.log.exists():
            return []
        return [json.loads(line) for line in self.log.read_text().splitlines()]

    def generated_c(self):
        return list(self.work.rglob(".yis-c-*"))

    def test_extra_flags_are_applied_and_defaults_remain(self):
        self.wrapper()
        self.env["YIS_CC_FLAGS"] = '-DYIS_TEST_FLAG=1 -O2'
        path = self.source()
        self.assertEqual(self.run_source(path), "ready\n")
        arguments = self.commands()[0]
        self.assertIn("-DYIS_TEST_FLAG=1", arguments)
        self.assertIn("-O1", arguments)
        self.assertIn("-O2", arguments)
        self.assertIn("-std=c11", arguments)
        self.assertIn("-lm", arguments)
        self.assertEqual(arguments[arguments.index("-x") + 1], "c")
        self.assertEqual(self.invoke(path).returncode, 0)
        self.assertIn("-Os", self.commands()[1])
        self.assertIn("-DYIS_TEST_FLAG=1", self.commands()[1])

    def test_bad_extra_flags_fail_compile_and_run_without_retained_c(self):
        path = self.source()
        self.env["YIS_CC_FLAGS"] = '-this-is-an-intentional-invalid-option'
        for args in [(path,), ("run", path)]:
            with self.subTest(args=args):
                result = self.invoke(*args)
                self.assertNotEqual(result.returncode, 0, result.stdout)
                self.assertNotIn("ready\n", result.stdout)
                self.assertEqual(self.generated_c(), [])
        self.assertFalse((self.work / "program").exists())
        self.assertEqual(list(self.cache.rglob("program.yis_manifest")), [])
        self.assertEqual(list(self.cache.rglob(".yis-bin-*")), [])

    def test_compiler_and_flags_preserve_literal_arguments(self):
        wrapper = self.wrapper()
        self.env["CC"] = f'"{wrapper}" --wrapper-mode'
        self.env["YIS_CC_FLAGS"] = (
            '-DNAME="two words" "-DLITERAL=$(touch unexpected)" '
            '"-DBACKTICK=`touch unexpected`" "-DSEMICOLON=;touch unexpected"'
        )
        self.assertEqual(self.run_source(self.source()), "ready\n")
        arguments = self.commands()[0]
        self.assertIn("-DNAME=two words", arguments)
        self.assertIn("-DLITERAL=$(touch unexpected)", arguments)
        self.assertIn("-DBACKTICK=`touch unexpected`", arguments)
        self.assertIn("-DSEMICOLON=;touch unexpected", arguments)
        self.assertFalse((self.work / "unexpected").exists())

    def test_malformed_environment_argument_quotes_fail(self):
        path = self.source()
        for value in ['"unterminated', 'trailing\\']:
            self.env["YIS_CC_FLAGS"] = value
            result = self.invoke("run", path)
            self.assertNotEqual(result.returncode, 0, result.stdout)
            self.assertIn("YIS_CC_FLAGS", result.stdout)
            self.assertEqual(self.generated_c(), [])

    def test_cache_honors_flags_and_compiler_choice(self):
        self.wrapper()
        path = self.source()
        self.env["YIS_CC_FLAGS"] = "-DVERSION=1"
        for _ in range(2):
            self.assertEqual(self.run_source(path), "ready\n")
        self.assertEqual(len(self.commands()), 1)
        self.env["YIS_CC_FLAGS"] = "-DVERSION=2"
        self.assertEqual(self.run_source(path), "ready\n")
        self.assertEqual(len(self.commands()), 2)
        self.wrapper("second compiler")
        self.assertEqual(self.run_source(path), "ready\n")
        self.assertEqual(len(self.commands()), 3)
        self.assertFalse((self.work / "program").exists())
        self.assertEqual(len(list(self.cache.rglob("program"))), 3)

    def test_cache_separates_source_paths_with_the_same_basename(self):
        self.wrapper()
        first = self.source(name="first/program.yi")
        second = self.source(first.read_text().replace("ready", "second"), "second/program.yi")
        for path, expected in [(first, "ready\n"), (second, "second\n"), (first, "ready\n")]:
            self.assertEqual(self.run_source(path), expected)
        self.assertEqual(len(self.commands()), 2)
        self.assertEqual(len(list(self.cache.rglob("program"))), 2)
        self.assertEqual(self.generated_c(), [])

    def test_no_cache_recompiles(self):
        self.wrapper()
        path = self.source()
        self.assertEqual(self.run_source(path), "ready\n")
        self.env["YIS_NO_CACHE"] = "1"
        for _ in range(2):
            self.assertEqual(self.run_source(path), "ready\n")
        self.assertEqual(len(self.commands()), 3)

    def test_compiler_content_edits_preserving_metadata_invalidate(self):
        compiler = self.wrapper()
        compiler.write_text(compiler.read_text() + "# first compiler configuration\n")
        path = self.source()
        for _ in range(2):
            self.assertEqual(self.run_source(path), "ready\n")
        previous = compiler.stat()
        compiler.write_text(compiler.read_text().replace("first compiler", "later compiler"))
        os.utime(compiler, ns=(previous.st_atime_ns, previous.st_mtime_ns))
        self.assertEqual(compiler.stat().st_size, previous.st_size)
        self.assertEqual(self.run_source(path), "ready\n")
        self.assertEqual(len(self.commands()), 2)

    def test_transitive_dependency_edits_preserving_timestamps_invalidate(self):
        self.wrapper()
        leaf = self.source('cask leaf\n:: value() (( string ))\n  <- "first"\n;\n', "leaf.yi")
        self.source('cask helper\nbring leaf\n:: value() (( string ))\n  <- leaf.value()\n;\n', "helper.yi")
        path = self.source('cask program\nbring stdr\nbring helper\n-> ()\n  write(helper.value())\n  write("\\n")\n;\n')
        self.assertEqual(self.run_source(path), "first\n")
        previous = leaf.stat()
        leaf.write_text(leaf.read_text().replace("first", "later"))
        os.utime(leaf, ns=(previous.st_atime_ns, previous.st_mtime_ns))
        self.assertEqual(self.run_source(path), "later\n")
        self.assertEqual(len(self.commands()), 2)

    def test_runtime_content_edits_preserving_timestamps_invalidate(self):
        self.wrapper()
        runtime = self.work / "src/runtime.inc"
        runtime.parent.mkdir()
        shutil.copy2(ROOT / "src/runtime.inc", runtime)
        path = self.source()
        self.assertEqual(self.run_source(path), "ready\n")
        previous = runtime.stat()
        runtime.write_text(runtime.read_text() + "\n/* changed runtime */\n")
        os.utime(runtime, ns=(previous.st_atime_ns, previous.st_mtime_ns))
        self.assertEqual(self.run_source(path), "ready\n")
        self.assertEqual(len(self.commands()), 2)

    def test_shared_import_through_parent_alias_is_emitted_once(self):
        self.source('cask shared\n:: value() (( string ))\n  <- "shared"\n;\n', "shared.yi")
        self.source('cask left\nbring shared\n:: value() (( string ))\n  <- shared.value()\n;\n', "left.yi")
        self.source('cask right\nbring shared\n:: value() (( string ))\n  <- shared.value()\n;\n', "nested/right.yi")
        path = self.source('cask program\nbring stdr\nbring left\nbring nested.right\n-> ()\n  write(left.value())\n  write(right.value())\n;\n')
        self.assertEqual(self.run_source(path), "sharedshared")

    def test_cyclic_parent_import_fails_without_recursing_forever(self):
        self.source('cask first\nbring nested.second\n', "first.yi")
        self.source('cask second\nbring first\n', "nested/second.yi")
        path = self.source('cask program\nbring first\n-> ()\n;\n')
        result = self.invoke("run", path)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("Cyclic module import", result.stdout)

    def test_transitive_native_module_flags_and_cache_bypass(self):
        self.wrapper()
        native = self.source('cask native\n:: value() (( string ))\n  <- "native"\n;\n', "native.yi")
        self.source('cask wrapper\nbring native\n:: value() (( string ))\n  <- native.value()\n;\n', "wrapper.yi")
        self.env["YIS_NATIVE_PATH"] = str(native)
        self.env["YIS_NATIVE_CFLAGS"] = "-DNATIVE_MODULE_FLAG=1"
        self.env["YIS_NATIVE_FLAGS"] = "-lm"
        path = self.source('cask program\nbring stdr\nbring wrapper\n-> ()\n  write(wrapper.value())\n;\n')
        for _ in range(2):
            self.assertEqual(self.run_source(path), "native")
        self.assertEqual(len(self.commands()), 2)
        for arguments in self.commands():
            self.assertIn("-DNATIVE_MODULE_FLAG=1", arguments)

    def test_native_header_flags_rebuild_when_header_changes(self):
        self.wrapper()
        header = self.work / "configuration header.h"
        header.write_text("/* first configuration */\n")
        self.env["YIS_CC_FLAGS"] = f'-include "{header}"'
        path = self.source()
        self.assertEqual(self.run_source(path), "ready\n")
        previous = header.stat()
        header.write_text("/* later configuration */\n")
        os.utime(header, ns=(previous.st_atime_ns, previous.st_mtime_ns))
        self.assertEqual(self.run_source(path), "ready\n")
        self.assertEqual(len(self.commands()), 2)

    def test_c_cleanup_preserves_unrelated_user_files(self):
        sentinels = [self.work / name for name in
                     ["program.c", ".yi_compile_out.c", ".yi_run_compile_out.c", ".yis-c-user-file"]]
        for path in sentinels:
            path.write_text("user contents\n")
        path = self.source()
        self.assertEqual(self.invoke(path).returncode, 0)
        self.assertEqual(self.run_source(path), "ready\n")
        self.env["YIS_CC_FLAGS"] = '-this-is-an-intentional-invalid-option'
        self.assertNotEqual(self.invoke("run", path).returncode, 0)
        for sentinel in sentinels:
            self.assertEqual(sentinel.read_text(), "user contents\n")
        self.assertEqual(self.generated_c(), [sentinels[-1]])

    def test_keep_c_retains_unique_generated_sources_on_success_and_failure(self):
        path = self.source()
        self.env["YIS_KEEP_C"] = "1"
        self.assertEqual(self.run_source(path), "ready\n")
        retained = self.generated_c()
        self.assertEqual(len(retained), 1)
        self.assertIn("int main(", retained[0].read_text())
        self.assertEqual(self.run_source(path), "ready\n")
        self.assertEqual(len(self.generated_c()), 2)
        self.env["YIS_CC_FLAGS"] = '-this-is-an-intentional-invalid-option'
        result = self.invoke(path)
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("Generated C:", result.stdout)
        self.assertEqual(len(self.generated_c()), 3)

    def test_explicit_unusable_cache_directory_fails_clearly(self):
        blocking_file = self.work / "cache-file"
        blocking_file.write_text("unchanged")
        self.env["YIS_CACHE_DIR"] = str(blocking_file / "child")
        result = self.invoke("run", self.source())
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn("YIS_CACHE_DIR", result.stdout)
        self.assertEqual(blocking_file.read_text(), "unchanged")


if __name__ == "__main__":
    unittest.main()
