/* Telemetry only: no render/XR decisions may depend on these results. */
#include <stdarg.h>
#include <string.h>

#ifndef UMAVR_BUILD_ID
#define UMAVR_BUILD_ID "unidentified-build"
#endif
#define TELEMETRY_FORMAT_ERROR "\"telemetry_format_error\":true"
static int (*telemetry_vformat)(char*,size_t,const char*,va_list)=vsnprintf;

static int telemetry_snprintf(char* out, size_t cap, const char* fmt, ...) {
    va_list ap; va_start(ap, fmt);
    int n = telemetry_vformat(out, cap, fmt, ap); va_end(ap);
    if (n < 0 || (size_t)n >= cap) {
        if (cap) {
            const char fallback[] = TELEMETRY_FORMAT_ERROR;
            if (cap >= sizeof(fallback)) memcpy(out, fallback, sizeof(fallback));
            else out[0] = 0;
        }
    }
    return n;
}

/* Strict UTF-8, JSON syntax and finite JSON numbers. Bounded by LINE_LIMIT.
   This is a last line of defence; it never repairs a partial producer record. */
static BOOL telemetry_utf8(const char* s) {
    const unsigned char* p = (const unsigned char*)s;
    while (*p) {
        unsigned c = *p++;
        if (c < 0x80) continue;
        unsigned n, value, min;
        if (c >= 0xc2 && c <= 0xdf) { n=1; value=c&31; min=0x80; }
        else if (c >= 0xe0 && c <= 0xef) { n=2; value=c&15; min=0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { n=3; value=c&7; min=0x10000; }
        else return FALSE;
        for (unsigned i=0; i<n; ++i) { if ((*p & 0xc0)!=0x80) return FALSE; value=(value<<6)|(*p++&63); }
        if (value<min || value>0x10ffff || (value>=0xd800 && value<=0xdfff)) return FALSE;
    }
    return TRUE;
}
static void tj_ws(const char** p) { while (**p==' ' || **p=='\t' || **p=='\r' || **p=='\n') ++*p; }
static BOOL tj_string(const char** p) {
    if (*(*p)++!='"') return FALSE;
    while (**p && **p!='"') {
        unsigned char c=(unsigned char)*(*p)++;
        if (c<32) return FALSE;
        if (c=='\\') {
            char e=*(*p)++; if (!e) return FALSE;
            if (e=='u') { for (int i=0;i<4;++i) { char h=**p; if (!h || !strchr("0123456789abcdefABCDEF",h)) return FALSE; ++*p; } }
            else if (!strchr("\"\\/bfnrt",e)) return FALSE;
        }
    }
    if (**p!='"') return FALSE; ++*p; return TRUE;
}
static BOOL tj_value(const char** p, unsigned depth) {
    if (depth>12) return FALSE;
    tj_ws(p);
    if (**p=='"') return tj_string(p);
    if (**p=='{' || **p=='[') {
        BOOL object=**p=='{'; char close=object?'}':']'; ++*p; tj_ws(p);
        if (**p==close) { ++*p; return TRUE; }
        for (;;) {
            if (object) { if (!tj_string(p)) return FALSE; tj_ws(p); if (*(*p)++!=':') return FALSE; }
            if (!tj_value(p,depth+1)) return FALSE;
            tj_ws(p); if (**p==close) { ++*p; return TRUE; }
            if (*(*p)++!=',') return FALSE; tj_ws(p);
        }
    }
    const char* literals[]={"true","false","null"};
    for (unsigned i=0;i<3;++i) { size_t n=strlen(literals[i]); if (strncmp(*p,literals[i],n)==0) { *p+=n; return TRUE; } }
    if (**p=='-') ++*p;
    if (**p=='0') ++*p;
    else { if (**p<'1' || **p>'9') return FALSE; do { ++*p; } while (**p>='0' && **p<='9'); }
    if (**p=='.') { ++*p; if (**p<'0' || **p>'9') return FALSE; do { ++*p; } while (**p>='0' && **p<='9'); }
    if (**p=='e' || **p=='E') { ++*p; if (**p=='+' || **p=='-') ++*p; if (**p<'0' || **p>'9') return FALSE; do { ++*p; } while (**p>='0' && **p<='9'); }
    return TRUE;
}
static BOOL telemetry_json(const char* s) {
    if (!telemetry_utf8(s)) return FALSE;
    const char* p=s; if (!tj_value(&p,0)) return FALSE; tj_ws(&p); return !*p;
}

/* Invalid/truncated dynamic text becomes an explicit value, never partial UTF-8. */
static void json_escape(const char* in, char* out, size_t cap) {
    size_t o=0;
    if (!in || !telemetry_utf8(in)) in="[invalid_utf8]";
    for (size_t i=0; in[i]; ++i) {
        unsigned char c=(unsigned char)in[i];
        size_t need=c<32?6:((c=='"'||c=='\\')?2:1);
        if (o+need>=cap) { if (cap>=sizeof("[text_too_long]")) memcpy(out,"[text_too_long]",sizeof("[text_too_long]")); else if (cap) out[0]=0; return; }
        if (c=='"'||c=='\\') { out[o++]='\\'; out[o++]=(char)c; }
        else if (c<32) { const char* hex="0123456789abcdef"; out[o++]='\\'; out[o++]='u'; out[o++]='0'; out[o++]='0'; out[o++]=hex[c>>4]; out[o++]=hex[c&15]; }
        else out[o++]=(char)c;
    }
    if (cap) out[o]=0;
}

typedef struct TelemetryStats {
    ULONGLONG attempted, emitted, dropped, io_failed, format_failed, seq;
    DWORD bytes[3], records[3];
} TelemetryStats;
static TelemetryStats telemetry_stats;
static HANDLE telemetry_file=INVALID_HANDLE_VALUE, telemetry_emergency=INVALID_HANDLE_VALUE;
/* Independent 512-byte health/crash files: no shared seek pointer or latch. */
static HANDLE telemetry_health=INVALID_HANDLE_VALUE;
static volatile LONG telemetry_health_logged, telemetry_health_io_failed;
static void telemetry_primary_failure(const char* stage,DWORD error,DWORD requested,DWORD written);
static HANDLE telemetry_owner=INVALID_HANDLE_VALUE;
static BOOL telemetry_poisoned;
static DWORD telemetry_pid;
static ULONGLONG telemetry_attach;
static volatile LONG telemetry_ready;
static unsigned long long telemetry_important_keys[92];
static unsigned telemetry_key_count;
static BOOL telemetry_saturation_logged;
static BOOL telemetry_normal_quota_reached;

/* Injectable OS boundaries; production always uses the normal Win32 functions. */
static BOOL (WINAPI *telemetry_write)(HANDLE,LPCVOID,DWORD,LPDWORD,LPOVERLAPPED)=WriteFile;
static HANDLE (WINAPI *telemetry_open)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,DWORD,DWORD,HANDLE)=CreateFileW;
static BOOL (WINAPI *telemetry_size)(HANDLE,PLARGE_INTEGER)=GetFileSizeEx;

