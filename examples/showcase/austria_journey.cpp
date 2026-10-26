// A continuous camera journey through Austria's history. All images and sounds are procedural.
#include "rmtn/raymotion.hpp"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <string>
#include <vector>

namespace rmtn {
namespace {
constexpr int W=1920,H=1080,FPS=30,LEN=2170;
const Color BG=mkColor(0x101522), INK=mkColor(0xf5ecdc), DIM=mkColor(0xadb6bd),
            RUBY=mkColor(0xe63948), AMBER=mkColor(0xe8b966), CYAN=mkColor(0x69c4d4);
const Vector2 P[11]={{1000,1100},{3450,950},{5900,1260},{8350,920},{10800,1250},
                     {13250,920},{15700,1220},{18150,920},{20600,1150},{23050,1030},{25500,1190}};
const int ARR[11]={0,165,315,475,645,805,1020,1210,1395,1660,1910};
float reveal(int t,int at,int len=23) {
    return (float)interpolate(t-at,{0,(double)len},{0,1},clamped(ease::out(ease::cubic)));
}
void text(const Ctx& c,const std::string& s,Vector2 pos,int size,Color col,float alpha=1,bool bold=true) {
    Ctx k=c;k.a*=alpha;
    dr::text(fonts::ui(bold),k,s.c_str(),pos,(float)size,0,col);
}
void centered(const Ctx& c,const std::string& s,Vector2 pos,int size,Color col,float alpha=1) {
    Ctx k=c;k.a*=alpha;
    dr::text(fonts::ui(true),k,s.c_str(),pos,(float)size,0,col,0.5,0.5);
}
void sound(Comp& c,int at,const char* name,float volume) {
    TNode* n=addRoot(c.roots,at,50);
    media::AudioOptions o;o.volume=volume;
    media::audio(n,std::string("assets/audio/austria/")+name+".wav",o);
}
cam::Track cameraTrack() {
    cam::Track tr;
    tr.start({P[0],0.92f,0}).hold(118)
      .fly(ARR[1],{P[1],0.94f,-3}).hold(278)
      .fly(ARR[2],{P[2],0.92f,3}).hold(436)
      .fly(ARR[3],{P[3],0.95f,-2}).hold(605)
      .fly(ARR[4],{P[4],0.94f,2}).hold(765)
      .fly(ARR[5],{P[5],0.90f,-3}).hold(970)
      .fly(ARR[6],{P[6],0.95f,2}).hold(1170)
      .fly(ARR[7],{P[7],0.94f,-2}).hold(1350)
      .fly(ARR[8],{P[8],0.94f,2}).hold(1590)
      .fly(ARR[9],{P[9],0.91f,-2}).hold(1835)
      .fly(ARR[10],{P[10],0.91f,2}).hold(2070)
      .to(2160,{P[10],0.78f,0}).hold(LEN);
    return tr;
}
cam::Shot movingShot(const cam::Track& tr,int t) {
    cam::Shot s=tr.at(t);
    s.center.x+=12*std::sin(t*0.018f);
    s.center.y+=9*std::cos(t*0.016f);
    if(t>=ARR[5]&&t<ARR[5]+38) {
        const float decay=std::exp(-(t-ARR[5])/12.f);
        s=cam::shake(s,t,8*decay,2.6f,1938);
    }
    return s;
}

// One station is a place on a single large canvas, never a full-screen scene.
TNode* station(TNode* world,int i,const char* label,const char* year,const char* title,
               const char* a,const char* b,Color accent) {
    TNode* n=world->add(0,LEN);
    n->props.pos={P[i].x-960,P[i].y-540};
    n->fn=[=](const Ctx& c,int t) {
        if(i!=10 && std::abs(t-ARR[i])>240)return;
        const float u=reveal(t,ARR[i]-22);
        Ctx k=c;k.a*=u;
        dr::circle(k,{1440,505},390,withAlpha(accent,0.045f));
        dr::ring(k,{1440,505},390,2,withAlpha(accent,0.18f));
        dr::rect(k,90,105,50,7,accent);
        text(k,label,{160,89},27,accent);
        text(k,year,{92,195},116,withAlpha(accent,0.58f));
        text(k,title,{92,383},64,INK);
        text(k,a,{95,488},31,DIM,1,false);
        text(k,b,{95,539},31,DIM,1,false);
        text(k,"AUSTRIA / THROUGH TIME",{95,860},22,withAlpha(DIM,0.65f),1,false);
        dr::circle(k,{960,825},17,accent);
    };
    return n;
}

// A physical bronze crown, created entirely from PBR primitives.
g3d::ScenePtr imperialCrown() {
    auto s=std::make_shared<g3d::Scene>();
    s->camera.position={0,2.1f,7.6f}; s->camera.target={0,0.6f,0}; s->camera.fov=34;
    s->camera.dof={7.5f,0.025f,5,0};
    s->environmentGenerator=g3d::studioEnvironment(mkColor(0x415779),mkColor(0xe2c995),mkColor(0x362c40),4);
    s->environmentIntensity=0.8f;
    s->lights.push_back(g3d::Light::directional({4,7,5},WHITE,1.8f,true));
    s->lights.push_back(g3d::Light::point({-4,2,2},mkColor(0x84bcd5),0.7f,12));
    g3d::Material gold;gold.color=mkColor(0xd9a849);gold.metalness=0.92f;gold.roughness=0.22f;
    g3d::Material ruby=gold;ruby.color=mkColor(0xa41135);ruby.metalness=0.28f;ruby.roughness=0.12f;
    g3d::Material velvet;velvet.color=mkColor(0x4f1530);velvet.roughness=0.9f;
    g3d::Object3D* group=s->add();
    group->up=[](int t,g3d::Object3D& o){o.rotation.y=-22+t*0.45f;o.position.y=0.08f*std::sin(t*0.035f);};
    group->add(g3d::Geometry::cylinder(1.8f,0.36f,64),gold)->position={0,-0.75f,0};
    group->add(g3d::Geometry::sphere(1.35f,32,48),velvet)->position={0,0.05f,0};
    group->add(g3d::Geometry::torus(1.67f,0.14f,64,24),gold)->position={0,-0.51f,0};
    for(int j=0;j<8;++j) {
        const float a=(float)(j*TAU/8);
        g3d::Object3D* tip=group->add(g3d::Geometry::cone(0.35f,1.18f,20),gold);
        tip->position={1.17f*std::cos(a),0.73f,1.17f*std::sin(a)};
        group->add(g3d::Geometry::sphere(0.18f,16,20),ruby)->position={1.17f*std::cos(a),1.36f,1.17f*std::sin(a)};
    }
    group->add(g3d::Geometry::sphere(0.3f,24,32),gold)->position={0,1.62f,0};
    return s;
}

// Inked cel shading for the emerging republic: a 3D parliament facade.
g3d::ScenePtr parliamentModel() {
    auto s=std::make_shared<g3d::Scene>();
    s->camera.position={5.7f,3.8f,9.2f};s->camera.target={0,1.2f,0};s->camera.fov=36;
    s->camera.up=[](int t,g3d::Camera& c){c.position.x=5.7f-2.2f*std::sin(t*0.014f);};
    s->lights.push_back(g3d::Light::directional({-3,7,5},WHITE,1.6f,true));
    s->ambient=0.5f;s->skyColor=mkColor(0xd5e0e6);s->groundColor=mkColor(0x574d4b);
    s->lines.enabled=true;s->lines.color=mkColor(0x2c2732);s->lines.width=3;
    s->lines.wobble=0.9f;s->lines.boilEvery=3;
    g3d::Material stone;stone.color=mkColor(0xe4d8bf);stone.shading=g3d::Shading::Toon;
    stone.toon.bands=3;stone.toon.rim=Color{255,235,190,75};
    g3d::Material roof=stone;roof.color=mkColor(0xcbbfa9);
    s->add(g3d::Geometry::box(8.6f,0.38f,2.7f),stone)->position={0,0.15f,0};
    s->add(g3d::Geometry::box(8.1f,0.4f,2.4f),stone)->position={0,3.2f,0};
    s->add(g3d::Geometry::box(8.5f,0.3f,2.8f),roof)->position={0,3.55f,0};
    for(int i=0;i<8;++i)
        s->add(g3d::Geometry::cylinder(0.19f,2.9f,16),stone)->position={-3.35f+i*0.96f,1.7f,0.86f};
    s->add(g3d::Geometry::box(8.7f,0.18f,2.9f),roof)->position={0,3.84f,0};
    return s;
}

// A true textured globe; the highlighted country and Vienna are drawn from geo data.
g3d::ScenePtr europeGlobe() {
    auto s=std::make_shared<g3d::Scene>();
    globe::Map map(1280,mkColor(0x182d43));
    map.fill(geo::land().merged(),mkColor(0x91a982));
    map.stroke(geo::countries().merged(),mkColor(0xd1dacd,140),1.3f);
    map.fill(geo::countries().get("AUT").geometry,RUBY);
    map.disc({16.3738,48.2082},95,AMBER);
    map.graticule(30,mkColor(0xffffff,40),1.1f);
    g3d::Material earth;earth.dynamicMap=map.texture();earth.roughness=0.8f;
    s->add(globe::sphere(),earth);
    s->camera.fov=38;
    globe::look(s->camera,{14.5,48},3.65f);
    s->camera.up=[](int t,g3d::Camera& c){globe::look(c,{14.5f-13.f*std::sin(t*0.011f),48.f+3.f*std::sin(t*0.015f)},3.65f);};
    s->lights.push_back(g3d::Light::directional({4,4,7},WHITE,2.3f,true));
    s->ambient=0.42f;s->skyColor=mkColor(0x131d30);s->groundColor=mkColor(0x101522);
    return s;
}

// A symbolic rebuilt town: every building rises from code-generated 3D geometry.
g3d::ScenePtr rebuiltTown() {
    auto s=std::make_shared<g3d::Scene>();
    s->camera.position={8,7,10};s->camera.target={0,0.5f,0};s->camera.fov=41;
    s->camera.up=[](int t,g3d::Camera& c){c.position.x=8-2.f*std::sin(t*0.012f);};
    s->skyColor=mkColor(0xb8c8cf);s->groundColor=mkColor(0x675d53);s->ambient=0.55f;
    s->lights.push_back(g3d::Light::directional({-5,9,7},mkColor(0xffe6c4),1.65f,true));
    s->fogColor=mkColor(0xb8c8cf);s->fogNear=10;s->fogFar=23;
    g3d::Material ground;ground.color=mkColor(0x777c78);ground.roughness=1;
    s->add(g3d::Geometry::box(14,0.2f,14),ground)->position={0,-0.2f,0};
    g3d::Material road=ground;road.color=mkColor(0x515b60);
    s->add(g3d::Geometry::box(14,0.03f,1.2f),road)->position={0,-0.08f,0};
    s->add(g3d::Geometry::box(1.2f,0.03f,14),road)->position={0,-0.07f,0};
    for(int z=-2;z<=2;++z)for(int x=-2;x<=2;++x) {
        if(x==0||z==0)continue;
        const float h=1.2f+2.4f*(float)rnd01(1955,x*11+z+40);
        g3d::Material wall;wall.color=(x+z)%2?mkColor(0xc9ac91):mkColor(0xdfccb0);wall.roughness=0.9f;
        auto* b=s->add(g3d::Geometry::box(1.65f,h,1.65f),wall);
        b->position={x*2.35f,h/2,z*2.35f};
        b->up=[h,x,z](int t,g3d::Object3D& o){
            const float u=reveal(t,ARR[6]-18+(std::abs(x)+std::abs(z))*5,45);
            o.position.y=-h/2+h*u;
        };
    }
    return s;
}

float alpineHeight(float x,float z) {
    const float ridge=4.8f*std::exp(-std::pow((z+2.f-2.f*std::sin(x*0.34f))/3.7f,2));
    const float peaks=3.5f*std::pow(std::fabs(std::sin(x*0.46f+z*0.13f)),5.f);
    return 0.8f+ridge+peaks+0.32f*std::sin(x*2.f)*std::cos(z*1.7f);
}
g3d::ScenePtr alpineScene() {
    auto s=std::make_shared<g3d::Scene>();
    s->camera.position={12,11,23};s->camera.target={0,3.f,0};s->camera.fov=44;
    s->camera.up=[](int t,g3d::Camera& c){
        c.position.x=12-7.f*reveal(t,ARR[9]-55,145);
        c.position.z=23-2.f*reveal(t,ARR[9]-55,145);
    };
    s->skyColor=mkColor(0x829eb5);s->groundColor=mkColor(0x403f46);s->ambient=0.5f;
    s->lights.push_back(g3d::Light::directional({-6,8,4},mkColor(0xffddb0),1.8f,true));
    s->fogColor=mkColor(0x94a9b8);s->fogNear=11;s->fogFar=34;
    auto terrain=std::make_shared<g3d::Geometry>([] {
        constexpr int n=110;const int nv=(n+1)*(n+1);
        Mesh m{};m.vertexCount=nv;m.triangleCount=2*n*n;
        m.vertices=(float*)MemAlloc(nv*3*sizeof(float));
        m.normals=(float*)MemAlloc(nv*3*sizeof(float));
        m.texcoords=(float*)MemAlloc(nv*2*sizeof(float));
        m.colors=(unsigned char*)MemAlloc(nv*4);
        m.indices=(unsigned short*)MemAlloc(m.triangleCount*3*sizeof(unsigned short));
        for(int j=0;j<=n;++j)for(int i=0;i<=n;++i) {
            const int k=j*(n+1)+i;const float x=24.f*i/n-12.f,z=24.f*j/n-12.f;
            const float h=alpineHeight(x,z),dx=alpineHeight(x-0.1f,z)-alpineHeight(x+0.1f,z);
            const float dz=alpineHeight(x,z-0.1f)-alpineHeight(x,z+0.1f);
            const float l=std::sqrt(dx*dx+dz*dz+0.04f);
            m.vertices[3*k]=x;m.vertices[3*k+1]=h;m.vertices[3*k+2]=z;
            m.normals[3*k]=dx/l;m.normals[3*k+1]=0.2f/l;m.normals[3*k+2]=dz/l;
            m.texcoords[2*k]=i/(float)n;m.texcoords[2*k+1]=j/(float)n;
            const Color col=h>5.3f?mkColor(0xe7ecea):(h>3.9f?mkColor(0x8f9796):mkColor(0x58796d));
            m.colors[4*k]=col.r;m.colors[4*k+1]=col.g;m.colors[4*k+2]=col.b;m.colors[4*k+3]=255;
        }
        int p=0;for(int j=0;j<n;++j)for(int i=0;i<n;++i){
            const unsigned short a=(unsigned short)(j*(n+1)+i),b=(unsigned short)(a+1);
            const unsigned short c=(unsigned short)(a+n+1),d=(unsigned short)(c+1);
            m.indices[p++]=a;m.indices[p++]=c;m.indices[p++]=b;
            m.indices[p++]=b;m.indices[p++]=c;m.indices[p++]=d;
        }
        UploadMesh(&m,false);return m;
    });
    g3d::Material rock;rock.color=WHITE;rock.roughness=0.95f;
    s->add(terrain,rock);
    return s;
}

} // namespace

void registerAustriaJourney(Registry& out) {
    Comp c;c.name="austria-through-time";c.w=W;c.h=H;c.fps=FPS;c.duration=LEN;c.background=BG;c.music=true;
    // Registration runs from any directory (the CLI tests run elsewhere): no captions without the file.
    const sub::Track narration=std::filesystem::exists("assets/vo/austria/narration.srt")
        ?sub::load("assets/vo/austria/narration.srt"):sub::Track{};
    const cam::Track tr=cameraTrack();
    const vg::Path route=vg::Path::smooth({{P[0].x,P[0].y+340},{P[1].x,P[1].y+340},{P[2].x,P[2].y+340},
        {P[3].x,P[3].y+340},{P[4].x,P[4].y+340},{P[5].x,P[5].y+340},{P[6].x,P[6].y+340},
        {P[7].x,P[7].y+340},{P[8].x,P[8].y+340},{P[9].x,P[9].y+340},{P[10].x,P[10].y+340}});
    // The distant moving field gives every camera flight depth.
    TNode* far=cam::world(c.roots,0,LEN,[tr](int t){return movingShot(tr,t);},{W,H},0.28f);
    far->fn=[](const Ctx& c,int t){
        for(int i=0;i<420;++i) {
            const float x=100+19000*(float)rnd01(1204,2*i),y=100+2200*(float)rnd01(1204,2*i+1);
            dr::circle(c,{x,y},1.5f+(i%3),withAlpha(AMBER,0.13f+0.08f*std::sin(t*0.03f+i)));
        }
    };
    TNode* world=cam::world(c.roots,0,LEN,[tr](int t){return movingShot(tr,t);},{W,H});
    world->fn=[route](const Ctx& c,int t) {
        vg::Stroke under(mkColor(0x344050),7);vg::stroke(c,route,under);
        vg::Stroke stroke(AMBER,8);stroke.dash={34,22};stroke.dashOffset=-t*3.f;
        vg::stroke(c,route.trim(0,std::min(1.f,t/1960.f)),stroke);
        Vector2 dot=route.pointAt(std::min(1.f,t/1960.f));
        dr::circle(c,dot,26,withAlpha(AMBER,0.18f));dr::circle(c,dot,9,AMBER);
        for(int i=0;i<11;++i) {
            dr::ring(c,{P[i].x,P[i].y+340},37,4,withAlpha(i==5?RUBY:CYAN,0.7f));
            dr::circle(c,{P[i].x,P[i].y+340},8,i==5?RUBY:CYAN);
        }
    };

    // Opening portal; a kinetic title reveals as the camera commits to the route.
    TNode* intro=station(world,0,"A NATIONAL DAY JOURNEY","26 OCT","AUSTRIA THROUGH TIME",
        "A thousand years of change — and the roots before it.","Follow the line from Noricum to the present.",RUBY);
    auto globeDraw=g3d::draw(europeGlobe(),{1050,185,790,670});
    intro->add(0,LEN)->fn=[globeDraw](const Ctx& c,int t){if(t<205)globeDraw(c,t);};
    intro->add(0,LEN)->fn=[](const Ctx& c,int t){
        if(t>205)return;Ctx k=c;k.a*=reveal(t,0);
        dr::ring(k,{1450,530},320,3,withAlpha(AMBER,0.28f));
        text(k,"48°12′ N  16°22′ E",{1240,855},23,AMBER);
    };

    // Roman roots: an arch assembles and an animated road heads toward the next century.
    TNode* roman=station(world,1,"01 / EARLY ROOTS","15 BC","NORICUM & ROME",
        "Noricum is incorporated into the Roman Empire.","Roman Vindobona will become Vienna.",AMBER);
    roman->add(0,LEN)->fn=[](const Ctx& c,int t){
        if(std::abs(t-ARR[1])>215)return;Ctx k=c;k.a*=reveal(t,ARR[1]-24);
        vg::Path arch=vg::Path::parse("M1170 760 L1170 360 Q1170 235 1450 235 Q1730 235 1730 360 L1730 760 M1260 760 L1260 380 Q1260 315 1450 315 Q1640 315 1640 380 L1640 760");
        vg::stroke(k,arch.trim(0,reveal(t,ARR[1]-18,55)),vg::Stroke(AMBER,19));
        for(int i=0;i<6;++i) dr::strokeSeg(k,{1120.f+i*120,770},{1030.f+i*150,920},4,withAlpha(AMBER,0.38f));
        dr::circle(k,{1450,400},22,RUBY);
    };

    // A deed appears; the lettering writes itself as the camera flies over parchment.
    TNode* name=station(world,2,"02 / FIRST RECORD","996","“OSTARRICHI”",
        "The name appears in a deed issued by Otto III.","It is the first recorded form of Austria's name.",AMBER);
    name->add(0,LEN)->fn=[](const Ctx& c,int t){
        if(std::abs(t-ARR[2])>215)return;Ctx k=c;k.a*=reveal(t,ARR[2]-25);
        dr::roundRect(k,1080,245,650,520,24,mkColor(0xe9ddc4));
        for(int i=0;i<6;++i)dr::rect(k,1130,425+i*42,475-i%3*45,2,mkColor(0xb7a890));
        const std::string word="Ostarrichi";
        int n=std::clamp((t-(ARR[2]-17))/3,0,(int)word.size());
        text(k,word.substr(0,n),{1140,320},58,mkColor(0x4b3029));
        vg::Path quill=vg::Path::parse("M1500 685 C1560 590 1650 415 1690 315 Q1590 370 1530 480 Q1470 585 1500 685 Z");
        vg::write(k,quill,vg::Style::filled(mkColor(0x614b3d)),reveal(t,ARR[2]-19,50));
        dr::circle(k,{1640,700},38,RUBY);
    };

    // The PBR crown revolves in a studio-lit 3D viewport as dynastic dates branch out.
    TNode* empire=station(world,3,"03 / DYNASTY & EMPIRE","1282–1867","HABSBURG CENTURIES",
        "1282: Habsburg rule. 1804: the Austrian Empire.","1867: the Austro-Hungarian Dual Monarchy.",AMBER);
    auto crownDraw=g3d::draw(imperialCrown(),{1080,160,740,670});
    empire->add(0,LEN)->fn=[crownDraw](const Ctx& c,int t){if(std::abs(t-ARR[3])<225)crownDraw(c,t);};
    dg::Theme historyTheme=dg::Theme::dark();historyTheme.fontSize=22;
    historyTheme.fill=mkColor(0x283847);historyTheme.stroke=AMBER;historyTheme.text=INK;
    historyTheme.edge=AMBER;historyTheme.highlight=RUBY;
    auto history=dg::Diagram::make(dg::mermaid(
        "graph LR\n A[1282 Habsburg rule] --> B[1804 Austrian Empire]\n"
        " B --> C[1867 Dual Monarchy]"),historyTheme);
    history->build(ARR[3]-18,8,20).trace(ARR[3]+50,60,{"A","B","C"});
    empire->add(0,LEN)->fn=dg::draw(history,{105,650,860,190});

    // NPR 3D parliament is drawn in inked cel shading, not a flat silhouette.
    TNode* republic=station(world,4,"04 / NEW REPUBLIC","1918","THE MONARCHY ENDS",
        "After World War I, the old state collapses.","The republic is proclaimed on 12 November.",RUBY);
    auto parliamentDraw=g3d::draw(parliamentModel(),{1050,170,790,660});
    republic->add(0,LEN)->fn=[parliamentDraw](const Ctx& c,int t){if(std::abs(t-ARR[4])<220)parliamentDraw(c,t);};
    republic->add(0,LEN)->fn=[](const Ctx& c,int t){
        if(std::abs(t-ARR[4])>220)return;Ctx k=c;k.a*=reveal(t,ARR[4]-16);
        dr::rect(k,1310,785,280,43,RUBY);dr::rect(k,1310,799,280,15,INK);
        text(k,"12 NOVEMBER 1918",{1240,846},25,DIM);
    };

    // The route fractures here. Its palette and music darken; there is no celebratory motion.
    TNode* dark=station(world,5,"05 / DEMOCRACY BROKEN","1933–45","DICTATORSHIP & WAR",
        "Democracy ends in 1933/34; annexation follows in 1938.",
        "The Holocaust and war follow. Austrians take part.",mkColor(0xa86870));
    dark->add(0,LEN)->fn=[](const Ctx& c,int t){
        if(std::abs(t-ARR[5])>235)return;Ctx k=c;k.a*=reveal(t,ARR[5]-28);
        for(int i=0;i<7;++i)dr::rect(k,1100+i*95,280,45,495,mkColor(0x414149));
        vg::Path split=vg::Path::polyline({{1370,295},{1440,390},{1395,495},{1480,585},{1420,745}});
        vg::stroke(k,split.trim(0,reveal(t,ARR[5]-8,55)),vg::Stroke(mkColor(0xa96b71),7));
        dr::circle(k,{1440,735},32,AMBER);
        text(k,"REMEMBRANCE · RESPONSIBILITY",{1140,840},23,DIM);
    };

    // Building blocks reassemble as a stamped treaty settles into place.
    TNode* rebuild=station(world,6,"06 / RESTORATION","1945–55","SOVEREIGN AGAIN",
        "The republic returns on 27 April 1945.","The State Treaty is signed on 15 May 1955.",CYAN);
    auto townDraw=g3d::draw(rebuiltTown(),{1040,245,735,510});
    rebuild->add(0,LEN)->fn=[townDraw](const Ctx& c,int t){if(std::abs(t-ARR[6])<220)townDraw(c,t);};
    rebuild->add(0,LEN)->fn=[](const Ctx& c,int t){
        if(std::abs(t-ARR[6])>220)return;Ctx k=c;k.a*=reveal(t,ARR[6]-25);
        dr::roundRect(k,1300,735,350,95,14,mkColor(0xeee1c8));
        dr::circle(k,{1340,782},27,RUBY);
        text(k,"STATE TREATY / 1955",{1375,762},25,mkColor(0x3f3540));
        text(k,"SYMBOLIC RECONSTRUCTION",{1115,845},20,DIM);
    };

    // The flag emerges inside a kinetic orbit; this is why the national day is 26 October.
    TNode* national=station(world,7,"07 / NATIONAL DAY","26 OCT 1955","NEUTRALITY ADOPTED",
        "Parliament adopts permanent neutrality on 26 October.",
        "The date becomes Austria's National Day in 1965.",RUBY);
    national->add(0,LEN)->fn=[](const Ctx& c,int t){
        if(std::abs(t-ARR[7])>220)return;Ctx k=c;k.a*=reveal(t,ARR[7]-25);
        for(int i=0;i<28;++i) {
            float a=(float)(i*TAU/28+t*0.006);
            dr::circle(k,{1440+305*std::cos(a),505+305*std::sin(a)},i%4?3:6,AMBER);
        }
        dr::ring(k,{1440,505},288,3,withAlpha(AMBER,0.5f));
        flags::draw(k,"at",{1440,505},310,true,reveal(t,ARR[7]-17,42));
        const float daySwitch=reveal(t,ARR[7]+55,25);
        Ctx first=k;first.a*=1-daySwitch;
        Ctx second=k;second.a*=daySwitch;
        cal::page(first,{1050,655,185,200},cal::days({1955,10,26}));
        cal::page(second,{1050,655,185,200},cal::days({1965,10,26}));
    };
    fx::Emitter confetti=fx::confetti(560,{RUBY,INK,AMBER});confetti.seed=1955;
    confetti.rate=0.8f;confetti.from=ARR[7]+15;confetti.until=ARR[7]+65;
    TNode* paper=national->add(0,LEN);paper->props.pos={1440,270};paper->fn=fx::fn(confetti);

    // A real projected outline, EU orbit, then an animated chart of population at EU entry vs now.
    TNode* modern=station(world,8,"08 / EUROPE & TODAY","1995–2026","AUSTRIA TODAY",
        "Austria joins the European Union in 1995.","Population: 7.95m then → 9.22m in January 2026.",CYAN);
    const geo::Geometry land=geo::countries().get("AUT").geometry;
    const geo::Projection projection=geo::Projection::mercator().fit({1110,275,630,480},land,30);
    const vg::Path outline=projection.path(land);
    modern->add(0,LEN)->fn=[outline,projection](const Ctx& c,int t){
        if(std::abs(t-ARR[8])>230||t>ARR[8]+127)return;
        Ctx k=c;k.a*=reveal(t,ARR[8]-25)*std::min(1.f,(ARR[8]+127-t)/18.f);
        vg::fill(k,outline,mkColor(0x386c79),vg::FillRule::EvenOdd);
        vg::stroke(k,outline.trim(0,reveal(t,ARR[8]-17,60)),vg::Stroke(CYAN,6));
        for(int i=0;i<12;++i) {
            const float a=(float)(i*TAU/12-PI/2+t*0.002);
            dr::star(k,{1430+365*std::cos(a),500+310*std::sin(a)},9,4,5,-90,AMBER);
        }
        Vector2 vienna=projection({16.3738,48.2082});
        dr::circle(k,vienna,11,RUBY);dr::ring(k,vienna,22,3,RUBY);
        text(k,"VIENNA",{vienna.x+30,vienna.y-14},23,INK);
    };
    const plot::Table people={{"1995","2026"},{{"Residents (millions)",{7.948278,9.215956},CYAN}}};
    plot::ChartStyle chart;chart.box={1110,260,660,490};chart.y={0,10,2};chart.values=true;
    chart.decimals=2;chart.legend=false;chart.textSize=26;chart.textColor=INK;
    chart.axes.color=DIM;chart.axes.numberColor=DIM;chart.axes.gridColor=withAlpha(DIM,0.15f);
    modern->add(0,LEN)->fn=[people,chart](const Ctx& c,int t){
        if(t<ARR[8]+96)return;Ctx k=c;k.a*=reveal(t,ARR[8]+112,20);
        plot::bars(k,people,chart,reveal(t,ARR[8]+120,30));
        text(k,"POPULATION (MILLIONS)",{1170,792},23,DIM);
    };
    const math::Formula growth=math::Formula::tex(
        R"(9.22-7.95\approx 1.27\,\mathrm{million})",{52,INK});
    modern->add(0,LEN)->fn=[growth](const Ctx& c,int t){
        if(t<ARR[8]+100)return;Ctx k=c;k.a*=reveal(t,ARR[8]+105,20);
        text(k,"POPULATION CHANGE / 1995–2026",{125,640},23,CYAN);
        math::write(k,growth,{500,720},{0.5f,0.5f},reveal(t,ARR[8]+105,60));
    };

    // A procedural alpine flyover connects the present-day figures to physical place.
    TNode* alpine=station(world,9,"09 / LANDSCAPE","THE ALPS","AUSTRIA'S HIGH COUNTRY",
        "Tyrol alone has 573 summits above 3,000 metres.",
        "An alpine landscape generated as 3D geometry.",AMBER);
    auto alpineDraw=g3d::draw(alpineScene(),{1050,180,790,660});
    alpine->add(0,LEN)->fn=[alpineDraw](const Ctx& c,int t){if(std::abs(t-ARR[9])<230)alpineDraw(c,t);};
    alpine->add(0,LEN)->fn=[](const Ctx& c,int t){
        if(std::abs(t-ARR[9])>230)return;Ctx k=c;k.a*=reveal(t,ARR[9]-24);
        text(k,"PROCEDURAL TERRAIN  /  LIGHT  /  FOG",{1110,855},23,INK);
    };

    // The camera leaves the historical timeline at the code that created it.
    TNode* made=station(world,10,"10 / THE TOOL BEHIND THE JOURNEY","C++ → MP4",
        "MADE WITH RAYMOTION","One timeline brings history, data and geography to life.",
        "The visuals and sound are generated from code.",CYAN);
    const code::Block source=code::Block::parse(
        "Comp film;\n"
        "film.name = \"austria-through-time\";\n"
        "auto globe = europeGlobe();\n"
        "auto* shot = addRoot(film.roots, 0, 2170);\n"
        "shot->fn = g3d::draw(globe);", "cpp");
    code::Style codeStyle;codeStyle.title="austria_journey.cpp";codeStyle.size=25;
    codeStyle.lineNumbers=false;codeStyle.padding={27,26};
    made->add(0,LEN)->fn=[source,codeStyle](const Ctx& c,int t){
        if(t<ARR[10]-35)return;Ctx k=c;k.a*=reveal(t,ARR[10]-28);
        code::Look look;look.reveal=std::max(0,(t-ARR[10]+22)*2);look.cursor=t<ARR[10]+85;look.t=t;
        code::draw(k,source,{1045,265},codeStyle,look);
        text(k,"raymotion render austria-through-time",{1080,620},22,DIM,
             reveal(t,ARR[10]+50,22),false);
        text(k,"3D   ·   MAPS   ·   CHARTS   ·   PARTICLES   ·   AUDIO",{1060,780},23,AMBER,
             reveal(t,ARR[10]+35,25));
    };
    auto* logo=prefab::use(made,ARR[10]+35,LEN-ARR[10]-35,"raymotion-logo",
        Json::parse(R"({"size":180,"wordmark":true,"tagline":"video as code","spin":1.1})"));
    logo->props.pos={1430,870};

    // Travel is a feature demonstration of the camera and true temporal motion blur.
    // Every portal lives only during the flight between two stations.
    const int depart[10]={118,278,436,605,765,970,1170,1350,1590,1835};
    const char* jumpTo[10]={"15 BC","996","1282","1918","1933–45","1945","26 OCT 1955",
                            "1995","THE ALPS","C++ → MP4"};
    for(int i=0;i<10;++i) {
        const int span=ARR[i+1]-depart[i];
        const std::string destination=jumpTo[i];
        TNode* portal=addRoot(c.roots,depart[i],span);
        portal->isolate().motionBlur={8,0.55f};
        portal->fn=[destination,span,i](const Ctx& c,int t){
            const float f=(float)frameTime(t),u=std::clamp(f/span,0.f,1.f);
            const float fade=std::clamp(f/16.f,0.f,1.f)*std::clamp((span-f)/23.f,0.f,1.f);
            if(fade<=0)return;Ctx k=c;k.a*=fade;
            for(int n=0;n<28;++n) {
                const float y=155.f+n*29.f+55.f*std::sin(n*1.7f);
                const float x=std::fmod(n*211.f+f*(14.f+n%5),2220.f)-150.f;
                dr::strokeSeg(k,{x,y},{x+120.f+n%4*26.f,y-13.f},2.5f,
                              withAlpha(i==4?RUBY:CYAN,0.16f+0.2f*(n%3==0)));
            }
            for(int n=0;n<5;++n) {
                const float radius=130.f+n*67.f+f*2.2f;
                dr::ring(k,{960,485},radius,3,withAlpha(i==4?RUBY:AMBER,0.42f*(1-u)));
            }
            dr::roundRect(k,550,352,820,265,22,withAlpha(BG,0.88f));
            centered(k,"FLY THROUGH TIME",{960,395},28,CYAN);
            centered(k,destination,{960,500},89,INK);
            dr::strokeSeg(k,{665,575},{665+590*u,575},5,AMBER);
        };
    }

    // Fixed, sparse HUD: the camera and route do the work, while the place in time stays legible.
    TNode* hud=addRoot(c.roots,0,LEN);
    hud->fn=[](const Ctx& c,int t){
        dr::rect(c,0,0,W,13,RUBY);
        dr::rect(c,0,0,W*std::min(1.f,t/(float)LEN),13,AMBER);
        dr::text(fonts::ui(true),c,"AUSTRIA / THROUGH TIME",{70,38},27,1,INK);
        dr::text(fonts::ui(false),c,"26 OCTOBER · NATIONAL DAY",{1440,43},24,0,DIM);
    };

    // The original synth score lowers to a single bass line for the dictatorship chapter.
    const double darkA=ARR[5]/(double)FPS-0.6,darkB=ARR[6]/(double)FPS+0.4;
    const double bass[]={110,87.31,130.81,98};
    const double notes[][4]={{220,261.63,329.63,392},{174.61,220,261.63,349.23},
                             {261.63,329.63,392,523.25},{196,246.94,293.66,392}};
    for(int bar=0;bar*2<LEN/(double)FPS;++bar) {
        double at=bar*2.;int ch=bar%4;
        c.song.push_back({at,2.1,bass[ch],narration.at(at+0.4)?0.045:0.10,false});
        for(int j=0;j<8;++j) {
            double beat=at+j*0.25;if(beat>darkA&&beat<darkB)continue;
            c.song.push_back({beat,0.37,notes[ch][(j*3+j/4)%4],narration.at(beat)?0.022:0.052,true});
        }
    }
    for(int i : {1,2,3,4,6,7,8,9,10})sound(c,ARR[i]-17,"whoosh",0.23f);
    sound(c,ARR[2]+15,"stamp",0.30f);
    sound(c,ARR[5],"lowhit",0.42f);
    sound(c,ARR[7]+27,"chime",0.26f);
    sound(c,2070,"chime",0.23f);
    const int speechAt[11]={8,173,323,483,653,813,1028,1218,1403,1668,1918};
    media::AudioOptions voice;voice.volume=0.84f;
    for(int i=0;i<11;++i) {
        const std::string number=(i<10?"0":"")+std::to_string(i);
        TNode* line=addRoot(c.roots,speechAt[i],LEN-speechAt[i]);
        media::audio(line,"assets/vo/austria/line-"+number+".wav",voice);
    }
    sub::Style caption=sub::defaultStyle();caption.text.size=31;caption.bottom=31;
    caption.maxWidth=1600;caption.box=mkColor(0x101522,210);caption.boxPadding=13;
    caption.outlineWidth=1.5f;
    addRoot(c.roots,0,LEN)->fn=sub::captions(narration,caption);
    out.push_back(std::move(c));
}

} // namespace rmtn
