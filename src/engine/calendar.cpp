// calendar.cpp -- cal::: civil-date arithmetic (Howard Hinnant's days_from_civil and
// civil_from_days), formatting, the frame clock, and the page and month drawings.
#include "rmtn/calendar.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include "rmtn/fonts.hpp"

namespace rmtn {
namespace cal {

long days(Date d) {
    const long y = d.year - (d.month <= 2);
    const long era = (y >= 0 ? y : y - 399) / 400;
    const long yoe = y - era * 400;
    const long doy = (153 * (d.month + (d.month > 2 ? -3 : 9)) + 2) / 5 + d.day - 1;
    const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

Date date(long z) {
    z += 719468;
    const long era = (z >= 0 ? z : z - 146096) / 146097;
    const long doe = z - era * 146097;
    const long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const long doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const long mp = (5 * doy + 2) / 153;
    const int d = (int)(doy - (153 * mp + 2) / 5 + 1), m = (int)(mp < 10 ? mp + 3 : mp - 9);
    return { (int)(yoe + era * 400 + (m <= 2)), m, d };
}

Date date(double day) { return date((long)std::floor(day)); }

int weekday(Date d) {
    const long z = days(d);
    return (int)(((z + 3) % 7 + 7) % 7);        // 1970-01-01 was a Thursday
}

int daysInMonth(int year, int month) {
    static const int n[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month == 2) return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) ? 29 : 28;
    return n[std::clamp(month, 1, 12) - 1];
}

Date parse(const std::string& iso) {
    int y = 0, m = 0, d = 0;
    char tail = 0;
    if (std::sscanf(iso.c_str(), "%d-%d-%d%c", &y, &m, &d, &tail) != 3 || m < 1 || m > 12 || d < 1 || d > daysInMonth(y, m))
        throw std::runtime_error("cal::parse: not a YYYY-MM-DD date: \"" + iso + "\"");
    return { y, m, d };
}

const char* monthName(int month, bool abbreviated) {
    static const char* full[] = { "January", "February", "March", "April", "May", "June", "July",
                                  "August", "September", "October", "November", "December" };
    static const char* abbr[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    const int i = std::clamp(month, 1, 12) - 1;
    return abbreviated ? abbr[i] : full[i];
}

const char* weekdayName(int wd, bool abbreviated) {
    static const char* full[] = { "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday" };
    static const char* abbr[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
    const int i = ((wd % 7) + 7) % 7;
    return abbreviated ? abbr[i] : full[i];
}

std::string format(Date d, const std::string& pattern) {
    std::string out;
    char buf[16];
    for (size_t i = 0; i < pattern.size(); i++) {
        if (pattern[i] != '%' || i + 1 == pattern.size()) { out += pattern[i]; continue; }
        switch (pattern[++i]) {
        case 'Y': std::snprintf(buf, sizeof buf, "%d", d.year); out += buf; break;
        case 'm': std::snprintf(buf, sizeof buf, "%02d", d.month); out += buf; break;
        case 'd': std::snprintf(buf, sizeof buf, "%02d", d.day); out += buf; break;
        case 'e': std::snprintf(buf, sizeof buf, "%d", d.day); out += buf; break;
        case 'b': out += monthName(d.month, true); break;
        case 'B': out += monthName(d.month); break;
        case 'a': out += weekdayName(weekday(d), true); break;
        case 'A': out += weekdayName(weekday(d)); break;
        case 'j': std::snprintf(buf, sizeof buf, "%03ld", days(d) - days({ d.year, 1, 1 }) + 1); out += buf; break;
        case '%': out += '%'; break;
        default: out += '%'; out += pattern[i]; break;
        }
    }
    return out;
}

Clock& Clock::key(double frame, double day) {
    keys.push_back({ frame, day });
    std::stable_sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    return *this;
}

double Clock::day(double frame) const {
    if (keys.empty()) return 0;
    if (frame <= keys.front().first) return keys.front().second;
    if (frame >= keys.back().first) return keys.back().second;
    for (size_t i = 1; i < keys.size(); i++) {
        if (frame > keys[i].first) continue;
        const auto& a = keys[i - 1];
        const auto& b = keys[i];
        const double span = b.first - a.first;
        return span <= 0 ? b.second : a.second + (b.second - a.second) * (frame - a.first) / span;
    }
    return keys.back().second;
}

// ================================================================ drawing

namespace {

float smooth(float x) {
    x = std::clamp(x, 0.f, 1.f);
    return x * x * (3 - 2 * x);
}

// How far the current day has turned towards the next (0 until `turn`, 1 at the next day).
float turning(double day, const Style& st) {
    const double f = day - std::floor(day);
    return smooth((float)((f - st.turn) / std::max(1e-3, 1.0 - st.turn)));
}

std::string upper(std::string s) {
    for (char& ch : s) ch = (char)std::toupper((unsigned char)ch);
    return s;
}

// One page, its top-left at the context's origin.
void sheet(const Ctx& c, float w, float h, Date d, const Style& st) {
    const float r = w * 0.06f, band = h * 0.25f;
    dr::roundRect(c, 0, 0, w, h, r, st.paper);
    dr::roundRect(c, 0, 0, w, band, r, st.accent);
    dr::rect(c, 0, band - r, w, r, st.accent);
    char head[32];
    std::snprintf(head, sizeof head, "%s %d", upper(monthName(d.month, true)).c_str(), d.year);
    dr::text(fonts::ui(true), c, head, { w / 2, band * 0.54f }, band * 0.46f, band * 0.04f, st.header, 0.5, 0.5);
    char num[8];
    std::snprintf(num, sizeof num, "%d", d.day);
    const int wd = weekday(d);
    dr::text(fonts::ui(true), c, num, { w / 2, band + (h - band) * (st.weekday ? 0.43f : 0.5f) }, (h - band) * 0.62f, 0,
             wd == 6 ? st.accent : st.ink, 0.5, 0.5);
    if (st.weekday)
        dr::text(fonts::ui(false), c, upper(weekdayName(wd)).c_str(), { w / 2, band + (h - band) * 0.84f }, (h - band) * 0.12f,
                 (h - band) * 0.02f, st.muted, 0.5, 0.5);
}

Ctx at(const Ctx& c, Vector2 local, float rot, float alpha) {
    Ctx k = c;
    k.o = mapP(c, local);
    k.rot = c.rot + rot;
    k.a = c.a * alpha;
    return k;
}

} // namespace

void page(const Ctx& c, Rectangle r, double day, const Style& st) {
    const long whole = (long)std::floor(day);
    const float k = turning(day, st);
    // The pad: a shadow and the edges of the pages below.
    dr::roundRect(c, r.x + r.width * 0.03f, r.y + r.height * 0.04f, r.width, r.height, r.width * 0.06f, st.shadow);
    for (int i = 3; i >= 1; i--)
        dr::roundRect(c, r.x, r.y + i * 3.f, r.width, r.height, r.width * 0.06f, mixC(st.paper, st.muted, 0.18f * i));
    // Binder rings over the top edge (under a page on its way off).
    auto rings = [&] {
        for (int i = 0; i < 2; i++) {
            const float x = r.x + r.width * (i ? 0.72f : 0.28f);
            dr::roundRect(c, x - r.width * 0.025f, r.y - r.height * 0.05f, r.width * 0.05f, r.height * 0.1f, r.width * 0.025f,
                          mixC(st.ink, st.muted, 0.3f));
        }
    };
    if (k <= 0) {
        sheet(at(c, { r.x, r.y }, 0, 1), r.width, r.height, date(whole), st);
        rings();
        return;
    }
    sheet(at(c, { r.x, r.y }, 0, 1), r.width, r.height, date(whole + 1), st);
    rings();
    if (k < 1) {
        // Torn off: up and to the left, turning, fading only once it is clear of the pad.
        const float lift = std::pow(k, 1.4f);
        sheet(at(c, { r.x - r.width * 0.35f * lift, r.y - r.height * 0.8f * lift }, -24 * lift, 1 - smooth((k - 0.55f) / 0.45f)),
              r.width, r.height, date(whole), st);
    }
}

void month(const Ctx& c, Rectangle r, double day, const Style& st) {
    const long whole = (long)std::floor(day);
    const float k = turning(day, st);
    const Date d = date(whole), next = date(whole + 1);
    const float titleH = r.height * 0.16f, headH = r.height * 0.1f, cellW = r.width / 7, cellH = (r.height - titleH - headH) / 6;
    dr::roundRect(c, r.x + r.width * 0.015f, r.y + r.height * 0.02f, r.width, r.height, r.width * 0.035f, st.shadow);
    dr::roundRect(c, r.x, r.y, r.width, r.height, r.width * 0.035f, st.paper);
    auto cell = [&](Date x) {
        const int first = (weekday({ x.year, x.month, 1 }) - st.firstWeekday + 7) % 7;
        const int i = first + x.day - 1;
        return Vector2{ r.x + cellW * (i % 7 + 0.5f), r.y + titleH + headH + cellH * (i / 7 + 0.5f) };
    };
    auto grid = [&](Date m, float dy, float alpha, Vector2 ring, float ringAlpha) {
        Ctx o = c;
        o.a *= alpha;
        o.o = mapP(c, { 0, dy });
        char title[32];
        std::snprintf(title, sizeof title, "%s %d", monthName(m.month), m.year);
        dr::text(fonts::ui(true), o, title, { r.x + cellW * 0.2f, r.y + titleH * 0.5f }, titleH * 0.62f, 0, st.ink, 0, 0.5);
        for (int i = 0; i < 7; i++) {
            const int wd = (i + st.firstWeekday) % 7;
            const char ini[2] = { weekdayName(wd)[0], 0 };
            dr::text(fonts::ui(false), o, ini, { r.x + cellW * (i + 0.5f), r.y + titleH + headH * 0.5f }, headH * 0.62f, 0,
                     wd == 6 ? st.accent : st.muted, 0.5, 0.5);
        }
        Ctx ringCtx = o;
        ringCtx.a *= ringAlpha;
        if (ringAlpha > 0) dr::circle(ringCtx, ring, std::min(cellW, cellH) * 0.44f, st.accent);
        const int n = daysInMonth(m.year, m.month);
        for (int dd = 1; dd <= n; dd++) {
            const Date x{ m.year, m.month, dd };
            const Vector2 p = cell(x);
            const bool on = ringAlpha > 0 && std::hypot(p.x - ring.x, p.y - ring.y) < std::min(cellW, cellH) * 0.3f;
            char num[4];
            std::snprintf(num, sizeof num, "%d", dd);
            dr::text(fonts::ui(on), o, num, p, cellH * 0.48f, 0, on ? st.header : (weekday(x) == 6 ? st.accent : st.ink), 0.5, 0.5);
        }
    };
    if (next.month == d.month) {
        const Vector2 a = cell(d), b = cell(next);
        grid(d, 0, 1, { a.x + (b.x - a.x) * k, a.y + (b.y - a.y) * k }, 1);
    } else {
        const float slide = r.height * 0.04f;   // the old month rises out, the new one rises in
        if (k < 1) grid(d, -slide * k, 1 - k, cell(d), 1);
        if (k > 0) grid(next, slide * (1 - k), k, cell(next), 1);
    }
}

} // namespace cal
} // namespace rmtn
