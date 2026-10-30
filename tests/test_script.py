"""Runtime loader regressions. Fixtures are generated in a temporary working directory."""
import pathlib
import subprocess
import sys
import tempfile
import unittest

EXE = str(pathlib.Path(sys.argv[1]).resolve())
SAMPLE = pathlib.Path(__file__).resolve().parents[1] / "examples/scripts/hello.as"
GL = "--gl" in sys.argv[2:]
sys.argv[1:] = []

CONFIG = '''void configure(Composition &inout c) {
    c.name = "fixture"; c.w = 64; c.h = 32; c.duration = 4; c.supersample = 1;
    c.w = int(props::number("width", 64));
    scene(0, 2, "first"); scene(2, 2, "second");
}
'''
SOURCE = CONFIG + '''
void first(int t) {
    if (!props::flag("visible", true)) return;
    double u = ease(E_OUT_CUBIC, clamp01(double(t) / 2));
    double seeded = rnd01(42, 7);
    double ranged = rrange(42, 7, 0, 1);
    if (seeded != ranged || u < 0 || sqrt(4.0f) != 2.0f) { int zero = 0; int bad = 1/zero; }
    draw::rect(0, 0, 64, 32, 0xff0000);
    draw::circle(8 + 8*t, 16, 3, 0xffffff);
}
void second(int t) {
    draw::rect(0, 0, 64, 32, 0x0000ff);
    draw::rect(8*t, 8, 4, 4, 0xffffff);
}
'''


class Scripts(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="raymotion scripts ", dir=pathlib.Path(EXE).parent)
        self.addCleanup(self.temp.cleanup)
        self.cwd = pathlib.Path(self.temp.name)
        self.path = self.cwd / "scene with spaces.as"
        self.path.write_text(SOURCE)

    def run_cli(self, *args, ok=True):
        result = subprocess.run([EXE, *map(str, args)], cwd=self.cwd,
                                text=True, capture_output=True, timeout=45)
        self.assertEqual(result.returncode, 0 if ok else 2, result.stdout + result.stderr)
        return result.stdout + result.stderr

    def test_list_from_other_directory(self):
        self.assertIn("64x32", self.run_cli("--script", self.path, "list"))
        self.assertFalse((self.cwd / "out").exists())

    def test_shipped_sample(self):
        self.assertIn("hello-script", self.run_cli("list", "--script", SAMPLE))

    @unittest.skipUnless(GL, "needs OpenGL")
    def test_shipped_sample_draws(self):
        self.run_cli("still", "hello-script", "--script", SAMPLE, "--at", 110, "--ss", 1, "--png", "sample.png")
        self.assertGreater((self.cwd / "sample.png").stat().st_size, 1000)

    def test_reload_after_edit_without_build(self):
        self.assertIn("64x32", self.run_cli("list", "--script", self.path))
        self.path.write_text(SOURCE.replace('c.h = 32', 'c.h = 48'))
        self.assertIn("64x48", self.run_cli("list", "--script", self.path))

    def test_default_draw_function_and_multiple_files(self):
        self.path.write_text(SOURCE.replace('scene(0, 2, "first"); scene(2, 2, "second");', '') +
                             '\nvoid draw(int t) { first(t); }')
        other = self.cwd / "other.as"
        other.write_text(SOURCE.replace('"fixture"', '"other"'))
        output = self.run_cli("list", "--script", self.path, "--script", other)
        self.assertIn("fixture", output)
        self.assertIn("other", output)

    def test_compile_diagnostics(self):
        self.path.write_text("void configure(Composition &inout c) { unknown(); }")
        output = self.run_cli("list", "--script", self.path, ok=False)
        self.assertIn(str(self.path), output)
        self.assertIn(":1:", output)

    def test_invalid_inputs(self):
        self.assertIn("requires", self.run_cli("list", "--script", ok=False))
        self.assertIn("cannot read", self.run_cli("list", "--script", "missing.as", ok=False))
        self.assertIn("no script backend", self.run_cli("list", "--script", "future.lua", ok=False))
        self.assertIn("duplicate", self.run_cli("list", "--script", self.path, "--script", self.path, ok=False))

    def test_contract_failures(self):
        cases = [
            (SOURCE.replace('c.w = 64', 'c.w = 0').replace('c.w = int(props::number("width", 64));', ''), "invalid"),
            (SOURCE.replace('scene(0, 2, "first")', 'scene(3, 2, "first")'), "timeline"),
            (SOURCE.replace('void first(int t)', 'void missing(int t)'), "missing void first"),
            (SOURCE.replace('void configure', 'void missing'), "missing void configure"),
            ('int counter = 0;\n' + SOURCE, "Global variables"),
            (SOURCE.replace('c.name = "fixture";', 'draw::circle(1,1,1,0); c.name = "fixture";'), "only available"),
            (SOURCE.replace('c.name = "fixture";', 'while(true) {} c.name = "fixture";'), "budget"),
        ]
        for source, message in cases:
            with self.subTest(message=message):
                self.path.write_text(source)
                self.assertIn(message, self.run_cli("list", "--script", self.path, ok=False))

    @unittest.skipUnless(GL, "needs OpenGL")
    def test_frames_props_and_parallel_workers(self):
        import struct
        def png_size(path):
            return struct.unpack(">II", path.read_bytes()[16:24])
        for frame in [3, 0, 2, 1, 3]:
            self.run_cli("still", "fixture", "--script", self.path, "--at", frame, "--png", f"f{frame}.png")
        self.run_cli("render", "fixture", "--script", self.path, "--png-frames", "--jobs", 2)
        for frame in range(4):
            self.assertEqual((self.cwd / f"f{frame}.png").read_bytes(),
                             (self.cwd / f"out/fixture_frames/f{frame:05}.png").read_bytes())
        self.run_cli("still", "fixture", "--script", self.path, "--props", '{"width":80}', "--png", "props.png")
        self.assertEqual(png_size(self.cwd / "props.png"), (80, 32))
        self.path.write_text(SOURCE.replace('draw::circle(8 + 8*t, 16, 3, 0xffffff);',
            'draw::text(props::text("title", "text"), 0, 0, 12, 0xffffff);'))
        self.run_cli("still", "fixture", "--script", self.path, "--png", "text.png")
        self.path.write_text(SOURCE.replace('draw::circle(8 + 8*t, 16, 3, 0xffffff);', 'int zero = 0; int bad = 1/zero;'))
        self.assertIn("Divide by zero", self.run_cli("still", "fixture", "--script", self.path, ok=False))


if __name__ == "__main__":
    unittest.main()
