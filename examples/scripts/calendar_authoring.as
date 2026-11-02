void configure(Composition &inout c) {
    c.name = "calendar-authoring";
    c.w = 1280; c.h = 720; c.fps = 30; c.duration = 120;
    c.background = 0x151922;
    scene(0, c.duration, "draw");
}

void draw(int frame) {
    array<double> frameDayPairs = {0, 20000, 120, 20031};
    double currentDay = cal::dayAt(frame, frameDayPairs);
    cal::monthStyled(50, 45, 720, 620, currentDay,
        "{\"paper\":\"#f5eee0\",\"ink\":\"#252b3a\",\"accent\":\"#9d334c\",\"muted\":\"#8f929a\",\"shadow\":\"#00000055\",\"firstWeekday\":6}");
    cal::pageStyled(840, 105, 350, 480, currentDay,
        "{\"paper\":\"#fffaf0\",\"ink\":\"#242b38\",\"accent\":\"#326d63\",\"header\":\"#ffffff\",\"turn\":0.62,\"weekday\":true}");
    int year, month, day;
    cal::date(currentDay, year, month, day);
    draw::text(cal::format(year, month, day, "%A, %e %B %Y"), 840, 635, 24, 0xe9e5dc);
    draw::text("WEEKDAY " + cal::weekdayName(cal::weekday(year, month, day)), 840, 672, 17, 0xb8c6cc);
}
