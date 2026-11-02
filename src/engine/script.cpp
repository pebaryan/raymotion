#include "rmtn/script.hpp"
#include "rmtn/media.hpp"
#include "rmtn/prefab.hpp"
#include "rmtn/transitions.hpp"
#include "rmtn/subtitles.hpp"
#include <filesystem>
#include <map>
#include <stdexcept>
#include <cctype>
#include <cmath>
#include <limits>

namespace rmtn::script {
#ifdef RMTN_ANGELSCRIPT
std::shared_ptr<Program> loadAngelScript(const std::string& path);
#endif
namespace {
std::string normalized(std::string extension) {
    for (char& ch : extension) ch = (char)std::tolower((unsigned char)ch);
    return extension;
}
std::map<std::string, Loader>& backends() {
    static std::map<std::string, Loader> value = {
#ifdef RMTN_ANGELSCRIPT
        {".as", loadAngelScript}
#endif
    };
    return value;
}
void validate(const Description& d) {
    if (d.name.empty() || d.name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-") != std::string::npos)
        throw std::runtime_error("script composition name must contain only letters, digits, '_' or '-'");
    if (d.w < 2 || d.h < 2 || d.w > 16384 || d.h > 16384 || d.fps < 1 || d.fps > 1000 || d.duration < 1)
        throw std::runtime_error("invalid script composition dimensions, fps or duration");
    if (d.supersample != 1 && d.supersample != 2 && d.supersample != 4)
        throw std::runtime_error("script supersample must be 1, 2 or 4");
    if (d.backgroundAlpha < 0 || d.backgroundAlpha > 255)
        throw std::runtime_error("script backgroundAlpha must be between 0 and 255");
    if (d.statementBudget < 1 || d.statementBudget > 100000000)
        throw std::runtime_error("script statementBudget must be between 1 and 100000000");
    if(d.motionBlurSamples<1||d.motionBlurSamples>32||!std::isfinite(d.motionBlurShutter)||d.motionBlurShutter<0||d.motionBlurShutter>1)
        throw std::runtime_error("script motion blur needs 1..32 samples and shutter 0..1");
    if(d.cameraWorlds.size()>1)throw std::runtime_error("a script composition supports one cam::world track");
    for(const auto& camera:d.cameraWorlds)if(!std::isfinite(camera.track.frame.x)||!std::isfinite(camera.track.frame.y)||camera.track.frame.x<=0||camera.track.frame.y<=0||!std::isfinite(camera.parallax)||camera.function.size()>128)throw std::runtime_error("script cam::world needs positive frame dimensions, finite parallax and a callback name up to 128 characters");
    if(d.song.size()>10000) throw std::runtime_error("script procedural score supports at most 10000 events");
    const double songDuration=(double)d.duration/d.fps;
    for(const auto& event:d.song) if(!std::isfinite(event.t0)||!std::isfinite(event.dur)||!std::isfinite(event.freq)||!std::isfinite(event.amp)||
        event.t0<0||event.dur<=0||event.t0+event.dur>songDuration||event.freq<=0||event.freq>24000||event.amp<0||event.amp>2)
        throw std::runtime_error("script score notes must fit the composition, use 0 < frequency <= 24000 Hz and amplitude 0..2");
    for (const auto& s : d.scenes)
        if (s.function.empty() || s.start < 0 || s.duration < 1 || (long long)s.start + s.duration > d.duration ||
            s.loopEvery<0 || s.loopEvery>s.duration || s.freezeAt < -1 || (s.loopEvery>0 && s.freezeAt>=0))
            throw std::runtime_error("script scene must lie inside the composition timeline");
    for(size_t i=0;i<d.scenes.size();++i) {
        const auto& scene=d.scenes[i];
        if(scene.parent < -1 || scene.parent >= (int)d.scenes.size() || scene.parent == (int)i)
            throw std::runtime_error("script scene parent index is invalid");
    }
    for(size_t i=0;i<d.scenes.size();++i) {
        const auto& scene=d.scenes[i];
        if(scene.parent>=0 && (long long)scene.start+scene.duration>d.scenes[(size_t)scene.parent].duration)
            throw std::runtime_error("nested script scene must fit inside its parent's local duration");
        int ancestor=scene.parent;
        for(size_t depth=0;ancestor>=0 && depth<=d.scenes.size();++depth) {
            if(ancestor==(int)i) throw std::runtime_error("script scene nesting contains a cycle");
            ancestor=d.scenes[(size_t)ancestor].parent;
            if(depth==d.scenes.size()) throw std::runtime_error("script scene nesting contains a cycle");
        }
    }
    for(const auto& s:d.scenes) if(s.isolated && (!std::isfinite(s.layerOpacity)||s.layerOpacity<0||s.layerOpacity>1||
        !std::isfinite(s.layerOffsetX)||!std::isfinite(s.layerOffsetY)||s.layerBlurSamples<1||s.layerBlurSamples>32||
        !std::isfinite(s.layerShutter)||s.layerShutter<0||s.layerShutter>1||s.filters.size()>16))
        throw std::runtime_error("script layer opacity, offset, blur or filter count is invalid");
    for(const auto& s:d.scenes) for(const auto& f:s.filters) {
        const float values[]={f.size,f.angle,f.threshold,f.strength,f.granulation,f.position,f.band,f.falloff};
        for(float value:values) if(!std::isfinite(value)) throw std::runtime_error("script layer filter values must be finite");
        if(f.levels<1||f.levels>256||f.size<0||f.size>4096||f.threshold<0||f.threshold>1||
           f.granulation<0||f.granulation>2||f.position<0||f.position>1||f.band<0||f.band>1||f.falloff<0||f.falloff>1)
            throw std::runtime_error("script layer filter parameter is outside its supported range");
    }
    for (const auto& cue : d.media)
        if (cue.file.empty() || cue.start < 0 || cue.duration < 1 ||
            (long long)cue.start + cue.duration > d.duration || cue.playbackRate <= 0 ||
            cue.parentScene < -1 || cue.parentScene >= (int)d.scenes.size() ||
            cue.volume < 0 || cue.volume > 4 ||
            (cue.kind == MediaCue::Kind::Video && (cue.w <= 0 || cue.h <= 0 ||
              (cue.fit!="fill"&&cue.fit!="contain"&&cue.fit!="cover"&&cue.fit!="none")||
              !std::isfinite(cue.alignX)||!std::isfinite(cue.alignY)||cue.alignX<0||cue.alignX>1||cue.alignY<0||cue.alignY>1)))
            throw std::runtime_error("invalid script media cue");
    for(const auto& cue:d.media)
        if(cue.parentScene>=0 && (long long)cue.start+cue.duration>d.scenes[(size_t)cue.parentScene].duration)
            throw std::runtime_error("nested script media cue must fit inside its parent scene's local duration");
    for (const auto& cue : d.media) {
        if(!cue.volumeFunction.empty() && (!cue.volumeKeys.empty() || cue.volumeFunction.size()>128))
            throw std::runtime_error("script media volumeFunction must be at most 128 characters and cannot be combined with frame/gain pairs");
        if (cue.volumeKeys.empty()) continue;
        if (cue.volumeKeys.size()<4 || cue.volumeKeys.size()%2 || cue.volumeKeys.size()>2048)
            throw std::runtime_error("script media volume envelope needs 2–1024 frame/gain pairs");
        float previous=-1;
        for(size_t i=0;i<cue.volumeKeys.size();i+=2) {
            const float frame=cue.volumeKeys[i], gain=cue.volumeKeys[i+1];
            if(!std::isfinite(frame)||!std::isfinite(gain)||frame<0||frame>cue.duration||frame<=previous||gain<0||gain>1)
                throw std::runtime_error("script media envelope frames must increase within the cue and gains must be 0..1");
            previous=frame;
        }
    }
    for (const auto& cue : d.prefabs)
        if (cue.name.empty() || cue.start < 0 || cue.duration < 1 ||
            (long long)cue.start + cue.duration > d.duration ||
            cue.parentScene < -1 || cue.parentScene >= (int)d.scenes.size())
            throw std::runtime_error("invalid script prefab cue");
    for(const auto& cue:d.prefabs)
        if(cue.parentScene>=0 && (long long)cue.start+cue.duration>d.scenes[(size_t)cue.parentScene].duration)
            throw std::runtime_error("nested script prefab cue must fit inside its parent scene's local duration");
    for(size_t i=0;i<d.scriptPrefabs.size();++i) {
        const auto& definition=d.scriptPrefabs[i];
        if(definition.name.empty()||definition.function.empty()||!definition.defaultProps.isObject())
            throw std::runtime_error("script prefab definitions need a name, draw function and object defaults");
        for(size_t j=0;j<i;++j)if(d.scriptPrefabs[j].name==definition.name)
            throw std::runtime_error("duplicate script prefab definition: "+definition.name);
    }
    if(d.voiceovers.size()>32) throw std::runtime_error("script supports at most 32 voiceover cues");
    for(const auto& cue:d.voiceovers) {
        if(cue.start<0||cue.start>=d.duration||cue.lines.empty()||cue.lines.size()>256||
           cue.parentScene < -1 || cue.parentScene >= (int)d.scenes.size() ||
           !std::isfinite(cue.volume)||cue.volume<0||cue.volume>4||!std::isfinite(cue.gap)||cue.gap<0||cue.gap>60||
           !std::isfinite(cue.lead)||cue.lead<0||cue.lead>60||!std::isfinite(cue.wordsPerSecond)||cue.wordsPerSecond<=0||cue.wordsPerSecond>20||
           cue.command.size()>8192||cue.cacheDir.empty()||cue.cacheDir.size()>1024)
            throw std::runtime_error("invalid script voiceover cue");
        if(cue.parentScene>=0 && cue.start>=d.scenes[(size_t)cue.parentScene].duration)
            throw std::runtime_error("nested script voiceover must start inside its parent scene's local duration");
        for(const auto& line:cue.lines) if(line.empty()||line.size()>16384)
            throw std::runtime_error("voiceover lines must contain 1..16384 bytes");
    }
    for (const auto& cue : d.transitions)
        if (cue.fromScene < 0 || cue.toScene < 0 || cue.fromScene >= (int)d.scenes.size() ||
            cue.toScene >= (int)d.scenes.size() || cue.fromScene == cue.toScene ||
            cue.frames < 1 || cue.frames > d.scenes[cue.fromScene].duration ||
            cue.frames > d.scenes[cue.toScene].duration || cue.direction < 0 || cue.direction > 3 || cue.easingFunction.size()>128)
            throw std::runtime_error("invalid script transition cue");
}
void apply(Comp& c, const Description& d, const std::shared_ptr<Program>& program,
           const Json& props) {
    validate(d);
    c.name = d.name; c.w = d.w; c.h = d.h; c.fps = d.fps;
    c.duration = d.duration; c.supersample = d.supersample; c.background = mkColor(d.background,(unsigned char)d.backgroundAlpha);
    if(d.defaultProps.isObject()) c.defaultProps=d.defaultProps;
    c.motionBlur={d.motionBlurSamples,d.motionBlurShutter};
    c.render={}; if(!d.renderFunction.empty()) { const auto renderFunction=d.renderFunction; const int statementBudget=d.statementBudget; c.render=[program,renderFunction,props,statementBudget](Comp&,int frame,RenderTexture) { program->draw(renderFunction,Ctx{},frame,props,statementBudget); }; }
    c.music=d.music; c.song=d.song;
    c.assets={};
    const bool scriptAssets=program->hasAssetHook(), voiceAssets=!d.voiceovers.empty();
    if(scriptAssets||voiceAssets) c.assets=[program,scriptAssets,voiceAssets](const Comp&,const Json& props) {
        if(scriptAssets) program->generateAssets(props);
        if(voiceAssets) {
            const auto description=program->configure(props);
            for(const auto& cue:description.voiceovers) {
                tts::Voice voice=cue.command.empty()?tts::Voice::system():tts::Voice{};
                if(!cue.command.empty()) voice.command=cue.command;
                voice.cacheDir=cue.cacheDir; voice.wordsPerSecond=cue.wordsPerSecond;
                tts::Script spoken(std::move(voice),cue.lines,cue.gap,cue.lead);
                spoken.generate();
            }
        }
    };
    c.roots.clear();
    auto scenes = d.scenes;
    if (scenes.empty()) scenes.push_back({0, d.duration, "draw"});
    std::vector<TNode*> sceneNodes(scenes.size(),nullptr);
    const int statementBudget = d.statementBudget;
    std::vector<unsigned char> sceneBuildState(scenes.size(),0);
    std::function<void(size_t)> addScene = [&](size_t index) {
        if(sceneBuildState[index]==2)return;
        if(sceneBuildState[index]==1)throw std::runtime_error("script scene nesting contains a cycle");
        sceneBuildState[index]=1;
        const auto& scene=scenes[index];
        TNode* parent=nullptr;
        if(scene.parent>=0) {
            addScene((size_t)scene.parent);
            parent=sceneNodes[(size_t)scene.parent];
        }
        auto function = scene.function;
        const Json drawProps=scene.hasDrawProps?props.merged(scene.drawProps):props;
        auto* node=parent?parent->add(scene.start,scene.duration):addRoot(c.roots, scene.start, scene.duration);
        node->loopEvery=scene.loopEvery; node->freezeAt=scene.freezeAt;
        if(!scene.updateFunction.empty() || !scene.layerUpdateFunction.empty()) {
            const auto updateFunction=scene.updateFunction;
            const NodeProps baseProps=node->props;
            node->up=[program,updateFunction,drawProps,statementBudget,baseProps](int frame,NodeProps& propsAtFrame) {
                propsAtFrame=baseProps;
                if(!updateFunction.empty())
                    program->updateScene(updateFunction,frame,propsAtFrame,drawProps,statementBudget);
            };
        }
        if(scene.isolated) {
            auto& layer=node->isolate(); layer.enabled=true; layer.opacity=scene.layerOpacity;
            layer.offset={scene.layerOffsetX,scene.layerOffsetY}; layer.blend=scene.layerBlend;
            layer.maskMode=scene.layerMaskMode; layer.invertMask=scene.invertMask;
            layer.motionBlur={scene.layerBlurSamples,scene.layerShutter}; layer.filters=scene.filters;
            if(!scene.maskFunction.empty()) {
                auto maskFunction=scene.maskFunction;
                layer.mask=[program,maskFunction,props,statementBudget](const Ctx& ctx,int t) {
                    program->draw(maskFunction,ctx,t,props,statementBudget);
                };
            }
        }
        if(!scene.layerUpdateFunction.empty()) {
            const auto updateFunction=scene.layerUpdateFunction;
            const Layer baseLayer=*node->layer;
            node->layerUp=[program,updateFunction,drawProps,statementBudget,baseLayer](int frame,NodeProps& propsAtFrame,Layer& layer) {
                layer=baseLayer;
                program->updateLayer(updateFunction,frame,propsAtFrame,layer,drawProps,statementBudget);
            };
        }
        node->fn = [program, function, drawProps, statementBudget](const Ctx& ctx, int t) {
            program->draw(function, ctx, t, drawProps, statementBudget);
        };
        sceneNodes[index]=node;
        sceneBuildState[index]=2;
    };
    for(size_t i=0;i<scenes.size();++i)addScene(i);
    TransitionSeries transitions(c.roots,(float)c.w,(float)c.h);
    for (const auto& cue : d.transitions) {
        Transition transition; transition.frames=cue.frames;
        transition.kind=(Transition::Kind)cue.kind; transition.dir=(Direction)cue.direction;
        if(cue.easingFunction.empty()) transition.easing=ease::fromIndex(cue.easing);
        else { const auto name=cue.easingFunction; transition.easing=[program,name,props,statementBudget](double progress){return program->evaluateScalar(name,progress,props,statementBudget);}; }
        transitions.connect(sceneNodes[cue.fromScene],sceneNodes[cue.toScene],transition,cue.frames);
    }
    for (const auto& cue : d.media) {
        auto* node = cue.parentScene>=0
            ? sceneNodes[(size_t)cue.parentScene]->add(cue.start,cue.duration)
            : addRoot(c.roots, cue.start, cue.duration);
        if (cue.kind == MediaCue::Kind::Video) {
            media::VideoOptions options;
            options.startFrom = cue.startFrom; options.endAt = cue.endAt;
            options.playbackRate = cue.playbackRate; options.loop = cue.loop;
            options.volume = cue.volume; options.muted = cue.muted;
            if(!cue.volumeFunction.empty()) { const auto name=cue.volumeFunction; options.volumeAt=[program,name,props,statementBudget](int t){return program->evaluateFloat(name,t,props,statementBudget);}; }
            options.style.fit=cue.fit=="fill"?media::Fit::Fill:cue.fit=="none"?media::Fit::None:cue.fit=="contain"?media::Fit::Contain:media::Fit::Cover;
            options.style.align={cue.alignX,cue.alignY}; options.style.tint=mkColor(cue.tint);
            if(!cue.volumeKeys.empty()) {
                const auto keys=cue.volumeKeys;
                options.volumeAt=[keys](int t) {
                    if(t<=keys[0]) return keys[1];
                    for(size_t i=2;i<keys.size();i+=2) if(t<=keys[i]) {
                        const float u=(t-keys[i-2])/(keys[i]-keys[i-2]);
                        return keys[i-1]+(keys[i+1]-keys[i-1])*u;
                    }
                    return keys.back();
                };
            }
            media::video(node, cue.file, {cue.x, cue.y, cue.w, cue.h}, options);
        } else {
            media::AudioOptions options;
            options.startFrom = cue.startFrom; options.endAt = cue.endAt;
            options.playbackRate = cue.playbackRate; options.loop = cue.loop;
            options.volume = cue.volume;
            if(!cue.volumeFunction.empty()) { const auto name=cue.volumeFunction; options.volumeAt=[program,name,props,statementBudget](int t){return program->evaluateFloat(name,t,props,statementBudget);}; }
            if(!cue.volumeKeys.empty()) {
                const auto keys=cue.volumeKeys;
                options.volumeAt=[keys](int t) {
                    if(t<=keys[0]) return keys[1];
                    for(size_t i=2;i<keys.size();i+=2) if(t<=keys[i]) {
                        const float u=(t-keys[i-2])/(keys[i]-keys[i-2]);
                        return keys[i-1]+(keys[i+1]-keys[i-1])*u;
                    }
                    return keys.back();
                };
            }
            media::audio(node, cue.file, options);
        }
    }
    for (const auto& cue : d.prefabs) {
        Nodes& destination=cue.parentScene>=0?sceneNodes[(size_t)cue.parentScene]->kids:c.roots;
        prefab::use(destination, cue.start, cue.duration, cue.name, cue.props);
    }
    for(const auto& cue:d.voiceovers) {
        tts::Voice voice=cue.command.empty()?tts::Voice::system():tts::Voice{};
        if(!cue.command.empty()) voice.command=cue.command;
        voice.cacheDir=cue.cacheDir; voice.wordsPerSecond=cue.wordsPerSecond;
        tts::Script spoken(std::move(voice),cue.lines,cue.gap,cue.lead); spoken.measure();
        const int length=std::max(1,spoken.frames(c.fps)+1);
        const int limit=cue.parentScene>=0?scenes[(size_t)cue.parentScene].duration:c.duration;
        if((long long)cue.start+length>limit) throw std::runtime_error(cue.parentScene>=0?"nested script voiceover exceeds its parent scene's local duration":"script voiceover exceeds composition duration");
        auto* node=cue.parentScene>=0
            ?sceneNodes[(size_t)cue.parentScene]->add(cue.start,length)
            :addRoot(c.roots,cue.start,length);
        if(cue.captions) {
            const auto track=spoken.track(); const int fps=c.fps;
            node->fn=[track,fps](const Ctx& ctx,int frame) {
                const double seconds=(double)frame/fps;
                if(const auto* caption=track.at(seconds)) sub::draw(ctx,*caption,seconds);
            };
        }
        spoken.attach(node,c.fps,cue.volume);
    }
    if(!d.cameraWorlds.empty()) {
        if(d.cameraWorlds.size()!=1)throw std::runtime_error("a script composition supports one cam::world track");
        auto children=std::move(c.roots);c.roots.clear();
        const auto& camera=d.cameraWorlds.front();TNode* world=nullptr;
        if(camera.function.empty())world=cam::world(c.roots,0,c.duration,camera.track,camera.parallax);
        else {const auto name=camera.function;const auto callbackProps=props;const int budget=d.statementBudget;const Vector2 frame=camera.track.frame;world=cam::world(c.roots,0,c.duration,[program,name,callbackProps,budget](int t){return program->evaluateShot(name,t,callbackProps,budget);},frame,camera.parallax);}
        world->kids=std::move(children);
    }
}
}
void registerBackend(const std::string& extension, Loader loader) {
    auto key = normalized(extension);
    if (key.size() < 2 || key.front() != '.' || !loader)
        throw std::runtime_error("script backend needs an extension and a loader");
    if (!backends().emplace(key, std::move(loader)).second)
        throw std::runtime_error("script backend already registered: " + key);
}
void registerFile(Registry& registry, const std::string& path) {
    auto ext = normalized(std::filesystem::path(path).extension().string());
    auto it = backends().find(ext);
    if (it == backends().end()) throw std::runtime_error("no script backend for '" + ext + "' (enable RAYMOTION_SCRIPTING for .as)");
    auto program = it->second(std::filesystem::absolute(path).string());
    if (!program) throw std::runtime_error("script backend returned no program: " + path);
    Comp c;
    apply(c, program->configure(Json()), program, Json());
    for (const auto& existing : registry)
        if (existing.name == c.name) throw std::runtime_error("duplicate composition name: " + c.name);
    const auto name = c.name;
    c.build = [program, name](Comp& target, const Json& props) {
        auto description = program->configure(props);
        if (description.name != name) throw std::runtime_error("script props must not change composition name");
        apply(target, description, program, props);
    };
    registry.push_back(std::move(c));
}
} // namespace rmtn::script
