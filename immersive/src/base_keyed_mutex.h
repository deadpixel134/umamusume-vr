/* AcquireSync returns positive WAIT_* values: SUCCEEDED is not ownership.
   A partial acquisition is returned using the SAME key (no copy occurred).
   Neither caller may copy/release a pair unless the result is BP_KM_READY. */
enum { BP_KM_BROKEN=-1, BP_KM_PENDING=0, BP_KM_READY=1 };
static int bp_keyed_pair_acquire(IDXGIKeyedMutex* left,IDXGIKeyedMutex* right,UINT64 key,HRESULT result[3]) {
 result[0]=IDXGIKeyedMutex_AcquireSync(left,key,0);
 result[1]=S_FALSE;result[2]=S_FALSE; /* not called */
 if(result[0]!=S_OK) return result[0]==WAIT_TIMEOUT?BP_KM_PENDING:BP_KM_BROKEN;
 result[1]=IDXGIKeyedMutex_AcquireSync(right,key,0);
 if(result[1]==S_OK) return BP_KM_READY;
 result[2]=IDXGIKeyedMutex_ReleaseSync(left,key);
 if(result[2]!=S_OK) return BP_KM_BROKEN;
 return result[1]==WAIT_TIMEOUT?BP_KM_PENDING:BP_KM_BROKEN;
}
