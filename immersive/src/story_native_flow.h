/* Bounded static traversal of copied instructions. No instruction is executed. */
#include "hde/hde64.h"
#include <bcrypt.h>
#define CFG_BYTES 49152u
#define CFG_BLOCKS 256u
#define CFG_EDGES 1024u
#define CFG_INSTRUCTIONS 8192u
#define CFG_HASH 16384u
#define CFG_CALLEES 24u
#define CFG_TIME_MS 20u
#define CFG_FILE_CAP 98304u
#define CFG_CHUNK_BYTES 512u
#define CFG_CHUNKS 96u
enum { CF_NEXT, CF_COND, CF_JUMP, CF_CALL, CF_RETURN, CF_INDIRECT_CALL, CF_INDIRECT_JUMP, CF_TRAP, CF_UNSUPPORTED };
enum { CR_QUEUED, CR_JOIN, CR_TERMINAL, CR_INDIRECT, CR_OUTSIDE, CR_DEPTH, CR_FREQUENT, CR_BUDGET, CR_DECODE, CR_READ, CR_OVERLAP, CR_TIME };
#pragma pack(push,1)
typedef struct FlowBlock { uint64_t address; uint32_t offset, bytes, instructions; uint16_t root, depth, stop, reserved; } FlowBlock;
typedef struct FlowEdge { uint64_t from, to; uint16_t kind, reason, root, depth; } FlowEdge;
typedef struct FlowHeader {
    char magic[8]; uint32_t schema, header_size, block_size, edge_size;
    uint64_t module_base, module_size, roots[3], pdata_ends[3];
    uint32_t blocks, edges, bytes, instructions, callees, unresolved, time_limit_ms, flags;
    uint64_t elapsed_us; uint32_t byte_limit, block_limit, edge_limit, instruction_limit, callee_limit, depth_limit;
} FlowHeader;
#pragma pack(pop)
typedef struct FlowInstruction { uintptr_t at; uint32_t length; } FlowInstruction;
typedef struct FlowCall { uintptr_t target; unsigned frequency, rootmask; } FlowCall;
typedef struct FlowStep { unsigned length, kind; uintptr_t target; bool valid; } FlowStep;
/* Capture-owned snapshots only. Never reuse page permissions/bytes between
 * captures or retry a failed chunk after a live protection/lifetime change. */
typedef struct FlowChunk { uintptr_t address; NativeRead read; unsigned char bytes[CFG_CHUNK_BYTES]; } FlowChunk;
typedef struct Flow {
    FlowHeader h; FlowBlock blocks[CFG_BLOCKS]; FlowEdge edges[CFG_EDGES];
    unsigned char code[CFG_BYTES]; FlowInstruction instructions[CFG_INSTRUCTIONS]; uint32_t hash[CFG_HASH];
    FlowCall calls[96]; unsigned call_count, root_bytes[3], root_blocks[3], callee_bytes[CFG_CALLEES];
    uintptr_t callee_entries[CFG_CALLEES]; unsigned block_cursor; LONGLONG start, frequency, ticks;
    FlowChunk chunks[CFG_CHUNKS]; unsigned chunk_count, snapshot_fetches, chunk_hits, live_bytes;
    bool chunk_budget;
} Flow;
static Flow flow_capture;
static unsigned char flow_file[CFG_FILE_CAP];
#ifdef SURVEY_NATIVE_TEST
static LONGLONG (*flow_test_clock)(void);
#endif
static LONGLONG flow_now(void) {
#ifdef SURVEY_NATIVE_TEST
    if (flow_test_clock) return flow_test_clock();
#endif
    LARGE_INTEGER n; QueryPerformanceCounter(&n); return n.QuadPart;
}
static bool flow_time(Flow* f) { if (flow_now() - f->start >= f->ticks) { f->h.flags |= 1; return true; } return false; }
static unsigned flow_slot(uintptr_t at) { return (unsigned)(((at >> 1) ^ (at >> 17) ^ (at >> 33)) * 2654435761u) & (CFG_HASH-1); }
static int flow_find(Flow* f, uintptr_t at) {
    unsigned slot = flow_slot(at);
    for (unsigned n=0;n<CFG_HASH;++n,slot=(slot+1)&(CFG_HASH-1)) { unsigned v=f->hash[slot]; if(!v)return -1; if(f->instructions[v-1].at==at)return (int)v-1; }
    return -1;
}
static bool flow_overlap(Flow* f, uintptr_t at, unsigned length) {
    for(unsigned back=1;back<15 && at>=back;++back){int i=flow_find(f,at-back);if(i>=0&&f->instructions[i].length>back)return true;}
    for(unsigned next=1;next<length;++next)if(flow_find(f,at+next)>=0)return true;
    return false;
}
static bool flow_inside(Flow* f, uintptr_t at) { return at>=f->h.module_base && at-f->h.module_base<f->h.module_size; }
/* Every cache miss uses the unchanged guarded, page-splitting kernel reader.
 * 512-byte aligned windows cannot cross a Windows page (>=4096 bytes).
 * Only instruction decoding reads these owned copies; they are not live state. */
