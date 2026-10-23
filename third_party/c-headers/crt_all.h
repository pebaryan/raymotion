#ifndef RAYMOTION_CRT_ALL_H
#define RAYMOTION_CRT_ALL_H
/* Master stand-in for the missing plain-C runtime headers. */
typedef struct _ShimFile { int _Opaque; } FILE;
typedef struct _RayMbStat { unsigned short _Shift; } mbstate_t;
typedef mbstate_t _Mbstatet;
typedef struct _FposShim { mbstate_t _St; unsigned long long _Pos; } fpos_t;
#ifdef __cplusplus
extern "C" {
#endif
double acos(double);
float acosf(float);
double acosh(double);
float acoshf(float);
long double acoshl(long double);
long double acosl(long double);
double asin(double);
float asinf(float);
double asinh(double);
float asinhf(float);
long double asinhl(long double);
long double asinl(long double);
double atan(double);
double atan2(double, double);
float atan2f(float, float);
long double atan2l(long double, long double);
float atanf(float);
double atanh(double);
float atanhf(float);
long double atanhl(long double);
long double atanl(long double);
double cbrt(double);
float cbrtf(float);
long double cbrtl(long double);
double ceil(double);
float ceilf(float);
long double ceill(long double);
double cos(double);
float cosf(float);
double cosh(double);
float coshf(float);
long double coshl(long double);
long double cosl(long double);
double erf(double);
double erfc(double);
float erfcf(float);
long double erfcl(long double);
float erff(float);
long double erfl(long double);
double exp(double);
double exp2(double);
float exp2f(float);
long double exp2l(long double);
float expf(float);
long double expl(long double);
double expm1(double);
float expm1f(float);
long double expm1l(long double);
double fabs(double);
float fabsf(float);
long double fabsl(long double);
double fdim(double, double);
float fdimf(float, float);
long double fdiml(long double, long double);
double floor(double);
float floorf(float);
long double floorl(long double);
double fma(double, double, double);
float fmaf(float, float, float);
long double fmal(long double, long double, long double);
double fmax(double, double);
float fmaxf(float, float);
long double fmaxl(long double, long double);
double fmin(double, double);
float fminf(float, float);
long double fminl(long double, long double);
double fmod(double, double);
float fmodf(float, float);
long double fmodl(long double, long double);
int fpclassify(double);
double frexp(double, int*);
float frexpf(float, int*);
long double frexpl(long double, int*);
double hypot(double, double);
float hypotf(float, float);
long double hypotl(long double, long double);
int ilogb(double);
int ilogbf(float);
int ilogbl(long double);
int isfinite(double);
int isinf(double);
int isnan(double);
int isnormal(double);
double ldexp(double, int);
float ldexpf(float, int);
long double ldexpl(long double, int);
double lgamma(double);
float lgammaf(float);
long double lgammal(long double);
int llrint(double);
int llrintf(float);
int llrintl(long double);
int llround(double);
int llroundf(float);
int llroundl(long double);
double log(double);
double log10(double);
float log10f(float);
long double log10l(long double);
double log1p(double);
float log1pf(float);
long double log1pl(long double);
double log2(double);
float log2f(float);
long double log2l(long double);
double logb(double);
float logbf(float);
long double logbl(long double);
float logf(float);
long double logl(long double);
int lrint(double);
int lrintf(float);
int lrintl(long double);
int lround(double);
int lroundf(float);
int lroundl(long double);
double modf(double, double*);
float modff(float, float*);
long double modfl(long double, long double*);
int nearbyint(double);
int nearbyintf(float);
int nearbyintl(long double);
double nextafter(double, double);
float nextafterf(float, float);
long double nextafterl(long double, long double);
double nexttoward(double, double);
float nexttowardf(float, float);
long double nexttowardl(long double, long double);
double pow(double, double);
float powf(float, float);
long double powl(long double, long double);
double remainder(double, double);
float remainderf(float, float);
long double remainderl(long double, long double);
double remquo(double, double, int*);
float remquof(float, float, int*);
long double remquol(long double, long double, int*);
int rint(double);
int rintf(float);
int rintl(long double);
double round(double);
float roundf(float);
long double roundl(long double);
double scalbln(double, int);
float scalblnf(float, int);
long double scalblnl(long double, int);
double scalbn(double, int);
float scalbnf(float, int);
long double scalbnl(long double, int);
int signbit(double);
double sin(double);
float sinf(float);
double sinh(double);
float sinhf(float);
long double sinhl(long double);
long double sinl(long double);
double sqrt(double);
float sqrtf(float);
long double sqrtl(long double);
double tan(double);
float tanf(float);
double tanh(double);
float tanhf(float);
long double tanhl(long double);
long double tanl(long double);
double tgamma(double);
float tgammaf(float);
long double tgammal(long double);
double trunc(double);
float truncf(float);
long double truncl(long double);
FILE* fopen(const char*, const char*);
int fclose(FILE*);
size_t fread(void*, size_t, size_t, FILE*);
size_t fwrite(const void*, size_t, size_t, FILE*);
int fseek(FILE*, long, int);
long ftell(FILE*);
int fflush(FILE*);
int getc(FILE*);
int fgetc(FILE*);
int fprintf(FILE*, const char*, ...);
int printf(const char*, ...);
int snprintf(char*, size_t, const char*, ...);
char* getenv(const char*);
int system(const char*);
void* malloc(size_t);
void* calloc(size_t, size_t);
void free(void*);
int atoi(const char*);
double atof(const char*);
int abs(int);
int fputs(const char*, FILE*);
int fputc(int, FILE*);
void* memcpy(void*, const void*, size_t);
void* memset(void*, int, size_t);
void* memmove(void*, const void*, size_t);
size_t strlen(const char*);
int strcmp(const char*, const char*);
char* strncpy(char*, const char*, size_t);
int rand(void);
void srand(unsigned int);
void abort(void);
void exit(int);
#ifdef __cplusplus
}
#endif
int FUN(void); /* filler only */
int _Exit(void); /* filler only */
int _ctime64(void); /* filler only */
int _difftime64(void); /* filler only */
int _gmtime64(void); /* filler only */
int _localtime64(void); /* filler only */
int _mktime64(void); /* filler only */
int _time64(void); /* filler only */
int _timespec64_get(void); /* filler only */
int asctime(void); /* filler only */
int at_quick_exit(void); /* filler only */
int atexit(void); /* filler only */
int atol(void); /* filler only */
int atoll(void); /* filler only */
int bsearch(void); /* filler only */
int btowc(void); /* filler only */
int clearerr(void); /* filler only */
int clock(void); /* filler only */
int clock_t(void); /* filler only */
int copysign(void); /* filler only */
int copysignf(void); /* filler only */
int copysignl(void); /* filler only */
int ctime(void); /* filler only */
int difftime(void); /* filler only */
int div(void); /* filler only */
int div_t(void); /* filler only */
int double_t(void); /* filler only */
int feof(void); /* filler only */
int ferror(void); /* filler only */
int fgetpos(void); /* filler only */
int fgets(void); /* filler only */
int fgetwc(void); /* filler only */
int fgetws(void); /* filler only */
int float_t(void); /* filler only */
int fputwc(void); /* filler only */
int fputws(void); /* filler only */
int freopen(void); /* filler only */
int fscanf(void); /* filler only */
int fsetpos(void); /* filler only */
int fwide(void); /* filler only */
int fwprintf(void); /* filler only */
int fwscanf(void); /* filler only */
int getchar(void); /* filler only */
int getwc(void); /* filler only */
int getwchar(void); /* filler only */
int gmtime(void); /* filler only */
int isalnum(void); /* filler only */
int isalpha(void); /* filler only */
int isblank(void); /* filler only */
int iscntrl(void); /* filler only */
int isdigit(void); /* filler only */
int isgraph(void); /* filler only */
int isgreater(void); /* filler only */
int isgreaterequal(void); /* filler only */
int isless(void); /* filler only */
int islessequal(void); /* filler only */
int islessgreater(void); /* filler only */
int islower(void); /* filler only */
int isprint(void); /* filler only */
int ispunct(void); /* filler only */
int isspace(void); /* filler only */
int isunordered(void); /* filler only */
int isupper(void); /* filler only */
int isxdigit(void); /* filler only */
int labs(void); /* filler only */
int ldiv(void); /* filler only */
int ldiv_t(void); /* filler only */
int llabs(void); /* filler only */
int lldiv(void); /* filler only */
int lldiv_t(void); /* filler only */
int localtime(void); /* filler only */
int mblen(void); /* filler only */
int mbrlen(void); /* filler only */
int mbrtowc(void); /* filler only */
int mbsinit(void); /* filler only */
int mbsrtowcs(void); /* filler only */
int mbstowcs(void); /* filler only */
int mbtowc(void); /* filler only */
int memchr(void); /* filler only */
int memcmp(void); /* filler only */
int mktime(void); /* filler only */
int nan(void); /* filler only */
int nanf(void); /* filler only */
int nanl(void); /* filler only */
int perror(void); /* filler only */
int putc(void); /* filler only */
int putchar(void); /* filler only */
int puts(void); /* filler only */
int putwc(void); /* filler only */
int putwchar(void); /* filler only */
int qsort(void); /* filler only */
int quick_exit(void); /* filler only */
int realloc(void); /* filler only */
int remove(void); /* filler only */
int rename(void); /* filler only */
int rewind(void); /* filler only */
int scanf(void); /* filler only */
int setbuf(void); /* filler only */
int setvbuf(void); /* filler only */
int sprintf(void); /* filler only */
int sscanf(void); /* filler only */
int strcat(void); /* filler only */
int strchr(void); /* filler only */
int strcoll(void); /* filler only */
int strcpy(void); /* filler only */
int strcspn(void); /* filler only */
int strerror(void); /* filler only */
int strftime(void); /* filler only */
int strncat(void); /* filler only */
int strncmp(void); /* filler only */
int strpbrk(void); /* filler only */
int strrchr(void); /* filler only */
int strspn(void); /* filler only */
int strstr(void); /* filler only */
int strtod(void); /* filler only */
int strtof(void); /* filler only */
int strtok(void); /* filler only */
int strtol(void); /* filler only */
int strtold(void); /* filler only */
int strtoll(void); /* filler only */
int strtoul(void); /* filler only */
int strtoull(void); /* filler only */
int strxfrm(void); /* filler only */
int swprintf(void); /* filler only */
int swscanf(void); /* filler only */
int time(void); /* filler only */
int time_t(void); /* filler only */
int timespec(void); /* filler only */
int timespec_get(void); /* filler only */
int tm(void); /* filler only */
int tmpfile(void); /* filler only */
int tmpnam(void); /* filler only */
int tolower(void); /* filler only */
int toupper(void); /* filler only */
int ungetc(void); /* filler only */
int ungetwc(void); /* filler only */
int vfprintf(void); /* filler only */
int vfscanf(void); /* filler only */
int vfwprintf(void); /* filler only */
int vfwscanf(void); /* filler only */
int vprintf(void); /* filler only */
int vscanf(void); /* filler only */
int vsnprintf(void); /* filler only */
int vsprintf(void); /* filler only */
int vsscanf(void); /* filler only */
int vswprintf(void); /* filler only */
int vswscanf(void); /* filler only */
int vwprintf(void); /* filler only */
int vwscanf(void); /* filler only */
int wcrtomb(void); /* filler only */
int wcscat(void); /* filler only */
int wcschr(void); /* filler only */
int wcscmp(void); /* filler only */
int wcscoll(void); /* filler only */
int wcscpy(void); /* filler only */
int wcscspn(void); /* filler only */
int wcsftime(void); /* filler only */
int wcslen(void); /* filler only */
int wcsncat(void); /* filler only */
int wcsncmp(void); /* filler only */
int wcsncpy(void); /* filler only */
int wcspbrk(void); /* filler only */
int wcsrchr(void); /* filler only */
int wcsrtombs(void); /* filler only */
int wcsspn(void); /* filler only */
int wcsstr(void); /* filler only */
int wcstod(void); /* filler only */
int wcstof(void); /* filler only */
int wcstok(void); /* filler only */
int wcstol(void); /* filler only */
int wcstold(void); /* filler only */
int wcstoll(void); /* filler only */
int wcstombs(void); /* filler only */
int wcstoul(void); /* filler only */
int wcstoull(void); /* filler only */
int wcsxfrm(void); /* filler only */
int wctob(void); /* filler only */
int wctomb(void); /* filler only */
int wint_t(void); /* filler only */
int wmemchr(void); /* filler only */
int wmemcmp(void); /* filler only */
int wmemcpy(void); /* filler only */
int wmemmove(void); /* filler only */
int wmemset(void); /* filler only */
int wprintf(void); /* filler only */
int wscanf(void); /* filler only */
#define _INFINITY     (1.0e300*1.0e300)
#define _NEG_INFINITY (-1.0e300*1.0e300)
#define _NAN          (0.0e0/(0.0e0*0.0e0))
#define CLOCKS_PER_SEC 1000
#define WCHAR_MAX 0xffffu
#define FP_NAN 0
#define FP_INFINITE 1
#define FP_ZERO 2
#define FP_SUBNORMAL 3
#define FP_NORMAL 4
#endif
