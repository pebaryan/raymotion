// math_font.hpp -- an OpenType math font for the math:: typesetter (engine-internal):
// glyph outlines and metrics through stb_truetype, and the MATH table's constants, italic
// corrections, top-accent attachments, size variants and glyph assemblies.
#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "rmtn/math.hpp"

namespace rmtn {
namespace math {

// MathConstants records, in table order (OpenType MATH spec).
enum Mc {
    MathLeading, AxisHeight, AccentBaseHeight, FlattenedAccentBaseHeight, SubscriptShiftDown, SubscriptTopMax,
    SubscriptBaselineDropMin, SuperscriptShiftUp, SuperscriptShiftUpCramped, SuperscriptBottomMin,
    SuperscriptBaselineDropMax, SubSuperscriptGapMin, SuperscriptBottomMaxWithSubscript, SpaceAfterScript,
    UpperLimitGapMin, UpperLimitBaselineRiseMin, LowerLimitGapMin, LowerLimitBaselineDropMin, StackTopShiftUp,
    StackTopDisplayStyleShiftUp, StackBottomShiftDown, StackBottomDisplayStyleShiftDown, StackGapMin,
    StackDisplayStyleGapMin, StretchStackTopShiftUp, StretchStackBottomShiftDown, StretchStackGapAboveMin,
    StretchStackGapBelowMin, FractionNumeratorShiftUp, FractionNumeratorDisplayStyleShiftUp,
    FractionDenominatorShiftDown, FractionDenominatorDisplayStyleShiftDown, FractionNumeratorGapMin,
    FractionNumDisplayStyleGapMin, FractionRuleThickness, FractionDenominatorGapMin, FractionDenomDisplayStyleGapMin,
    SkewedFractionHorizontalGap, SkewedFractionVerticalGap, OverbarVerticalGap, OverbarRuleThickness,
    OverbarExtraAscender, UnderbarVerticalGap, UnderbarRuleThickness, UnderbarExtraDescender, RadicalVerticalGap,
    RadicalDisplayStyleVerticalGap, RadicalRuleThickness, RadicalExtraAscender, RadicalKernBeforeDegree,
    RadicalKernAfterDegree, McCount
};

// Glyph box in em, y up (as in the font): height = y1, depth = -y0.
struct Metrics { float advance = 0, x0 = 0, y0 = 0, x1 = 0, y1 = 0; };

class Font {
public:
    struct Variant { int glyph; float advance; };
    struct Part { int glyph; float start, end, full; bool extender; };
    struct Construction { std::vector<Variant> variants; std::vector<Part> parts; };

    // Cached by path; "" is the bundled Latin Modern Math. Throws if unreadable or not a math font.
    static std::shared_ptr<const Font> load(const std::string& file);
    ~Font();

    float k[McCount] = {};            // constants, em
    float scriptScale = 0.7f, scriptScriptScale = 0.5f;
    float delimitedSubFormulaMinHeight = 0, displayOperatorMinHeight = 0;
    float radicalDegreeBottomRaise = 0.6f;   // fraction of the radical's height
    float minConnectorOverlap = 0;

    int glyph(int codepoint) const;   // 0 when missing
    Metrics metrics(int glyph) const;
    float italic(int glyph) const;    // italic correction, em (0 if none)
    bool topAccent(int glyph, float& x) const;
    const Construction* vertical(int glyph) const;
    const Construction* horizontal(int glyph) const;
    const vg::Path& outline(int glyph) const;   // em, y down, origin on the baseline

private:
    Font() = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::unordered_map<int, float> italic_, accent_;
    std::unordered_map<int, Construction> vert_, horiz_;
    mutable std::unordered_map<int, vg::Path> outlines_;
};

} } // rmtn::math
