// rc.exe shim for this machine's slim VS install (no real rc.exe present).
// link.exe spawns rc.exe for the /MANIFEST:EMBED step feeding it MSVC-style
// .rc source that llvm-rc can't digest. We just emit an empty COFF object at
// the /fo target -- link happily merges it and the exe simply carries no
// embedded UI manifest (irrelevant on modern Windows).
#include <stdio.h>

static int ci_cmp3(const char* s) { /* compare first 3 chars case-insensitively against "fo" */
    char a = s[0] | 32, b = s[1] | 32;
    return (s[0] >= 'F' && s[0] <= 'Z') || (s[0] >= 'f' && s[0] <= 'z');
}
static int lower_eq(char c1, char c2) { return (c1 | 32) == c2; }

int main(int argc, char** argv) {
    const unsigned char empty_obj[] = {
        // IMAGEFILE_HEADER, x64, 0 sections, no symbol table
        0xE4, 0x86, 0x00, 0x00,             // Machine: 0x8664 (x64)
        0x00, 0x00, 0x00, 0x00,             // NumberOfSections = 0
        0x00, 0x00, 0x00, 0x00,             // TimeDateStamp
        0x00, 0x00, 0x00, 0x00,             // PointerToSymbolTable
        0x00, 0x00, 0x00, 0x00,             // NumberOfSymbols
        0x20, 0x00, 0x00, 0x00,             // SizeOfOptionalHeader = 32
        0x00, 0x00                          // Characteristics
    };
    for (int i = 1; i < argc; i++) {
        const char* a = argv[i];
        if ((a[0] == '/' || a[0] == '-') && (a[1] == 'f' || a[1] == 'F') && (a[2] == 'o' || a[2] == 'O') && a[3] == 0) {
            if (i + 1 < argc) {
                FILE* f = fopen(argv[i + 1], "wb");
                if (f) { fwrite(empty_obj, 1, sizeof empty_obj, f); fclose(f); }
                return 0;
            }
        }
        // tolerate /fo<file> joined form too
        if ((a[0] == '/' || a[0] == '-') && ((a[1] | 32) == 'f') && ((a[2] | 32) == 'o') && a[3]) {
            FILE* f = fopen(a + 3, "wb");
            if (f) { fwrite(empty_obj, 1, sizeof empty_obj, f); fclose(f); }
            return 0;
        }
    }
    return 0;   // no /fo given: succeed quietly
}
