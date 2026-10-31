#include "rmtn/script.hpp"
#include "rmtn/draw.hpp"
#include "rmtn/fonts.hpp"
#include "rmtn/media.hpp"
#include <angelscript.h>
#include <scriptstdstring.h>
#include <scriptmath.h>
#include <scriptarray.h>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <new>
#include <vector>
#include <algorithm>

namespace rmtn::script {
namespace {
void check(int result) {
    if (result < 0) throw std::runtime_error("AngelScript API registration failed: " + std::to_string(result));
}
struct Invocation {
    const Json* props;
    Description* description = nullptr;
    Ctx context{};
    bool drawing = false;
    std::vector<Ctx> transforms;
    unsigned int remaining = 1000000;
};
Invocation& invocation() { return *static_cast<Invocation*>(asGetActiveContext()->GetUserData()); }
void lineCallback(asIScriptContext* ctx, void*) {
    auto& call = *static_cast<Invocation*>(ctx->GetUserData());
    if (--call.remaining == 0) ctx->Abort();
}
void message(const asSMessageInfo* msg, void* output) {
    *static_cast<std::string*>(output) += std::string(msg->section) + ":" + std::to_string(msg->row) + ":" +
        std::to_string(msg->col) + ": " + msg->message + "\n";
}
enum Operation { Rect, Circle, Line, RoundRect, Gradient, FillPoly, Image, Text, Push, Pop,
                 SceneOp, NumberProp, TextProp, FlagProp };
template<Operation op> void invoke(asIScriptGeneric* g) {
    try {
        auto& call = invocation();
        auto f = [g](int i) { return g->GetArgFloat(i); };
        auto str = [g](int i) -> const std::string& { return *static_cast<std::string*>(g->GetArgObject(i)); };
        if constexpr (op == SceneOp) {
            if (!call.description) throw std::runtime_error("scene() is only available in configure()");
            call.description->scenes.push_back({(int)g->GetArgDWord(0), (int)g->GetArgDWord(1), str(2)});
        } else if constexpr (op == NumberProp) {
            g->SetReturnDouble(call.props->num(str(0), g->GetArgDouble(1)));
        } else if constexpr (op == TextProp) {
            new(g->GetAddressOfReturnLocation()) std::string(call.props->str(str(0), str(1)));
        } else if constexpr (op == FlagProp) {
            g->SetReturnByte(call.props->flag(str(0), g->GetArgByte(1) != 0));
        } else if constexpr (op == Push) {
            if (!call.drawing) throw std::runtime_error("draw functions are only available in scene callbacks");
            Ctx next = call.context;
            call.transforms.push_back(next);
            next.o = mapP(next, {f(0), f(1)});
            next.rot += f(2);
            next.s *= f(3);
            next.a *= std::clamp(f(4), 0.0f, 1.0f);
            call.context = next;
        } else if constexpr (op == Pop) {
            if (!call.drawing) throw std::runtime_error("draw functions are only available in scene callbacks");
            if (call.transforms.empty()) throw std::runtime_error("draw::pop() has no matching draw::push()");
            call.context = call.transforms.back();
            call.transforms.pop_back();
        } else {
            if (!call.drawing) throw std::runtime_error("draw functions are only available in scene callbacks");
            const Ctx& c = call.context;
            if constexpr (op == Rect) dr::rect(c, f(0), f(1), f(2), f(3), withAlpha(mkColor(g->GetArgDWord(4)), f(5)));
            if constexpr (op == Circle) dr::circle(c, {f(0),f(1)}, f(2), withAlpha(mkColor(g->GetArgDWord(3)), f(4)));
            if constexpr (op == Line) dr::strokeSeg(c, {f(0),f(1)}, {f(2),f(3)}, f(4), withAlpha(mkColor(g->GetArgDWord(5)), f(6)));
            if constexpr (op == RoundRect) dr::roundRect(c, f(0), f(1), f(2), f(3), f(4), withAlpha(mkColor(g->GetArgDWord(5)), f(6)));
            if constexpr (op == Gradient) dr::rectGrad4(c, f(0), f(1), f(2), f(3),
                withAlpha(mkColor(g->GetArgDWord(4)), f(8)), withAlpha(mkColor(g->GetArgDWord(5)), f(8)),
                withAlpha(mkColor(g->GetArgDWord(6)), f(8)), withAlpha(mkColor(g->GetArgDWord(7)), f(8)));
            if constexpr (op == FillPoly) {
                auto* values = static_cast<CScriptArray*>(g->GetArgObject(0));
                const asUINT count = values ? values->GetSize() : 0;
                if (count < 6 || count % 2 || count > 2048)
                    throw std::runtime_error("draw::fillPoly() needs 3–1024 x/y pairs");
                std::vector<Vector2> points; points.reserve(count / 2);
                for (asUINT i = 0; i < count; i += 2) {
                    points.push_back({*static_cast<float*>(values->At(i)), *static_cast<float*>(values->At(i+1))});
                }
                dr::fillPoly(c, points.data(), (int)points.size(), withAlpha(mkColor(g->GetArgDWord(1)), f(2)));
            }
            if constexpr (op == Image) {
                auto asset = media::image(str(0));
                media::ImageStyle style; style.tint = withAlpha(WHITE, f(5));
                media::drawImage(c, *asset, {f(1), f(2), f(3), f(4)}, style, 0);
            }
            if constexpr (op == Text) {
                Font font = fonts::ui(g->GetArgDWord(8) == 1);
                if (g->GetArgDWord(8) == 2) font = fonts::load({"JetBrainsMono-Regular.ttf"});
                if (g->GetArgDWord(8) == 3) font = fonts::load({"JetBrainsMono-Bold.ttf"});
                dr::text(font, c, str(0).c_str(), {f(1),f(2)}, f(3), 0,
                    withAlpha(mkColor(g->GetArgDWord(4)), f(5)), f(6), f(7));
            }
        }
    } catch (const std::exception& error) { asGetActiveContext()->SetException(error.what()); }
}
struct EngineDeleter { void operator()(asIScriptEngine* p) const { p->ShutDownAndRelease(); } };
struct ContextDeleter { void operator()(asIScriptContext* p) const { p->Release(); } };
class AngelProgram final : public Program {
    // diagnostics outlives the engine callback, including during destruction.
    std::string diagnostics_, path_;
    std::unique_ptr<asIScriptEngine, EngineDeleter> engine_;
    std::unique_ptr<asIScriptContext, ContextDeleter> context_;
    asIScriptModule* module_ = nullptr;
    asIScriptFunction* function(const std::string& name) {
        auto declaration = "void " + name + "(int)";
        auto* fn = module_->GetFunctionByDecl(declaration.c_str());
        if (!fn) throw std::runtime_error(path_ + ": missing " + declaration);
        return fn;
    }
    void execute(asIScriptFunction* fn, Invocation& call, void* object, int frame) {
        auto* ctx = context_.get();
        if (!ctx) throw std::runtime_error("cannot allocate script context");
        if (ctx->GetState() != asEXECUTION_UNINITIALIZED) ctx->Unprepare();
        check(ctx->Prepare(fn));
        ctx->SetUserData(&call);
        check(ctx->SetLineCallback(asFUNCTION(lineCallback), nullptr, asCALL_CDECL));
        if (object) check(ctx->SetArgObject(0, object));
        else check(ctx->SetArgDWord(0, frame));
        int result = ctx->Execute();
        struct ResetContext {
            asIScriptContext* value;
            ~ResetContext() {
                value->SetUserData(nullptr);
                if (value->GetState() != asEXECUTION_UNINITIALIZED) value->Unprepare();
            }
        } reset{ctx};
        if (result == asEXECUTION_FINISHED) return;
        if (result == asEXECUTION_EXCEPTION) {
            const char* section = nullptr;
            int row = ctx->GetExceptionLineNumber(nullptr, &section);
            throw std::runtime_error(std::string(section ? section : path_.c_str()) + ":" + std::to_string(row) +
                " in " + ctx->GetExceptionFunction()->GetDeclaration() + ": " + ctx->GetExceptionString());
        }
        throw std::runtime_error(path_ + ": script execution aborted (statement budget exceeded)");
    }
public:
    explicit AngelProgram(const std::string& path) : path_(path), engine_(asCreateScriptEngine()) {
        if (!engine_) throw std::runtime_error("cannot initialize AngelScript");
        auto* e = engine_.get();
        context_.reset(e->CreateContext());
        if (!context_) throw std::runtime_error("cannot allocate script context");
        check(e->SetMessageCallback(asFUNCTION(message), &diagnostics_, asCALL_CDECL));
        // No persistent script variables: seeking or drawing frames out of order stays deterministic.
        check(e->SetEngineProperty(asEP_DISALLOW_GLOBAL_VARS, true));
        RegisterStdString(e);
        RegisterScriptMath(e);
        check(e->RegisterObjectType("Composition", 0, asOBJ_REF | asOBJ_NOCOUNT));
#define FIELD(type, name) check(e->RegisterObjectProperty("Composition", type " " #name, asOFFSET(Description, name)))
        FIELD("string", name); FIELD("int", w); FIELD("int", h); FIELD("int", fps);
        FIELD("int", duration); FIELD("int", supersample); FIELD("uint", background);
        FIELD("int", statementBudget);
#undef FIELD
        check(e->RegisterGlobalFunction("void scene(int start, int duration, const string &in function)", asFUNCTION(invoke<SceneOp>), asCALL_GENERIC));
        check(e->RegisterGlobalFunction("double ease(int kind, double t)", asFUNCTION(easeFn), asCALL_CDECL));
        check(e->RegisterGlobalFunction("double clamp01(double t)", asFUNCTION(clamp01), asCALL_CDECL));
        check(e->RegisterGlobalFunction("double rnd01(uint64 seed, uint64 index)", asFUNCTION(rnd01), asCALL_CDECL));
        check(e->RegisterGlobalFunction("double rrange(uint64 seed, uint64 index, double a, double b)", asFUNCTION(rrange), asCALL_CDECL));
        RegisterScriptArray(e, true);
        check(e->RegisterEnum("Ease"));
        const char* names[] = {"E_LINEAR", "E_IN_QUAD", "E_OUT_QUAD", "E_IN_OUT_QUAD", "E_IN_CUBIC", "E_OUT_CUBIC",
            "E_IN_OUT_CUBIC", "E_IN_EXPO", "E_OUT_EXPO", "E_IN_OUT_EXPO", "E_BACK", "E_ELASTIC", "E_BOUNCE"};
        for (int i = 0; i < E_COUNT; ++i) check(e->RegisterEnumValue("Ease", names[i], i));
        check(e->SetDefaultNamespace("props"));
        check(e->RegisterGlobalFunction("double number(const string &in key, double fallback = 0)", asFUNCTION(invoke<NumberProp>), asCALL_GENERIC));
        check(e->RegisterGlobalFunction("string text(const string &in key, const string &in fallback = '')", asFUNCTION(invoke<TextProp>), asCALL_GENERIC));
        check(e->RegisterGlobalFunction("bool flag(const string &in key, bool fallback = false)", asFUNCTION(invoke<FlagProp>), asCALL_GENERIC));
        check(e->SetDefaultNamespace("draw"));
        check(e->RegisterGlobalFunction("void rect(float x, float y, float w, float h, uint color, float alpha = 1)", asFUNCTION(invoke<Rect>), asCALL_GENERIC));
        check(e->RegisterGlobalFunction("void circle(float x, float y, float radius, uint color, float alpha = 1)", asFUNCTION(invoke<Circle>), asCALL_GENERIC));
        check(e->RegisterGlobalFunction("void line(float x1, float y1, float x2, float y2, float width, uint color, float alpha = 1)", asFUNCTION(invoke<Line>), asCALL_GENERIC));
        check(e->RegisterGlobalFunction("void roundRect(float x, float y, float w, float h, float radius, uint color, float alpha = 1)", asFUNCTION(invoke<RoundRect>), asCALL_GENERIC));
        check(e->RegisterGlobalFunction("void gradient(float x, float y, float w, float h, uint topLeft, uint topRight, uint bottomRight, uint bottomLeft, float alpha = 1)", asFUNCTION(invoke<Gradient>), asCALL_GENERIC));
        check(e->RegisterGlobalFunction("void fillPoly(const array<float> &in points, uint color, float alpha = 1)", asFUNCTION(invoke<FillPoly>), asCALL_GENERIC));
        check(e->RegisterGlobalFunction("void image(const string &in file, float x, float y, float w, float h, float alpha = 1)", asFUNCTION(invoke<Image>), asCALL_GENERIC));
        check(e->RegisterGlobalFunction("void push(float x = 0, float y = 0, float rotation = 0, float scale = 1, float alpha = 1)", asFUNCTION(invoke<Push>), asCALL_GENERIC));
        check(e->RegisterGlobalFunction("void pop()", asFUNCTION(invoke<Pop>), asCALL_GENERIC));
        check(e->RegisterEnum("FontFace"));
        check(e->RegisterEnumValue("FontFace", "FONT_UI", 0));
        check(e->RegisterEnumValue("FontFace", "FONT_UI_BOLD", 1));
        check(e->RegisterEnumValue("FontFace", "FONT_MONO", 2));
        check(e->RegisterEnumValue("FontFace", "FONT_MONO_BOLD", 3));
        check(e->RegisterGlobalFunction("void text(const string &in text, float x, float y, float size, uint color, float alpha = 1, float anchorX = 0, float anchorY = 0, FontFace face = FONT_UI)", asFUNCTION(invoke<Text>), asCALL_GENERIC));
        check(e->SetDefaultNamespace(""));
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("cannot read script: " + path);
        std::ostringstream source; source << input.rdbuf();
        auto text = source.str();
        module_ = e->GetModule("composition", asGM_ALWAYS_CREATE);
        check(module_->AddScriptSection(path.c_str(), text.data(), text.size()));
        if (module_->Build() < 0) throw std::runtime_error(diagnostics_);
    }
    Description configure(const Json& props) override {
        Description result;
        auto* fn = module_->GetFunctionByDecl("void configure(Composition &inout)");
        if (!fn) throw std::runtime_error(path_ + ": missing void configure(Composition &inout)");
        Invocation call; call.props = &props; call.description = &result;
        execute(fn, call, &result, 0);
        if (result.scenes.empty()) function("draw");
        for (const auto& scene : result.scenes) function(scene.function);
        return result;
    }
    void draw(const std::string& name, const Ctx& ctx, int frame,
              const Json& props, int statementBudget) override {
        Invocation call; call.props = &props; call.context = ctx; call.drawing = true;
        call.remaining = (unsigned int)statementBudget;
        execute(function(name), call, nullptr, frame);
        if (!call.transforms.empty()) throw std::runtime_error(path_ + ": unbalanced draw::push()/draw::pop() in " + name);
    }
};
}
std::shared_ptr<Program> loadAngelScript(const std::string& path) { return std::make_shared<AngelProgram>(path); }
} // namespace rmtn::script
