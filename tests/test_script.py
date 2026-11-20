"""Runtime loader regressions. Fixtures are generated in a temporary working directory."""
import json
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


def png_rows(path):
    """The rows of an 8-bit RGB or RGBA PNG as bytearrays, and the bytes a pixel takes."""
    import struct
    import zlib
    data = pathlib.Path(path).read_bytes()
    pos, idat, header = 8, b"", None
    while pos < len(data):
        size, tag = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + size]
        if tag == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif tag == b"IDAT":
            idat += body
        pos += 12 + size
    w, h, depth, ctype = header[:4]
    bpp = {2: 3, 6: 4}[ctype]
    raw, stride, rows, prev = zlib.decompress(idat), w * bpp, [], bytearray(w * bpp)
    for y in range(h):
        kind, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            if kind == 1: line[i] = (line[i] + a) & 255
            elif kind == 2: line[i] = (line[i] + b) & 255
            elif kind == 3: line[i] = (line[i] + ((a + b) >> 1)) & 255
            elif kind == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        prev = line
        rows.append(line)
    return rows, bpp


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
    def test_grass_field(self):
        # A "grass" object draws, the same frame comes out the same in any order (the field is
        # kept between frames), and bad presets and grids say what is wrong.
        def write(preset="meadow", density=300, extra=""):
            grass = (f'\\"preset\\":\\"{preset}\\",\\"min\\":[-6,-6],\\"max\\":[6,6],'
                     f'\\"density\\":{density},\\"seed\\":3{extra}')
            self.path.write_text('''void configure(Composition &inout c) {
    c.name = "grass"; c.w = 160; c.h = 90; c.fps = 30; c.duration = 10; c.supersample = 1;
    scene(0, c.duration, "draw");
}
void draw(int frame) {
    g3d::draw("{\\"camera\\":{\\"position\\":[0,1.2,4],\\"target\\":[0,0.3,0]},"
              "\\"lights\\":[{\\"type\\":\\"directional\\",\\"position\\":[2,4,-6],\\"castShadow\\":true}],"
              "\\"objects\\":[{\\"geometry\\":\\"plane\\",\\"width\\":20,\\"depth\\":20,\\"material\\":{\\"color\\":\\"#c828c8\\"}},"
              "{\\"grass\\":{''' + grass + '''}}]}", frame, 0, 0, 160, 90);
}
''')
        write()
        for frame in (5, 4, 5):
            self.run_cli("still", "grass", "--script", self.path, "--at", frame, "--png", f"g{frame}-{len(list(self.cwd.glob('*.png')))}.png")
        first, moved, again = (self.cwd / "g5-0.png"), (self.cwd / "g4-1.png"), (self.cwd / "g5-2.png")
        self.assertEqual(first.read_bytes(), again.read_bytes())
        self.assertNotEqual(first.read_bytes(), moved.read_bytes())       # the wind moves it
        write(density=0)
        self.run_cli("still", "grass", "--script", self.path, "--at", 5, "--png", "bare.png")
        self.assertNotEqual(first.read_bytes(), (self.cwd / "bare.png").read_bytes())
        write(preset="jungle")
        self.assertIn('no grass look "jungle"', self.run_cli("still", "grass", "--script", self.path, "--png", "x.png", ok=False))
        write(extra=',\\"mask\\":{\\"width\\":3,\\"height\\":3,\\"values\\":[1,0]}')
        self.assertIn("grass mask needs", self.run_cli("still", "grass", "--script", self.path, "--png", "x.png", ok=False))

    @unittest.skipUnless(GL, "needs OpenGL")
    def test_flora_area_scatter_terrain(self):
        # One area feeds a terrain, grass and scattered instances; frames repeat exactly, and
        # bad areas and oversized scatters say what is wrong.
        def write(area, spacing=3, grass_extra=""):
            land = area.replace('"', '\\"')
            self.path.write_text('''void configure(Composition &inout c) {
    c.name = "land"; c.w = 160; c.h = 90; c.fps = 30; c.duration = 10; c.supersample = 1;
    scene(0, c.duration, "draw");
}
void draw(int frame) {
    string land = "''' + land + '''";
    g3d::draw("{\\"camera\\":{\\"position\\":[0,14,30],\\"target\\":[0,2,0]},"
              "\\"lights\\":[{\\"type\\":\\"directional\\",\\"position\\":[10,20,-10],\\"castShadow\\":true,\\"shadowExtent\\":30}],"
              "\\"objects\\":[{\\"geometry\\":\\"terrain\\",\\"area\\":" + land + ",\\"resX\\":40,\\"resZ\\":40,\\"material\\":{\\"color\\":\\"#c828c8\\"}},"
              "{\\"grass\\":{\\"preset\\":\\"lawn\\",\\"area\\":" + land + ",\\"seed\\":2''' + grass_extra + '''}},"
              "{\\"geometry\\":\\"cone\\",\\"radius\\":0.8,\\"height\\":2.5,\\"slices\\":6,"
              "\\"instances\\":{\\"scatter\\":{\\"area\\":" + land + ",\\"spacing\\":''' + str(spacing) + ''',\\"lift\\":1.25,"
              "\\"tints\\":[\\"#2f5a2a\\",\\"#5a7a30\\"],\\"seed\\":4}}}]}", frame, 0, 0, 160, 90);
}
''')
        hills = ('{"min":[-25,-25],"max":[25,25],"ground":{"hills":{"height":4,"scale":20,"seed":3}},'
                 '"strips":[{"points":[[-25,0],[25,6]],"width":4}],"clearings":[[8,-8,5]],"maxSlope":35}')
        write(hills)
        for name, frame in (("a", 3), ("b", 7), ("c", 3)):
            self.run_cli("still", "land", "--script", self.path, "--at", frame, "--png", f"{name}.png")
        self.assertEqual((self.cwd / "a.png").read_bytes(), (self.cwd / "c.png").read_bytes())
        self.assertGreater((self.cwd / "a.png").stat().st_size, 5000)
        write('{"min":[-25,-25],"max":[25,25],"strips":[{"points":[[0,0]],"width":4}]}')
        self.assertIn("strip points", self.run_cli("still", "land", "--script", self.path, "--png", "x.png", ok=False))
        write(hills.replace('"min":[-25,-25],"max":[25,25]', '"min":[-900,-900],"max":[900,900]'), spacing=4)
        self.assertIn("at most", self.run_cli("still", "land", "--script", self.path, "--png", "x.png", ok=False, timeout=90))
        write(hills, grass_extra=',\\"mask\\":{\\"width\\":2,\\"height\\":2,\\"values\\":[1,1,1,1]}')
        self.assertIn("not both", self.run_cli("still", "land", "--script", self.path, "--png", "x.png", ok=False))

    @unittest.skipUnless(GL, "needs OpenGL")
    def test_flora_forest(self):
        # A forest from presets over a scattered area, and one at given spots; frames repeat
        # exactly, and bad trees say what is wrong.
        def write(forest):
            world = json.dumps({
                "camera": {"position": [0, 12, 40], "target": [0, 5, 0]},
                "lights": [{"type": "directional", "position": [10, 20, -10], "castShadow": True, "shadowExtent": 30}],
                "objects": [{"geometry": "plane", "width": 120, "depth": 120, "material": {"color": "#c828c8"}},
                            {"forest": forest}]})
            literal = world.replace("\\", "\\\\").replace('"', '\\"')
            self.path.write_text('''void configure(Composition &inout c) {
    c.name = "wood"; c.w = 160; c.h = 90; c.fps = 30; c.duration = 10; c.supersample = 1;
    scene(0, c.duration, "draw");
}
void draw(int frame) { g3d::draw("''' + literal + '''", frame, 0, 0, 160, 90); }
''')
        build = {"frames": 4, "views": 4, "viewSize": 32}
        write({"trees": ["shrub", {"preset": "birch", "height": 8, "season": 0.7, "seed": 3}], "build": build,
               "scatter": {"area": {"min": [-30, -30], "max": [30, 10]}, "spacing": 6, "clumping": 0.3, "seed": 2},
               "reducedFrom": 20, "impostorFrom": 45, "windDirection": [1, 0]})
        for name, frame in (("a", 3), ("b", 7), ("c", 3)):
            self.run_cli("still", "wood", "--script", self.path, "--at", frame, "--png", f"{name}.png", timeout=120)
        self.assertEqual((self.cwd / "a.png").read_bytes(), (self.cwd / "c.png").read_bytes())
        self.assertNotEqual((self.cwd / "a.png").read_bytes(), (self.cwd / "b.png").read_bytes())
        write({"trees": ["shrub"], "build": build, "at": [[0, 0, 0], [4, 0, -3, 45, 1.5, 0]]})
        self.run_cli("still", "wood", "--script", self.path, "--png", "at.png", timeout=120)
        # Far canopy: with canopyFrom inside the forest the picture changes, and repeats exactly.
        far = {"trees": ["shrub", "birch"], "build": build, "wind": 0,
               "scatter": {"area": {"min": [-60, -60], "max": [60, 60]}, "spacing": 5, "seed": 2}}
        write(far)
        self.run_cli("still", "wood", "--script", self.path, "--at", 3, "--png", "far0.png", timeout=120)
        write(dict(far, canopyFrom=20, canopyCell=16, canopyDetail=2, canopyBlend=4, canopyShadows=False))
        self.run_cli("still", "wood", "--script", self.path, "--at", 3, "--png", "far1.png", timeout=120)
        self.run_cli("still", "wood", "--script", self.path, "--at", 3, "--png", "far2.png", timeout=120)
        self.assertNotEqual((self.cwd / "far0.png").read_bytes(), (self.cwd / "far1.png").read_bytes())
        self.assertEqual((self.cwd / "far1.png").read_bytes(), (self.cwd / "far2.png").read_bytes())
        write({"trees": ["baobab"], "build": build, "at": [[0, 0, 0]]})
        self.assertIn("unknown preset", self.run_cli("still", "wood", "--script", self.path, "--png", "x.png", ok=False))
        write({"trees": [], "at": [[0, 0, 0]]})
        self.assertIn("1..8 trees", self.run_cli("still", "wood", "--script", self.path, "--png", "x.png", ok=False))
        write({"trees": ["oak"], "build": {"views": 99}, "at": [[0, 0, 0]]})
        self.assertIn("views 1..32", self.run_cli("still", "wood", "--script", self.path, "--png", "x.png", ok=False))

    @unittest.skipUnless(GL, "needs OpenGL")
    def test_flora_growth(self):
        # Grass with an arrival front and a growing grove: at grow 0 both draw nothing (the
        # picture is the bare ground), part way they show, and a big growing forest is refused.
        def write(objects):
            world = json.dumps({
                "camera": {"position": [0, 10, 26], "target": [0, 2, 0]},
                "lights": [{"type": "directional", "position": [10, 20, -10], "castShadow": True, "shadowExtent": 30}],
                "objects": [{"geometry": "plane", "width": 120, "depth": 120, "material": {"color": "#c828c8"}}] + objects})
            literal = world.replace("\\", "\\\\").replace('"', '\\"')
            self.path.write_text('''void configure(Composition &inout c) {
    c.name = "grow"; c.w = 160; c.h = 90; c.fps = 30; c.duration = 10; c.supersample = 1;
    scene(0, c.duration, "draw");
}
void draw(int frame) { g3d::draw("''' + literal + '''", frame, 0, 0, 160, 90); }
''')

        def growing(u, spot=12):
            return [{"grass": {"preset": "meadow", "min": [-20, -20], "max": [20, 20], "seed": 1,
                               "arrival": {"from": [0, 0], "reach": 20}, "grow": u}},
                    {"forest": {"trees": ["shrub"], "build": {"frames": 4, "views": 4, "viewSize": 32},
                                "scatter": {"area": {"min": [-spot, -spot], "max": [spot, spot]}, "spacing": 6, "seed": 2},
                                "grow": u, "growFrom": [0, 0], "growReach": 12}}]
        write([])
        self.run_cli("still", "grow", "--script", self.path, "--at", 3, "--png", "bare.png")
        write(growing(0))
        self.run_cli("still", "grow", "--script", self.path, "--at", 3, "--png", "zero.png", timeout=120)
        write(growing(0.5))
        self.run_cli("still", "grow", "--script", self.path, "--at", 3, "--png", "half.png", timeout=120)
        self.assertEqual((self.cwd / "bare.png").read_bytes(), (self.cwd / "zero.png").read_bytes())
        self.assertNotEqual((self.cwd / "bare.png").read_bytes(), (self.cwd / "half.png").read_bytes())
        write(growing(0.5, spot=60))
        self.assertIn("at most 64", self.run_cli("still", "grow", "--script", self.path, "--png", "x.png", ok=False, timeout=120))

    @unittest.skipUnless(GL, "needs OpenGL")
    def test_flora_styles(self):
        # Each tree style and shading draws, each differently, and a bad one is refused by name.
        def write(build):
            world = json.dumps({
                "camera": {"position": [0, 4, 14], "target": [0, 3, 0]},
                "lights": [{"type": "directional", "position": [10, 20, -10]}],
                "objects": [{"geometry": "plane", "width": 60, "depth": 60, "material": {"color": "#c828c8"}},
                            {"grass": {"preset": "toon", "min": [-4, -4], "max": [4, 4], "seed": 1}},
                            {"forest": {"trees": ["shrub"], "build": dict(build, frames=2, views=4, viewSize=32),
                                        "scatter": {"area": {"min": [-3, -3], "max": [3, 3]}, "spacing": 4, "seed": 2}}}]})
            literal = world.replace("\\", "\\\\").replace('"', '\\"')
            self.path.write_text('''void configure(Composition &inout c) {
    c.name = "styles"; c.w = 160; c.h = 90; c.fps = 30; c.duration = 10; c.supersample = 1;
    scene(0, c.duration, "draw");
}
void draw(int frame) { g3d::draw("''' + literal + '''", frame, 0, 0, 160, 90); }
''')
        pictures = set()
        for i, build in enumerate([{}, {"style": "lowpoly", "shading": "toon"}, {"style": "lowpoly", "shading": "hatch"},
                                   {"style": "paper"}, {"style": "splat", "shading": "gooch"}]):
            write(build)
            self.run_cli("still", "styles", "--script", self.path, "--at", 3, "--png", f"s{i}.png", timeout=120)
            pictures.add((self.cwd / f"s{i}.png").read_bytes())
        self.assertEqual(len(pictures), 5)
        write({"style": "cubist"})
        self.assertIn("realistic, lowpoly, paper or splat", self.run_cli("still", "styles", "--script", self.path, "--png", "x.png", ok=False))
        write({"shading": "oil"})
        self.assertIn("standard, toon, gooch or hatch", self.run_cli("still", "styles", "--script", self.path, "--png", "x.png", ok=False))

    @unittest.skipUnless(GL, "needs OpenGL")
    def test_flora_2d(self):
        # 2D vegetation: u 0 draws only the ground, growth changes the picture, frames are pure
        # in t, and bad items are refused by name.
        def write(u, items=None):
            d = {"wind": {"gust": 1}, "camera": [10, 0], "items": items if items is not None else [
                {"ground": {"depth": 0.6, "hills": {"x0": 0, "x1": 200, "y": 60, "amplitude": 6, "wavelength": 80}, "bottom": 90}},
                {"tree": {"depth": 0.6, "preset": "pine", "x": 40, "height": 40, "silhouette": True, "u": u}},
                {"ground": {"line": [[0, 80], [160, 80]], "bottom": 90, "color": "#4a6a34"}},
                {"grass": {"preset": "tall", "height": 20, "u": u}},
                {"tree": {"x": 90, "height": 60, "trunkWidth": 4, "leafSize": 6, "u": u}},
                {"shrub": {"at": [130, 80], "size": 20, "height": 14, "u": u}},
                {"tuft": {"at": [20, 80], "count": 8, "u": u}},
                {"meadow": {"shape": [[0, 82], [160, 82], [160, 90], [0, 90]], "preset": "lawn", "height": 4, "u": u}}]}
            literal = json.dumps(d).replace("\\", "\\\\").replace('"', '\\"')
            self.path.write_text('''void configure(Composition &inout c) {
    c.name = "flat"; c.w = 160; c.h = 90; c.fps = 30; c.duration = 40; c.supersample = 1;
    scene(0, c.duration, "draw");
}
void draw(int frame) { flora2d::draw("''' + literal + '''", frame); }
''')
        write(0, [{"ground": {"depth": 0.6, "hills": {"x0": 0, "x1": 200, "y": 60, "amplitude": 6, "wavelength": 80}, "bottom": 90}},
                  {"ground": {"line": [[0, 80], [160, 80]], "bottom": 90, "color": "#4a6a34"}}])
        self.run_cli("still", "flat", "--script", self.path, "--at", 5, "--png", "bare.png")
        write(0)
        self.run_cli("still", "flat", "--script", self.path, "--at", 5, "--png", "zero.png")
        self.assertEqual((self.cwd / "bare.png").read_bytes(), (self.cwd / "zero.png").read_bytes())
        write(1)
        for name, frame in (("a.png", 5), ("b.png", 30), ("a2.png", 5)):
            self.run_cli("still", "flat", "--script", self.path, "--at", frame, "--png", name)
        self.assertNotEqual((self.cwd / "zero.png").read_bytes(), (self.cwd / "a.png").read_bytes())
        self.assertNotEqual((self.cwd / "a.png").read_bytes(), (self.cwd / "b.png").read_bytes())
        self.assertEqual((self.cwd / "a.png").read_bytes(), (self.cwd / "a2.png").read_bytes())
        write(1, [{"cactus": {}}])
        self.assertIn("ground, grass, meadow, tuft, shrub, tree", self.run_cli("still", "flat", "--script", self.path, "--png", "x.png", ok=False))
        write(1, [{"tree": {"preset": "baobab", "at": [10, 10]}}])
        self.assertIn("oak, birch, pine", self.run_cli("still", "flat", "--script", self.path, "--png", "x.png", ok=False))
        write(1, [{"tree": {"x": 10}}])
        self.assertIn("needs \"at\"", self.run_cli("still", "flat", "--script", self.path, "--png", "x.png", ok=False))

    @unittest.skipUnless(GL, "needs OpenGL")
    def test_flora_2d_path_meadow(self):
        # A meadow filling SVG path data stays in its shape (a curved edge and a hole), a
        # malformed path or rule is refused with a clear message.
        def write(item):
            literal = json.dumps({"items": [{"meadow": item}]}).replace("\\", "\\\\").replace('"', '\\"')
            self.path.write_text('''void configure(Composition &inout c) {
    c.name = "pm"; c.w = 160; c.h = 90; c.fps = 30; c.duration = 10; c.supersample = 1;
    scene(0, c.duration, "draw");
}
void draw(int frame) { flora2d::draw("''' + literal + '''", frame); }
''')

        def still(name="pm.png"):
            return self.run_cli("still", "pm", "--script", self.path, "--at", 3, "--png", name, ok=name != "bad.png")

        # a curved-topped shape on the left, with a hole; the right third of the picture is outside
        d = "M 10 70 C 10 30 90 30 90 70 L 90 88 L 10 88 Z M 40 70 L 40 80 L 60 80 L 60 70 Z"
        write({"path": d, "rule": "evenodd", "preset": "lawn", "height": 4, "density": 2.5})
        still()
        rows, bpp = png_rows(self.cwd / "pm.png")

        def green(x0, y0, x1, y1):
            n = 0
            for y in range(y0, y1):
                for x in range(x0, x1):
                    r, g, b = rows[y][x * bpp:x * bpp + 3]
                    n += g > 40 and g > r
            return n
        inside, outside, hole = green(14, 60, 36, 86), green(110, 0, 160, 90), green(44, 71, 56, 75)
        print(f"[flora2d] svg path meadow: {inside} green px inside, {outside} outside, {hole} in the hole")
        self.assertGreater(inside, 100)
        self.assertEqual(outside, 0)
        self.assertLess(hole, 3)
        write({"path": "M 10 10 L 50 abc Z"})
        self.assertIn("flora2d meadow path:", still("bad.png"))
        write({"path": d, "rule": "winding"})
        self.assertIn("nonzero", still("bad.png"))
        write({"path": d, "shape": [[0, 0], [1, 1], [0, 1]]})
        self.assertIn("not both", still("bad.png"))

    @unittest.skipUnless(GL, "needs OpenGL")
    def test_flora_city_and_map(self):
        # A city's vegetation as plants, and forest cover on a map: each changes the picture,
        # and bad descriptors are refused by name.
        (self.cwd / "green.city.json").write_text(json.dumps({
            "type": "CityJSON", "version": "2.0", "transform": {"scale": [1, 1, 1], "translate": [0, 0, 0]},
            "CityObjects": {
                "oak": {"type": "SolitaryVegetationObject", "attributes": {"height": 9},
                        "geometry": [{"type": "MultiPoint", "lod": "1", "boundaries": [0]}]},
                "lawn": {"type": "PlantCover", "geometry": [{"type": "MultiSurface", "lod": "1", "boundaries": [[[1, 2, 3, 4]]]}]}},
            "vertices": [[0, 0, 0], [-8, -8, 0], [8, -8, 0], [8, 8, 0], [-8, 8, 0]]}))

        def write(scene):
            literal = json.dumps(scene).replace("\\", "\\\\").replace('"', '\\"')
            self.path.write_text('''void configure(Composition &inout c) {
    c.name = "green"; c.w = 160; c.h = 90; c.fps = 30; c.duration = 10; c.supersample = 1;
    scene(0, c.duration, "draw");
}
void draw(int frame) { g3d::draw("''' + literal + '''", frame, 0, 0, 160, 90); }
''')

        def town(plants):
            d = {"camera": {"position": [0, 6, 20], "target": [0, 3, 0]},
                 "lights": [{"type": "directional", "position": [5, 10, 5]}],
                 "city": {"file": "green.city.json"}}
            if plants is not None:
                d["city"]["plants"] = plants
            return d
        write(town(None))
        self.run_cli("still", "green", "--script", self.path, "--at", 2, "--png", "solids.png")
        write(town({"style": "lowpoly", "wind": 0}))
        self.run_cli("still", "green", "--script", self.path, "--at", 2, "--png", "plants.png", timeout=120)
        self.assertNotEqual((self.cwd / "solids.png").read_bytes(), (self.cwd / "plants.png").read_bytes())
        write(town({"style": "cubist"}))
        self.assertIn("realistic, lowpoly, paper or splat", self.run_cli("still", "green", "--script", self.path, "--png", "x.png", ok=False))

        def globe(forest):
            g = {"projection": "equirectangular", "centerLon": 18, "centerLat": 62, "mapWidth": 256}
            if forest is not None:
                g["forest"] = forest
            return {"camera": {"position": [0, 0.2, 2.4], "target": [0, 0, 0]},
                    "lights": [{"type": "directional", "position": [-2, 3, 4]}], "geo3d": g}
        write(globe(None))
        self.run_cli("still", "green", "--script", self.path, "--at", 2, "--png", "bare.png")
        write(globe({"regions": ["FIN", [5, 58, 12, 63]], "spacing": 1, "height": 0.03}))
        self.run_cli("still", "green", "--script", self.path, "--at", 2, "--png", "wooded.png", timeout=120)
        self.assertNotEqual((self.cwd / "bare.png").read_bytes(), (self.cwd / "wooded.png").read_bytes())
        # Levels by size on screen: the same cover as meshes only (impostorBelow 0, mergeBelow 0) and by
        # default; far away they differ, and the keys are accepted.
        far = {"regions": [[5, 55, 25, 65]], "spacing": 0.3, "height": 0.03}
        write(globe(far))
        self.run_cli("still", "green", "--script", self.path, "--at", 2, "--png", "levels.png", timeout=120)
        write(globe(dict(far, impostorBelow=0, mergeBelow=0)))
        self.run_cli("still", "green", "--script", self.path, "--at", 2, "--png", "meshes.png", timeout=120)
        self.assertNotEqual((self.cwd / "levels.png").read_bytes(), (self.cwd / "meshes.png").read_bytes())
        write(globe(dict(far, impostorBelow=20, blend=0.5, mergeBelow=6, dropBelow=1)))
        self.run_cli("still", "green", "--script", self.path, "--at", 2, "--png", "tuned.png", timeout=120)
        write(globe({"regions": ["Atlantis"]}))
        self.assertIn("no country", self.run_cli("still", "green", "--script", self.path, "--png", "x.png", ok=False))
        write(globe({"regions": [[10, 60, 5, 50]]}))
        self.assertIn("[west, south, east, north]", self.run_cli("still", "green", "--script", self.path, "--png", "x.png", ok=False))

    @unittest.skipUnless(GL, "needs OpenGL")
    def test_environment_faces(self):
        # A skybox from six cube faces (and from one cross): looking along +x, -z and -x shows
        # the -x, -z and +x faces; a bad descriptor says what is wrong.
        import struct
        import zlib

        def chunk(tag, data):
            return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

        def write_png(path, w, h, rgb_at):
            raw = b"".join(b"\0" + b"".join(bytes(rgb_at(x, y)) for x in range(w)) for y in range(h))
            path.write_bytes(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
                             chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))

        def read_pixel(path, px, py):
            data = path.read_bytes()
            pos, idat, header = 8, b"", None
            while pos < len(data):
                size, tag = struct.unpack(">I4s", data[pos:pos + 8])
                body = data[pos + 8:pos + 8 + size]
                if tag == b"IHDR":
                    header = struct.unpack(">IIBBBBB", body)
                elif tag == b"IDAT":
                    idat += body
                pos += 12 + size
            w, h, depth, ctype = header[:4]
            self.assertEqual((depth, header[6]), (8, 0))
            bpp = {2: 3, 6: 4}[ctype]
            raw, stride, rows, prev = zlib.decompress(idat), w * bpp, [], bytearray(w * bpp)
            for y in range(h):
                kind, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
                for i in range(stride):
                    a = line[i - bpp] if i >= bpp else 0
                    b = prev[i]
                    c = prev[i - bpp] if i >= bpp else 0
                    if kind == 1: line[i] = (line[i] + a) & 255
                    elif kind == 2: line[i] = (line[i] + b) & 255
                    elif kind == 3: line[i] = (line[i] + ((a + b) >> 1)) & 255
                    elif kind == 4:
                        p = a + b - c
                        pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                        line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
                prev = line
                rows.append(line)
            return tuple(rows[py][px * bpp:px * bpp + 3])

        colours = [(220, 40, 40), (40, 200, 200), (40, 200, 40), (200, 40, 200), (40, 40, 220), (220, 220, 40)]
        names = ["px", "nx", "py", "ny", "pz", "nz"]
        for name, rgb in zip(names, colours):
            write_png(self.cwd / f"{name}.png", 16, 16, lambda x, y, rgb=rgb: rgb)
        # the same six faces as a cross: row 0 +y, row 1 -x +z +x -z, row 2 -y
        spots = {"py": (1, 0), "nx": (0, 1), "pz": (1, 1), "px": (2, 1), "nz": (3, 1), "ny": (1, 2)}
        by_name = dict(zip(names, colours))
        def cross_at(x, y):
            for name, (cx, cy) in spots.items():
                if cx == x // 16 and cy == y // 16:
                    return by_name[name]
            return (0, 0, 0)
        write_png(self.cwd / "cross.png", 64, 48, cross_at)

        def draw(environment, target):
            world = json.dumps({"camera": {"position": [0, 0, 0], "target": target, "fov": 60},
                                "environment": environment, "showEnvironment": True})
            literal = world.replace("\\", "\\\\").replace('"', '\\"')
            self.path.write_text('''void configure(Composition &inout c) {
    c.name = "cube"; c.w = 64; c.h = 36; c.fps = 30; c.duration = 1; c.supersample = 1;
    scene(0, c.duration, "draw");
}
void draw(int frame) { g3d::draw("''' + literal + '''", frame, 0, 0, 64, 36); }
''')
            (self.cwd / "sky.png").unlink(missing_ok=True)
            return self.run_cli("still", "cube", "--script", self.path, "--at", 0, "--ss", 1, "--png", "sky.png", ok=None)

        def near(a, b):
            return all(abs(x - y) <= 8 for x, y in zip(a, b))
        files = [f"{n}.png" for n in names]
        # the faces labelled +x and -x swap sides (packs are left-handed, the engine is not)
        for target, index in (([1, 0, 0], 1), ([0, 0, -1], 5), ([-1, 0, 0], 0)):
            draw({"faces": files}, target)
            self.assertTrue(near(read_pixel(self.cwd / "sky.png", 32, 18), colours[index]), (target, index))
            draw({"cross": "cross.png"}, target)
            self.assertTrue(near(read_pixel(self.cwd / "sky.png", 32, 18), colours[index]), ("cross", target))
        self.assertIn("six files", draw({"faces": files[:5]}, [1, 0, 0]))
        self.assertIn("exactly one of", draw({"faces": files, "cross": "cross.png"}, [1, 0, 0]))
        self.assertIn("exactly one of", draw({}, [1, 0, 0]))
        self.assertIn("file not found", draw({"faces": files[:5] + ["missing.png"]}, [1, 0, 0]))

    @unittest.skipUnless(GL, "needs OpenGL")
    def test_flora_prefabs(self):
        # Each flora prefab draws, changes as it grows, and refuses unknown trees and grass.
        def write(name, props):
            self.path.write_text('''void configure(Composition &inout c) {
    c.name = "fp"; c.w = 192; c.h = 108; c.fps = 30; c.duration = 60; c.supersample = 1;
    prefab::use(0, 60, "''' + name + '''", "''' + json.dumps(props).replace('"', '\\"') + '''");
    scene(0, c.duration, "draw");
}
void draw(int frame) {}
''')
        for name, props in (("flora-2d-hill", {"box": [0, 0, 192, 108], "duration": 40}),
                            ("flora-tree-grow", {"box": [0, 0, 192, 108], "tree": "birch", "duration": 40}),
                            ("flora-meadow", {"box": [0, 0, 192, 108], "trees": 3, "size": 12, "kinds": ["birch"]})):
            write(name, props)
            self.run_cli("still", "fp", "--script", self.path, "--at", 2, "--png", "a.png", timeout=180)
            self.run_cli("still", "fp", "--script", self.path, "--at", 50, "--png", "b.png", timeout=180)
            self.assertNotEqual((self.cwd / "a.png").read_bytes(), (self.cwd / "b.png").read_bytes(), name)
        write("flora-tree-grow", {"tree": "baobab"})
        self.assertIn('no tree "baobab"', self.run_cli("still", "fp", "--script", self.path, "--png", "x.png", ok=False))
        write("flora-2d-hill", {"grass": "astroturf"})
        self.assertIn('no grass "astroturf"', self.run_cli("still", "fp", "--script", self.path, "--png", "x.png", ok=False))
        write("flora-meadow", {"box": [0, 0, 0, 10]})
        self.assertIn("positive size", self.run_cli("still", "fp", "--script", self.path, "--png", "x.png", ok=False))

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

    @unittest.skipUnless(GL, "needs OpenGL")
    def test_preview_opens_paused_at_a_frame(self):
        # --at opens the window paused on that frame, with the seek bar showing.
        import os
        env = dict(os.environ, RAYMOTION_PREVIEW_SECONDS="2", RAYMOTION_PREVIEW_SHOT="at.png")
        done = subprocess.run([EXE, "preview", "fixture", "--script", str(self.path), "--mute", "--at", "3"], cwd=self.cwd,
                              text=True, capture_output=True, env=env, timeout=60)
        self.assertEqual(done.returncode, 0, done.stdout + done.stderr)
        self.assertGreater((self.cwd / "at.png").stat().st_size, 500)
        self.assertIn("--at FRAME", self.run_cli("help", ok=None))

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
        # --frames-dir moves the sequence (the workers' frames too) and leaves the default alone.
        self.run_cli("render", "fixture", "--script", self.path, "--png-frames", "--jobs", 2, "--frames-dir", "elsewhere/seq")
        for frame in range(4):
            self.assertEqual((self.cwd / f"f{frame}.png").read_bytes(),
                             (self.cwd / f"elsewhere/seq/f{frame:05}.png").read_bytes())
        self.assertIn("--frames-dir", self.run_cli("help", ok=None))
        self.run_cli("still", "fixture", "--script", self.path, "--props", '{"width":80}', "--png", "props.png")
        self.assertEqual(png_size(self.cwd / "props.png"), (80, 32))
        self.path.write_text(SOURCE.replace('draw::circle(8 + 8*t, 16, 3, 0xffffff);',
            'draw::text(props::text("title", "text"), 0, 0, 12, 0xffffff);'))
        self.run_cli("still", "fixture", "--script", self.path, "--png", "text.png")
        self.path.write_text(SOURCE.replace('draw::circle(8 + 8*t, 16, 3, 0xffffff);', 'int zero = 0; int bad = 1/zero;'))
        self.assertIn("Divide by zero", self.run_cli("still", "fixture", "--script", self.path, ok=False))


if __name__ == "__main__":
    unittest.main()
