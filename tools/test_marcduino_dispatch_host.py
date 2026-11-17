#!/usr/bin/env python3
"""Host-compiled regression test for Marcduino command dispatch.

Builds tools/host/marcduino_dispatch_harness.cpp against ReelTwo's real
core/Animation.h and core/Marcduino.h from .pio/libdeps (run `make build`
first) and the firmware's real IngressAnimationPlayer class and
drainMarcduinoCommandQueue() body, then runs it.

Guards queued dispatch against: a handler reading its Marcduino::getCommand()
arguments after the drain buffer is gone; a burst where only the last command
runs; and a DM:* dome sequence requested earlier in a burst starting after (and
overriding) a later command such as :SE00.
"""

from __future__ import annotations

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
REELTWO_SRC = ROOT / ".pio" / "libdeps" / "astropixelsplus" / "Reeltwo" / "src"
SHIM = ROOT / "tools" / "host" / "reeltwo_shim"
HARNESS = ROOT / "tools" / "host" / "marcduino_dispatch_harness.cpp"


def extract(text: str, start: str, end: str) -> str:
    start_index = text.find(start)
    if start_index == -1:
        raise AssertionError(f"missing block start {start!r}")
    end_index = text.find(end, start_index)
    if end_index == -1:
        raise AssertionError(f"missing block end {end!r}")
    return text[start_index : end_index + len(end)]


class MarcduinoDispatchHostTest(unittest.TestCase):
    def test_queued_and_legacy_dispatch(self) -> None:
        self.assertTrue(
            (REELTWO_SRC / "core" / "Marcduino.h").is_file(),
            f"ReelTwo sources not found at {REELTWO_SRC}; run `make build` first",
        )
        compiler = shutil.which("g++") or shutil.which("c++")
        self.assertIsNotNone(compiler, "no host C++ compiler (g++/c++) on PATH")

        ino = (ROOT / "AstroPixelsPlus.ino").read_text(encoding="utf-8")
        ingress = (ROOT / "MarcduinoIngress.h").read_text(encoding="utf-8")
        player_class = extract(ino, "class IngressAnimationPlayer : public AnimationPlayer", "\n};\n")
        drain = extract(ingress, "static void drainMarcduinoCommandQueue()\n{", "\n}\n")
        dome_dispatch = extract(ino, "static void dispatchPendingDomeAnimation()\n{", "\n}\n")

        with tempfile.TemporaryDirectory() as tmp:
            tmp_path = Path(tmp)
            (tmp_path / "firmware_player_class.inc").write_text(player_class, encoding="utf-8")
            (tmp_path / "firmware_drain.inc").write_text(drain, encoding="utf-8")
            (tmp_path / "firmware_dome_dispatch.inc").write_text(dome_dispatch, encoding="utf-8")
            binary = tmp_path / "harness"
            build = subprocess.run(
                [
                    compiler,
                    "-std=gnu++11",
                    "-O0",
                    "-w",
                    f"-I{tmp_path}",
                    f"-I{SHIM}",
                    f"-I{REELTWO_SRC}",
                    str(HARNESS),
                    "-o",
                    str(binary),
                ],
                capture_output=True,
                text=True,
            )
            self.assertEqual(build.returncode, 0, f"harness build failed:\n{build.stderr}")
            run = subprocess.run(
                [str(binary)], capture_output=True, text=True, errors="backslashreplace"
            )
            self.assertEqual(run.returncode, 0, f"dispatch harness failed:\n{run.stdout}{run.stderr}")


if __name__ == "__main__":
    unittest.main()
