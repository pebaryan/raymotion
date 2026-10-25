// plot_util.hpp -- helpers shared by plot.cpp and plot_charts.cpp (defined in plot.cpp).
#pragma once
#include "rmtn/math.hpp"
#include "rmtn/plot.hpp"
#include <string>
#include <vector>

namespace rmtn {
namespace plot {
namespace detail {

float clamp01(float v);
float easeOut(float v);
int decimalsFor(double step);
std::string number(double v, int decimals);
std::string textTex(const std::string& s);   // plain text as \text{...}
const math::Formula& formula(const std::string& tex, float size, Color color);   // cached
Range fitted(Range r, float px, float minPx);   // an automatic step widened until ticks are minPx apart
bool clipSegment(Vector2& a, Vector2& b, Rectangle r);
Color seriesColor(const Table& t, size_t i, const std::vector<Color>& pal);
void categoryLabels(const Ctx& c, const std::vector<std::string>& names, const Axes& a, const ChartStyle& s, float u);
void legend(const Ctx& c, const Table& t, const ChartStyle& s, float u);

// (plot_charts.cpp)
const Color kDark{ 20, 23, 31, 255 }, kWhite{ 255, 255, 255, 255 };
float stagger(float u, size_t i, size_t n, float dur);   // item i of n: each takes dur of u, starts spread over the rest
Color pick(const std::vector<Color>& pal, size_t i);    // palette entry i (the built-in one when empty)
float textWidth(const std::string& tex, float size);
std::vector<Vector2> upToX(const std::vector<Vector2>& pts, float xEnd);   // prefix of a left-to-right polyline
vg::Path sector(Vector2 c, float rIn, float rOut, float fromDeg, float toDeg);
Vector2 outward(float dx, float dy);                     // label anchor for a direction
Color onFill(Color fill);                                // readable text colour on a fill
std::vector<std::string> splitCsv(const std::string& line);

} } } // rmtn::plot::detail
