"""Check decoded bytes survive lexer and generated C string literals."""

import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


COMPILER = Path(sys.argv.pop(1)).resolve()
ROOT = Path(sys.argv.pop(1)).resolve()


class StringTests(unittest.TestCase):
    def test_escaped_and_unicode_bytes_in_both_compile_modes(self):
        cases = {
            "control_followed_by_hex": ('"\\x1bABC"', b"\x1bABC"),
            "nul": ('"a\\x00b"', b"a\0b"),
            "octal_nul": ('"\\0007"', b"\0007"),
            "unicode": ('"\\u{1F642}ABC"', "🙂ABC".encode()),
            "literal_unicode_escape": ('"\\\\u{0041}"', b"\\u{0041}"),
            "utf8": ('"Olá 世界"', "Olá 世界".encode()),
        }
        for name, (literal, expected) in cases.items():
            for mode in ("run", "compile"):
                with self.subTest(case=name, mode=mode), tempfile.TemporaryDirectory(prefix="yis-string-") as directory:
                    work = Path(directory)
                    path = work / 'program "quoted".yi'
                    path.write_text('cask sample\nbring stdr\n-> ()\n  write(' + literal + ')\n;\n')
                    environment = os.environ.copy()
                    environment["YIS_STDLIB"] = str(ROOT / "src/stdlib")
                    environment["YIS_CACHE_DIR"] = str(work / "cache")
                    environment["YIS_NO_CACHE"] = "1"
                    arguments = [str(COMPILER)] + (["run"] if mode == "run" else []) + [str(path)]
                    result = subprocess.run(arguments, cwd=work, env=environment, capture_output=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    if mode == "compile":
                        executables = [item for item in work.iterdir() if item.is_file() and os.access(item, os.X_OK)]
                        self.assertEqual(len(executables), 1, executables)
                        result = subprocess.run([str(executables[0])], cwd=work, capture_output=True, timeout=5)
                        self.assertEqual(result.returncode, 0, result.stderr)
                    self.assertEqual(result.stdout, expected)


if __name__ == "__main__":
    unittest.main()
