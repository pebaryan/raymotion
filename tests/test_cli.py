"""CLI integration tests. Artifacts are isolated from the project's out directory."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

EXE = Path(sys.argv.pop(1)).resolve()

class CliTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="raymotion-test-", dir=Path(__file__).resolve().parents[1] / "build")
        self.cwd = Path(self.tmp.name)
        self.addCleanup(self.tmp.cleanup)

    def run_cli(self, *args):
        result = subprocess.run([str(EXE), *args], cwd=self.cwd, capture_output=True, text=True, timeout=180)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        return result

    def png_size(self, path):
        import struct
        return struct.unpack(">II", path.read_bytes()[16:24])

    def test_composition_resolution_not_screen_resolution(self):
        path = self.cwd / "full.png"
        self.run_cli("still", "demo-neon", "--at", "0", "--png", str(path))
        self.assertEqual(self.png_size(path), (1920, 1080))

    def test_draft_scales_the_whole_composition(self):
        full = self.cwd / "full.png"
        draft = self.cwd / "draft.png"
        self.run_cli("still", "fx-2-bloom", "--at", "1", "--png", str(full))
        self.run_cli("still", "fx-2-bloom", "--at", "1", "--draft", "--png", str(draft))
        self.assertEqual(self.png_size(draft), (960, 516))
        def pixels(path, scale=False):
            args = ["ffmpeg", "-v", "error", "-i", str(path)]
            if scale:
                args += ["-vf", "scale=960:516"]
            return subprocess.run(args + ["-f", "rawvideo", "-pix_fmt", "rgb24", "-"], capture_output=True, check=True).stdout
        expected, actual = pixels(full, True), pixels(draft)
        self.assertEqual(len(expected), len(actual))
        error = sum(abs(a-b) for a,b in zip(expected, actual)) / len(actual)
        self.assertLess(error, 5.0, "draft is cropped instead of downscaled")

    def test_stale_frames_do_not_extend_render(self):
        import json
        frames = self.cwd / "out" / "probe_draft_frames"
        frames.mkdir(parents=True)
        stale = frames / "f00060.png"
        self.run_cli("still", "probe", "--draft", "--at", "0", "--png", str(stale))
        note = frames / "keep.txt"
        note.write_text("not a generated frame")
        self.run_cli("render", "probe", "--draft", "--no-music")
        self.assertFalse(stale.exists(), "previous render's trailing frame survived")
        self.assertTrue(note.exists(), "unrelated file was deleted")
        result = subprocess.run(["ffprobe", "-v", "error", "-count_frames", "-select_streams", "v:0", "-show_entries", "stream=nb_read_frames,width,height", "-of", "json", str(self.cwd / "out" / "probe_draft.mp4")], capture_output=True, text=True, check=True)
        stream = json.loads(result.stdout)["streams"][0]
        self.assertEqual(int(stream["nb_read_frames"]), 60)
        self.assertEqual((stream["width"], stream["height"]), (640, 360))

    def test_nested_png_filename(self):
        self.run_cli("still", "probe", "--at", "0", "--png", "nested/images/frame.png")
        self.assertTrue((self.cwd / "nested/images/frame.png").is_file())

    def test_failed_png_write_is_reported(self):
        (self.cwd / "frame.png").mkdir()
        result = subprocess.run([str(EXE), "still", "probe", "--at", "0", "--png", "frame.png"], cwd=self.cwd, capture_output=True, text=True, timeout=30)
        self.assertEqual(result.returncode, 2)
        self.assertIn("cannot write frame", result.stderr)

    def test_bare_png_filename(self):
        self.run_cli("still", "probe", "--at", "0", "--png", "frame.png")
        self.assertTrue((self.cwd / "frame.png").is_file())

if __name__ == "__main__":
    unittest.main()
