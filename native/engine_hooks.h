#pragma once
#include <cmath>
#include "camera_lens.h"
#include "interpolation.h"
#include "save_metadata.h"
#include "save_worker.h"
#include "menu_preview.h"

namespace osm {
static bool camera_hooks_active=false,timing_hooks_active=false;
static bool camera_horplus_enabled=false;
static float engine_aspect=16.f/9.f;
static double render_limit=240;
static int camera_zoom_index=0;
static ULONGLONG last_simulation_tick=0;
static unsigned long long camera_calls=0,camera_position_changes=0;
static float camera_vertical_min=100,camera_vertical_max=0;
static void* last_camera_source=nullptr;
static bool dialog_state_verified=false;
static bool engine_gameplay() {
    // Resource-loaded flags remain set throughout a match. The dialog manager
    // alone distinguishes an active pause/submenu from ordinary gameplay.
    if(dialog_state_verified) {
        const auto* manager=*reinterpret_cast<const unsigned* const*>(0x681b00);
        if(!manager||manager[0x84/4]==0)return false;
    }
    return timing_hooks_active&&last_simulation_tick&&GetTickCount64()-last_simulation_tick<300&&
        *reinterpret_cast<const unsigned*>(0x5b75cc)>0;
}
static char engine_log[MAX_PATH];
static bool trace_save_status=false;
using SaveStatus=int (__cdecl *)();
static SaveStatus original_save_status;
static int __cdecl save_status() {
    const int result=original_save_status();
    struct ErrorState {DWORD value;~ErrorState(){SetLastError(value);}} restore_error{GetLastError()};
    const auto caller=reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
    if(trace_save_status) {
        static LONG reports=0;
        if(InterlockedIncrement(&reports)<=512) {
            static const ULONGLONG started=GetTickCount64();
            const ULONGLONG elapsed=GetTickCount64()-started;
            FILE* file=std::fopen(engine_log,"a");
            if(file){std::fprintf(file,"SAVE_STATUS elapsed_ms=%llu caller=%08lx status=%d io_command=%u list_phase=%u list_index=%u list_result=%u load_expected=%u load_read=%u load_phase=%u\n",elapsed,static_cast<unsigned long>(caller),result,*reinterpret_cast<const volatile unsigned*>(0x61d638),*reinterpret_cast<const volatile unsigned*>(0x5b596c),*reinterpret_cast<const volatile unsigned*>(0x682fcc),*reinterpret_cast<const volatile unsigned*>(0x682fc8),*reinterpret_cast<const volatile unsigned*>(0x683700),*reinterpret_cast<const volatile unsigned*>(0x683708),*reinterpret_cast<const volatile unsigned*>(0x5b5960));std::fclose(file);}
        }
    }
    if(result!=0||caller!=0x4f9c3b)return result;
    auto*& source=*reinterpret_cast<const char**>(0x683750);
    std::size_t available=0;
    if(source) {
        MEMORY_BASIC_INFORMATION memory={};
        if(VirtualQuery(source,&memory,sizeof(memory))&&memory.State==MEM_COMMIT&&!(memory.Protect&PAGE_GUARD)) {
            const DWORD access=memory.Protect&0xff;
            if(access==PAGE_READONLY||access==PAGE_READWRITE||access==PAGE_WRITECOPY||
               access==PAGE_EXECUTE_READ||access==PAGE_EXECUTE_READWRITE||access==PAGE_EXECUTE_WRITECOPY)
                available=std::min<std::size_t>(64,reinterpret_cast<std::uintptr_t>(memory.BaseAddress)+memory.RegionSize-reinterpret_cast<std::uintptr_t>(source));
        }
    }
    static thread_local char normalized[64];
    if(normalize_save_metadata(source,available,normalized)) {
        source=normalized;
        static unsigned reports=0;
        if(reports++<32){FILE* file=std::fopen(engine_log,"a");if(file){std::fputs("SAVE_METADATA normalized empty or malformed list record; files unchanged\n",file);std::fclose(file);}}
    }
    return result;
}
static volatile float* legacy_wait_rate=nullptr;
static volatile float* legacy_logic_rate=nullptr;
using CameraBuild=int (__cdecl *)(void*,void*);
using FrameDelta=double (__cdecl *)(int);
using SimulationStep=void (__cdecl *)();
static CameraBuild original_camera;
static FrameDelta original_delta;
static SimulationStep original_step;
static SimulationStep original_interpolation;
static SimulationStep original_snapshot_allocate,original_snapshot_save;
static SnapshotHistory snapshot_history;
static bool snapshot_history_hooks_active=false;
static void __cdecl allocate_snapshots() {original_snapshot_allocate();snapshot_history.reset();}
static void __cdecl save_snapshots() {
    original_snapshot_save();
    const unsigned bank=*reinterpret_cast<const unsigned*>(0x5e8208);
    const unsigned bytes=*reinterpret_cast<const unsigned*>(0x5e8230);
    auto** banks=reinterpret_cast<unsigned char**>(0x5e8228);
    if(bank<=1&&snapshot_history.prime(banks[bank],banks[1-bank],bytes)) {
        FILE* file=std::fopen(engine_log,"a");if(file){std::fprintf(file,"INTERPOLATION_HISTORY seeded_bytes=%u\n",bytes);std::fclose(file);}
    }
}
static bool smooth_interpolation=false,interpolation_hook_active=false;
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
[[maybe_unused]] static void __cdecl interpolate_frame() {
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
    if(preview_camera_context(reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)),preview_hooks_active,preview_model_depth!=0))
        return original_camera(output,camera);
    if(isolated_camera_probe&&camera) {
        const auto caller=reinterpret_cast<std::uintptr_t>(__builtin_return_address(0));
        static std::uintptr_t observed[16]={};
        for(auto& site:observed) {
            if(site==caller)break;
            if(!site) {
                site=caller;
                FILE* file=std::fopen(engine_log,"a");
                if(file){std::fprintf(file,"CAMERA_CONTEXT caller=%08lx fov=%d aspect=%.6f\n",static_cast<unsigned long>(caller),*reinterpret_cast<const int*>(static_cast<const unsigned char*>(camera)+0x24),*reinterpret_cast<const float*>(static_cast<const unsigned char*>(camera)+0x28));std::fclose(file);}
                break;
            }
        }
    }
    // Preserve the unmodified builder path when neither lens feature nor an
    // authorized isolated camera probe needs temporary source changes.
    const bool pitch_probe=isolated_camera_probe&&probe_camera_pitch>=-80.f&&probe_camera_pitch<=80.f;
    if(!camera_horplus_enabled&&camera_zoom_index==0&&!pitch_probe)return original_camera(output,camera);
    if(camera&&camera_hooks_active&&(!camera_horplus_enabled||(std::isfinite(engine_aspect)&&engine_aspect>.3f&&engine_aspect<8.f))) {
        auto* aspect=reinterpret_cast<float*>(static_cast<unsigned char*>(camera)+0x28);
        auto* fov=reinterpret_cast<int*>(static_cast<unsigned char*>(camera)+0x24);
        if(std::isfinite(*aspect)&&*aspect>.3f&&*aspect<8.f) {
            const float saved_aspect=*aspect;const int saved_fov=*fov;
            auto* lens=reinterpret_cast<float*>(camera);
            float source_position[6];std::memcpy(source_position,lens,sizeof(source_position));
            const float saved_target_z=lens[5];
            if(isolated_camera_probe&&probe_camera_pitch>=-80.f&&probe_camera_pitch<=80.f) {
                const float dx=lens[3]-lens[0],dy=lens[4]-lens[1];
                const float distance=std::sqrt(dx*dx+dy*dy);
                if(std::isfinite(distance)&&distance>1.f)lens[5]=lens[2]+distance*std::tan(probe_camera_pitch*3.14159265358979323846f/180.f);
            }
            const double zoom=camera_zoom_index==1?.85:camera_zoom_index==2?.70:1.;
            const auto projection=camera_projection(saved_fov,saved_aspect,engine_aspect,camera_horplus_enabled,zoom);
            *fov=projection.fov;*aspect=static_cast<float>(projection.aspect);
            static bool reported=false;
            if(!reported){FILE* file=std::fopen(engine_log,"a");if(file){std::fprintf(file,"CAMERA_PROJECTION horplus=%d original_fov=%d original_aspect=%.6f output_fov=%d output_aspect=%.6f\n",int(camera_horplus_enabled),saved_fov,saved_aspect,*fov,*aspect);std::fclose(file);}reported=true;}
            const int result=original_camera(output,camera);
            ++camera_calls;last_camera_source=camera;
            if(!pitch_probe&&std::memcmp(source_position,lens,sizeof(source_position)))++camera_position_changes;
            if(saved_fov>0&&saved_fov<1980) {
                const double expected=saved_aspect/std::tan(saved_fov*3.14159265358979323846/3960.);
                const float actual=std::fabs(*reinterpret_cast<float*>(static_cast<unsigned char*>(camera)+0x138));
                const float ratio=static_cast<float>(actual/expected);
                if(std::isfinite(ratio)){camera_vertical_min=std::min(camera_vertical_min,ratio);camera_vertical_max=std::max(camera_vertical_max,ratio);}
            }
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
static void __cdecl simulation_step() {original_step();++simulation_steps;last_simulation_tick=GetTickCount64();}

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
        smooth_interpolation=false;
    }
    if(isolated_camera_probe) {
        char pitch[32];GetPrivateProfileStringA("Diagnostics","ProbeCameraPitch","999",pitch,sizeof(pitch),engine_config);
        const float value=std::strtof(pitch,nullptr);
        if(value!=probe_camera_pitch){probe_camera_pitch=value;file=std::fopen(engine_log,"a");if(file){std::fprintf(file,"CAMERA_PROBE pitch_degrees=%.3f (isolated test only)\n",value);std::fclose(file);}}
    }
    if(camera_calls) {
        file=std::fopen(engine_log,"a");if(file){std::fprintf(file,"CAMERA_LENS calls=%llu source=%p zoom_preset=%d original_position_changes=%llu vertical_scale_ratio=%.6f:%.6f\n",camera_calls,last_camera_source,camera_zoom_index,camera_position_changes,camera_vertical_min,camera_vertical_max);std::fclose(file);}
        camera_calls=camera_position_changes=0;camera_vertical_min=100;camera_vertical_max=0;
    }
    camera_zoom_index=GetPrivateProfileIntA("Graphics","CameraZoom",0,engine_config);
    if(camera_zoom_index<0||camera_zoom_index>2)camera_zoom_index=0;
    report_time=now;simulation_steps=delta_calls=0;
}
static void start_engine_hooks(const char* directory,const char* config) {
    std::snprintf(engine_log,MAX_PATH,"%sopensoccermania-engine.log",directory);
    std::snprintf(engine_config,MAX_PATH,"%s",config);
    isolated_camera_probe=GetPrivateProfileIntA("Diagnostics","IsolateInstance",0,config)!=0;
    trace_save_status=isolated_camera_probe&&GetPrivateProfileIntA("Diagnostics","TraceSaveStatus",0,config)!=0;
    camera_zoom_index=GetPrivateProfileIntA("Graphics","CameraZoom",0,config);
    if(camera_zoom_index<0||camera_zoom_index>2)camera_zoom_index=0;
    if(!GetPrivateProfileIntA("Graphics","VerifiedExecutable",0,config))return;
    auto* base=reinterpret_cast<unsigned char*>(GetModuleHandleA(nullptr));
    const bool save_worker_guard=start_save_worker_guard(base);
    const unsigned char save_status_entry[]={0xa1,0xa4,0xea,0x61,0x00,0xc3};
    const unsigned char save_status_call[]={0xe8,0xe5,0x6d,0xf3,0xff,0xb9,0x07,0x00,0x00,0x00};
    const unsigned char save_header_read[]={0x8b,0x3d,0x50,0x37,0x68,0x00};
    if(std::memcmp(base+0x30a20,save_status_entry,sizeof(save_status_entry))==0&&
       std::memcmp(base+0xf9c36,save_status_call,sizeof(save_status_call))==0&&
       std::memcmp(base+0xf9cc9,save_header_read,sizeof(save_header_read))==0)
        MH_CreateHook(base+0x30a20,reinterpret_cast<void*>(save_status),reinterpret_cast<void**>(&original_save_status));
    // Dialog push reads the singleton and tests its empty flag at +0x84.
    // Only inspect this layout in the verified executable with matching code.
    const unsigned char dialog_push[]={0xa1,0x00,0x1b,0x68,0x00,0x33,0xd2,0x56,0x39,0x90,0x84,0x00,0x00,0x00};
    dialog_state_verified=std::memcmp(base+0xe9c30,dialog_push,sizeof(dialog_push))==0;
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
    const unsigned char allocation_signature[]={0x56,0x8b,0x35,0x30,0x82,0x5e,0x00};
    const unsigned char save_signature[]={0xa1,0x08,0x82,0x5e,0x00,0x56};
    if(std::memcmp(base+0x1f7c0,allocation_signature,sizeof(allocation_signature))==0&&
       std::memcmp(base+0x1fca0,save_signature,sizeof(save_signature))==0) {
        const auto a=MH_CreateHook(base+0x1f7c0,reinterpret_cast<void*>(allocate_snapshots),reinterpret_cast<void**>(&original_snapshot_allocate));
        const auto b=MH_CreateHook(base+0x1fca0,reinterpret_cast<void*>(save_snapshots),reinterpret_cast<void**>(&original_snapshot_save));
        snapshot_history_hooks_active=a==MH_OK&&b==MH_OK;
        if(!snapshot_history_hooks_active){if(a==MH_OK)MH_RemoveHook(base+0x1f7c0);if(b==MH_OK)MH_RemoveHook(base+0x1fca0);}
    }
    if(std::memcmp(base+0x1fcd0,interpolation_signature,sizeof(interpolation_signature))==0) {
        smooth_interpolation=false; // Retired: preserve the original interpolation path.
    }
    camera_horplus_enabled=GetPrivateProfileIntA("Graphics","ExperimentalHorPlus",0,config)!=0;
    // Install independently of Hor+ so live zoom works in original-framing/4:3 modes.
    if(std::memcmp(base+0x1ae90,camera_signature,sizeof(camera_signature))==0)
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
    FILE* file=std::fopen(engine_log,"a");if(file){std::fprintf(file,"ENGINE_HOOKS camera_frustum=%d separate_render_wait=%d primed_history=%d target_fps=%.1f save_worker_idle_guard=%d\n",camera_hooks_active,timing_hooks_active,snapshot_history_hooks_active,render_limit,save_worker_guard);std::fclose(file);}
}
}
