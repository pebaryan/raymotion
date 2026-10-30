"""Use the existing native starter unchanged; compare runtime and direct builds."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

HOST = Path(sys.argv[1]).resolve()
DIRECT = Path(sys.argv[2]).resolve()
SDK = Path(sys.argv[3]).resolve()
# The engine archive, from CMake: its directory depends on the generator (build/ or build/Release/).
ENGINE = Path(sys.argv[4]).resolve()
GL = "--gl" in sys.argv[5:]
ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "examples/starter/main.cpp"
sys.argv[1:] = []


class NativeCpp(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="cpp runtime ", dir=HOST.parent)
        self.addCleanup(self.temp.cleanup)
        self.cwd = Path(self.temp.name)

    def call(self, *args, source=SOURCE, direct=False, ok=True):
        command = [str(DIRECT if direct else HOST), *map(str, args)]
        if not direct:
            command.extend(["--script", str(source)])
        result = subprocess.run(command, cwd=self.cwd, capture_output=True, text=True, timeout=120)
        self.assertEqual(result.returncode, 0 if ok else 2, result.stdout + result.stderr)
        return result.stdout + result.stderr

    def test_unchanged_source_and_props(self):
        self.assertEqual(self.call("list"), self.call("list", direct=True))
        self.assertEqual(json.loads(self.call("props", "promo")), json.loads(self.call("props", "promo", direct=True)))

    def test_cache_and_header_dependencies(self):
        source = self.cwd / "scene with spaces.cpp"
        header = self.cwd / "size.hpp"
        source.write_text('#include "size.hpp"\n' + SOURCE.read_text().replace("c.w = 1280;", "c.w = FIXTURE_WIDTH;"))
        header.write_text("#define FIXTURE_WIDTH 640\n")
        unchanged = {p: p.stat().st_mtime_ns for p in (HOST, ENGINE)}
        self.assertIn("640x720", self.call("list", source=source))
        key = hashlib.sha256((source.as_posix() + "|registerComps").encode()).hexdigest()[:20]
        cache = SDK.parents[1] / "cpp-cache" / SDK.name / key
        binary = cache / "build/bin" / ("scene.exe" if sys.platform == "win32" else "scene")
        timestamp = binary.stat().st_mtime_ns
        self.assertIn("640x720", self.call("list", source=source))
        self.assertEqual(timestamp, binary.stat().st_mtime_ns)
        header.write_text("#define FIXTURE_WIDTH 800\n")
        self.assertIn("800x720", self.call("list", source=source))
        self.assertNotEqual(timestamp, binary.stat().st_mtime_ns)
        source.write_text(source.read_text().replace("c.h = 720;", "c.h = 480;"))
        self.assertIn("800x480", self.call("list", source=source))
        for path, timestamp in unchanged.items():
            self.assertEqual(path.stat().st_mtime_ns, timestamp, f"unexpected rebuild: {path}")

    def test_diagnostics_and_entry_function(self):
        source = self.cwd / "bad.cpp"
        source.write_text("this is invalid C++\n")
        self.assertIn("compilation failed", self.call("list", source=source, ok=False))
        self.assertIn("cannot read", self.call("list", source=self.cwd / "missing.cpp", ok=False))
        self.assertIn("function name", self.call("list", "--cpp-entry", "bad();", ok=False))
        source.write_text(SOURCE.read_text().replace("registerComps", "registerMovie"))
        self.assertIn("hello", self.call("list", "--cpp-entry", "registerMovie", source=source))

    @unittest.skipUnless(GL, "needs OpenGL")
    def test_render_matches_native_and_forwards_arguments(self):
        # JSON string is built by the JSON encoder, preserving quotes and semicolons.
        props = json.dumps({"title": 'Space; quote " and $ literal', "secondsPerScene": 0.1})
        for name, frame in [("hello", 45), ("promo", 1)]:
            for direct in [False, True]:
                self.call("still", name, "--at", frame, "--props", props,
                          "--ss", 1, "--png", f"{name}-{direct}.png", direct=direct)
            self.assertEqual((self.cwd / f"{name}-False.png").read_bytes(),
                             (self.cwd / f"{name}-True.png").read_bytes())
        for direct in [False, True]:
            self.call("render", "promo", "--props", props, "--ss", 1,
                      "--png-frames", "--jobs", 2, direct=direct)
            frames = [p.read_bytes() for p in sorted((self.cwd / "out/promo_frames").glob("*.png"))]
            if direct:
                self.assertEqual(frames, runtime_frames)
            else:
                runtime_frames = frames


if __name__ == "__main__":
    unittest.main()
