"""Exercise both native runtimes, optionally under ASan/UBSan.

Usage: python3 tests/test_runtime.py [--cc cc] [--sanitizers]
"""

import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest


parser = argparse.ArgumentParser()
parser.add_argument("--cc", default=os.environ.get("CC", "cc"))
parser.add_argument("--sanitizers", action="store_true")
options, unittest_args = parser.parse_known_args()
ROOT = Path(__file__).resolve().parents[1]


class RuntimeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix="yis-native-tests-")
        cls.addClassCleanup(cls.temp.cleanup)
        cls.work = Path(cls.temp.name)
        cls.binaries = {}
        for runtime in ["src/runtime.inc", "src/bootstrap/runtime.inc"]:
            output = cls.work / ("runtime-bootstrap" if "bootstrap" in runtime else "runtime")
            command = [*shlex.split(options.cc), "-std=c11", "-O1", "-g",
                       '-DYIS_RUNTIME_HEADER="' + str(ROOT / runtime) + '"',
                       str(ROOT / "tests/test_runtime.c"), "-lm", "-o", str(output)]
            if options.sanitizers:
                command[1:1] = ["-fsanitize=address,undefined,float-cast-overflow",
                                "-fno-omit-frame-pointer"]
            result = subprocess.run(command, text=True, capture_output=True, timeout=60)
            if result.returncode:
                raise RuntimeError("runtime harness failed to compile:\n" + result.stderr)
            cls.binaries[runtime] = output

    def run_case(self, case, error=None):
        for runtime, binary in self.binaries.items():
            with self.subTest(runtime=runtime, case=case):
                env = os.environ.copy()
                env["YIS_RUNTIME_TEST_VALUE"] = "test value with spaces"
                env.pop("YIS_RUNTIME_TEST_UNSET", None)
                if options.sanitizers:
                    env["ASAN_OPTIONS"] = "detect_leaks=1:halt_on_error=1"
                    env["UBSAN_OPTIONS"] = "halt_on_error=1:print_stacktrace=1"
                result = subprocess.run([str(binary), case, str(self.work)], env=env,
                                        text=True, capture_output=True, timeout=10)
                if error:
                    self.assertNotEqual(result.returncode, 0, result.stderr)
                    self.assertIn("runtime error: " + error, result.stderr)
                else:
                    self.assertEqual(result.returncode, 0, result.stderr)
                self.assertNotIn("AddressSanitizer", result.stderr)
                self.assertNotIn("UndefinedBehaviorSanitizer", result.stderr)

    def test_borrowed_self_assignment(self):
        self.run_case("self-assignment")

    def test_array_growth_and_sparse_indexes(self):
        self.run_case("array")

    def test_dictionary_growth_and_ownership(self):
        self.run_case("dict")

    def test_string_operations(self):
        self.run_case("strings")

    def test_arithmetic_boundaries_and_float_behavior(self):
        self.run_case("arithmetic")

    def test_float_integer_conversion_and_rounding(self):
        self.run_case("float-conversion")

    def test_invalid_float_integer_conversion_traps(self):
        for operation in ["convert", "floor", "ceil"]:
            for kind in ["upper", "lower", "nan", "positive-inf", "negative-inf"]:
                self.run_case(operation + "-" + kind, "integer conversion out of range")

    def test_arithmetic_overflow_traps(self):
        for case in ["add-positive-overflow", "add-negative-overflow", "sub-positive-overflow",
                     "sub-negative-overflow", "mul-positive-overflow", "mul-negative-overflow",
                     "div-overflow", "mod-overflow", "neg-overflow", "prune-overflow"]:
            self.run_case(case, "integer overflow")

    def test_integer_division_by_zero_traps(self):
        for case in ["div-zero", "mod-zero"]:
            self.run_case(case, "division by zero")

    def test_environment_and_stable_hash(self):
        self.run_case("environment-hash")

    def test_exclusive_temporary_files(self):
        self.run_case("temp-file")

    def test_canonical_existing_paths(self):
        self.run_case("canonical-path")

    def test_failed_write_closes_file(self):
        self.run_case("short-write")

    def test_allocation_overflow_traps(self):
        for case in ["array-overflow", "string-overflow"]:
            self.run_case(case, "allocation size overflow")

    def test_allocation_failure_traps(self):
        for case in ["oom-array", "oom-array-items", "oom-string", "oom-concat",
                     "oom-dict", "oom-function", "oom-object", "oom-array-growth",
                     "oom-dict-growth"]:
            self.run_case(case, "out of memory")


if __name__ == "__main__":
    unittest.main(argv=[__file__, *unittest_args])
