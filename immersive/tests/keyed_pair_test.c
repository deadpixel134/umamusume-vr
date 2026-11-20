#include <windows.h>
#include <stdio.h>
#include <stdbool.h>
typedef struct IDXGIKeyedMutex { UINT64 key; bool held; HRESULT forced,release_result; unsigned acquires,releases; } IDXGIKeyedMutex;
static HRESULT acquire(IDXGIKeyedMutex* m,UINT64 key,DWORD wait) {
 ++m->acquires;if(wait) return E_INVALIDARG;
 if(m->forced!=S_OK) return m->forced;
 if(m->held || m->key!=key) return WAIT_TIMEOUT;
 m->held=true;return S_OK;
}
static HRESULT release(IDXGIKeyedMutex* m,UINT64 key) {
 ++m->releases;if(!m->held) return E_FAIL;
 if(m->release_result!=S_OK) return m->release_result;
 m->held=false;m->key=key;return S_OK;
}
#define IDXGIKeyedMutex_AcquireSync acquire
#define IDXGIKeyedMutex_ReleaseSync release
#include "../src/base_keyed_mutex.h"
static unsigned assertions;
#define CHECK(x) do { ++assertions; if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);return 1;} } while(0)
int main(void) {
 HRESULT r[3];IDXGIKeyedMutex a={0},b={0};unsigned copies=0;
 /* Producer -> consumer -> consumer races before next producer: reproduce
    the observed second-draw boundary without copying/releasing on timeout. */
 CHECK(SUCCEEDED((HRESULT)WAIT_TIMEOUT) && SUCCEEDED((HRESULT)WAIT_ABANDONED));
 CHECK(bp_keyed_pair_acquire(&a,&b,0,r)==BP_KM_READY);++copies;
 CHECK(release(&a,1)==S_OK && release(&b,1)==S_OK);
 CHECK(bp_keyed_pair_acquire(&a,&b,1,r)==BP_KM_READY);++copies;
 CHECK(release(&a,0)==S_OK && release(&b,0)==S_OK);
 unsigned releases=a.releases+b.releases,reads=b.acquires;
 CHECK(bp_keyed_pair_acquire(&a,&b,1,r)==BP_KM_PENDING);
 CHECK(a.releases+b.releases==releases && b.acquires==reads && copies==2 && !a.held && !b.held);
 CHECK(bp_keyed_pair_acquire(&a,&b,0,r)==BP_KM_READY);
 CHECK(release(&a,1)==S_OK && release(&b,1)==S_OK);
 CHECK(bp_keyed_pair_acquire(&a,&b,1,r)==BP_KM_READY);
 for(UINT64 key=0;key<=1;++key) {
  a=(IDXGIKeyedMutex){.key=key};b=(IDXGIKeyedMutex){.key=1-key};
  CHECK(bp_keyed_pair_acquire(&a,&b,key,r)==BP_KM_PENDING);
  CHECK(!a.held && a.key==key && a.releases==1 && !b.releases);
  b.key=key;CHECK(bp_keyed_pair_acquire(&a,&b,key,r)==BP_KM_READY);
  CHECK(a.held && b.held);
 }
 HRESULT faults[]={WAIT_ABANDONED,E_FAIL,DXGI_ERROR_DEVICE_REMOVED};
 for(unsigned i=0;i<3;++i) for(unsigned eye=0;eye<2;++eye) {
  a=(IDXGIKeyedMutex){.key=1};b=(IDXGIKeyedMutex){.key=1};
  (eye?&b:&a)->forced=faults[i];
  CHECK(bp_keyed_pair_acquire(&a,&b,1,r)==BP_KM_BROKEN);
  CHECK(!a.held && !b.held && a.releases==eye && !b.releases);
 }
 a=(IDXGIKeyedMutex){.key=1,.release_result=E_FAIL};b=(IDXGIKeyedMutex){.key=0};
 CHECK(bp_keyed_pair_acquire(&a,&b,1,r)==BP_KM_BROKEN && a.held && !b.held);
 CHECK(r[0]==S_OK && r[1]==WAIT_TIMEOUT && r[2]==E_FAIL);
 printf("PASS keyed pair scripted ownership: %u assertions; no real GPU executed\n",assertions);return 0;
}
