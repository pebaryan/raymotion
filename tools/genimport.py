# Regenerate MSVC import libs (__imp_ qualified aliases only) directly from system DLL PE tables.
import subprocess, sys, struct, json, os
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__))).replace("\\", "/")
MSVC_DIR = os.environ.get("MSVC_DIR", r"C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207")
BIN = MSVC_DIR + "/bin/Hostx64/x64"
OUT = ROOT + "/third_party/msvc-sdk-libs"
os.makedirs(OUT, exist_ok=True)

def exports(dllpath):
    """Parse the PE export directory; return list of (name, ordinal)."""
    d = open(dllpath, 'rb').read()
    e_lfanew = struct.unpack_from('<I', d, 0x3C)[0]
    assert d[e_lfanew:e_lfanew+4] == b'PE\x00\x00', (dllpath, "not pe")
    coff = e_lfanew + 4
    nsec = struct.unpack_from('<H', d, coff+2)[0]
    opt = coff + 20
    magic = struct.unpack_from('<H', d, opt)[0]
    sizeopt = struct.unpack_from('<I', d, opt+16)[0]
    # optional header fields: StandardCoff at opt.. ; DataDirectory is at offset 112 bytes into opts for PE (index 14 of directories*8? )
    off = {
        0x14b: 240,   # COFF+PE32 x86_64 standard: ExportDir at Opt+0xF0? use robust scan below instead
    }
    # robust: search common known offsets via all section headers mapping RVA->raw
    def rva2off(rva):
        sh = opt + sizeopt
        for i in range(nsec):
            o = sh + i*40
            name = d[o:o+8]
            if name[:1] == b'/':
                stroff = int(struct.unpack_from('<I', d, o+16)[0]); rsz = struct.unpack_from('<I', d, o+20)[0]
                nm = d[stroff:stroff+rsz].split(b'\x00')[0].decode('ascii', 'ignore')
            else:
                nm = name.rstrip(b'\x00').decode('ascii','ignore')
            rawptr = struct.unpack_from('<I', d, o+20)[0]
            vsz    = struct.unpack_from('<I', d, o+8)[0]
            vaddr  = struct.unpack_from('<I', d, o+12)[0]
            szraw  = struct.unpack_from('<I', d, o+16)[0]
            if vaddr <= rva < vaddr+max(vsz, szraw):
                return rawptr + (rva - vaddr)
        raise ValueError(hex(rva))
    # PE optional header: after image base etc, directories table starts at opt+112 (StandardCofsizes differ by machine);
    # Data Directories offset = opt + 112 works for x64 (COFF+112 = WindowsNT pad 112? actual std = 240)
    base_std = 240 if magic == 0x8664 else 240
    ddopt = opt + base_std + (10 if False else 0)
    # find export dir entry: directory 0 is Attribute, 1 Code, 2 Export... simply scan 15 directory entries pairs of (rva,size)
    found = None
    for di in range(1, 18):  # attribute, loader, timedatestamp, filereloc, bump, security, basedreloc, debug, import, delayedimport, upxdire..., export?? Actually ExportDirectory index is fixed per machine: it is directory number 5 (after 2 for x64?). Scan ALL small ranges near typical offsets and validate candidates instead.
        pass
    # Simpler reliable approach: brute over directory table at known start:
    tables = [opt+base_std, opt+base_std-96, opt+base_std+16]
    for tstart in tables:
        for di in range(16):
            rva, sz = struct.unpack_from('<II', d, tstart + di*8)
            if sz > 8 and rva > 0:
                try:
                    ed = rva2off(rva)
                except Exception:
                    continue
                # validate: first dword flags, then machine (0x8664 or our), then a name ptr readable
                machine = struct.unpack_from('<H', d, ed+4)[0]
                if machine not in (0x8664, 0x14b, 0xaa64, 0x014c): continue
                nbias = struct.unpack_from('<I', d, ed+12)[0]
                libor = struct.unpack_from('<I', d, ed+16)[0]
                ncnt  = struct.unpack_from('<I', d, ed+20)[0]
                fcnt  = struct.unpack_from('<I', d, ed+24)[0]
                if 0 and False: pass
                # sanity counts
                if ncnt > 50000: continue
                names_off = ed + 40 + (libor if libor else 120) + ncnt*12 + fcnt*8
                sample = d[names_off:names_off+16]
                print(f"candidate diag: di={di} ncnt={ncnt} fcnt={fcnt}", end=' ')
                sys.stdout.flush()
                found=(ed,ncnt,fcnt,names_off)
                break
        if found: break
    if not found: raise SystemExit("no export table located")
    ed, ncnt, fcnt, names_off = found
    entries=[]
    nsz=0
    for i in range(ncnt):
        flags, ordinal, rva, nameoff = struct.unpack_from('<HHII', d, ed+40 + i*12 + (0 if True else 0))
        entries.append((flags & 0xFF00) >> 8 if False else (flags, ordinal, rva, nameoff))
    fstarts=[]
    for i in range(fcnt):
        fstarts.append(struct.unpack_from('<I', d, ed+40+ncnt*12 + i*4)[0])
    out=[]
    name_area = ed + 40 + (ncnt*12 if len(entries)*12==ncnt*12 else ncnt*12) + (fcnt*4 if len(fstarts)*4==fcnt*4 else 0)
    # name area position computed above; but simpler: each entry's nameoff field indexes names separately per-name strings at 'ed+...'
    for idx,(flags, ordinal, rva, nameoff) in enumerate(entries):
        # name pointer stored as offset into NamesArray region starting right after functions array
        p = name_area + nameoff
        nm = b''
        while d[p] != 0:
            nm += bytes([d[p]]); p += 1
        nm = nm.decode('ascii','ignore')
        if nm:
            out.append((nm, ordinal & 0xFFFF, bool(flags & 0x8000)))
    return out

def gen(dllstem, libname=None):
    dllpath = fr"C:/Windows/System32/{dllstem}.dll"
    if not os.path.exists(dllpath): return False
    ex = exports(dllpath)
    mod = dllstem.upper() + ".DLL"
    lines = [f"LIBRARY {mod}", "EXPORTS"]
    seen=set()
    for nm, ordv, fwd in ex:
        key = nm
        if key in seen: continue
        seen.add(key)
        base = nm if not fwd else nm   # dumpbin-forward info lost; ordinal fallback when no name handled by skip
        lines.append(f"  __imp_{nm} = {mod[:-4]}.{nm}" )
    defn = fr"{OUT}/{libname}.def"
    open(defn,'w').write("\n".join(lines)+"\n")
    r = subprocess.run([BIN+r"/lib.exe","/nologo",f"/def:{defn}", f"/out:{OUT}/{libname}.lib","/machine:X64"], capture_output=True, text=True)
    print(libname, "rc=", r.returncode, len(seen), "exports")
    return True

print(exports.__doc__)
for stem, lib in [("kernel32","kernel32"),("user32","user32"),("gdi32","gdi32"),("shell32","shell32"),
                  ("ole32","ole32"),("oleaut32","oleaut32"),("comdlg32","comdlg32"),("advapi32","advapi32"),
                  ("ucrtbase","ucrt"),("ucrtbased","ucrtd")]:
    try:
        gen(stem, lib)
    except Exception as e:
        print(stem, "ERR", repr(e))
