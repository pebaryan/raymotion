"""CLI integration tests. Artifacts are isolated from the project's out directory."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

EXE = Path(sys.argv.pop(1)).resolve()
STARTER = Path(sys.argv.pop(1)).resolve()

class CliTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="raymotion-test-", dir=Path(__file__).resolve().parents[1] / "build")
        self.cwd = Path(self.tmp.name)
        self.addCleanup(self.tmp.cleanup)

    def run_cli(self, *args, exe=EXE):
        result = subprocess.run([str(exe), *args], cwd=self.cwd, capture_output=True, text=True, timeout=180)
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

    def test_encoder_and_preset(self):
        self.run_cli("render", "probe", "--draft", "--no-music", "--encoder", "libx264", "--preset", "ultrafast")
        result = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries", "stream=codec_name,nb_frames", "-of", "json", str(self.cwd / "out" / "probe_draft.mp4")], capture_output=True, text=True, check=True)
        stream = json.loads(result.stdout)["streams"][0]
        self.assertEqual((stream["codec_name"], int(stream["nb_frames"])), ("h264", 60))
        bad = subprocess.run([str(EXE), "render", "probe", "--draft", "--no-music", "--encoder", "no_such_encoder"],
                             cwd=self.cwd, capture_output=True, text=True, timeout=60)
        self.assertEqual(bad.returncode, 3)
        self.assertIn("render failed", bad.stderr)
        self.assertNotIn("rendered", bad.stdout)

    def test_list_generates_no_assets(self):
        listing = self.run_cli("list").stdout
        self.assertIn("raymotion-promo        1920x1080 @60fps  210.0s  audio=1", listing)
        self.assertIn("own-your-compute       1920x1080 @30fps  66.0s  audio=1", listing)
        self.assertEqual(list(self.cwd.iterdir()), [], "list wrote files")

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

    def distinct_colors(self, path):
        raw = subprocess.run(["ffmpeg", "-v", "error", "-i", str(path), "-f", "rawvideo", "-pix_fmt", "rgb24", "-"],
                             capture_output=True, check=True).stdout
        return len({raw[i:i+3] for i in range(0, len(raw), 3)})

    def test_supersampling_antialiases_and_keeps_size(self):
        hard, smooth = self.cwd / "ss1.png", self.cwd / "ss4.png"
        self.run_cli("still", "probe", "--at", "20", "--ss", "1", "--png", str(hard))
        self.run_cli("still", "probe", "--at", "20", "--ss", "4", "--png", str(smooth))
        self.assertEqual(self.png_size(smooth), (1280, 720))
        self.assertGreater(self.distinct_colors(smooth), self.distinct_colors(hard),
                           "supersampled frame has no extra edge shades")

    def rgba(self, *args):
        return subprocess.run(["ffmpeg", "-v", "error", *args, "-frames:v", "1", "-f", "rawvideo", "-pix_fmt", "rgba", "-"],
                              capture_output=True, check=True).stdout

    def test_alpha_still_is_straight_alpha(self):
        clear, opaque = self.cwd / "clear.png", self.cwd / "opaque.png"
        self.run_cli("still", "lowerthird", "--at", "70", "--alpha", "--draft", "--png", str(clear))
        self.run_cli("still", "lowerthird", "--at", "70", "--draft", "--png", str(opaque))
        a, o = self.rgba("-i", str(clear)), self.rgba("-i", str(opaque))
        alphas = a[3::4]
        self.assertEqual(alphas[0], 0, "background is not transparent")
        self.assertTrue(any(0 < x < 255 for x in alphas), "no partial alpha (translucent panel, edges)")
        self.assertTrue(all(x == 255 for x in o[3::4]), "a normal still has alpha")
        # Straight alpha: composited over black, it is the normal render (premultiplied
        # colour would come out darker at every edge).
        err = sum(abs(a[i + k] * a[i + 3] / 255 - o[i + k]) for i in range(0, len(a), 4 * 7) for k in range(3))
        self.assertLess(err / (len(a) / (4 * 7) * 3), 0.5)

    def test_alpha_video_formats(self):
        self.run_cli("render", "lowerthird", "--draft", "--alpha", "--no-music")
        self.run_cli("render", "lowerthird", "--draft", "--alpha", "--format", "webm", "--no-music")
        def probe(name):
            out = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0", "-show_entries",
                                  "stream=codec_name,pix_fmt,width:stream_tags=alpha_mode", "-of", "json",
                                  str(self.cwd / "out" / name)], capture_output=True, text=True, check=True)
            return json.loads(out.stdout)["streams"][0]
        mov, webm = probe("lowerthird_draft.mov"), probe("lowerthird_draft.webm")
        self.assertEqual((mov["codec_name"], mov["width"]), ("prores", 960))
        self.assertTrue(mov["pix_fmt"].startswith("yuva"), mov)
        self.assertEqual((webm["codec_name"], webm["tags"]["alpha_mode"]), ("vp9", "1"))
        # The draft downscale keeps the transparency, and frames decode with it.
        for args in (["-i", str(self.cwd / "out" / "lowerthird_draft.mov")],
                     ["-c:v", "libvpx-vp9", "-i", str(self.cwd / "out" / "lowerthird_draft.webm")]):
            alphas = self.rgba(*args, "-vf", "select=eq(n\\,70)")[3::4]
            self.assertLess(alphas[0], 8)
            self.assertGreater(max(alphas), 200)
        bad = subprocess.run([str(EXE), "render", "lowerthird", "--draft", "--alpha", "--format", "mp4"],
                             cwd=self.cwd, capture_output=True, text=True, timeout=60)
        self.assertEqual(bad.returncode, 1)
        self.assertIn("no alpha", bad.stdout)

    def test_starter_project(self):
        listing = self.run_cli("list", exe=STARTER).stdout
        self.assertIn("hello", listing)
        path = self.cwd / "hello.png"
        self.run_cli("still", "hello", "--at", "45", "--png", str(path), exe=STARTER)
        self.assertEqual(self.png_size(path), (1280, 720))

    def test_props(self):
        defaults = json.loads(self.run_cli("props", "promo", exe=STARTER).stdout)
        self.assertEqual(defaults["secondsPerScene"], 3)
        self.assertIn("promo", self.run_cli("list", exe=STARTER).stdout)

        # Length follows the props: 3 x 1s scenes minus two transition overlaps (18 + 20).
        short = '{"secondsPerScene": 1, "features": ["One"]}'
        self.run_cli("still", "promo", "--at", "51", "--props", short, "--png", "short.png", exe=STARTER)
        late = subprocess.run([str(STARTER), "still", "promo", "--at", "52", "--props", short],
                              cwd=self.cwd, capture_output=True, text=True, timeout=60)
        self.assertNotEqual(late.returncode, 0, "frame past the props-derived duration was accepted")

        # @file and colour props reach the frame: the outro background is the accent.
        (self.cwd / "props.json").write_text('{"accent": "#ff0000"}', encoding="utf-8")
        self.run_cli("still", "promo", "--at", "200", "--props", "@props.json", "--png", "red.png", exe=STARTER)
        raw = subprocess.run(["ffmpeg", "-v", "error", "-i", str(self.cwd / "red.png"), "-vf", "crop=1:1:5:5",
                              "-f", "rawvideo", "-pix_fmt", "rgb24", "-"], capture_output=True, check=True).stdout
        self.assertEqual(tuple(raw), (255, 0, 0))

        bad = subprocess.run([str(STARTER), "still", "promo", "--at", "0", "--props", '{"title": tru}'],
                             cwd=self.cwd, capture_output=True, text=True, timeout=60)
        self.assertEqual(bad.returncode, 2)
        self.assertIn("line 1, column", bad.stderr)

if __name__ == "__main__":
    unittest.main()
