/* Execute bounded transition evidence and first-generation export policy. */
#include <windows.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "../src/base_producer_state.h"
#include "../src/base_source_capability.h"
#include "../src/base_transport.h"
static struct {BpControl c;BpSourceKind source_kind;} bp;
static BpTransport bp_transport;
static unsigned records,bytes,checks;
static int telemetry_snprintf(char* out,size_t cap,const char* fmt,...) {
 va_list args;va_start(args,fmt);int n=vsnprintf(out,cap,fmt,args);va_end(args);return n;
}
static void write_record(const char* evidence,const char* event,const char* detail) {
 if(strcmp(evidence,"OWN-BASE") || strcmp(event,"owned_context_transition")) abort();
 ++records;bytes+=(unsigned)strlen(detail)+384u;printf("{%s}\n",detail);
}
#include "context_budget.inc"
#include "witness_export.inc"
#define CHECK(x) do {++checks;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);return 1;}} while(0)
int main(void) {
 CHECK(bp_story_component_set(15,0,true,true,true,true));
 CHECK(bp_transport_live_current(7,7,true));CHECK(!bp_transport_blocked(&bp_transport));
 CHECK(bp_transport_begin(&bp_transport,BP_TRANSPORT_LIVE,7,9));
 CHECK(bp_transport_matches(&bp_transport,BP_TRANSPORT_LIVE,7,9));
 CHECK(bp_transport_reclaimable(&bp_transport,0,false,7));CHECK(bp_transport_ack(&bp_transport,BP_TRANSPORT_LIVE,7,9));
 bp.c.identity=(BpIdentity){1,2,1,1,0,3,4};bp.source_kind=BP_SOURCE_HOME;
 for(unsigned i=0;i<100;++i) {
  bp.c.identity.generation=i+1;
  bp.source_kind=i%2?BP_SOURCE_STORY:BP_SOURCE_HOME;
  bp_context_event(i%2?"retired_clean":"first_fresh_pair","bounded_process_transition");
 }
 CHECK(records>0 && records<=16 && bytes<=8192);
 CHECK(bp_context_records==records && bp_context_bytes==bytes && bp_context_attempted==100);
 CHECK(bp_context_dropped==100-records);
 unsigned before=records;
 bp.c.identity.generation=1000;bp_context_event_at("selection","eligible_owner",9,BP_SOURCE_STORY,88);
 CHECK(records==before && bp_context_attempted==101 && bp_context_dropped==101-records);
 CHECK(fixture_export(1));CHECK(fixture_export(1));CHECK(!fixture_export(1));
 for(unsigned g=2;g<=100;++g) CHECK(!fixture_export(g));
 CHECK(bp_witness_export_generation==1 && bp_witness_export_attempts==2);
 bp_transport_quarantine(&bp_transport);CHECK(bp_transport_blocked(&bp_transport));
 printf("PASS actual process lifecycle/export budgets: %u assertions records=%u bytes=%u; no disk/GPU/Unity executed\n",checks,records,bytes);return 0;
}
