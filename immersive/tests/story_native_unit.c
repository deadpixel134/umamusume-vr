/* Self-owned synthetic pages only. Production collector never changes protection. */
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
typedef void(*Il2CppMethodPointer)(void);
typedef struct Il2CppClass Il2CppClass;
typedef struct Il2CppType Il2CppType;
#define SURVEY_NATIVE_TEST
#include "../src/story_native_read.h"
#include "../src/story_native_flow.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"native check failed line %d: %s\n",__LINE__,#x); return 1; } ++checks; } while(0)
static BOOL WINAPI fail_read(HANDLE p,LPCVOID a,LPVOID o,SIZE_T n,SIZE_T* got) {(void)p;(void)a;(void)o;(void)n;*got=0;SetLastError(ERROR_PARTIAL_COPY);return FALSE;}
int main(void) {
    unsigned checks=0;SYSTEM_INFO si;GetSystemInfo(&si);size_t page=si.dwPageSize;
    unsigned char* mem=VirtualAlloc(NULL,page*2,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(mem!=NULL);
    memset(mem,0x90,page*2);unsigned char out[512];
    CHECK(!native_readable(PAGE_EXECUTE,true));CHECK(!native_readable(PAGE_READWRITE|PAGE_GUARD,false));CHECK(!native_readable(PAGE_NOACCESS,false));
    CHECK(native_copy(0,out,16,false).copied==0);CHECK(native_copy(UINTPTR_MAX-2,out,16,false).copied==0);CHECK(native_copy((uintptr_t)mem,out,513,false).copied==0);
    CHECK(native_copy((uintptr_t)mem,out,512,false).copied==512);CHECK(native_copy((uintptr_t)mem,out,16,true).copied==0);
    DWORD old;CHECK(VirtualProtect(mem,page,PAGE_EXECUTE_READ,&old));CHECK(VirtualProtect(mem+page,page,PAGE_NOACCESS,&old));
    NativeRead r=native_copy((uintptr_t)mem+page-16,out,32,true);CHECK(r.copied==16);CHECK(strcmp(r.stop,"not_committed_readable_page")==0);
    native_read=fail_read;r=native_copy((uintptr_t)mem,out,32,true);CHECK(r.copied==0);CHECK(r.error==ERROR_PARTIAL_COPY);native_read=ReadProcessMemory;
    CHECK(VirtualProtect(mem+page,page,PAGE_READWRITE|PAGE_GUARD,&old));CHECK(native_copy((uintptr_t)mem+page,out,16,false).copied==0);
    CHECK(VirtualProtect(mem+page,page,PAGE_READWRITE,&old));CHECK(native_copy((uintptr_t)mem+page-16,out,32,false).copied==32);
    NativeImage own=native_image(GetModuleHandleW(NULL));DWORD begin=0,end=0;
    CHECK(own.valid);CHECK(!native_image(NULL).valid);
    CHECK(native_function_span(&own,(uintptr_t)main,&begin,&end));CHECK(begin<end);
    CHECK(!native_function_span(&own,0,&begin,&end));
    CHECK(VirtualFree(mem,0,MEM_RELEASE));CHECK(native_copy((uintptr_t)mem,out,16,false).copied==0);
    printf("native_unit: %u checks PASS pages/guard/noaccess/overflow/cap/read-failure/module-header\n",checks);return 0;
}

