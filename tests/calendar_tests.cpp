// calendar_tests.cpp -- cal::: day counts both ways (epochs, leap years, negative days),
// weekdays, month lengths, parsing, formatting, the frame clock; then drawing (GL): a page
// and a month grid, whose pixels change as the day turns.
#include "rmtn/raymotion.hpp"
#include <rlgl.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>

using namespace rmtn;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)
#define NEAR(a, b, eps) CHECK(std::fabs((double)(a) - (double)(b)) <= (eps))

static bool same(cal::Date a, cal::Date b) { return a.year == b.year && a.month == b.month && a.day == b.day; }

static void arithmetic() {
    CHECK(cal::days({ 1970, 1, 1 }) == 0);
    CHECK(cal::days({ 2000, 3, 1 }) == 11017);
    CHECK(cal::days({ 1969, 12, 31 }) == -1);
    CHECK(cal::days({ 2024, 10, 20 }) - cal::days({ 2024, 1, 1 }) == 293);
    CHECK(same(cal::date(cal::days({ 2024, 2, 29 }) + 1L), { 2024, 3, 1 }));
    CHECK(same(cal::date(cal::days({ 1900, 2, 28 }) + 1L), { 1900, 3, 1 }));   // not a leap year
    for (long z = -800000; z < 800000; z += 997) CHECK(cal::days(cal::date(z)) == z);
    CHECK(same(cal::date(-0.25), { 1969, 12, 31 }));
    CHECK(same(cal::date(20000.9), cal::date(20000L)));

    CHECK(cal::weekday({ 1970, 1, 1 }) == 3);     // Thursday
    CHECK(cal::weekday({ 2024, 10, 20 }) == 6);   // Sunday
    CHECK(cal::weekday({ 1969, 12, 29 }) == 0);   // Monday
    CHECK(cal::daysInMonth(2024, 2) == 29);
    CHECK(cal::daysInMonth(2100, 2) == 28);
    CHECK(cal::daysInMonth(2000, 2) == 29);
    CHECK(cal::daysInMonth(2026, 9) == 30);
}

static void text() {
    CHECK(same(cal::parse("2024-11-08"), { 2024, 11, 8 }));
    for (const char* bad : { "2024-13-01", "2023-02-29", "08/11/2024", "2024-11-08x", "" }) {
        bool threw = false;
        try { cal::parse(bad); } catch (const std::runtime_error&) { threw = true; }
        CHECK(threw);
    }
    const cal::Date d{ 2026, 9, 5 };
    CHECK(cal::format(d) == "5 September 2026");
    CHECK(cal::format(d, "%Y-%m-%d") == "2026-09-05");
    CHECK(cal::format(d, "%a %e %b") == "Sat 5 Sep");
    CHECK(cal::format(d, "%A, day %j, 100%%") == "Saturday, day 248, 100%");
    CHECK(cal::format(d, "%q%") == "%q%");
    CHECK(std::string(cal::monthName(1)) == "January");
    CHECK(std::string(cal::weekdayName(6, true)) == "Sun");
}

static void clocks() {
    cal::Clock c;
    CHECK(c.day(10) == 0);
    c.key(100, cal::Date{ 2024, 11, 8 }).key(0, cal::Date{ 2024, 10, 20 }).key(150, cal::Date{ 2024, 11, 8 });
    NEAR(c.day(-5), cal::days({ 2024, 10, 20 }), 1e-9);
    NEAR(c.day(50), cal::days({ 2024, 10, 20 }) + 9.5, 1e-9);
    CHECK(same(c.date(100), { 2024, 11, 8 }));
    CHECK(same(c.date(130), { 2024, 11, 8 }));    // held
    CHECK(same(c.date(1000), { 2024, 11, 8 }));
}

// ---- drawing

static Image draw(const std::function<void(const Ctx&)>& fn) {
    RenderTexture rt = LoadRenderTexture(400, 300);
    BeginTextureMode(rt);
    rlDisableBackfaceCulling();   // as the renderer does for 2D
    ClearBackground({ 20, 20, 30, 255 });
    fn(Ctx{});
    EndTextureMode();
    Image img = LoadImageFromTexture(rt.texture);
    UnloadRenderTexture(rt);
    return img;
}

static double diff(Image a, Image b) {
    const Color* p = (const Color*)a.data;
    const Color* q = (const Color*)b.data;
    double s = 0;
    for (int i = 0; i < a.width * a.height; i++) s += std::abs(p[i].r - q[i].r) + std::abs(p[i].g - q[i].g) + std::abs(p[i].b - q[i].b);
    return s / (a.width * a.height);
}

static void drawing() {
    const double day = cal::days({ 2026, 9, 30 });
    for (int kind = 0; kind < 2; kind++) {
        auto at = [&](double d) {
            return draw([&](const Ctx& c) {
                if (kind == 0) cal::page(c, { 120, 40, 160, 200 }, d);
                else cal::month(c, { 20, 20, 360, 260 }, d);
            });
        };
        Image a = at(day), b = at(day + 0.3), m = at(day + 0.8), n = at(day + 1);
        CHECK(diff(a, b) < 0.01);       // before `turn`, nothing moves
        CHECK(diff(a, m) > 1);          // turning
        CHECK(diff(a, n) > 1);          // the next day (and month) is up
        CHECK(diff(m, n) > 0.5);
        // Something was drawn: the paper or the text is lighter than the background.
        const Color* p = (const Color*)a.data;
        int light = 0;
        for (int i = 0; i < a.width * a.height; i++) light += p[i].r > 200;
        CHECK(light > 1000);
        for (Image* i : { &a, &b, &m, &n }) UnloadImage(*i);
    }
}

int main() {
    arithmetic();
    text();
    clocks();
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(320, 180, "calendar");
    if (!IsWindowReady()) {
        std::puts("no window: calendar drawing skipped");
        return failures ? 1 : 77;
    }
    drawing();
    shutdownGraphics();
    CloseWindow();
    if (failures) { std::fprintf(stderr, "%d calendar check(s) failed\n", failures); return 1; }
    std::puts("calendar tests passed");
    return 0;
}
