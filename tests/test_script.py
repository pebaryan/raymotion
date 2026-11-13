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

    def run_cli(self, *args, ok=True, timeout=45):
        result = subprocess.run([EXE, *map(str, args)], cwd=self.cwd,
                                text=True, capture_output=True, timeout=timeout)
        if ok is not None:
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
        lower_third = pathlib.Path(__file__).resolve().parents[1] / "examples/scripts/lower_third.as"
        self.run_cli("still", "lower-third", "--script", lower_third, "--at", 90,
                     "--props", '{"title":"Runtime API"}', "--ss", 1, "--png", "lower-third.png")
        self.assertGreater((self.cwd / "lower-third.png").stat().st_size, 1000)
        bounded = CONFIG.replace('c.duration = 4;', 'c.duration = 4; c.statementBudget = 5;') + '''
void first(int t) { int i = 0; while (i < 100) i++; }
void second(int t) { }
'''
        self.path.write_text(bounded)
        output = self.run_cli("still", "fixture", "--script", self.path, "--at", 0,
                              "--ss", 1, "--png", "budget.png", ok=False, timeout=5)
        self.assertIn("statement budget exceeded", output)

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

    def test_every_shipped_example(self):
        # Each example compiles, configures from another directory, is UTF-8, and opens with
        # the two header lines the Studio's gallery reads: "// Title -- what it shows." and
        # "// Category: Name".
        examples = sorted(SAMPLE.parent.glob("*.as"))
        self.assertGreater(len(examples), 50)
        for example in examples:
            with self.subTest(example=example.name):
                lines = example.read_bytes().decode("utf-8").splitlines()
                self.assertRegex(lines[0], r"^// \S.* -- \S.*[.)]$")
                self.assertRegex(lines[1], r"^// Category: \S")
                self.assertIn("fps", self.run_cli("list", "--script", example))

    def test_app_exception_keeps_its_message(self):
        # A C++ exception from a bound function used to reach the user as
        # "Caught an exception from the application".
        self.path.write_text(CONFIG.replace('scene(0, 2, "first");', 'media::resolve("no such file.png");'))
        output = self.run_cli("list", "--script", self.path, ok=False)
        self.assertIn("file not found", output)
        self.assertNotIn("Caught an exception", output)

    @unittest.skipUnless(GL, "needs OpenGL")
    def test_splat_color_function_in_draw(self):
        example = SAMPLE.parent / "splat_color_function.as"
        self.run_cli("still", "splat-color-function", "--script", example, "--at", 10, "--ss", 1, "--png", "splat.png")
        self.assertGreater((self.cwd / "splat.png").stat().st_size, 1000)

    @unittest.skipUnless(GL, "needs OpenGL")
    def test_map_helpers_draw(self):
        # geo::base, choropleth, colorbar, labels, highlight, markers, route, fly, focus,
        # valuesAt and the geo-route / geo-flyto prefabs: every frame sampled must draw.
        for stem, name, frames in (("map_choropleth", "map-choropleth", (20, 149)), ("map_route", "map-route", (10, 95, 179)),
                                   ("map_timeline", "map-timeline", (100,)), ("map_prefabs", "map-prefabs", (45, 150))):
            for frame in frames:
                png = f"{stem}-{frame}.png"
                self.run_cli("still", name, "--script", SAMPLE.parent / f"{stem}.as", "--at", frame, "--ss", 1, "--png", png)
                self.assertGreater((self.cwd / png).stat().st_size, 20000, png)

    @unittest.skipUnless(GL, "needs OpenGL")
    def test_preview_reloads_on_save(self):
        # The preview window picks up a saved script, survives one that does not compile
        # and one that throws while drawing, and takes the next good save.
        import os, time
        env = dict(os.environ, RAYMOTION_PREVIEW_SECONDS="13", RAYMOTION_PREVIEW_SHOT="shot.png")
        proc = subprocess.Popen([EXE, "preview", "fixture", "--script", str(self.path), "--mute"], cwd=self.cwd,
                                text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
        try:
            time.sleep(3)
            self.path.write_text(SOURCE.replace("c.h = 32", "c.h = 48").replace("c.duration = 4", "c.duration = 6")
                                 .replace('scene(2, 2, "second")', 'scene(2, 4, "second")'))
            time.sleep(2.5)
            self.path.write_text(SOURCE.replace("draw::rect(0, 0, 64, 32, 0xff0000);", "this is not angelscript;"))
            time.sleep(2.5)
            self.path.write_text(SOURCE.replace("draw::rect(0, 0, 64, 32, 0xff0000);", "array<int> a; a[3] = 1;"))
            time.sleep(2.5)
            out, err = proc.communicate(timeout=30)
        finally:
            if proc.poll() is None:
                proc.kill()
        self.assertEqual(proc.returncode, 0, out + err)
        self.assertIn("reloaded fixture (64x48, 6 frames)", out)
        self.assertIn("reloaded fixture (64x32, 4 frames)", out)       # the last save compiles; it throws when drawn
        self.assertRegex(err, r"scene with spaces\.as:\d+:\d+: ")       # the compile error, with its place
        self.assertIn("out of bounds", err.lower())
        self.assertEqual(err.lower().count("out of bounds"), 1, err)   # reported once, not every frame
        self.assertGreater((self.cwd / "shot.png").stat().st_size, 500)

    def test_preview_no_reload_flag(self):
        self.assertIn("--no-reload", self.run_cli("help", ok=None))

    def test_map_helper_errors(self):
        head = CONFIG.replace('scene(0, 2, "first"); scene(2, 2, "second");', '')
        # Drawing calls belong in draw callbacks; data helpers work anywhere.
        self.path.write_text(head.replace('c.name = "fixture";', 'c.name = "fixture"; geo::base(geo::projection("equalEarth"));'))
        self.assertIn("geo::base is only available inside draw callbacks", self.run_cli("list", "--script", self.path, ok=False))
        self.path.write_text(head.replace('c.name = "fixture";', '''c.name = "fixture";
    array<double> stops = {0, 0, 90, 0};
    array<double> at;
    geo::along(stops, 0.5, at);
    if (abs(at[0] - 45) > 1e-6 || abs(at[2] - 90) > 1e-6 || abs(geo::length(stops) - 10007.5) > 1) { int zero = 0; int bad = 1 / zero; }
    if (geo::valuesAt("code,2000,2010\\nIDN,10,20\\n", 2005) != "{\\"IDN\\":15}") { int zero = 0; int bad = 1 / zero; }
    if (geo::layer("countries").count() != 177) { int zero = 0; int bad = 1 / zero; }
    geo::GeoProjection@ a = geo::projection("orthographic");
    geo::GeoProjection@ b = geo::projection("orthographic");
    b.rotate(90, 0);
    if (abs(geo::fly(a, b, 0.5, 1).centerLongitude() - 45) > 1e-6) { int zero = 0; int bad = 1 / zero; }
    scene(0, 4, "first");''') + "void first(int t) {}\n")
        self.assertIn("64x32", self.run_cli("list", "--script", self.path))
        for call, message in (('geo::layer("moon")', "unknown geographic layer"),
                              ('array<double> one = {1, 2, 3}; geo::length(one)', "lon/lat pairs"),
                              ('geo::fly(null, null, 0.5)', "needs two projections")):
            self.path.write_text(head.replace('c.name = "fixture";', f'c.name = "fixture"; {call};'))
            self.assertIn(message, self.run_cli("list", "--script", self.path, ok=False))

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
            (SOURCE.replace('c.duration = 4;', 'c.duration = 4; c.statementBudget = 100000001;'), "statementBudget"),
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

    def test_statement_budget_aborts_with_diagnostic(self):
        self.path.write_text(CONFIG.replace('scene(0, 2, "first"); scene(2, 2, "second");',
                                            'while (true) {}'))
        output = self.run_cli("list", "--script", self.path, ok=False, timeout=5)
        self.assertIn("statement budget exceeded", output)

    def test_global_variable_is_rejected_during_build(self):
        self.path.write_text("int counter = 0;\n" + SOURCE)
        output = self.run_cli("list", "--script", self.path, ok=False, timeout=5)
        self.assertIn("Global variables", output)
        self.assertIn(":1:", output)

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
