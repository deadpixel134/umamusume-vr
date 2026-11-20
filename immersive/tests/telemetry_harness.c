#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#define PROBE_ID "IMMERSIVE-001"
#define UMAVR_BUILD_ID "test-source-identity"
#define LOG_LIMIT 65536
#define LINE_LIMIT 2048
static SRWLOCK log_lock=SRWLOCK_INIT;
static WCHAR log_path[MAX_PATH];
static volatile LONG records_written, fault_logged;
static ULONGLONG attach_tick;
#include "../src/telemetry.h"
#include "../src/telemetry_frame.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)

static unsigned injection;
static unsigned health_injection;
static BOOL recursive_fault;
static BOOL race_writes;
static HANDLE health_write_entered, fault_write_entered;
static void synthetic_fault(void) {
    EXCEPTION_RECORD er; CONTEXT ctx; EXCEPTION_POINTERS ep={&er,&ctx};
    ZeroMemory(&er,sizeof(er)); ZeroMemory(&ctx,sizeof(ctx));
    er.ExceptionCode=0xc0000005; er.NumberParameters=2; er.ExceptionInformation[1]=0x1234;
    ctx.Rip=0x5678;
    (void)panel_veh(&ep); /* Direct call; never raise a real render/process fault. */
}
static BOOL WINAPI injected_write(HANDLE f,LPCVOID p,DWORD n,LPDWORD done,LPOVERLAPPED ov) {
    if (recursive_fault) synthetic_fault();
    /* Force overlap of the independent health/crash writes, rather than relying
       on thread-start timing to exercise their file-pointer/latch separation. */
    if (race_writes && (f==telemetry_health || f==telemetry_emergency)) {
        SetEvent(f==telemetry_health?health_write_entered:fault_write_entered);
        if (WaitForSingleObject(f==telemetry_health?fault_write_entered:health_write_entered,5000)!=WAIT_OBJECT_0) {
            *done=0; SetLastError(ERROR_TIMEOUT); return FALSE;
        }
    }
    if (f==telemetry_health && health_injection==1) { *done=0; SetLastError(ERROR_DISK_FULL); return FALSE; }
    if (f==telemetry_health && health_injection==2) return WriteFile(f,p,n/2,done,ov);
    if (f==telemetry_emergency && injection==3) { *done=0; SetLastError(ERROR_DISK_FULL); return FALSE; }
    if (f==telemetry_emergency && injection==4) return WriteFile(f,p,n/2,done,ov);
    if (f==telemetry_file && injection==1) { *done=0; SetLastError(ERROR_DISK_FULL); return FALSE; }
    if (f==telemetry_file && injection==2) return WriteFile(f,p,n/2,done,ov);
    return WriteFile(f,p,n,done,ov);
}
static BOOL WINAPI injected_size(HANDLE f,PLARGE_INTEGER n) { (void)f; (void)n; SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
static HANDLE WINAPI injected_open(LPCWSTR p,DWORD a,DWORD s,LPSECURITY_ATTRIBUTES sa,DWORD d,DWORD flags,HANDLE t) {
    (void)p;(void)a;(void)s;(void)sa;(void)d;(void)flags;(void)t;
    SetLastError(ERROR_ACCESS_DENIED); return INVALID_HANDLE_VALUE;
}
static HANDLE WINAPI primary_open_failure(LPCWSTR p,DWORD a,DWORD s,LPSECURITY_ATTRIBUTES sa,DWORD d,DWORD flags,HANDLE t) {
    if (d==CREATE_ALWAYS && wcsstr(p,L"immersive-001.log")) { SetLastError(ERROR_ACCESS_DENIED); return INVALID_HANDLE_VALUE; }
    return CreateFileW(p,a,s,sa,d,flags,t);
}
static int negative_format(char* p,size_t n,const char* fmt,va_list ap) { (void)p;(void)n;(void)fmt;(void)ap; return -1; }
static DWORD WINAPI log_thread(LPVOID arg) {
    for (unsigned i=0;i<1000;++i) { char d[96]; telemetry_snprintf(d,sizeof(d),"\"thread\":%llu,\"iteration\":%u",(unsigned long long)(uintptr_t)arg,i); write_record(PROBE_ID,"normal",d); }
    return 0;
}
static HANDLE race_start;
static DWORD WINAPI health_race_thread(LPVOID unused) {
    (void)unused; WaitForSingleObject(race_start,10000);
    write_record(PROBE_ID,"normal","\"ok\":true"); return 0;
}
static DWORD WINAPI fault_race_thread(LPVOID unused) {
    (void)unused; WaitForSingleObject(race_start,10000); synthetic_fault(); return 0;
}
static int frame_tests(void) {
    StereoCacheTelemetry cache={0}; StereoTelemetry f={0}; char d[768];
    stereo_observe_cache(&cache,7,101); stereo_observe_draw(&f,&cache,2,FALSE);
    CHECK(f.generation==7 && f.pair_serial==101 && f.eye_mask==3);
    /* A newer producer serial is intentionally not copied: consumer reuses old cache. */
    long long latest_published=102;
    stereo_observe_draw(&f,&cache,3,TRUE);
    CHECK(f.pair_serial!=latest_published && f.pair_serial==101 && f.cache_reused);
    f.frame_id=88; f.predicted_time=987654321; f.layer_count=1;
    CHECK(!stereo_end_accepted(&f,0));
    f.stereo_layer=TRUE; f.release_called=TRUE; f.release_result=0;
    CHECK(!stereo_end_accepted(&f,0)); /* draw before end */
    f.end_called=TRUE; CHECK(stereo_end_accepted(&f,0));
    CHECK(!stereo_end_accepted(&f,-2)); CHECK(!stereo_end_accepted(&f,9));
    f.release_result=-1; CHECK(!stereo_end_accepted(&f,0)); f.release_result=0;
    f.stereo_layer=FALSE; CHECK(!stereo_end_accepted(&f,0)); f.stereo_layer=TRUE;
    f.eye_mask=1; CHECK(!stereo_end_accepted(&f,0)); f.eye_mask=3;
    stereo_telemetry_detail(&f,-2,d,sizeof(d));
    write_record(PROBE_ID,"stereo_frame_end_failed",d);
    ++f.frame_id;
    CHECK(stereo_end_accepted(&f,0)); stereo_telemetry_detail(&f,0,d,sizeof(d));
    write_record(PROBE_ID,"first_stereo_frame_end_accepted",d);
    return 0;
}
int main(int argc,char** argv) {
    if (argc!=2) return 2;
    attach_tick=GetTickCount64();
    telemetry_write=injected_write;
    if (!strcmp(argv[1],"open_failure")) {
        telemetry_open=injected_open; telemetry_init();
        CHECK(!telemetry_ready); write_record(PROBE_ID,"normal","\"ok\":true"); CHECK(!records_written); return 0;
    }
    if (!strcmp(argv[1],"primary_open_failure")) {
        telemetry_open=primary_open_failure; telemetry_init(); CHECK(!telemetry_ready);
        CHECK(telemetry_poisoned && telemetry_health_logged==1 && !fault_logged && telemetry_stats.io_failed==1);
        write_record(PROBE_ID,"disabled","\"stage\":\"after_failure\"");
        synthetic_fault(); CHECK(fault_logged==1); return 0;
    }
    telemetry_init();
    if (!strcmp(argv[1],"conflict_child")) { CHECK(!telemetry_ready); return 0; }
    CHECK(telemetry_ready);
    telemetry_write=injected_write;
    if (!strcmp(argv[1],"write_failure") || !strcmp(argv[1],"short_write") || !strcmp(argv[1],"size_failure")) {
        injection=!strcmp(argv[1],"write_failure")?1:2;
        if (!strcmp(argv[1],"size_failure")) telemetry_size=injected_size;
        write_record(PROBE_ID,"normal","\"ok\":true");
        CHECK(!records_written && !telemetry_stats.emitted && telemetry_stats.io_failed==1 && telemetry_poisoned);
        LARGE_INTEGER before,after; CHECK(GetFileSizeEx(telemetry_file,&before));
        write_record(PROBE_ID,"disabled","\"stage\":\"after_failure\"");
        CHECK(GetFileSizeEx(telemetry_file,&after) && before.QuadPart==after.QuadPart);
        CHECK(telemetry_stats.io_failed==1 && telemetry_stats.dropped==1 && telemetry_health_logged==1 && !fault_logged);
        synthetic_fault(); CHECK(fault_logged==1); return 0;
    }
    if (!strncmp(argv[1],"health_",7) || !strcmp(argv[1],"veh_then_health")) {
        injection=1;
        if (!strcmp(argv[1],"health_write_failure")) health_injection=1;
        if (!strcmp(argv[1],"health_short_write")) health_injection=2;
        if (!strcmp(argv[1],"health_veh_recursive")) recursive_fault=TRUE;
        if (!strcmp(argv[1],"veh_then_health")) synthetic_fault();
        if (!strcmp(argv[1],"health_veh_concurrent")) {
            race_start=CreateEventW(NULL,TRUE,FALSE,NULL); CHECK(race_start);
            health_write_entered=CreateEventW(NULL,TRUE,FALSE,NULL);
            fault_write_entered=CreateEventW(NULL,TRUE,FALSE,NULL);
            CHECK(health_write_entered && fault_write_entered); race_writes=TRUE;
            HANDLE threads[2]={CreateThread(NULL,0,health_race_thread,NULL,0,NULL),CreateThread(NULL,0,fault_race_thread,NULL,0,NULL)};
            CHECK(threads[0] && threads[1] && SetEvent(race_start));
            CHECK(WaitForMultipleObjects(2,threads,TRUE,10000)==WAIT_OBJECT_0);
            CloseHandle(threads[0]); CloseHandle(threads[1]); CloseHandle(race_start);
            race_writes=FALSE; CloseHandle(health_write_entered); CloseHandle(fault_write_entered);
        } else write_record(PROBE_ID,"normal","\"ok\":true");
        synthetic_fault();
        CHECK(telemetry_health_logged==1 && fault_logged==1 && telemetry_stats.io_failed==1 && !records_written);
        CHECK(telemetry_health_io_failed==(health_injection?1:0));
        LARGE_INTEGER primary_before,primary_after,health_before,health_after,crash_before,crash_after;
        CHECK(GetFileSizeEx(telemetry_file,&primary_before) && GetFileSizeEx(telemetry_health,&health_before) && GetFileSizeEx(telemetry_emergency,&crash_before));
        write_record(PROBE_ID,"disabled","\"stage\":\"after_failure\""); synthetic_fault();
        CHECK(GetFileSizeEx(telemetry_file,&primary_after) && primary_before.QuadPart==primary_after.QuadPart);
        CHECK(GetFileSizeEx(telemetry_health,&health_after) && health_before.QuadPart==health_after.QuadPart);
        CHECK(GetFileSizeEx(telemetry_emergency,&crash_after) && crash_before.QuadPart==crash_after.QuadPart);
        CHECK(health_after.QuadPart+crash_after.QuadPart<=1024); return 0;
    }
    if (!strcmp(argv[1],"veh")) {
        recursive_fault=TRUE;
        /* Primary WriteFile is called while log_lock is held. The synthetic VEH
           reenters injected_write and itself; the first-fault latch stops recursion. */
        write_record(PROBE_ID,"normal","\"ok\":true");
        CHECK(records_written==1 && fault_logged==1);
        synthetic_fault(); CHECK(telemetry_stats.emitted==1); return 0;
    }
    if (!strcmp(argv[1],"veh_write_failure") || !strcmp(argv[1],"veh_short_write")) {
        injection=!strcmp(argv[1],"veh_write_failure")?3:4;
        synthetic_fault(); CHECK(fault_logged==1 && telemetry_emergency_io_failed==1);
        LARGE_INTEGER before,after; CHECK(GetFileSizeEx(telemetry_emergency,&before));
        synthetic_fault(); CHECK(GetFileSizeEx(telemetry_emergency,&after) && before.QuadPart==after.QuadPart);
        CHECK(telemetry_emergency_io_failed==1); return 0;
    }
    if (!strcmp(argv[1],"concurrent")) {
        HANDLE threads[8];
        for (unsigned i=0;i<8;++i) { threads[i]=CreateThread(NULL,0,log_thread,(LPVOID)(uintptr_t)i,0,NULL); CHECK(threads[i]); }
        CHECK(WaitForMultipleObjects(8,threads,TRUE,10000)==WAIT_OBJECT_0);
        for (unsigned i=0;i<8;++i) CloseHandle(threads[i]);
        CHECK(telemetry_stats.attempted==8001 && telemetry_stats.records[0]==160 && telemetry_stats.records[1]==1);
    } else if (!strcmp(argv[1],"conflict")) {
        WCHAR exe[MAX_PATH], cmd[MAX_PATH+64]; GetModuleFileNameW(NULL,exe,MAX_PATH);
        swprintf(cmd,MAX_PATH+64,L"\"%ls\" conflict_child",exe);
        STARTUPINFOW si; PROCESS_INFORMATION pi; ZeroMemory(&si,sizeof(si)); si.cb=sizeof(si);
        CHECK(CreateProcessW(exe,cmd,NULL,NULL,FALSE,CREATE_NO_WINDOW,NULL,NULL,&si,&pi));
        CHECK(WaitForSingleObject(pi.hProcess,10000)==WAIT_OBJECT_0);
        DWORD code; CHECK(GetExitCodeProcess(pi.hProcess,&code) && code==0); CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
        write_record(PROBE_ID,"owner_survived","\"ok\":true");
    } else if (!strcmp(argv[1],"quota") || !strcmp(argv[1],"byte_quota")) {
        char d[1800]; const char* simple="\"ok\":true";
        memset(d,'x',sizeof(d)); memcpy(d,"\"text\":\"",8); d[sizeof(d)-2]='"'; d[sizeof(d)-1]=0;
        for (unsigned i=0;i<1000;++i) write_record(PROBE_ID,"normal",!strcmp(argv[1],"quota")?simple:d);
        if (!strcmp(argv[1],"byte_quota")) CHECK(telemetry_stats.bytes[0]>39000 && telemetry_stats.records[0]<160 && !telemetry_stats.format_failed);
        for (unsigned i=0;i<150;++i) { char error[96]; telemetry_snprintf(error,sizeof(error),"\"stage\":\"error_%u\",\"hr\":-1",i); write_record(PROBE_ID,"disabled",error); }
        CHECK(telemetry_stats.records[0]<=160 && telemetry_stats.records[1]<=92);
        CHECK(telemetry_stats.dropped>0);
    } else if (!strcmp(argv[1],"format")) {
        char tiny[32], huge[4096], escaped[128], d[1024];
        CHECK(telemetry_snprintf(tiny,sizeof(tiny),"%0400d",1)>=400);
        CHECK(!strcmp(tiny,TELEMETRY_FORMAT_ERROR)); write_record(PROBE_ID,"producer_overflow",tiny);
        telemetry_vformat=negative_format; CHECK(telemetry_snprintf(tiny,sizeof(tiny),"%d",1)<0);
        telemetry_vformat=vsnprintf; CHECK(!strcmp(tiny,TELEMETRY_FORMAT_ERROR));
        char exact[64]; CHECK(telemetry_snprintf(exact,sizeof(exact),"%063d",1)==63 && strlen(exact)==63);
        CHECK(telemetry_snprintf(exact,sizeof(exact),"%064d",1)==64 && !strcmp(exact,TELEMETRY_FORMAT_ERROR));
        json_escape("quote\"slash\\\n\xED\x95\x9C\xEA\xB8\x80",escaped,sizeof(escaped));
        telemetry_snprintf(d,sizeof(d),"\"text\":\"%s\"",escaped); write_record(PROBE_ID,"unicode",d);
        CHECK(telemetry_utf8(escaped));
        json_escape("\xF0\x9F\x92",escaped,sizeof(escaped)); CHECK(!strcmp(escaped,"[invalid_utf8]"));
        json_escape("\xED\x95\x9C\xEA\xB8\x80",tiny,5); CHECK(!tiny[0]);
        CHECK(!telemetry_json("{\"x\":nan}") && !telemetry_json("{\"x\":1,}") && !telemetry_json("{\"x\":01}"));
        memset(huge,'x',sizeof(huge)-1); huge[sizeof(huge)-1]=0;
        write_record(PROBE_ID,"outer_overflow",huge);
        write_record(PROBE_ID,"malformed","\"broken\":\"unfinished");
        /* The formerly truncated producer, using its actual longest contract values. */
        telemetry_snprintf(d,sizeof(d),"\"status\":\"created\",\"generation\":1,\"scene_handle\":42,\"context\":\"Home\",\"owner_contract\":\"LowResolutionCamera+HomeCameraController+GallopImageEffectOnRenderImage._cachedCamera\",\"width\":3840,\"height\":2160,\"flat_width\":3840,\"flat_height\":2160,\"optics\":\"openxr_fov_ipd\",\"visual_pipeline\":\"engine_frame_center_left_right_post_effect\",\"pose_policy\":\"current_authored_yaw_pitch_plus_physical_hmd\",\"tracking_origin\":\"first_immersive_entry\",\"original_camera_restored\":true");
        CHECK(strlen(d)>384); write_record("IMMERSIVE-SOURCE","source_generation_created",d);
        CHECK(telemetry_stats.format_failed==3);
        CHECK(frame_tests()==0);
        ULONGLONG prior=telemetry_stats.emitted;
        write_record(PROBE_ID,"disabled","\"stage\":\"same_stage\",\"hr\":-1,\"count\":1");
        write_record(PROBE_ID,"disabled","\"stage\":\"same_stage\",\"hr\":-1,\"count\":999");
        CHECK(telemetry_stats.emitted==prior+1);
        write_record(PROBE_ID,"disabled","\"stage\":\"same_stage\",\"hr\":-2");
        CHECK(telemetry_stats.emitted==prior+2);
    } else if (strcmp(argv[1],"rotate")) return 2;
    write_record(PROBE_ID,"session_summary","\"status\":\"summary\",\"stereo_end_accepted\":0");
    CHECK(telemetry_stats.records[2]==2);
    if (!strcmp(argv[1],"quota") || !strcmp(argv[1],"byte_quota")) {
        write_record(PROBE_ID,"session_summary","\"status\":\"summary\"");
        CHECK(telemetry_stats.records[2]==4);
        write_record(PROBE_ID,"session_summary","\"status\":\"summary\"");
        CHECK(telemetry_stats.records[2]==4);
    }
    CHECK(telemetry_stats.emitted==(ULONGLONG)records_written);
    CHECK(telemetry_stats.bytes[0]+telemetry_stats.bytes[1]+telemetry_stats.bytes[2]<=65536);
    return 0;
}
