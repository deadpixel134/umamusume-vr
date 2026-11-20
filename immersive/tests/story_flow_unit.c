/* Synthetic continuations are tests, never claimed as missing game bytes. */
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
#include "story_flow_regression.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"flow check failed %d: %s\n",__LINE__,#x);return 1;}++checks;}while(0)
static LONGLONG expired_clock(void){static LONGLONG n=0;return n+=1000000000;}
static unsigned cache_reads;static bool cache_fail;
static BOOL WINAPI cache_reader(HANDLE p,LPCVOID a,LPVOID o,SIZE_T n,SIZE_T* got){++cache_reads;if(cache_fail){*got=0;SetLastError(ERROR_PARTIAL_COPY);return FALSE;}return ReadProcessMemory(p,a,o,n,got);}
static void rel32(unsigned char* code,unsigned at,unsigned target,unsigned char opcode){code[at]=opcode;int32_t delta=(int32_t)target-(int32_t)(at+5);memcpy(code+at+1,&delta,4);}
int main(void){
    unsigned checks=0;SYSTEM_INFO si;GetSystemInfo(&si);size_t page=si.dwPageSize,total=page*8;DWORD old;
    unsigned char* mem=VirtualAlloc(NULL,total,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(mem!=NULL);memset(mem,0x90,total);
    /* fallthrough beyond advisory pdata272 and old512 cutoff, forward branch,
     * one-hop call, loop, second-hop call deliberately unexpanded. */
    mem[0]=0x75;mem[1]=0x7e;rel32(mem,128,1200,0xe9);rel32(mem,600,4096,0xe8);mem[605]=0xc3;mem[1200]=0xeb;mem[1201]=0xfe;
    rel32(mem,4096,8192,0xe8);mem[4101]=0xc3;mem[8192]=0xc3;
    CHECK(VirtualProtect(mem,total,PAGE_EXECUTE_READ,&old));NativeImage im={0};im.base=(uintptr_t)mem;im.size=(DWORD)total;im.valid=true;
    uintptr_t entries[3]={(uintptr_t)mem,(uintptr_t)mem+256,(uintptr_t)mem};DWORD hints[3]={272,272,272};Flow* f=&flow_capture;
    flow_run(f,im,entries,hints,2000);
    CHECK(flow_find(f,(uintptr_t)mem+600)>=0);CHECK(flow_find(f,(uintptr_t)mem+1200)>=0);CHECK(flow_find(f,(uintptr_t)mem+4096)>=0);CHECK(flow_find(f,(uintptr_t)mem+8192)<0);
    CHECK(f->h.callees==1);CHECK(f->h.bytes<=CFG_BYTES&&f->h.blocks<=CFG_BLOCKS&&f->h.edges<=CFG_EDGES);CHECK(f->h.unresolved>0);
    bool indirect=false,depth=false;for(unsigned i=0;i<f->h.edges;++i){CHECK(f->edges[i].depth<=1);if(f->edges[i].reason==CR_DEPTH)depth=true;}CHECK(depth);
    CHECK(VirtualProtect(mem,total,PAGE_READWRITE,&old));memset(mem,0x90,total);mem[0]=0xff;mem[1]=0xe0;CHECK(VirtualProtect(mem,total,PAGE_EXECUTE_READ,&old));
    entries[0]=(uintptr_t)mem;entries[1]=entries[2]=0;flow_run(f,im,entries,hints,2000);for(unsigned i=0;i<f->h.edges;++i)if(f->edges[i].reason==CR_INDIRECT)indirect=true;CHECK(indirect);CHECK(f->h.bytes==2);
    flow_test_clock=expired_clock;entries[1]=(uintptr_t)mem+128;flow_run(f,im,entries,hints,20);CHECK(f->h.flags&1);CHECK(f->h.bytes==0);CHECK(f->blocks[1].stop==CR_TIME);CHECK(f->edges[1].reason==CR_TIME);CHECK(f->h.unresolved>=2);entries[1]=0;flow_test_clock=NULL;
    CHECK(VirtualProtect(mem,total,PAGE_READWRITE,&old));memset(mem,0x90,total);for(unsigned i=0;i<5;++i)rel32(mem,i*5,4096,0xe8);mem[25]=0xc3;mem[4096]=0xc3;CHECK(VirtualProtect(mem,total,PAGE_EXECUTE_READ,&old));
    flow_run(f,im,entries,hints,2000);CHECK(f->h.callees==0);CHECK(flow_find(f,(uintptr_t)mem+4096)<0);
    CHECK(VirtualProtect(mem,total,PAGE_READWRITE,&old));memset(mem,0x90,total);CHECK(VirtualProtect(mem,total,PAGE_EXECUTE_READ,&old));
    flow_run(f,im,entries,hints,2000);CHECK(f->h.flags&2);CHECK(f->h.bytes<=8192);
    CHECK(VirtualProtect(mem+page,page,PAGE_NOACCESS,&old));entries[0]=(uintptr_t)mem+page-8;flow_run(f,im,entries,hints,2000);CHECK(f->h.bytes==8);CHECK(f->h.unresolved>0);
    CHECK(VirtualProtect(mem+page,page,PAGE_EXECUTE_READ,&old));CHECK(native_copy((uintptr_t)mem,mem,CFG_BYTES,false).copied==0);
    /* A capture snapshot stays an owned copy, never a live permission claim.
     * The next capture revalidates and a failed window is not retried. */
    CHECK(VirtualProtect(mem,page,PAGE_READWRITE,&old));mem[0]=0xc3;CHECK(VirtualProtect(mem,page,PAGE_EXECUTE_READ,&old));
    entries[0]=(uintptr_t)mem;native_read=cache_reader;cache_reads=0;flow_run(f,im,entries,hints,2000);
    CHECK(f->h.bytes==1&&f->chunk_count==1&&f->live_bytes==512&&cache_reads==1);
    CHECK(VirtualProtect(mem,page,PAGE_READWRITE,&old));mem[0]=0x90;CHECK(VirtualProtect(mem,page,PAGE_NOACCESS,&old));
    unsigned char snapshot=0;CHECK(flow_snapshot(f,(uintptr_t)mem,&snapshot,1).copied==1&&snapshot==0xc3&&cache_reads==1);
    flow_run(f,im,entries,hints,2000);CHECK(f->h.bytes==0&&f->blocks[0].stop==CR_READ&&cache_reads==1);
    CHECK(VirtualProtect(mem,page,PAGE_READWRITE,&old));mem[0]=0xc3;CHECK(VirtualProtect(mem,page,PAGE_EXECUTE_READ,&old));
    cache_fail=true;flow_run(f,im,entries,hints,2000);CHECK(f->h.bytes==0&&f->blocks[0].stop==CR_READ&&cache_reads==2);
    CHECK(flow_snapshot(f,(uintptr_t)mem,&snapshot,1).copied==0&&cache_reads==2);
    cache_fail=false;flow_run(f,im,entries,hints,2000);CHECK(f->h.bytes==1&&cache_reads==3);native_read=ReadProcessMemory;
    f->chunk_count=CFG_CHUNKS;memset(f->chunks,0,sizeof(f->chunks));CHECK(flow_snapshot(f,(uintptr_t)mem,&snapshot,1).copied==0&&f->chunk_budget&&(f->h.flags&2));
    /* Prior bytes only: instruction lengths and direct targets independently
     * decoded by the existing pinned Iced1.21 offline dependency. */
    for(unsigned r=0;r<regression_count;++r){unsigned at=0;for(unsigned i=0;i<regression[r].count;++i){FlowStep s=flow_decode(regression[r].rva+at,regression[r].bytes+at,regression[r].size-at);CHECK(s.valid);CHECK(s.length==regression[r].lengths[i]);if(regression[r].targets[i])CHECK(s.target==regression[r].targets[i]);at+=s.length;}if(at<regression[r].size)CHECK(!flow_decode(regression[r].rva+at,regression[r].bytes+at,regression[r].size-at).valid);}
    CHECK(VirtualFree(mem,0,MEM_RELEASE));printf("flow_unit: %u checks PASS synthetic CFG/pdata-fallthrough/tail/branch/call/depth/loop/indirect/time/page/cap/snapshot-lifetime/failure-cache + real-prefix Iced regression\n",checks);return 0;
}

