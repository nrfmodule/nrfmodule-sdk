#!/usr/bin/env python3
"""Run a Zephyr ztest suite on qemu_cortex_m0 with timeout and clean output.

Usage:
    python scripts/run_test.py tests/led_effect
    python scripts/run_test.py tests/led_arbiter --pristine
    python scripts/run_test.py tests/led_effect --timeout 60
"""

import argparse
import subprocess
import sys
import re
import os

DEFAULT_BOARD = "qemu_cortex_m0"
DEFAULT_TIMEOUT = 30
BUILD_DIR = "build_test"
WEST_WORKSPACE = r"c:\ncs\nrfmodule_v3.2.1"


def kill_tree(pid):
    """Kill process and all children (QEMU is a grandchild of west)."""
    try:
        subprocess.run(
            ["taskkill", "/F", "/T", "/PID", str(pid)],
            capture_output=True, timeout=10
        )
    except Exception:
        pass


def run(test_dir: str, pristine: bool, timeout: int) -> int:
    build_cmd = [
        "west", "build",
        "-b", DEFAULT_BOARD,
        test_dir,
        "--build-dir", BUILD_DIR,
        "--no-sysbuild",
    ]
    if pristine:
        build_cmd.append("--pristine")

    # --- Build ---
    print(f"[build] {' '.join(build_cmd)}")
    result = subprocess.run(build_cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print("[build] FAILED")
        lines = (result.stdout + result.stderr).strip().splitlines()
        for line in lines[-30:]:
            print(line)
        return 1
    print("[build] OK")

    # --- Run (temp file output to avoid Windows pipe inheritance issues) ---
    run_cmd = ["west", "build", "-t", "run", "--build-dir", BUILD_DIR]
    print(f"[run]   {' '.join(run_cmd)}")
    returncode = 1
    log_file = os.path.join(BUILD_DIR, "test_output.log")

    import time
    with open(log_file, "w") as f:
        proc = subprocess.Popen(run_cmd, stdout=f, stderr=subprocess.STDOUT)
        deadline = time.monotonic() + timeout
        done = False
        while time.monotonic() < deadline:
            if proc.poll() is not None:
                returncode = proc.returncode
                done = True
                break
            try:
                with open(log_file, "r") as check:
                    content = check.read()
                if "PROJECT EXECUTION SUCCESSFUL" in content or "PROJECT EXECUTION FAILED" in content:
                    done = True
                    returncode = 0 if "SUCCESSFUL" in content else 1
                    break
            except OSError:
                pass
            time.sleep(0.25)

        kill_tree(proc.pid)
        if not done:
            print(f"[run]   TIMEOUT after {timeout}s - QEMU killed")

    with open(log_file, "r") as f:
        output = f.read()

    # --- Parse ztest results ---
    suite_pass = re.findall(r"SUITE\s+(\w+)\s+succeeded", output)
    suite_fail = re.findall(r"SUITE\s+(\w+)\s+failed", output)
    summary = re.search(
        r"(\d+) of (\d+) test suites passed.*?(\d+) of (\d+) test cases passed",
        output, re.DOTALL
    )

    for line in output.splitlines():
        if re.match(r"\s*(PASS|FAIL|SKIP)\s*-\s*", line):
            print(line.strip())
        elif "ASSERTION FAIL" in line or "Assertion failed" in line:
            print(line.strip())

    if summary:
        print(f"\n[result] Suites: {summary.group(1)}/{summary.group(2)} passed, "
              f"Cases: {summary.group(3)}/{summary.group(4)} passed")
    elif suite_fail:
        print(f"\n[result] FAILED suites: {', '.join(suite_fail)}")
    elif suite_pass:
        print(f"\n[result] PASSED suites: {', '.join(suite_pass)}")
    else:
        print("\n[result] No ztest output detected. Last 20 lines:")
        for line in output.strip().splitlines()[-20:]:
            print(line)
        return 1

    return 0 if not suite_fail and returncode == 0 else 1


def main():
    parser = argparse.ArgumentParser(description="Run Zephyr ztest with timeout")
    parser.add_argument("test_dir", help="Path to test directory")
    parser.add_argument("--pristine", action="store_true", help="Clean rebuild")
    parser.add_argument("--timeout", type=int, default=DEFAULT_TIMEOUT,
                        help=f"QEMU timeout in seconds (default: {DEFAULT_TIMEOUT})")
    args = parser.parse_args()

    # west needs to run from inside the workspace; test paths are relative to project root
    project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    os.environ["ZEPHYR_BASE"] = os.path.join(WEST_WORKSPACE, "zephyr")
    os.chdir(project_root)

    if not os.path.isdir(args.test_dir):
        print(f"[error] Test directory not found: {args.test_dir}")
        sys.exit(1)

    sys.exit(run(args.test_dir, args.pristine, args.timeout))


if __name__ == "__main__":
    main()
