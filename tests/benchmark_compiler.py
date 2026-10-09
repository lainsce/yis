"""Bounded compiler/runtime benchmarks; writes measurements outside the checkout.

Example: python3 tests/benchmark_compiler.py build/yis . \
    --output /workspace/yis-release-results/current.json

Times include process startup. RSS is the largest child-process RSS for each
sample (compiler samples include the C compiler), in KiB on Linux. Every sample
uses a fresh measurement worker so resource counters do not bleed across runs.
These are measurements, not performance thresholds or exhaustive release tests.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import shutil
import signal
import statistics
import subprocess
import sys
import tempfile
import time


WORKLOADS = {
    "integer_loop": (
        1_000_000,
        "  let ?total = 0\n"
        "  for (let ?i = 0; i < count; i += 1)\n"
        "    total += i\n"
        "  write(total)\n",
        lambda n: str(n * (n - 1) // 2) + "\n",
    ),
    "array_append_read": (
        50_000,
        "  let values = []: [any]\n"
        "  for (let ?i = 0; i < count; i += 1)\n"
        "    stdr.push(values, i)\n"
        "  let ?total = 0\n"
        "  for (let ?i = 0; i < count; i += 1)\n"
        "    total += num(values[i])\n"
        "  write(total)\n",
        lambda n: str(n * (n - 1) // 2) + "\n",
    ),
    "string_concat": (
        4_000,
        "  let ?text = \"\"\n"
        "  for (let ?i = 0; i < count; i += 1)\n"
        "    text = stdr.str_concat(text, \"abc\")\n"
        "  write(len(text))\n",
        lambda n: str(n * 3) + "\n",
    ),
}


def measure_worker(spec):
    started = time.perf_counter()
    child = subprocess.Popen(
        spec["argv"], cwd=spec["cwd"], text=True,
        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        start_new_session=(os.name == "posix"),
    )
    try:
        stdout, stderr = child.communicate(timeout=spec["timeout"])
        result = {
            "returncode": child.returncode,
            "stdout": stdout,
            "stderr": stderr,
        }
    except subprocess.TimeoutExpired as exc:
        if os.name == "posix":
            os.killpg(child.pid, signal.SIGKILL)
        else:
            child.kill()
        child.communicate()
        result = {"returncode": 124, "stdout": "", "stderr": str(exc)}
    result["seconds"] = time.perf_counter() - started
    try:
        import resource
        rss = resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss
        result["peak_rss_kib"] = rss / 1024 if sys.platform == "darwin" else rss
    except ImportError:
        result["peak_rss_kib"] = None
    print(json.dumps(result))


def measure(argv, cwd, env, timeout, expected):
    spec = {"argv": list(map(str, argv)), "cwd": str(cwd), "timeout": timeout}
    worker = subprocess.run(
        [sys.executable, str(Path(__file__).resolve()), "--_measure", json.dumps(spec)],
        env=env, text=True, capture_output=True, timeout=timeout + 10,
    )
    if worker.returncode:
        raise RuntimeError(worker.stderr or worker.stdout)
    result = json.loads(worker.stdout)
    if result["returncode"] or result["stdout"] != expected:
        raise RuntimeError(json.dumps({"command": spec, "result": result}, indent=2))
    result.pop("stdout")
    result.pop("stderr")
    return result


def summarize(samples):
    rss = [sample["peak_rss_kib"] for sample in samples if sample["peak_rss_kib"] is not None]
    return {
        "median_seconds": statistics.median(sample["seconds"] for sample in samples),
        "max_peak_rss_kib": max(rss) if rss else None,
        "samples": samples,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("compiler", type=Path)
    parser.add_argument("root", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--samples", type=int, default=3)
    parser.add_argument("--timeout", type=float, default=30)
    parser.add_argument("--scale", type=int, default=1, choices=range(1, 5))
    args = parser.parse_args()
    if args.samples < 1 or args.samples > 10 or args.timeout <= 0:
        parser.error("samples must be 1..10 and timeout must be positive")
    compiler = args.compiler.resolve()
    root = args.root.resolve()
    output = args.output.resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env["YIS_STDLIB"] = str(root / "src/stdlib")
    for name in ("YIS_CC_FLAGS", "YIS_NO_CACHE", "YIS_CACHE_DIR", "CC"):
        env.pop(name, None)
    env["PATH"] = os.pathsep.join(
        item for item in env.get("PATH", "").split(os.pathsep)
        if not item.endswith(".yis-tools/compat")
    )
    report = {
        "platform": platform.platform(),
        "python": platform.python_version(),
        "compiler": str(compiler),
        "compiler_sha256": hashlib.sha256(compiler.read_bytes()).hexdigest(),
        "runtime_sha256": hashlib.sha256((root / "src/runtime.inc").read_bytes()).hexdigest(),
        "samples": args.samples,
        "scale": args.scale,
        "rss_scope": "max RSS among descendants per fresh measurement worker",
        "rss_note": "May include the inherited worker footprint when process startup uses fork",
        "workloads": {},
    }
    version = subprocess.run(["cc", "--version"], env=env, text=True, capture_output=True)
    report["c_compiler"] = version.stdout.splitlines()[0] if version.stdout else version.stderr.strip()
    # Retain sources and generated C for reviewing or sanitizer recompilation.
    work = Path(tempfile.mkdtemp(prefix=output.stem + "-", dir=output.parent))
    report["work_directory"] = str(work)
    for name, (base_count, body, expected_output) in WORKLOADS.items():
        directory = work / name
        directory.mkdir()
        cache = directory / "cache"
        env["YIS_CACHE_DIR"] = str(cache)
        count = base_count * args.scale
        source = directory / (name + ".yi")
        source.write_text(
            "cask " + name + "\nbring stdr\n-> ()\n"
            "  let arguments = stdr.args()\n"
            "  let count = num(arguments[1])\n" + body + '  write("\\n")\n;\n'
        )
        binary = directory / name
        cold_runs = []
        for _ in range(args.samples):
            if cache.exists():
                shutil.rmtree(cache)
            binary.unlink(missing_ok=True)
            for manifest in directory.glob("*.yis_manifest"):
                manifest.unlink()
            for manifest in directory.glob(".*.yis_manifest"):
                manifest.unlink()
            cold_runs.append(measure([compiler, "run", source, "--", count], directory, env,
                                     args.timeout, expected_output(count)))
        warm_runs = [measure([compiler, "run", source, "--", count], directory, env,
                             args.timeout, expected_output(count)) for _ in range(args.samples)]
        compile_env = dict(env, YIS_NO_CACHE="1", YIS_KEEP_C="1")
        compile_runs = [measure([compiler, source], directory, compile_env, args.timeout, "")
                        for _ in range(args.samples)]
        # Compile mode supplies a stable standalone binary even when run mode
        # puts its artifact in YIS_CACHE_DIR.
        runtimes = [measure([binary, count], directory, env, args.timeout,
                            expected_output(count)) for _ in range(args.samples)]
        report["workloads"][name] = {
            "iterations": count,
            "source": str(source),
            "cold_run": summarize(cold_runs),
            "warm_run": summarize(warm_runs),
            "runtime": summarize(runtimes),
            "uncached_compile": summarize(compile_runs),
        }
        output.write_text(json.dumps(report, indent=2) + "\n")
        print(name + ": " + json.dumps({
            key: round(report["workloads"][name][key]["median_seconds"], 6)
            for key in ("cold_run", "warm_run", "runtime", "uncached_compile")
        }), flush=True)
    print("Report: " + str(output))


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--_measure":
        measure_worker(json.loads(sys.argv[2]))
    else:
        main()
