# SPDX-License-Identifier: Apache-2.0
"""Synthetic CLI integration checks; no vendor SDK, credentials or devices."""
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile

exe = sys.argv[1]


def run(*args):
    return subprocess.run([exe, *args], capture_output=True, text=True, timeout=5)


def check(condition):
    if not condition:
        raise AssertionError("CLI integration check failed")


result = run("simulate")
check(result.returncode == 0 and "SIMULATION" in result.stderr)
rows = [json.loads(line) for line in result.stdout.splitlines()]
check(len(rows) > 0)
check(all(set(row) == {"mode", "device", "kind", "code", "received_unix_ms",
                       "length", "dropped", "invalid"} for row in rows))
check(all(row["mode"] == "simulation" and row["device"] == "camera-demo" for row in rows))
stats = json.loads(result.stderr.splitlines()[-1])
check(stats["delivered"] == len(rows) == stats["accepted"] and stats["output_errors"] == 0)
check("synthetic-event" not in result.stdout + result.stderr)
for args in [("simulate", "--seconds", "0"), ("simulate", "--seconds", "2x"),
             ("simulate", "--seconds", "3601"), ("simulate", "--alias", 'bad"alias'),
             ("simulate", "--seconds"), ("simulate", "--password", "canary-secret"),
             ("simulate", "--seconds", "1", "--seconds", "1")]:
    result = run(*args)
    check(result.returncode != 0 and "canary-secret" not in result.stdout + result.stderr)
result = run("real", "--sdk", "/nonexistent/synthetic-sdk.so")
check(result.returncode != 0 and "SIMULATION" not in result.stdout + result.stderr)
with tempfile.TemporaryDirectory(prefix="hik-bridge-synthetic-") as directory:
    path = Path(directory) / "synthetic.password"
    path.write_text("synthetic-password-canary\n")
    path.chmod(0o600)
    result = run("check-password", str(path))
    check(result.returncode == 0 and "synthetic-password-canary" not in result.stdout + result.stderr)
    path.chmod(0o644)
    check(run("check-password", str(path)).returncode != 0)
    check(run("check-password", str(path) + "-missing").returncode != 0)
process = subprocess.Popen([exe, "simulate", "--seconds", "30"], stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, text=True)
check('"mode":"simulation"' in process.stdout.readline())
process.send_signal(signal.SIGTERM)
out, err = process.communicate(timeout=5)
check(process.returncode == 0 and '"output_errors":0' in err)
read_fd, write_fd = os.pipe()
os.close(read_fd)
process = subprocess.Popen([exe, "simulate", "--seconds", "30"], stdout=write_fd,
                           stderr=subprocess.PIPE, text=True)
os.close(write_fd)
_, err = process.communicate(timeout=5)
check(process.returncode != 0 and json.loads(err.splitlines()[-1])["output_errors"] >= 1)
# An already full pipe with an idle reader must also fail without blocking shutdown.
read_fd, write_fd = os.pipe()
os.set_blocking(write_fd, False)
try:
    while True:
        os.write(write_fd, b"x" * 4096)
except BlockingIOError:
    pass
process = subprocess.Popen([exe, "simulate", "--seconds", "30"], stdout=write_fd,
                           stderr=subprocess.PIPE, text=True)
os.close(write_fd)
_, err = process.communicate(timeout=5)
os.close(read_fd)
check(process.returncode != 0 and json.loads(err.splitlines()[-1])["output_errors"] >= 1)
print("synthetic CLI checks passed")