static BOOL telemetry_trim_previous(const WCHAR* path, LONGLONG limit) {
    HANDLE f=telemetry_open(path, GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,NULL);
    if (f==INVALID_HANDLE_VALUE) return GetLastError()==ERROR_FILE_NOT_FOUND;
    LARGE_INTEGER size; BOOL ok=telemetry_size(f,&size);
    if (ok && size.QuadPart>limit) { LARGE_INTEGER pos; pos.QuadPart=limit; ok=SetFilePointerEx(f,pos,NULL,FILE_BEGIN)&&SetEndOfFile(f); }
    CloseHandle(f); return ok;
}
static void telemetry_init(void) {
    WCHAR base[MAX_PATH], dir[MAX_PATH], owner[MAX_PATH], previous[MAX_PATH], side[MAX_PATH], side_previous[MAX_PATH];
    WCHAR health[MAX_PATH], health_previous[MAX_PATH];
    DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",base,MAX_PATH);
    if (!n || n>=MAX_PATH-96) return;
    swprintf(dir,MAX_PATH,L"%ls\\UmaVR",base); CreateDirectoryW(dir,NULL);
    swprintf(owner,MAX_PATH,L"%ls\\immersive-001.owner",dir);
    telemetry_owner=telemetry_open(owner,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (telemetry_owner==INVALID_HANDLE_VALUE) return; /* another process owns this route */
    swprintf(log_path,MAX_PATH,L"%ls\\immersive-001.log",dir);
    swprintf(previous,MAX_PATH,L"%ls\\immersive-001.previous.log",dir);
    swprintf(side,MAX_PATH,L"%ls\\immersive-001.emergency.log",dir);
    swprintf(side_previous,MAX_PATH,L"%ls\\immersive-001.emergency.previous.log",dir);
    swprintf(health,MAX_PATH,L"%ls\\immersive-001.health.log",dir);
    swprintf(health_previous,MAX_PATH,L"%ls\\immersive-001.health.previous.log",dir);
    if (!telemetry_trim_previous(previous,LOG_LIMIT) || !telemetry_trim_previous(side_previous,512) ||
        !telemetry_trim_previous(health_previous,512) || !telemetry_trim_previous(health,512) ||
        !telemetry_trim_previous(log_path,LOG_LIMIT) || !telemetry_trim_previous(side,512)) return;
    if (!MoveFileExW(log_path,previous,MOVEFILE_REPLACE_EXISTING) && GetLastError()!=ERROR_FILE_NOT_FOUND) return;
    if (!MoveFileExW(side,side_previous,MOVEFILE_REPLACE_EXISTING) && GetLastError()!=ERROR_FILE_NOT_FOUND) return;
    if (!MoveFileExW(health,health_previous,MOVEFILE_REPLACE_EXISTING) && GetLastError()!=ERROR_FILE_NOT_FOUND) return;
    LARGE_INTEGER attach_identity; QueryPerformanceCounter(&attach_identity);
    telemetry_pid=GetCurrentProcessId(); telemetry_attach=(ULONGLONG)attach_identity.QuadPart;
    telemetry_emergency=telemetry_open(side,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    telemetry_health=telemetry_open(health,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    telemetry_file=telemetry_open(log_path,GENERIC_WRITE|GENERIC_READ,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if (telemetry_file==INVALID_HANDLE_VALUE) {
        DWORD error=GetLastError();
        ++telemetry_stats.io_failed; telemetry_poisoned=TRUE;
        telemetry_primary_failure("primary_open",error,0,0);
    }
    if (telemetry_file!=INVALID_HANDLE_VALUE) InterlockedExchange(&telemetry_ready,1);
}

static unsigned telemetry_class(const char* event) {
    if (!strcmp(event,"session_summary") || !strcmp(event,"telemetry_summary")) return 2;
    if (!strcmp(event,"disabled") || strstr(event,"error") || strstr(event,"failed") ||
        !strcmp(event,"startup") || !strcmp(event,"ready") || !strcmp(event,"session_state_changed") ||
        !strcmp(event,"session_created") || !strcmp(event,"tracking_space_created") ||
        !strcmp(event,"source_generation_created") || !strcmp(event,"source_generation_retired") ||
        !strcmp(event,"home_touch_reaction") || !strcmp(event,"home_contact_selection") || !strcmp(event,"home_eye_post_readback") || !strcmp(event,"home_hand_query") || !strcmp(event,"home_eye_vfx_state") || !strcmp(event,"owned_context_transition") || !strcmp(event,"story_display") || !strcmp(event,"story_geometry") || !strncmp(event,"story_image_",12) ||
        !strcmp(event,"race_camera_sample") || !strcmp(event,"camera_follow_click") || !strcmp(event,"preview_left_witness") || !strcmp(event,"preview_right_witness") || !strcmp(event,"race_left_witness") || !strcmp(event,"race_right_witness") ||
        !strcmp(event,"stereo_cadence") || !strcmp(event,"live_cache_busy_reuse") || !strcmp(event,"vfx_override_apply") ||
        !strcmp(event,"presentation_fallback") || !strcmp(event,"first_stereo_frame_end_accepted") ||
        !strcmp(event,"stereo_frame_end_failed") || !strcmp(event,"telemetry_saturated")) return 1;
    return 0;
}
static unsigned long long telemetry_key(const char* event,const char* detail) {
    /* First important stage/code only. Dynamic counts/addresses are not keys. */
    unsigned long long h=1469598103934665603ULL;
    for (const char* p=event;*p;++p) h=(h^(unsigned char)*p)*1099511628211ULL;
    const char* fields[]={"\"stage\":","\"xr_result\":","\"hr\":","\"generation\":","\"state\":","\"producer_event\":","\"end_result\":","\"release_result\":"};
    for (unsigned i=0;i<8;++i) { const char* p=strstr(detail,fields[i]); if (!p) continue; p+=strlen(fields[i]);
        while (*p && *p!=',' && *p!='}') { h=(h^(unsigned char)*p)*1099511628211ULL; ++p; } }
    return h;
}
static BOOL telemetry_emit_locked(const char* evidence,const char* event,const char* detail,unsigned cls) {
    char line[LINE_LIMIT], ev[192], id[192]; SYSTEMTIME now; GetSystemTime(&now);
    char format_detail[256];
    if (strstr(detail, TELEMETRY_FORMAT_ERROR)) {
        json_escape(event, ev, sizeof(ev));
        telemetry_snprintf(format_detail,sizeof(format_detail),"\"telemetry_format_error\":true,\"producer_event\":\"%s\"",ev);
        detail=format_detail; event="telemetry_format_error"; cls=1;
        ++telemetry_stats.format_failed;
    }
    json_escape(event,ev,sizeof(ev)); json_escape(evidence,id,sizeof(id));
    ULONGLONG seq=++telemetry_stats.seq;
    int n=telemetry_snprintf(line,sizeof(line),
        "{\"schema\":2,\"probe_id\":\"%s\",\"evidence_id\":\"%s\",\"event\":\"%s\","
        "\"pid\":%lu,\"attach_id\":%llu,\"build_id\":\"%s\",\"seq\":%llu,"
        "\"timestamp_utc\":\"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\",%s}\r\n",
        PROBE_ID,id,ev,(unsigned long)telemetry_pid,telemetry_attach,UMAVR_BUILD_ID,seq,
        now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds,detail);
    if (n<=0 || n>=(int)sizeof(line) || !telemetry_json(line) || (cls==2 && n>512)) {
        ++telemetry_stats.format_failed;
        /* Bounded valid replacement also covers oversized terminal/outer records. */
        event="telemetry_format_error"; cls=1;
        n=telemetry_snprintf(line,sizeof(line),"{\"schema\":2,\"event\":\"%s\",\"producer_event\":\"%s\",\"pid\":%lu,\"attach_id\":%llu,\"build_id\":\"%s\",\"seq\":%llu,\"timestamp_utc\":\"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ\",\"telemetry_format_error\":true}\r\n",event,ev,(unsigned long)telemetry_pid,telemetry_attach,UMAVR_BUILD_ID,seq,now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds);
        if (n<=0 || n>=(int)sizeof(line)) { ++telemetry_stats.dropped; return FALSE; }
        telemetry_snprintf(format_detail,sizeof(format_detail),"\"producer_event\":\"%s\"",ev);
        detail=format_detail;
    }
    const DWORD byte_cap[]={40960,22528,2048}, record_cap[]={160,92,4};
    if (telemetry_stats.bytes[cls]+(DWORD)n>byte_cap[cls] || telemetry_stats.records[cls]>=record_cap[cls]) {
        if (cls==0) telemetry_normal_quota_reached=TRUE;
        ++telemetry_stats.dropped; return FALSE;
    }
    unsigned long long key=telemetry_key(event,detail);
    if (cls==1) { for (unsigned i=0;i<telemetry_key_count;++i) if (telemetry_important_keys[i]==key) { ++telemetry_stats.dropped; return FALSE; } }
    if (telemetry_poisoned || telemetry_file==INVALID_HANDLE_VALUE) { ++telemetry_stats.dropped; return FALSE; }
    LARGE_INTEGER size;
    if (!telemetry_size(telemetry_file,&size)) {
        DWORD error=GetLastError();
        ++telemetry_stats.io_failed; telemetry_poisoned=TRUE;
        telemetry_primary_failure("primary_size",error,(DWORD)n,0); return FALSE;
    }
    if (size.QuadPart+(LONGLONG)n>LOG_LIMIT) {
        ++telemetry_stats.io_failed; telemetry_poisoned=TRUE;
        telemetry_primary_failure("primary_size_limit",ERROR_FILE_TOO_LARGE,(DWORD)n,0); return FALSE;
    }
    DWORD done=0;
    BOOL ok=telemetry_write(telemetry_file,line,(DWORD)n,&done,NULL);
    DWORD error=ok?ERROR_SUCCESS:GetLastError();
    if (!ok || done!=(DWORD)n) {
        ++telemetry_stats.io_failed; telemetry_poisoned=TRUE;
        telemetry_primary_failure(ok?"primary_short_write":"primary_write",error,(DWORD)n,done);
        return FALSE; /* never append after a partial line */
    }
    ++telemetry_stats.emitted; ++telemetry_stats.records[cls]; telemetry_stats.bytes[cls]+=(DWORD)n;
    InterlockedIncrement(&records_written);
    if (cls==1 && telemetry_key_count<92) telemetry_important_keys[telemetry_key_count++]=key;
    return TRUE;
}
static void write_record(const char* evidence,const char* event,const char* detail) {
    if (!InterlockedCompareExchange(&telemetry_ready,0,0)) return;
    AcquireSRWLockExclusive(&log_lock);
    ++telemetry_stats.attempted;
    unsigned cls=telemetry_class(event);
    BOOL wrote=telemetry_emit_locked(evidence,event,detail,cls);
    if (!wrote && telemetry_normal_quota_reached && !telemetry_saturation_logged && !telemetry_poisoned) {
        telemetry_saturation_logged=TRUE; ++telemetry_stats.attempted;
        telemetry_emit_locked(PROBE_ID,"telemetry_saturated","\"stage\":\"normal_quota\"",1);
    }
    if (!strcmp(event,"session_summary")) {
        char stats[256];
        telemetry_snprintf(stats,sizeof(stats),"\"attempted\":%llu,\"emitted\":%llu,\"dropped\":%llu,\"io_failed\":%llu,\"format_failed\":%llu",telemetry_stats.attempted,telemetry_stats.emitted,telemetry_stats.dropped,telemetry_stats.io_failed,telemetry_stats.format_failed);
        ++telemetry_stats.attempted; telemetry_emit_locked(PROBE_ID,"telemetry_summary",stats,2);
    }
    ReleaseSRWLockExclusive(&log_lock);
}

/* No logger lock, heap, module lookup, or normal writer on the VEH path.
   The first-fault latch also prevents recursive exception logging. */
static void emergency_text(char* out,size_t cap,size_t* used,const char* s) {
    while (*s && *used+1<cap) out[(*used)++]=*s++;
}
static void emergency_uint(char* out,size_t cap,size_t* used,ULONGLONG value,unsigned radix) {
    char digits[32]; unsigned n=0;
    do { digits[n++]="0123456789abcdef"[value%radix]; value/=radix; } while (value && n<sizeof(digits));
    while (n && *used+1<cap) out[(*used)++]=digits[--n];
}
static volatile LONG telemetry_emergency_io_failed;
static void telemetry_primary_failure(const char* stage,DWORD error,DWORD requested,DWORD written) {
    if (InterlockedCompareExchange(&telemetry_health_logged,1,0)!=0 || telemetry_health==INVALID_HANDLE_VALUE) return;
    char line[512]; DWORD done=0; size_t n=0; SYSTEMTIME utc; GetSystemTime(&utc);
#define HT(s) emergency_text(line,sizeof(line),&n,s)
#define HU(v) emergency_uint(line,sizeof(line),&n,(ULONGLONG)(v),10)
    HT("{\"schema\":2,\"event\":\"telemetry_health\",\"stream\":\"health\",\"seq\":1,\"pid\":"); HU(telemetry_pid);
    HT(",\"attach_id\":"); HU(telemetry_attach); HT(",\"build_id\":\""); HT(UMAVR_BUILD_ID);
    HT("\",\"stage\":\""); HT(stage); HT("\",\"win32_error\":"); HU(error);
    HT(",\"requested\":"); HU(requested); HT(",\"written\":"); HU(written);
    HT(",\"utc\":["); HU(utc.wYear); HT(","); HU(utc.wMonth); HT(","); HU(utc.wDay); HT(",");
    HU(utc.wHour); HT(","); HU(utc.wMinute); HT(","); HU(utc.wSecond); HT(","); HU(utc.wMilliseconds);
    HT("]}\r\n"); line[n]=0;
#undef HT
#undef HU
    if (n+1<sizeof(line) && (!telemetry_write(telemetry_health,line,(DWORD)n,&done,NULL) || done!=(DWORD)n))
        InterlockedIncrement(&telemetry_health_io_failed);
}
static LONG WINAPI panel_veh(EXCEPTION_POINTERS* ep) {
    if (ep && ep->ExceptionRecord && ep->ContextRecord && ep->ExceptionRecord->ExceptionCode==0xc0000005 &&
        InterlockedCompareExchange(&fault_logged,1,0)==0 && telemetry_emergency!=INVALID_HANDLE_VALUE) {
        char line[512]; DWORD done=0; size_t n=0;
        SYSTEMTIME utc; GetSystemTime(&utc);
#define ET(s) emergency_text(line,sizeof(line),&n,s)
#define EU(v) emergency_uint(line,sizeof(line),&n,(ULONGLONG)(v),10)
        ET("{\"schema\":2,\"event\":\"emergency_fault\",\"stream\":\"emergency\",\"seq\":1,\"pid\":"); EU(telemetry_pid);
        ET(",\"attach_id\":"); EU(telemetry_attach); ET(",\"build_id\":\""); ET(UMAVR_BUILD_ID);
        /* Structured UTC avoids CRT formatting/locale locks on the exception path. */
        ET("\",\"utc\":["); EU(utc.wYear); ET(","); EU(utc.wMonth); ET(","); EU(utc.wDay); ET(",");
        EU(utc.wHour); ET(","); EU(utc.wMinute); ET(","); EU(utc.wSecond); ET(","); EU(utc.wMilliseconds);
        ET("],\"code\":"); EU(ep->ExceptionRecord->ExceptionCode); ET(",\"ip\":\"0x");
        emergency_uint(line,sizeof(line),&n,ep->ContextRecord->Rip,16);
        ET("\",\"access\":"); EU(ep->ExceptionRecord->NumberParameters>0?ep->ExceptionRecord->ExceptionInformation[0]:0);
        ET(",\"fault_addr\":\"0x"); emergency_uint(line,sizeof(line),&n,ep->ExceptionRecord->NumberParameters>1?ep->ExceptionRecord->ExceptionInformation[1]:0,16);
        ET("\"}\r\n"); line[n]=0;
#undef ET
#undef EU
        if (n+1<sizeof(line) && (!telemetry_write(telemetry_emergency,line,(DWORD)n,&done,NULL) || done!=(DWORD)n))
            InterlockedIncrement(&telemetry_emergency_io_failed);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}