static NativeRead flow_snapshot(Flow* f,uintptr_t at,unsigned char* output,size_t count) {
    NativeRead result={0,"complete",0};++f->snapshot_fetches;
    while(result.copied<count){
        uintptr_t current=at+result.copied,base=current&~(uintptr_t)(CFG_CHUNK_BYTES-1);
        if(base<f->h.module_base)base=f->h.module_base;
        FlowChunk* chunk=NULL;
        for(unsigned i=0;i<f->chunk_count;++i)if(f->chunks[i].address==base){chunk=&f->chunks[i];++f->chunk_hits;break;}
        if(!chunk){
            if(flow_time(f)){result.stop="capture_deadline";break;}
            if(f->chunk_count==CFG_CHUNKS){f->chunk_budget=true;f->h.flags|=2;result.stop="snapshot_chunk_budget";break;}
            chunk=&f->chunks[f->chunk_count++];chunk->address=base;
            size_t wanted=f->h.module_size-(base-f->h.module_base);if(wanted>CFG_CHUNK_BYTES)wanted=CFG_CHUNK_BYTES;
            chunk->read=native_copy(base,chunk->bytes,wanted,true);f->live_bytes+=(unsigned)chunk->read.copied;
        }
        size_t offset=current-base;
        if(offset>=chunk->read.copied){result.stop=chunk->read.stop;result.error=chunk->read.error;break;}
        size_t bytes=chunk->read.copied-offset;if(bytes>count-result.copied)bytes=count-result.copied;
        memcpy(output+result.copied,chunk->bytes+offset,bytes);result.copied+=bytes;
        if(result.copied<count&&strcmp(chunk->read.stop,"complete")){result.stop=chunk->read.stop;result.error=chunk->read.error;break;}
    }
    return result;
}
static void flow_edge(Flow* f,uintptr_t from,uintptr_t to,unsigned kind,unsigned reason,unsigned root,unsigned depth) {
    if(f->h.edges==CFG_EDGES){f->h.flags|=2;return;}
    f->edges[f->h.edges++]=(FlowEdge){from,to,(uint16_t)kind,(uint16_t)reason,(uint16_t)root,(uint16_t)(depth?1:0)};
    if(reason>=CR_INDIRECT)++f->h.unresolved;
}
static void flow_queue(Flow* f,uintptr_t from,uintptr_t to,unsigned kind,unsigned root,unsigned depth) {
    if(!flow_inside(f,to)){flow_edge(f,from,to,kind,CR_OUTSIDE,root,depth);return;}
    if(flow_find(f,to)>=0){flow_edge(f,from,to,kind,CR_JOIN,root,depth);return;}
    if(f->h.blocks==CFG_BLOCKS || (depth==0&&f->root_blocks[root]>=48)){flow_edge(f,from,to,kind,CR_BUDGET,root,depth);f->h.flags|=2;return;}
    for(unsigned i=f->block_cursor;i<f->h.blocks;++i)if(f->blocks[i].address==to){flow_edge(f,from,to,kind,CR_QUEUED,root,depth);return;}
    f->blocks[f->h.blocks++]=(FlowBlock){to,0,0,0,(uint16_t)root,(uint16_t)(depth?1:0),CR_QUEUED,(uint16_t)depth};
    if(!depth)++f->root_blocks[root];flow_edge(f,from,to,kind,CR_QUEUED,root,depth);
}
/* HDE64 is the existing pinned MinHook decoder. Unsupported VEX/EVEX and
 * three-byte opcode maps stop rather than guessing their lengths/semantics. */
