// fx_present.cpp -- tiered motion-graphics presentation renderer (fx-1-overlays .. fx-4-max).
// T1: base scene + single fullscreen overlay shader (grain/vignette/scanlines/sparkle).
// T2: base captured into an FBO; threshold -> quarter-res CPU buffer, separable blur x2, additive bloom.
// T3: + procedural lit 3D meshes (torus knot + geodesic sphere), painter depth-sort in-scene.
// T4: all layers at once with heavier grade/bloom/vignette.
#include <raylib.h>
#include <rlgl.h>
#include <raymath.h>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include "rmtn/timeline.hpp"
#include "rmtn/draw.hpp"
#include "rmtn/render3d.hpp"
#include "rmtn/render.hpp"

namespace rmtn { static void releaseFrameResources(); }

namespace rmtn {

static const Color NEONC{ 0x27, 0xe6, 0xff, 255 };
static const Color MAGC{ 0xff, 0x3c, 0xd2, 255 };
static const Color AMBC{ 0xff, 0xb1, 0x4e, 255 };

// ---------------------------- GLSL programs ----------------------------
static const char* VS_QUAD = "#version 330\n"
"in vec2 vertexPosition;\n"
"in vec2 vertexTexCoord;\n"
"out vec2 uv;\n"
"uniform mat4 mvp;\n"
"void main(){ gl_Position=mvp*vec4(vertexPosition,0.0,1.0); uv=vertexTexCoord; }\n";

static const char* FS_GRAIN = "#version 330\nnoperspective in vec2 uv;\n"
"layout(location=0) out vec4 f;\n"
"uniform float gtime; uniform vec2 gres; uniform float vig; uniform int gmode;\n"
"float h1(vec2 q){ q=fract(q*0.067 + vec2(0.13,0.71)); q+=dot(q,vec2(15.0,7.0)); q*=17.0; return fract(q.x+q.y); }\n"
"void main(){\n"
"    vec2 p=gl_FragCoord.xy;\n"
"    float n=h1(p + vec2(gtime*97.0, gtime*13.7))-0.5;\n"
"    float scan=pow(0.5+0.5*sin(p.y*6.28318), 12.0);\n"
"    vec2 d=p/gres-0.5; float vd=length(d*vec2(1.15,1.0));\n"
"    float vg=smoothstep(0.42,0.98,vd)*vig;\n"
"    float sp0=h1(p*0.5+vec2(gtime*13.0,floor(p.y)/2.0*7.13));\n"
"    float spark=step(0.9965, h1(floor(p*0.5)+vec2(sp0,sp0*1.7)));\n"
"    float amt=(gmode>2)?1.5:((gmode>1)?1.2:1.0);\n"
"    float a=clamp(abs(n)*0.14*amt + scan*0.10 + vg*0.55 + spark*0.85, 0.0, 1.0);\n"
"    vec3 o=vec3(n*0.09*amt)+vec3(spark);\n"
"    f=vec4(o,a);\n"
"}\n";

// ---------------------------- GL state / targets ----------------------------
struct Shx { Shader grain{}; };
static Shx SHX;
static int gMvp{}, gTime{}, gRes{}, gVig{}, gMode{};
static RenderTexture RTscene{}, RTbloom{};
static bool fxReady=false;
static int fw{}, fh{};

static void fxEnsure(int W,int H){
    if (!fxReady){
        SHX.grain  = LoadShaderFromMemory(VS_QUAD, FS_GRAIN);
        gMvp =GetShaderLocation(SHX.grain,"mvp");  gTime=GetShaderLocation(SHX.grain,"gtime");
        gRes =GetShaderLocation(SHX.grain,"gres"); gVig =GetShaderLocation(SHX.grain,"vig");
        gMode=GetShaderLocation(SHX.grain,"gmode");
        fxReady=true;
        onGraphicsShutdown(releaseFrameResources);
    }
    if (fw!=W || fh!=H){
        if (RTscene.id) UnloadRenderTexture(RTscene);
        if (RTbloom.id) UnloadRenderTexture(RTbloom);
        RTscene=LoadRenderTexture(W,H);
        RTbloom=LoadRenderTexture(W/4,H/4);
        fw=W; fh=H;
    }
}

static inline Matrix orthoPx(float w,float h){ return MatrixOrtho(0.0,(double)w,(double)h,0.0,-100.0,100.0); }

// overlay pass straight onto the window framebuffer (alpha blend, no sampling needed)
static void grainOverlay(float timeT,int W,int H,float vigAmt,int mode){
    BeginBlendMode(BLEND_ALPHA);
    BeginShaderMode(SHX.grain);
    Matrix mo=orthoPx((float)W,(float)H);
    SetShaderValueMatrix(SHX.grain,gMvp,mo);
    SetShaderValue(SHX.grain,gTime,&timeT,SHADER_UNIFORM_FLOAT);
    Vector2 r{ (float)W,(float)H };
    SetShaderValue(SHX.grain,gRes,&r,SHADER_UNIFORM_VEC2);
    SetShaderValue(SHX.grain,gVig,&vigAmt,SHADER_UNIFORM_FLOAT);
    SetShaderValue(SHX.grain,gMode,&mode,SHADER_UNIFORM_INT);
    DrawRectangle(0,0,W,H,WHITE);
    EndShaderMode();
    EndBlendMode();
}

// CPU bloom readback path (GPU sampler passes were fighting raylib internals too long)
//   rlReadTexturePixels(RTscene) -> quarter-res threshold -> separable box blur -> upload RTbloom texture.
static std::vector<unsigned char> gScr, gUp;
static std::vector<float> gA, gB;
static int qw{}, qh{};

static void cpuBloom(int W,int H,int tier){
    const int QW=W/4, QH=H/4;
    if (gScr.size() < (size_t)W*H*4) gScr.resize((size_t)W*H*4);
    if (qw!=QW || qh!=QH){ qw=QW; qh=QH; gA.assign((size_t)QW*QH*4,0.f); gB=gA; gUp.assign((size_t)QW*QH*4,0); }
    void* p = rlReadTexturePixels(RTscene.texture.id, W, H, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    if (!p) return;
    std::memcpy(gScr.data(), p, (size_t)W*H*4);
    std::free(p);                                   // buffer came from RL_CALLOC
    const float thr = (tier>=4)?0.34f:0.42f;
    // threshold + downsample 4x4 with vertical flip (glReadPixels origin bottom-left)
    for (int r=0;r<QH;r++){
        int sy0=(QH-1-r)*4;
        for (int c=0;c<QW;c++){
            float R=0,G=0,B=0,cnt=0;
            for (int dy=0;dy<4;dy+=2)for(int dx=0;dx<4;dx+=2){
                size_t idx=(((size_t)(sy0+dy))*W + (c*4+dx))*4;
                R+=gScr[idx]; G+=gScr[idx+1]; B+=gScr[idx+2]; cnt+=1;
            }
            R/=cnt; G/=cnt; B/=cnt;
            float L=(R*0.299f+G*0.587f+B*0.114f)/255.0f;
            float k=(L>thr)?1.7f*(L-thr)/(1.0f-thr):0.0f;
            size_t o=(((size_t)r*QW)+c)*4;
            gA[o]=R*k; gA[o+1]=G*k; gA[o+2]=B*k; gA[o+3]=255.f;
        }
    }
    // separable box blur x2 widening radii
    const int RAD[2]={ qh/96+3, 2*(qh/96+3)+2 };
    for (int p=0;p<2;p++){
        int rad=RAD[p];
        // horizontal
        for (int y=0;y<QH;y++){
            for (int x=0;x<QW;x++){
                float acc[4]={0,0,0,0}; float wsum=0;
                for (int d=-rad;d<=rad;d++){
                    int xx=x+d; if(xx<0||xx>=QW) continue;
                    float w=1.0f/(1.0f+(float)(d*d));   // gaussian-ish taper
                    size_t o=((((size_t)y*QW)+xx))*4;
                    for (int q=0;q<4;q++) acc[q]+=gA[o+q]*w;
                    wsum+=w;
                }
                size_t o=((((size_t)y*QW)+x))*4;
                for (int q=0;q<4;q++) gB[o+q]=acc[q]/wsum;
            }
        }
        // vertical
        for (int y=0;y<QH;y++){
            for (int x=0;x<QW;x++){
                float acc[4]={0,0,0,0}; float wsum=0;
                for (int d=-rad;d<=rad;d++){
                    int yy=y+d; if(yy<0||yy>=QH) continue;
                    float w=1.0f/(1.0f+(float)(d*d));
                    size_t o=((((size_t)yy*QW)+x))*4;
                    for (int q=0;q<4;q++) acc[q]+=gB[o+q]*w;
                    wsum+=w;
                }
                size_t o=((((size_t)y*QW)+x))*4;
                for (int q=0;q<4;q++) gA[o+q]=acc[q]/wsum;
            }
        }
    }
    for (size_t i=0;i<gUp.size();i++){ float v=gA[i]; gUp[i]=(unsigned char)(v>255.f?255.f:(v<0.f?0.f:v)); }
    rlUpdateTexture(RTbloom.texture.id, 0,0, qw, qh, RL_PIXELFORMAT_UNCOMPRESSED_R8G8B8A8, gUp.data());
}

// ---------------------------- 2D procedural base scene ----------------------------
static inline Color mkC(unsigned hex){ Color c{}; c.r=(unsigned char)((hex>>16)&0xff); c.g=(unsigned char)((hex>>8)&0xff); c.b=(unsigned char)(hex&0xff); c.a=255; return c; }
static unsigned hsh(unsigned a,unsigned b){ unsigned h=a*2654435761u+b*40503u+2246822519u; h^=h>>13; h*=1274126177u; h^=h>>16; return h; }

static void scene2D(int frame,int W,int H,int tier,int fps=30){
    using namespace dr;
    double t=(double)frame/fps;
    Ctx cx{ {0.f,0.f}, 1.f, 0.f, 1.f };
    // starfield
    for (int i=0;i<300;i++){
        unsigned h=hsh((unsigned)i,(unsigned)frame);
        float x=(float)(h%(unsigned)W), y=(float)((h>>7)%(unsigned)H);
        float tw=0.35f+0.65f*(float)(((h>>11)&255)/255.0)*(0.6f+0.4f*(float)std::sin(t*2.0+(i%17)));
        Color sc=ColorFromHSV((int)(h>>13)%360,0.35f,tw);
        DrawPixelV({x,y},sc);
        if ((h&7)==0) DrawPixelV({x+1.f,y},sc);
    }
    // horizon glow + sun disc
    Vector2 hs{ (float)W*0.5f,(float)H*0.66f };
    float R=(float)H*0.34f;
    for (int k=14;k>=1;k--){
        float fr=(float)k/14.f;
        Color g=withAlpha(ColorFromHSV(200+k*4,0.75f,0.85f), ((unsigned char)(26+16.0*(1.0-fr)))/255.f);
        DrawCircleV({ hs.x,hs.y+R*0.12f*fr }, R*(0.35f+0.65f*fr), g);
    }
    // perspective floor grid
    float gy=hs.y+R*0.40f;
    for (int i=-14;i<=14;i++)
        DrawLineEx({ (float)W*0.5f+(float)i*(float)W*0.042f,(float)H },
                   { (float)W*0.5f+(float)i*(float)W*0.011f,gy }, 1.f, withAlpha(mkC(0x2a5a8a), (140)/255.f));
    for (int j=0;j<8;j++){
        float yy=gy+((float)H-gy)*(float)j*(float)j/64.f;
        DrawLineEx({0.f,yy},{(float)W,yy},1.f,withAlpha(mkC(0x1d3f66), (95)/255.f));
    }
    // center emblem (bright -- bloom bait)
    Vector2 ctr{ (float)W*0.5f, hs.y-R*0.15f };
    float spin=(float)t*0.8f;
    float pulse=0.85f+0.15f*(float)std::sin(t*2.4);
    dr::circle(cx, ctr, 16.f*pulse, withAlpha(mkC(0xffffff), (235)/255.f));
    dr::ring(cx, ctr, 64.f*pulse, 4.f, NEONC);
    float a0=(float)std::fmod(spin*57.29578,360.0);
    dr::arc(cx, ctr, 86.f, 6.f, a0, a0+220.f, MAGC);
    for (int k=0;k<5;k++){
        double a=spin*0.7+(double)k*(6.28318530718/5.0);
        Vector2 p{ ctr.x+(float)std::cos(a)*118.f, ctr.y+(float)std::sin(a)*74.f };
        dr::circle(cx, p, 7.f+2.5f*(float)std::sin(t*3.0+k), ColorFromHSV((k*71+190)%360,0.8f,0.95f));
    }
    // sectioned showcase band
    int f=frame;
    if (f<58){
        float e=(float)easeFn(E_OUT_CUBIC,(double)f/57.0);
        for (int k=0;k<3;k++){
            float xx=(float)W*(0.14+0.26*k)-(float)W*0.36*(1.0-e);
            dr::roundRect(cx, xx,(float)H*0.20,(float)W*0.20,(float)H*0.10,18.f,
                          withAlpha(ColorFromHSV(k*90+190,0.65f,0.9f), (150)/255.f));
        }
    } else if (f<150){
        for (int i=0;i<40;i++){
            unsigned h=hsh((unsigned)i,(unsigned)(f/4));
            float bh=(float)H*0.08+(float)(h%160)+(float)std::sin(t*3.0+(float)i)*20.f;
            dr::rect(cx,(float)W*0.06+(float)i*(float)W*0.021,(float)H-26-bh,(float)W*0.015,bh,
                     ColorFromHSV((i*13+280)%360,0.75f,0.9f));
        }
    } else if (f<240){
        Vector2 pts[2];
        for (int i=0;i<22;i++){
            double a=spin*1.3+(double)i*(6.28318530718*2.0/22.0);
            pts[0]=ctr; pts[1]={ ctr.x+(float)std::cos(a)*(float)W*0.22f, ctr.y+(float)std::sin(a)*(float)W*0.22f };
            polyStroke(cx,pts,2,false,2.f,withAlpha(mkC(0xfff2b0), (190)/255.f));
        }
    } else {
        float e=(float)easeFn(E_IN_OUT_CUBIC,(double)(f-240)/59.0);
        for (int k=0;k<6;k++)
            dr::rect(cx,(float)W*0.10,(float)H*0.16+(float)H*0.11*(float)k*(1.0-e*0.94),(float)W*0.80*e,4.f,withAlpha(MAGC, (170)/255.f));
    }
}

// HUD: caption band, timer, progress bar
static void hudPass(int frame,int W,int H,int tier,int dur,int fps=30){
    Font fnt=GetFontDefault();
    const char* cap="";
    switch (tier){
        case 1: cap="T1 :: GRAIN + VIGNETTE + SCANLINES + SPARKLE (single-pass overlay shader)"; break;
        case 2: cap="T2 :: MULTI-PASS BLOOM :: threshold / quarter-res CPU blur / additive"; break;
        case 3: cap="T3 :: LIT 3D GEOMETRY :: procedural knot + geodesic sphere, depth-sorted"; break;
        default:cap="T4 :: MAX :: grade + fat bloom + geometry + full overlay stack"; break;
    }
    Rectangle panel{ 24.f, 26.f, (float)W*0.62f, 44.f };
    DrawRectangleRec(panel, withAlpha(mkC(0x060a14), (150)/255.f));
    DrawTextEx(fnt,cap,{ 36.f, 36.f },24.f,1.f, withAlpha(mkC(0xdde6f2), (235)/255.f));
    char tt[48]; std::snprintf(tt,sizeof tt,"T+%02d:%05.2f",(frame/(fps*60)),(float)(frame%(fps*60))/fps);
    Vector2 sz=MeasureTextEx(fnt,tt,20.f,1.f);
    DrawTextEx(fnt,tt,{ (float)W-24.f-sz.x, (float)H-46.f },20.f,1.f, withAlpha(AMBC, (235)/255.f));
    // progress bar bottom-left (track first, then fill)
    int len=(int)((float)(W-80)*(float)frame/(float)dur);
    DrawRectangle(40,H-30,W-80,10, withAlpha(mkC(0x10233a), (140)/255.f));
    DrawRectangle(40,H-30,len,10, NEONC);
}

// painter-lit 3D meshes (validated look; GPU rlgl path dropped: its uniform lookups never resolved)
static d3::Mesh gKnot, gBall;
static bool meshReady=false;
static void buildMeshes(){
    if (meshReady) return;
    gKnot = d3::torusKnot(1.0f, 200, 10, 0.26f);
    int idx=0; for (auto& fc : gKnot){ fc.col=ColorFromHSV((idx*7)%360,0.72f,0.92f); idx++; }
    gBall = d3::icosphere(1.0f);
    int j2=0; for (auto& fc : gBall){ fc.col=ColorFromHSV(200+(j2*3)%80,0.55f,0.9f); j2++; }
    meshReady=true;
}
static void paint3D(int frame,int W,int H){
    using namespace d3;
    buildMeshes();
    float spin=(float)((double)frame*0.021);
    Cam cam{ { std::sin((float)frame*0.008f)*1.1f, -1.6f, -13.5f }, { 0.f,-0.1f,3.5f }, 50.f };
    frameBegin();
    drawMesh(gKnot, trs({ -3.9f,0.3f,6.8f }, { spin*0.5f, spin, 0.06f }, 1.55f), cam, W, H, DrawOpts{});
    drawMesh(gBall, trs({ 3.3f,-0.15f,6.2f }, { 0.f, spin*0.7f, 0.f }, 1.15f), cam, W, H, DrawOpts{});
    frameEnd3D(W,H);
}

// ---------------------------- per-frame presentation ----------------------------
static void presentScene(Comp& c,int frame,RenderTexture output,int tier){
    const int W=c.w, H=c.h;
    ClearBackground(mkC(0x04040a));
    rlDisableDepthTest(); rlDisableDepthMask();       // 2D/painter path: no depth attachment interaction
    const int fps=c.fps>0?c.fps:30;
    double timeT=(double)frame/fps;
    if (tier<=1){
        scene2D(frame,W,H,tier,fps);
        hudPass(frame,W,H,tier,c.duration,fps);
        grainOverlay((float)timeT,W,H,0.35f,1);
        return;
    }
    // base layer -> RTscene (+ painter meshes inside the same target for tiers 3/4)
    BeginTextureMode(RTscene);
        ClearBackground(mkC(0x04040a));
        scene2D(frame,W,H,tier,fps);
        if (tier>=3) paint3D(frame,W,H);
    EndTextureMode();
    // EndTextureMode restores the window, not an enclosing render target.
    BeginTextureMode(output);
    cpuBloom(W,H,tier);
    // composite to window: opaque base, then additive bloom (stacked passes, tint-alpha dose)
    ClearBackground(mkC(0x020204));                            // fresh window target before additive layering
    Rectangle srcF{ 0.f,0.f,(float)W,-(float)H };
    Rectangle dstF{ 0.f,0.f,(float)W,(float)H };
    Vector2 org{ 0.f,0.f };
    BeginBlendMode(BLEND_ADDITIVE);
        DrawTexturePro(RTscene.texture,srcF,dstF,org,0.f,WHITE);   // additive over near-black == opaque here
    EndBlendMode();
    Rectangle srcH{ 0.f,0.f,(float)qw,(float)qh };
    BeginBlendMode(BLEND_ADDITIVE);
    int reps = (tier>=4)?2:1;
    Color tintA{ 255,255,255,(unsigned char)(200) };
    for (int q=0;q<reps;q++) DrawTexturePro(RTbloom.texture,srcH,dstF,org,0.f,tintA);
    EndBlendMode();
    grainOverlay((float)timeT,W,H, (tier==2)?0.35f:(tier==3)?0.5f:0.65f, tier);
    hudPass(frame,W,H,tier,c.duration,fps);
}

// GL objects die with the window; released through rmtn::shutdownGraphics().
static void releaseFrameResources(){
    if (fxReady){
        UnloadShader(SHX.grain);
        SHX=Shx{};
        fxReady=false;
    }
    for (RenderTexture* rt : { &RTscene, &RTbloom }){
        if (rt->id) UnloadRenderTexture(*rt);
        *rt=RenderTexture{};
    }
    fw=fh=0;
}

// Comp::render hook: `target` arrives bound and is left bound.
void renderFxTier(Comp& c,int frame,RenderTexture target,int tier){
    // Allocating render textures changes the framebuffer binding, so rebind afterwards.
    fxEnsure(c.w,c.h);
    BeginTextureMode(target);
    presentScene(c,frame,target,tier);
}

void registerFxSeries(Registry& out){
    static const char* fxl[] = {"fx-1-overlays","fx-2-bloom","fx-3-gpulit","fx-4-max"};
    for (int tier = 1; tier <= 4; tier++) {
        Comp c; c.name = fxl[tier-1]; c.w = 1920; c.h = 1032; c.fps = 30; c.duration = 300; c.music = true;
        c.render = [tier](Comp& comp,int frame,RenderTexture target){ renderFxTier(comp,frame,target,tier); };
        for (auto e : { SongEvent{0.4,3.0,96.0+tier*8,0.34,false}, SongEvent{3.4,3.2,147.0,0.28,false}, SongEvent{6.6,2.4,184.0,0.26,false} }) c.song.push_back(e);
        for (int i = 0; i < 30; i++) {
            SongEvent p{}; p.t0 = 1.0 + i*0.27; p.dur = 0.4; p.amp = 0.30; p.pluck = true;
            static const double pent[] = { 392.0, 448.4, 523.25, 587.3, 698.4 };
            p.freq = pent[i % 5] * (tier > 2 ? 1.18 : 1.0);
            c.song.push_back(p);
        }
        for (int i = 0; i < 8 + tier*2; i++) {
            SongEvent k{}; k.t0 = 0.9 + i*(1.0 - 0.05*tier); k.dur = 0.16; k.amp = 0.5; k.pluck = true; k.freq = 72.0;
            c.song.push_back(k);
        }
        out.push_back(std::move(c));
    }
}

} // namespace rmtn
