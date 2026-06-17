"""Persistent R worker bridge: LegacyRunWebVersion via SmokingHistoryGenerator."""

from __future__ import annotations

import hashlib
import json
import os
import subprocess
import threading
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from shg_paths import REPO_ROOT

_PYTESTS_DIR = Path(__file__).resolve().parent
_WORKER_SCRIPT = _PYTESTS_DIR / "r_worker.R"


@dataclass
class LegacyRunResult:
    """Mimics subprocess.CompletedProcess for ported CLI tests."""

    returncode: int
    stdout: str = ""
    stderr: str = ""


class ShgRWorker:
    """Session-scoped Rscript worker with JSON-line protocol."""

    def __init__(self) -> None:
        self._proc: Optional[subprocess.Popen[str]] = None
        self._lock = threading.Lock()

    def start(self) -> None:
        if self._proc is not None and self._proc.poll() is None:
            return
        env = os.environ.copy()
        env.setdefault("SHG_R_ROOT", str(REPO_ROOT))
        env.setdefault("SHG_PYTESTS_DIR", str(_PYTESTS_DIR))
        self._proc = subprocess.Popen(
            ["Rscript", str(_WORKER_SCRIPT)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
            cwd=str(REPO_ROOT),
            env=env,
        )

    def stop(self) -> None:
        if self._proc is None:
            return
        if self._proc.poll() is None:
            self._proc.stdin.close()
            self._proc.terminate()
            try:
                self._proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                self._proc.kill()
        self._proc = None

    def legacy_run(self, input_path: str, cwd: str | None = None) -> LegacyRunResult:
        self.start()
        assert self._proc is not None
        assert self._proc.stdin is not None
        assert self._proc.stdout is not None

        req = {
            "op": "legacy_run",
            "input": str(Path(input_path).resolve()),
            "cwd": str(Path(cwd or REPO_ROOT).resolve()),
        }
        line = json.dumps(req) + "\n"

        with self._lock:
            self._proc.stdin.write(line)
            self._proc.stdin.flush()
            resp_line = self._proc.stdout.readline()
            if not resp_line:
                stderr_tail = self._proc.stderr.read() if self._proc.stderr else ""
                raise RuntimeError(
                    f"R worker exited unexpectedly (stderr: {stderr_tail})"
                )
            resp = json.loads(resp_line)

        messages = resp.get("messages") or ""
        if resp.get("ok"):
            return LegacyRunResult(returncode=0, stdout=messages, stderr="")

        error = resp.get("error") or "LegacyRunWebVersion failed"
        error_file = resp.get("error_file") or ""
        if error_file and os.path.isfile(error_file):
            with open(error_file, encoding="utf-8") as f:
                error = f.read()
        return LegacyRunResult(returncode=1, stdout=messages, stderr=error)


_worker: Optional[ShgRWorker] = None


def get_worker() -> ShgRWorker:
    global _worker
    if _worker is None:
        _worker = ShgRWorker()
        _worker.start()
    return _worker


def shutdown_worker() -> None:
    global _worker
    if _worker is not None:
        _worker.stop()
        _worker = None


def run_legacy_config(input_path: str, cwd: str | None = None) -> None:
    """Run LegacyRunWebVersion; raise ValueError with error-file body on failure."""
    result = get_worker().legacy_run(input_path, cwd=cwd)
    if result.returncode != 0:
        raise ValueError(
            "Simulation error: " + (result.stderr or result.stdout or "unknown error")
        )


def run_legacy_config_result(input_path: str, cwd: str | None = None) -> LegacyRunResult:
    """Run LegacyRunWebVersion; return LegacyRunResult (for tests checking return codes)."""
    return get_worker().legacy_run(input_path, cwd=cwd)


def get_file_hash(file_path: str) -> str:
    with open(file_path, "rb") as file:
        return hashlib.sha256(file.read()).hexdigest()


def get_package_binary_hash() -> str:
    """Hash native library for simulation result cache keys."""
    src = REPO_ROOT / "src"
    for pattern in ("*.so", "*.dll"):
        for path in src.glob(pattern):
            return get_file_hash(str(path))[:8]
    # Fallback when no local compiled binary exists (e.g. clean checkout after R CMD INSTALL):
    # hash all native sources so result caches invalidate on any engine change.
    parts = []
    for pattern in ("*.cpp", "*.h", "Makevars", "Makevars.win"):
        for p in sorted((src).glob(pattern)):
            if p.is_file():
                parts.append(get_file_hash(str(p)))
    if parts:
        return hashlib.sha256("".join(parts).encode()).hexdigest()[:8]
    raise FileNotFoundError(
        "Cannot locate SmokingHistoryGenerator binary or src hashes for cache key"
    )
