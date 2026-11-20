#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static unsigned records,bytes,assertions;
#define CHECK(x) do{++assertions;if(!(x)){fprintf(stderr,"FAIL line%d\n",__LINE__);exit(1);}}while(0)
#define telemetry_snprintf snprintf
static void write_record(const char*e,const char*event,const char*detail){CHECK(!strcmp(e,"STORY-DRAW-OWNER"));CHECK(!strcmp(event,"story_draw_owner_contract"));CHECK(strlen(detail)+384<2048);++records;bytes+=(unsigned)strlen(detail)+384;printf("{%s}\n",detail);}
#include "story_meta_buffer.inc"
int main(void){BpStoryMetaOut out={.generation=2,.scene=-99};char row[768];memset(row,'a',sizeof(row));row[0]='"';row[sizeof(row)-2]='"';row[sizeof(row)-1]=0;
 for(unsigned i=0;i<128;++i)bp_story_meta_row(&out,row);bp_story_meta_flush(&out);CHECK(records<=10 && bytes<=12000 && out.dropped>0 && out.count==0);
 unsigned before=records;char oversized[1300];memset(oversized,'b',sizeof(oversized));oversized[1299]=0;unsigned old=out.dropped;bp_story_meta_row(&out,oversized);bp_story_meta_flush(&out);CHECK(records==before && out.dropped==old+1);
 memset(&out,0,sizeof(out));bp_story_meta_row(&out,"{\"kind\":\"missing\"}");bp_story_meta_flush(&out);CHECK(out.records==1 && out.dropped==0);fprintf(stderr,"PASS actual metadata output budget %u assertions\n",assertions);return 0;}
