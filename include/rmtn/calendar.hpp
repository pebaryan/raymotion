// rmtn/calendar.hpp -- dates (cal::): calendar arithmetic, formatting, a clock that maps
// frames to dates, and animated calendars to draw them.
//
//     cal::Clock clock;
//     clock.key(0, cal::days({ 2024, 10, 20 }));          // frame 0: 20 Oct 2024
//     clock.key(300, cal::days({ 2026, 9, 13 }));         // frame 300: 13 Sep 2026
//     node->fn = [clock](const Ctx& cx, int t) {
//         cal::page(cx, { 1600, 80, 220, 250 }, clock.day(t));   // a tear-off page per day
//         cal::month(cx, { 1500, 360, 360, 300 }, clock.day(t)); // the month, the day ringed
//     };
//
// A day is a count of days since 1 January 1970 (proleptic Gregorian). Fractional days
// animate: as the fraction passes `Style::turn`, the page tears off (or the ring moves on,
// or the next month rises in), so the next day arrives at the whole number.
#pragma once
#include <string>
#include <utility>
#include <vector>
#include "draw.hpp"

namespace rmtn {
namespace cal {

struct Date {
    int year = 1970, month = 1, day = 1;   // month 1..12, day 1..31
};

long days(Date d);                          // days since 1970-01-01
Date date(long days);
Date date(double day);                      // the whole day `day` falls in
int weekday(Date d);                        // 0 = Monday .. 6 = Sunday
int daysInMonth(int year, int month);
Date parse(const std::string& iso);         // "2024-11-08"; throws std::runtime_error when malformed
const char* monthName(int month, bool abbreviated = false);   // English
const char* weekdayName(int weekday, bool abbreviated = false);
// strftime-like: %Y year, %m month 01-12, %d day 01-31, %e day 1-31, %b Jan, %B January,
// %a Mon, %A Monday, %j day of the year 001-366, %% a percent sign.
std::string format(Date d, const std::string& pattern = "%e %B %Y");

// Frames to days: keys (frame, day), linear between them, held outside them. Add keys in
// any order. A key whose day repeats the previous one holds the date (a pause).
struct Clock {
    std::vector<std::pair<double, double>> keys;
    Clock& key(double frame, double day);
    Clock& key(double frame, Date d) { return key(frame, (double)days(d)); }
    double day(double frame) const;
    Date date(double frame) const { return cal::date(day(frame)); }
};

struct Style {
    Color paper{ 250, 248, 242, 255 }, ink{ 32, 34, 44, 255 }, accent{ 214, 69, 65, 255 };
    Color header{ 255, 255, 255, 255 };     // text on the accent band
    Color muted{ 150, 152, 162, 255 };      // weekdays, other months' days
    Color shadow{ 0, 0, 0, 90 };
    float turn = 0.55f;                     // the fraction of a day when the page starts to go
    bool weekday = true;                    // page: the weekday under the number
    int firstWeekday = 0;                   // month: 0 = weeks start on Monday, 6 = Sunday
};

// A tear-off desk calendar page: month and year on a band, the day large, the weekday under
// it. Each day's page lifts and flies off as the next arrives.
void page(const Ctx& c, Rectangle r, double day, const Style& style = {});
// A month on a card: title, weekday initials, the days, the current day ringed. The ring glides
// to the next day; at a month's end the next month rises in as the old one fades.
void month(const Ctx& c, Rectangle r, double day, const Style& style = {});

} // namespace cal
} // namespace rmtn