static FlowStep flow_decode(uintptr_t at,const unsigned char* bytes,size_t available) {
    FlowStep s={0,CF_UNSUPPORTED,0,false}; unsigned char padded[32]={0};if(available>15)available=15;memcpy(padded,bytes,available);
    hde64s h; hde64_disasm(padded,&h);
    if(!h.len||h.len>15||h.len>available||(h.flags&F_ERROR)||h.opcode==0xc4||h.opcode==0xc5||h.opcode==0x62||
        (h.opcode==0x0f&&(h.opcode2==0x38||h.opcode2==0x3a)))return s;
    s.length=h.len;s.valid=true;s.kind=CF_NEXT;int64_t delta=0;bool relative=false;
    if(h.opcode==0xe8||h.opcode==0xe9){if(!(h.flags&F_IMM32)){s.valid=false;return s;}s.kind=h.opcode==0xe8?CF_CALL:CF_JUMP;delta=(int32_t)h.imm.imm32;relative=true;}
    else if(h.opcode==0xeb||(h.opcode>=0x70&&h.opcode<=0x7f)||(h.opcode>=0xe0&&h.opcode<=0xe3)){s.kind=h.opcode==0xeb?CF_JUMP:CF_COND;delta=(int8_t)h.imm.imm8;relative=true;}
    else if(h.opcode==0x0f&&h.opcode2>=0x80&&h.opcode2<=0x8f){if(!(h.flags&F_IMM32)){s.valid=false;return s;}s.kind=CF_COND;delta=(int32_t)h.imm.imm32;relative=true;}
    else if(h.opcode==0xc3||h.opcode==0xc2)s.kind=CF_RETURN;
    else if(h.opcode==0xff&&(h.modrm_reg==2||h.modrm_reg==3))s.kind=CF_INDIRECT_CALL;
    else if(h.opcode==0xff&&(h.modrm_reg==4||h.modrm_reg==5))s.kind=CF_INDIRECT_JUMP;
    else if(h.opcode==0xcc||h.opcode==0xf4||(h.opcode==0x0f&&h.opcode2==0x0b))s.kind=CF_TRAP;
    else if(h.opcode==0xcb||h.opcode==0xca||h.opcode==0xcf||h.opcode==0xcd||h.opcode==0x9a||h.opcode==0xea||((h.opcode==0xc7||h.opcode==0xc6)&&h.modrm==0xf8)||(h.opcode==0x0f&&(h.opcode2==5||h.opcode2==7||h.opcode2==0x34||h.opcode2==0x35)))s.kind=CF_UNSUPPORTED;
    if(relative){uintptr_t next=at+h.len;if(next<at||(delta<0&&next<(uint64_t)(-delta))||(delta>=0&&next>UINTPTR_MAX-(uint64_t)delta)){s.valid=false;return s;}s.target=delta<0?next-(uintptr_t)(-delta):next+(uintptr_t)delta;}
    return s;
}
static void flow_note_call(Flow* f,uintptr_t target,unsigned root) {
    for(unsigned i=0;i<f->call_count;++i)if(f->calls[i].target==target){++f->calls[i].frequency;f->calls[i].rootmask|=1u<<root;return;}
    if(f->call_count<96)f->calls[f->call_count++]=(FlowCall){target,1,1u<<root};else f->h.flags|=2;
}
static void flow_walk(Flow* f) {
    while(f->block_cursor<f->h.blocks){
        unsigned block_index=f->block_cursor++;FlowBlock* block=&f->blocks[block_index];uintptr_t at=block->address;unsigned root=block->root,depth=block->reserved;
        block->offset=f->h.bytes;
        for(;;){
            unsigned* used=depth?&f->callee_bytes[depth-1]:&f->root_bytes[root];unsigned local_cap=depth?1024u:8192u;
            if(flow_time(f)){block->stop=CR_TIME;flow_edge(f,at,0,CF_UNSUPPORTED,CR_TIME,root,depth);return;}
            if(f->h.instructions==CFG_INSTRUCTIONS||f->h.bytes>=CFG_BYTES||*used>=local_cap){block->stop=CR_BUDGET;flow_edge(f,at,0,CF_UNSUPPORTED,CR_BUDGET,root,depth);f->h.flags|=2;break;}
            if(flow_find(f,at)>=0){block->stop=CR_JOIN;flow_edge(f,at,at,CF_NEXT,CR_JOIN,root,depth);break;}
            if(!flow_inside(f,at)){block->stop=CR_OUTSIDE;flow_edge(f,at,0,CF_UNSUPPORTED,CR_OUTSIDE,root,depth);break;}
            size_t fetch=f->h.module_size-(at-f->h.module_base);if(fetch>15)fetch=15;
            unsigned char instruction[15];NativeRead r=flow_snapshot(f,at,instruction,fetch);
            FlowStep step=flow_decode(at,instruction,r.copied);
            if(!step.valid||!flow_inside(f,at)||step.length>f->h.module_size-(at-f->h.module_base)){
                block->stop=(f->h.flags&1)?CR_TIME:!strcmp(r.stop,"snapshot_chunk_budget")?CR_BUDGET:r.copied<fetch?CR_READ:CR_DECODE;flow_edge(f,at,0,CF_UNSUPPORTED,block->stop,root,depth);break;
            }
            if(flow_overlap(f,at,step.length)){block->stop=CR_OVERLAP;flow_edge(f,at,0,CF_UNSUPPORTED,CR_OVERLAP,root,depth);break;}
            if(step.length>CFG_BYTES-f->h.bytes||step.length>local_cap-*used){block->stop=CR_BUDGET;flow_edge(f,at,0,CF_UNSUPPORTED,CR_BUDGET,root,depth);f->h.flags|=2;break;}
            unsigned slot=flow_slot(at);while(f->hash[slot])slot=(slot+1)&(CFG_HASH-1);
            f->instructions[f->h.instructions]=(FlowInstruction){at,step.length};f->hash[slot]=++f->h.instructions;
            memcpy(f->code+f->h.bytes,instruction,step.length);f->h.bytes+=step.length;block->bytes+=step.length;++block->instructions;*used+=step.length;
            uintptr_t next=at+step.length;
            if(step.kind==CF_CALL){flow_edge(f,at,step.target,CF_CALL,depth?CR_DEPTH:CR_QUEUED,root,depth);if(!depth)flow_note_call(f,step.target,root);}
            else if(step.kind==CF_INDIRECT_CALL)flow_edge(f,at,0,step.kind,CR_INDIRECT,root,depth);
            else if(step.kind==CF_COND){flow_queue(f,at,step.target,step.kind,root,depth);flow_queue(f,at,next,CF_NEXT,root,depth);block->stop=CR_TERMINAL;break;}
            else if(step.kind==CF_JUMP){flow_queue(f,at,step.target,step.kind,root,depth);block->stop=CR_TERMINAL;break;}
            else if(step.kind==CF_RETURN||step.kind==CF_TRAP){flow_edge(f,at,0,step.kind,CR_TERMINAL,root,depth);block->stop=CR_TERMINAL;break;}
            else if(step.kind==CF_INDIRECT_JUMP||step.kind==CF_UNSUPPORTED){flow_edge(f,at,0,step.kind,CR_INDIRECT,root,depth);block->stop=CR_INDIRECT;break;}
            if(block->bytes>=256){flow_queue(f,at,next,CF_NEXT,root,depth);block->stop=CR_TERMINAL;break;}
            at=next;
        }
    }
}
static unsigned flow_call_priority(const FlowCall* c) { return (c->rootmask&2)?0:(c->rootmask&4)?1:2; }
static void flow_run(Flow* f,NativeImage image,const uintptr_t entries[3],DWORD hint_ends[3],unsigned milliseconds) {
    memset(f,0,sizeof(*f));memcpy(f->h.magic,"UMACFG1",8);f->h.schema=1;f->h.header_size=sizeof(FlowHeader);f->h.block_size=sizeof(FlowBlock);f->h.edge_size=sizeof(FlowEdge);
    f->h.module_base=image.base;f->h.module_size=image.size;f->h.time_limit_ms=milliseconds;
    LARGE_INTEGER q;QueryPerformanceFrequency(&q);f->frequency=q.QuadPart;f->start=flow_now();f->ticks=(LONGLONG)milliseconds*f->frequency/1000;
    f->h.byte_limit=CFG_BYTES;f->h.block_limit=CFG_BLOCKS;f->h.edge_limit=CFG_EDGES;f->h.instruction_limit=CFG_INSTRUCTIONS;f->h.callee_limit=CFG_CALLEES;f->h.depth_limit=1;
    for(unsigned i=0;i<3;++i){f->h.roots[i]=entries[i];f->h.pdata_ends[i]=hint_ends[i];if(image.valid&&entries[i])flow_queue(f,entries[i],entries[i],CF_NEXT,i,0);else flow_edge(f,entries[i],0,CF_UNSUPPORTED,CR_OUTSIDE,i,0);}
    flow_walk(f);
    /* Rare OnDestroy boundaries first, then Initialize and constructor. Frequent
     * shared call targets are explicit deferred helper candidates, not symbols. */
    for(unsigned i=0;i<f->call_count;++i)for(unsigned n=i+1;n<f->call_count;++n)if(flow_call_priority(&f->calls[n])<flow_call_priority(&f->calls[i])||(flow_call_priority(&f->calls[n])==flow_call_priority(&f->calls[i])&&f->calls[n].frequency<f->calls[i].frequency)){FlowCall c=f->calls[i];f->calls[i]=f->calls[n];f->calls[n]=c;}
    for(unsigned i=0;i<f->call_count&&!flow_time(f);++i){FlowCall* c=&f->calls[i];unsigned root=(c->rootmask&2)?1:(c->rootmask&4)?2:0;
        unsigned reason=c->frequency>=4?CR_FREQUENT:f->h.callees==CFG_CALLEES?CR_BUDGET:!flow_inside(f,c->target)?CR_OUTSIDE:CR_QUEUED;
        if(reason!=CR_QUEUED){flow_edge(f,c->target,c->target,CF_CALL,reason,root,0);continue;}
        unsigned depth=++f->h.callees;f->callee_entries[depth-1]=c->target;flow_queue(f,c->target,c->target,CF_CALL,root,depth);flow_walk(f);
    }
    if(f->block_cursor<f->h.blocks){f->h.flags|=1;for(unsigned i=f->block_cursor;i<f->h.blocks;++i)f->blocks[i].stop=CR_TIME;}
    f->h.unresolved=0;
    for(unsigned i=0;i<f->h.edges;++i){FlowEdge* e=&f->edges[i];if(e->kind==CF_CALL&&e->depth==0&&e->reason==CR_QUEUED){
        if(flow_find(f,e->to)>=0)e->reason=CR_JOIN;else{e->reason=!flow_inside(f,e->to)?CR_OUTSIDE:(f->h.flags&1)?CR_TIME:CR_BUDGET;for(unsigned c=0;c<f->call_count;++c)if(f->calls[c].target==e->to&&f->calls[c].frequency>=4)e->reason=CR_FREQUENT;}}
        if(e->reason==CR_QUEUED&&(f->h.flags&1)&&flow_find(f,e->to)<0)e->reason=CR_TIME;
        if(e->reason>=CR_INDIRECT)++f->h.unresolved;}
    f->h.elapsed_us=(uint64_t)((flow_now()-f->start)*1000000/f->frequency);
    if(f->h.elapsed_us>(uint64_t)milliseconds*1000)f->h.flags|=1;
}
static bool flow_sha(const unsigned char* bytes,size_t count,char hex[65]) {
    BCRYPT_ALG_HANDLE alg=NULL;unsigned char hash[32];bool ok=false;
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0)>=0){if(BCryptHash(alg,NULL,0,(PUCHAR)bytes,(ULONG)count,hash,sizeof(hash))>=0){for(unsigned i=0;i<32;++i)snprintf(hex+i*2,3,"%02X",hash[i]);ok=true;}BCryptCloseAlgorithmProvider(alg,0);}
    return ok;
}
static bool flow_write_new(const WCHAR* path,const void* bytes,DWORD count) {
    HANDLE file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(file==INVALID_HANDLE_VALUE)return false;
    DWORD written=0;bool ok=WriteFile(file,bytes,count,&written,NULL)&&written==count;CloseHandle(file);return ok;
}

