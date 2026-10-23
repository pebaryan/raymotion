# Generate MSVC-style import libraries from system DLLs using dumpbin's export list.
# Def format that link.exe actually accepts for synthesized libs:
#   LIBRARY UPPERNAME.DLL
#   EXPORTS
#     __imp_NAME = UPPERNAME.NAME       (qualified aliases; bare rows break LNK1127)
import subprocess, sys, os, re
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))).replace("\\", "/")
MSVC_DIR = os.environ.get("MSVC_DIR", r"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207")
BIN = MSVC_DIR + "/bin/Hostx64/x64"
OUT = ROOT + "/third_party/msvc-sdk-libs"
os.chdir(ROOT)
os.makedirs(OUT, exist_ok=True)

SPEC = [("kernel32.dll","kernel32"),("user32.dll","user32"),("gdi32.dll","gdi32"),
        ("shell32.dll","shell32"),("ole32.dll","ole32"),("oleaut32.dll","oleaut32"),
        ("comdlg32.dll","comdlg32"),("advapi32.dll","advapi32"),("winspool.drv","winspool"),
        ("ucrtbase.dll","ucrt"),("ucrtbased.dll","ucrtd")]

def rows_from_dumpbin(dll):
    """Return [(alias, target)]; 'forwarded' rows carry an already-qualified NTDLL.X target."""
    r = subprocess.run([BIN + "/dumpbin.exe", "-EXPORTS", dll], capture_output=True, text=True)
    out = []
    started = False
    for ln in r.stdout.splitlines():
        s = ln.strip()
        if not started:
            if "ordinal" in s and "RVA" in s: started = True
            continue
        if not s: continue                              # blanks occur inside table
        if re.match(r"^(Summary|SECTION|Section|LINK)", s): break
        fm = re.search(r"\(forwarded to ([A-Za-z0-9_.]+)\)", s)
        if fm:
            tgt = fm.group(1)
            pre = s[:fm.start()].split("(")[0]
            ptoks = [t for t in pre.split() if re.fullmatch(r"[A-Za-z_?][A-Za-z0-9_.:$?-]*", t)]
            ptoks = [t for t in ptoks if not re.fullmatch(r"[0-9a-fA-F]+", t)]
            if ptoks: out.append((ptoks[-1].split("@")[0], tgt))
            continue
        toks = [t for t in s.replace("|", " ").replace("/", " ").split()
                if re.fullmatch(r"[A-Za-z_?][A-Za-z0-9_.:$?-]*", t) and not re.fullmatch(r"[0-9a-fA-F]+", t)]
        if toks:
            base = toks[-1].split("@")[0]
            out.append((base, base))
    return out

for stem_lib in SPEC:
    stem, lib = stem_lib
    dll = f"C:/Windows/System32/{stem}"
    if not dll.lower().endswith((".dll", ".drv")): dll += ".dll"
    if not os.path.exists(dll): print(lib, "skip: no dll"); continue
    mod = stem.split(".")[0].upper()           # KERNEL32, UCRTBASE, WINSPOOL ...
    rows = rows_from_dumpbin(dll)
    seen = set(); lines = [f"LIBRARY {mod}.DLL", "EXPORTS"]; plain = [f"LIBRARY {mod}.DLL", "EXPORTS"]
    for a, t in rows:
        a = a.split("@")[0]
        key = a.upper()
        if key in seen: continue
        seen.add(key)
        lines.append(f"  __imp_{a} = {mod}.{t}")
        plain.append(f"  {a}")            # plain import records live in a companion archive
    defp = f"{OUT}/{lib}.def"
    open(defp, "w").write("\n".join(lines) + "\n")
    plainp = f"{OUT}/{lib}b.def"
    open(plainp, "w").write("\n".join(plain) + "\n")
    r = subprocess.run([BIN + "/lib.exe", "/nologo", f"/def:{defp}", f"/out:{OUT}/{lib}.lib", "/machine:X64"],
                       capture_output=True, text=True)
    r2 = subprocess.run([BIN + "/lib.exe", "/nologo", f"/def:{plainp}", f"/out:{OUT}/{lib}b.lib", "/machine:X64"],
                        capture_output=True, text=True)
    ok = f"{OUT}/{lib}.lib"
    # verify linkable like CMake's TryCompile does (debug+incremental)
    pr = subprocess.run([BIN + "/link.exe", "/nologo", "tools/tmp_defs/t.obj",
                         "/OUT:" + ROOT + "/tools/tmp_defs/probe_exe.exe", "/DEBUG", "/INCREMENTAL",
                         "/SUBSYSTEM:CONSOLE", ok, ok[:-3] + "db.lib" if False else ok.replace(".lib", "b.lib"), "/MANIFEST", "/MANIFESTFILE:" + ROOT + "/tools/tmp_defs/m.res"],
                        capture_output=True, text=True, cwd=ROOT)
    bad = "LNK1127" in pr.stderr or "corrupt" in (pr.stderr+pr.stdout).lower()
    print(lib, "rc=", r.returncode, "exports:", len(seen), "linkcheck:", "CORRUPT" if bad else "ok")
