#pragma once
#include <cmath>
#include "camera_lens.h"
#include "interpolation.h"

namespace osm {
static bool camera_hooks_active=false,timing_hooks_active=false;
static float engine_aspect=16.f/9.f;
static double render_limit=240;
static char engine_log[MAX_PATH];
static volatile float* legacy_wait_rate=nullptr;
static volatile float* legacy_logic_rate=nullptr;
using CameraBuild=int (__cdecl *)(void*,void*);
using FrameDelta=double (__cdecl *)(int);
using SimulationStep=void (__cdecl *)();
static CameraBuild original_camera;
static FrameDelta original_delta;
static SimulationStep original_step;
static SimulationStep original_interpolation;
static bool smooth_interpolation=true,interpolation_hook_active=false;
static char engine_config[MAX_PATH];
static bool isolated_camera_probe=false;
static float probe_camera_pitch=999.f;
static unsigned long long interpolation_frames=0,interpolation_boundary_frames=0;
static unsigned long long interpolation_rewinds=0;
static float previous_interpolation_tick=-1,previous_interpolation_alpha=0;
static void record_interpolation(float effective_alpha) {
    const float tick=std::floor(*reinterpret_cast<const float*>(0x5e8214));
    if(tick==previous_interpolation_tick&&effective_alpha+.00001f<previous_interpolation_alpha)++interpolation_rewinds;
    previous_interpolation_tick=tick;previous_interpolation_alpha=effective_alpha;
}
static void __cdecl interpolate_frame() {
    ++interpolation_frames;
    const float alpha=*reinterpret_cast<const float*>(0x5e8218);
    if(timing_hooks_active&&smooth_interpolation&&alpha<=.05f&&valid_interpolation_fraction(alpha)) {
        const unsigned bank=*reinterpret_cast<const unsigned*>(0x5e8208);
        auto** banks=reinterpret_cast<unsigned char**>(0x5e8228);
        auto* node=*reinterpret_cast<InterpolationNode**>(0x5e8220);
        if(bank<=1&&banks[bank]&&banks[1-bank]) {
            ++interpolation_boundary_frames;
            record_interpolation(alpha);
            if(!interpolate_snapshots(node,banks[bank],banks[1-bank],alpha)) {
                FILE* file=std::fopen(engine_log,"a");if(file){std::fprintf(file,"INTERPOLATION invalid snapshot chain; boundary dispatch stopped\n");std::fclose(file);}
            }
            return;
        }
    }
    record_interpolation(alpha<=.05f?1.f:alpha);
    original_interpolation();
}
using AnimationUpdate=void (__cdecl *)(void*);
static AnimationUpdate original_animation_advance,original_animation_pose;
static bool animation_trace_active=false;
static unsigned long long animation_advances=0,animation_poses=0,animation_repeated_times=0;
struct AnimationSample {void* state=nullptr;float time=0;};
static AnimationSample animation_samples[512];
static void __cdecl animation_advance(void* state) {++animation_advances;original_animation_advance(state);}
static void __cdecl animation_pose(void* state) {
    ++animation_poses;
    if(state) {
        const float time=*reinterpret_cast<float*>(static_cast<unsigned char*>(state)+0x1c);
        // Fixed-size diagnostic storage, no allocation in the animation path.
        for(auto& sample:animation_samples)if(!sample.state||sample.state==state) {
            if(sample.state&&sample.time==time)++animation_repeated_times;
            sample.state=state;sample.time=time;break;
        }
    }
    original_animation_pose(state);
}
static unsigned long long simulation_steps=0,delta_calls=0;
static LARGE_INTEGER report_time={},report_frequency={};

static int __cdecl camera_build(void* output,void* camera) {
    if(camera&&camera_hooks_active&&std::isfinite(engine_aspect)&&engine_aspect>.3f&&engine_aspect<8.f) {
        auto* aspect=reinterpret_cast<float*>(static_cast<unsigned char*>(camera)+0x28);
        auto* fov=reinterpret_cast<int*>(static_cast<unsigned char*>(camera)+0x24);
        if(std::isfinite(*aspect)&&*aspect>.3f&&*aspect<8.f) {
            const float saved_aspect=*aspect;const int saved_fov=*fov;
            auto* lens=reinterpret_cast<float*>(camera);
            const float saved_target_z=lens[5];
            if(isolated_camera_probe&&probe_camera_pitch>=-80.f&&probe_camera_pitch<=80.f) {
                const float dx=lens[3]-lens[0],dy=lens[4]-lens[1];
                const float distance=std::sqrt(dx*dx+dy*dy);
                if(std::isfinite(distance)&&distance>1.f)lens[5]=lens[2]+distance*std::tan(probe_camera_pitch*3.14159265358979323846f/180.f);
            }
            *fov=horplus_fov(saved_fov,saved_aspect,engine_aspect);*aspect=engine_aspect;
            static bool reported=false;
            if(!reported){FILE* file=std::fopen(engine_log,"a");if(file){std::fprintf(file,"CAMERA_HORPLUS original_fov=%d original_aspect=%.6f output_fov=%d output_aspect=%.6f\n",saved_fov,saved_aspect,*fov,*aspect);std::fclose(file);}reported=true;}
            const int result=original_camera(output,camera);
            // The original builder copies the corrected camera into its active global
            // before returning. Restore the source lens to prevent cumulative expansion.
            *aspect=saved_aspect;*fov=saved_fov;lens[5]=saved_target_z;
            return result;
        }
    }
    return original_camera(output,camera);
}
static double __cdecl frame_delta(int interpolation_phase) {
    // Only the wait uses the new render rate. The caller's original simulation-rate
    // argument and the game's authoritative rate remain unchanged.
    const float original=*legacy_wait_rate;
    *legacy_wait_rate=static_cast<float>(render_limit>0?render_limit:1000000.0);
    const double elapsed=original_delta(interpolation_phase);
    *legacy_wait_rate=original;
    ++delta_calls;
    return elapsed;
}
static void __cdecl simulation_step() {original_step();++simulation_steps;}

static void engine_report() {
    if(!timing_hooks_active)return;
    LARGE_INTEGER now;QueryPerformanceCounter(&now);
    const double seconds=static_cast<double>(now.QuadPart-report_time.QuadPart)/report_frequency.QuadPart;
    if(seconds<=0)return;
    FILE* file=std::fopen(engine_log,"a");
    if(file){std::fprintf(file,"CLOCK configured_logic_hz=%.6f preserved_wait_rate=%.6f observed_steps=%llu observed_step_hz=%.4f delta_calls=%llu window_seconds=%.4f render_limit=%.1f\n",*legacy_logic_rate,*legacy_wait_rate,simulation_steps,simulation_steps/seconds,delta_calls,seconds,render_limit);std::fclose(file);}
    if(animation_trace_active) {
        file=std::fopen(engine_log,"a");if(file){std::fprintf(file,"ANIMATION advances=%llu poses=%llu repeated_pose_times=%llu seconds=%.4f\n",animation_advances,animation_poses,animation_repeated_times,seconds);std::fclose(file);}
        animation_advances=animation_poses=animation_repeated_times=0;
    }
    if(interpolation_hook_active) {
        file=std::fopen(engine_log,"a");if(file){std::fprintf(file,"INTERPOLATION smooth=%d frames=%llu corrected_boundary_frames=%llu within_tick_rewinds=%llu\n",smooth_interpolation,interpolation_frames,interpolation_boundary_frames,interpolation_rewinds);std::fclose(file);}
        interpolation_frames=interpolation_boundary_frames=interpolation_rewinds=0;
        smooth_interpolation=GetPrivateProfileIntA("Graphics","SmoothInterpolation",1,engine_config)!=0;
    }
    if(isolated_camera_probe) {
        char pitch[32];GetPrivateProfileStringA("Diagnostics","ProbeCameraPitch","999",pitch,sizeof(pitch),engine_config);
        const float value=std::strtof(pitch,nullptr);
        if(value!=probe_camera_pitch){probe_camera_pitch=value;file=std::fopen(engine_log,"a");if(file){std::fprintf(file,"CAMERA_PROBE pitch_degrees=%.3f (isolated test only)\n",value);std::fclose(file);}}
    }
    report_time=now;simulation_steps=delta_calls=0;
}
static void start_engine_hooks(const char* directory,const char* config) {
    std::snprintf(engine_log,MAX_PATH,"%sopensoccermania-engine.log",directory);
    std::snprintf(engine_config,MAX_PATH,"%s",config);
    isolated_camera_probe=GetPrivateProfileIntA("Diagnostics","IsolateInstance",0,config)!=0;
    if(!GetPrivateProfileIntA("Graphics","VerifiedExecutable",0,config))return;
    auto* base=reinterpret_cast<unsigned char*>(GetModuleHandleA(nullptr));
    const unsigned char advance_signature[]={0x56,0x8b,0x74,0x24,0x08,0x8d,0x46,0x18};
    const unsigned char pose_signature[]={0x81,0xec,0x24,0x01,0x00,0x00,0x53,0x55,0x56};
    if(GetPrivateProfileIntA("Diagnostics","TraceAnimation",0,config)&&
       std::memcmp(base+0x15a130,advance_signature,sizeof(advance_signature))==0&&
       std::memcmp(base+0x15a2a0,pose_signature,sizeof(pose_signature))==0) {
        const auto a=MH_CreateHook(base+0x15a130,reinterpret_cast<void*>(animation_advance),reinterpret_cast<void**>(&original_animation_advance));
        const auto b=MH_CreateHook(base+0x15a2a0,reinterpret_cast<void*>(animation_pose),reinterpret_cast<void**>(&original_animation_pose));
        animation_trace_active=a==MH_OK&&b==MH_OK;
        if(!animation_trace_active){if(a==MH_OK)MH_RemoveHook(base+0x15a130);if(b==MH_OK)MH_RemoveHook(base+0x15a2a0);}
    }
    const unsigned char camera_signature[]={0x83,0xec,0x24,0x53,0x8b,0x5c,0x24,0x30};
    const unsigned char delta_signature[]={0x83,0xec,0x28,0x53,0x55,0x56,0x8d,0x44};
    const unsigned char step_signature[]={0xd9,0x05,0x10,0x82,0x5e,0x00,0x8b,0x0d};
    const unsigned char interpolation_signature[]={0x51,0xd9,0x05,0x14,0x82,0x5e,0x00,0x53,0x56};
    if(std::memcmp(base+0x1fcd0,interpolation_signature,sizeof(interpolation_signature))==0) {
        smooth_interpolation=GetPrivateProfileIntA("Graphics","SmoothInterpolation",1,config)!=0;
        interpolation_hook_active=MH_CreateHook(base+0x1fcd0,reinterpret_cast<void*>(interpolate_frame),reinterpret_cast<void**>(&original_interpolation))==MH_OK;
    }
    if(GetPrivateProfileIntA("Graphics","ExperimentalHorPlus",0,config)&&std::memcmp(base+0x1ae90,camera_signature,sizeof(camera_signature))==0)
        camera_hooks_active=MH_CreateHook(base+0x1ae90,reinterpret_cast<void*>(camera_build),reinterpret_cast<void**>(&original_camera))==MH_OK;
    render_limit=GetPrivateProfileIntA("Graphics","RenderFPS",240,config);
    if(render_limit<0||render_limit>1000)render_limit=240;
    if(std::memcmp(base+0x1f9c0,delta_signature,sizeof(delta_signature))==0&&std::memcmp(base+0x1fc70,step_signature,sizeof(step_signature))==0) {
        legacy_wait_rate=reinterpret_cast<volatile float*>(base+0x21fc24);
        legacy_logic_rate=reinterpret_cast<volatile float*>(base+0x1a36ec);
        const auto a=MH_CreateHook(base+0x1f9c0,reinterpret_cast<void*>(frame_delta),reinterpret_cast<void**>(&original_delta));
        const auto b=MH_CreateHook(base+0x1fc70,reinterpret_cast<void*>(simulation_step),reinterpret_cast<void**>(&original_step));
        timing_hooks_active=a==MH_OK&&b==MH_OK;
        if(!timing_hooks_active) {
            if(a==MH_OK)MH_RemoveHook(base+0x1f9c0);
            if(b==MH_OK)MH_RemoveHook(base+0x1fc70);
        }
        QueryPerformanceFrequency(&report_frequency);QueryPerformanceCounter(&report_time);
    }
    FILE* file=std::fopen(engine_log,"a");if(file){std::fprintf(file,"ENGINE_HOOKS camera_frustum=%d separate_render_wait=%d target_fps=%.1f\n",camera_hooks_active,timing_hooks_active,render_limit);std::fclose(file);}
}
}
