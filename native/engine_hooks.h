#pragma once
#include <cmath>
#include "camera_lens.h"

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
static unsigned long long simulation_steps=0,delta_calls=0;
static LARGE_INTEGER report_time={},report_frequency={};

static int __cdecl camera_build(void* output,void* camera) {
    if(camera&&camera_hooks_active&&std::isfinite(engine_aspect)&&engine_aspect>.3f&&engine_aspect<8.f) {
        auto* aspect=reinterpret_cast<float*>(static_cast<unsigned char*>(camera)+0x28);
        auto* fov=reinterpret_cast<int*>(static_cast<unsigned char*>(camera)+0x24);
        if(std::isfinite(*aspect)&&*aspect>.3f&&*aspect<8.f) {
            const float saved_aspect=*aspect;const int saved_fov=*fov;
            *fov=horplus_fov(saved_fov,saved_aspect,engine_aspect);*aspect=engine_aspect;
            static bool reported=false;
            if(!reported){FILE* file=std::fopen(engine_log,"a");if(file){std::fprintf(file,"CAMERA_HORPLUS original_fov=%d original_aspect=%.6f output_fov=%d output_aspect=%.6f\n",saved_fov,saved_aspect,*fov,*aspect);std::fclose(file);}reported=true;}
            const int result=original_camera(output,camera);
            // The original builder copies the corrected camera into its active global
            // before returning. Restore the source lens to prevent cumulative expansion.
            *aspect=saved_aspect;*fov=saved_fov;
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
    report_time=now;simulation_steps=delta_calls=0;
}
static void start_engine_hooks(const char* directory,const char* config) {
    std::snprintf(engine_log,MAX_PATH,"%sopensoccermania-engine.log",directory);
    if(!GetPrivateProfileIntA("Graphics","VerifiedExecutable",0,config))return;
    auto* base=reinterpret_cast<unsigned char*>(GetModuleHandleA(nullptr));
    const unsigned char camera_signature[]={0x83,0xec,0x24,0x53,0x8b,0x5c,0x24,0x30};
    const unsigned char delta_signature[]={0x83,0xec,0x28,0x53,0x55,0x56,0x8d,0x44};
    const unsigned char step_signature[]={0xd9,0x05,0x10,0x82,0x5e,0x00,0x8b,0x0d};
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
