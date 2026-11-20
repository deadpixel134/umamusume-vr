/* The exported-API substitute is reused; the old request suite is not run.
   MethodInfo deliberately has no native pointer/layout tail. */
#define main request_fixture_main
#include "request_dispatch_test.c"
#undef main
/* Independent expectations are the pinned engine headers, not copied codes. */
#include ".tmp/umamusume-localify/src/il2cpp/il2cpp-blob.h"
#include ".tmp/umamusume-localify/src/il2cpp/il2cpp-tabledefs.h"
#include "../src/base_producer_lookup.h"
static Il2CppClass lookup_go,lookup_string,lookup_object,lookup_component,lookup_foreign;
static Il2CppClass* lookup_types[BT_COUNT];
static Il2CppType lookup_void,lookup_wrong;
static MethodInfo tag,other,duplicate;
static const MethodInfo* stream[514];
static unsigned stream_count;
static BpLookupEvidence evidence;
static const MethodInfo* lookup_methods(Il2CppClass* owner,void** iterator) {
 if(owner!=&lookup_go) return NULL;
 uintptr_t i=(uintptr_t)*iterator;
 if(i>=stream_count) return NULL;
 *iterator=(void*)(i+1); return stream[i];
}
static const char* lookup_namespace(Il2CppClass* c) {
 return c==&lookup_go || c==&lookup_component?"UnityEngine":"System";
}
static void lookup_reset(void) {
 reset(true); memset(lookup_types,0,sizeof(lookup_types)); memset(stream,0,sizeof(stream)); stream_count=1;
 lookup_go=(Il2CppClass){.name="GameObject"}; lookup_string=(Il2CppClass){.name="String"};
 lookup_object=(Il2CppClass){.name="Object"}; lookup_component=(Il2CppClass){.name="Component"};
 lookup_void=(Il2CppType){.code=IL2CPP_TYPE_VOID};
 lookup_string.type=(Il2CppType){.code=IL2CPP_TYPE_STRING,.klass=&lookup_string};
 lookup_object.type=(Il2CppType){.code=IL2CPP_TYPE_CLASS,.klass=&lookup_object};
 lookup_types[BT_GO]=&lookup_go; lookup_types[BT_STRING]=&lookup_string; lookup_types[BT_BOOL]=&boolean;
 /* Official GameObject.bindings.cs public instance CompareTag(string):bool. */
 tag=(MethodInfo){.name="CompareTag",.owner=&lookup_go,.ret=&boolean.type,
  .params={&lookup_string.type},.argc=1,
  .flags=METHOD_ATTRIBUTE_PUBLIC|METHOD_ATTRIBUTE_HIDE_BY_SIG};
 other=tag; other.name="get_tag"; duplicate=tag; stream[0]=&tag;
 api.class_get_methods=lookup_methods; api.class_get_namespace=lookup_namespace;
}
static int lookup_case(const char* name,const MethodInfo* expected,const char* outcome) {
 /* Observing vs not observing must make the exact same production selection. */
 CHECK(bp_find(&api,lookup_types,&bp_specs[BM_COMPARE_TAG],NULL)==expected);
 CHECK(bp_find(&api,lookup_types,&bp_specs[BM_COMPARE_TAG],&evidence)==expected);
 CHECK(!strcmp(bp_lookup_outcome(&evidence),outcome));
 CHECK(!root_calls && !make_calls && !submit_calls);
 CHECK(evidence.captured<=BP_LOOKUP_CAPTURE_LIMIT);
 ++cases; printf("PASS %s\n",name); return 0;
}
int main(void) {
 cases=assertions=0;
 CHECK(IL2CPP_TYPE_STRING==0x0e && IL2CPP_TYPE_CLASS==0x12);
 CHECK(METHOD_ATTRIBUTE_PUBLIC==0x6 && METHOD_ATTRIBUTE_STATIC==0x10 && METHOD_ATTRIBUTE_ABSTRACT==0x400);
 CHECK(bp_specs[BM_COMPARE_TAG].owner==BT_GO && bp_specs[BM_COMPARE_TAG].arg[0]==BT_STRING && !bp_specs[BM_COMPARE_TAG].is_static);
 CHECK(bp_type_code(BT_STRING)==IL2CPP_TYPE_STRING && bp_type_code(BT_BOOL)==IL2CPP_TYPE_BOOLEAN);
 CHECK(bp_specs[BM_COMPARE_TAG].ret==BT_BOOL && IL2CPP_TYPE_BOOLEAN==0x02);
 CHECK(sizeof(BpLookupEvidence)<=2048 && BP_LOOKUP_CAPTURE_LIMIT==3);
 lookup_reset(); stream_count=0; CHECK(!lookup_case("complete_empty_name_absent",NULL,"name_absent"));
 CHECK(evidence.scan_completed && !evidence.scan_count && !evidence.named_count && !evidence.capped);
 lookup_reset(); stream[0]=&other; CHECK(!lookup_case("complete_nonmatching_names_absent",NULL,"name_absent"));
 CHECK(evidence.scan_count==1 && evidence.evaluated_count==1 && evidence.captured==0);
 lookup_reset(); tag.name="CompareTag_Injected"; CHECK(!lookup_case("exact_name_only_no_neighbor_dump",NULL,"name_absent"));
 lookup_reset(); CHECK(!lookup_case("official_STRING14_public_instance_exact",&tag,"exact_match"));
 CHECK(evidence.valid_matches==1 && evidence.scan_completed && evidence.named_count==1);
 CHECK(evidence.candidate[0].parameter.code==IL2CPP_TYPE_STRING && evidence.candidate[0].parameter.klass.exact);
 CHECK(evidence.candidate[0].owner.exact && evidence.candidate[0].rejection==0);
 lookup_reset(); lookup_wrong=lookup_string.type; lookup_wrong.code=IL2CPP_TYPE_CLASS; tag.params[0]=&lookup_wrong;
 CHECK(!lookup_case("STRING_class_18_is_not_STRING14",NULL,"filtered_out"));
 CHECK(evidence.candidate[0].rejection==22 && evidence.candidate[0].parameter.code==IL2CPP_TYPE_CLASS);
 CHECK(evidence.candidate[0].parameter.klass.exact);
 lookup_reset(); lookup_wrong=lookup_string.type; lookup_wrong.klass=&lookup_object; tag.params[0]=&lookup_wrong;
 CHECK(!lookup_case("argument_wrong_class_identified",NULL,"filtered_out"));
 CHECK(evidence.candidate[0].rejection==23 && !evidence.candidate[0].parameter.klass.exact);
 CHECK(!strcmp(evidence.candidate[0].parameter.klass.name,"Object"));
 lookup_reset(); lookup_foreign=lookup_string; lookup_wrong=lookup_string.type; lookup_wrong.klass=&lookup_foreign; tag.params[0]=&lookup_wrong;
 CHECK(!lookup_case("same_class_label_foreign_identity_rejected",NULL,"filtered_out"));
 CHECK(!strcmp(evidence.candidate[0].parameter.klass.name,"String") && !evidence.candidate[0].parameter.klass.exact);
 lookup_reset(); lookup_wrong=lookup_string.type; lookup_wrong.byref=true; tag.params[0]=&lookup_wrong;
 CHECK(!lookup_case("byref_argument_rejected",NULL,"filtered_out")); CHECK(evidence.candidate[0].rejection==21);
 lookup_reset(); tag.owner=&lookup_component; CHECK(!lookup_case("declaring_Component_is_not_GameObject",NULL,"filtered_out"));
 CHECK(evidence.candidate[0].rejection==1 && !strcmp(evidence.candidate[0].owner.name,"Component"));
 lookup_reset(); tag.flags=METHOD_ATTRIBUTE_PRIVATE; CHECK(!lookup_case("private_access_rejected",NULL,"filtered_out")); CHECK(evidence.candidate[0].rejection==2);
 lookup_reset(); tag.flags|=METHOD_ATTRIBUTE_STATIC; CHECK(!lookup_case("static_rejected",NULL,"filtered_out")); CHECK(evidence.candidate[0].rejection==3);
 lookup_reset(); tag.flags|=METHOD_ATTRIBUTE_ABSTRACT; CHECK(!lookup_case("abstract_rejected",NULL,"filtered_out")); CHECK(evidence.candidate[0].rejection==4);
 lookup_reset(); tag.generic=true; CHECK(!lookup_case("generic_candidate_rejected",NULL,"filtered_out")); CHECK(evidence.candidate[0].generic && evidence.candidate[0].rejection==5);
 lookup_reset(); tag.inflated=true; CHECK(!lookup_case("inflated_candidate_rejected",NULL,"filtered_out")); CHECK(evidence.candidate[0].inflated && evidence.candidate[0].rejection==6);
 lookup_reset(); tag.argc=0; CHECK(!lookup_case("arity_mismatch_no_param_dereference",NULL,"filtered_out")); CHECK(evidence.candidate[0].rejection==7 && !evidence.candidate[0].parameter.present);
 lookup_reset(); tag.ret=&lookup_string.type; CHECK(!lookup_case("wrong_return_code",NULL,"filtered_out")); CHECK(evidence.candidate[0].rejection==12);
 lookup_reset(); lookup_wrong=boolean.type; lookup_wrong.klass=&lookup_object; tag.ret=&lookup_wrong;
 CHECK(!lookup_case("BOOLEAN2_wrong_System_Boolean_class",NULL,"filtered_out")); CHECK(evidence.candidate[0].rejection==13);
 lookup_reset(); lookup_wrong=boolean.type; lookup_wrong.byref=true; tag.ret=&lookup_wrong;
 CHECK(!lookup_case("BOOLEAN2_byref_return_rejected",NULL,"filtered_out")); CHECK(evidence.candidate[0].rejection==11);
 lookup_reset(); tag.ret=NULL; CHECK(!lookup_case("null_return_metadata",NULL,"filtered_out")); CHECK(evidence.candidate[0].rejection==10 && !evidence.candidate[0].result.present);
 lookup_reset(); tag.params[0]=NULL; CHECK(!lookup_case("null_parameter_metadata",NULL,"filtered_out")); CHECK(evidence.candidate[0].rejection==20 && !evidence.candidate[0].parameter.present);
 lookup_reset(); tag.owner=NULL; CHECK(!lookup_case("null_declaring_metadata",NULL,"filtered_out")); CHECK(!evidence.candidate[0].owner.present);
 lookup_reset(); tag.name=NULL; CHECK(!lookup_case("null_name_not_proven_absent",NULL,"name_unreadable")); CHECK(evidence.invalid_names==1 && evidence.scan_completed);
 lookup_reset(); lookup_wrong=lookup_string.type; lookup_wrong.code=IL2CPP_TYPE_END; tag.params[0]=&lookup_wrong;
 CHECK(!lookup_case("invalid_type_code_class_unknown",NULL,"filtered_out")); CHECK(!evidence.candidate[0].parameter.class_observed && evidence.candidate[0].rejection==22);
 lookup_reset(); lookup_wrong=lookup_string.type; lookup_wrong.code=IL2CPP_TYPE_MVAR; tag.params[0]=&lookup_wrong;
 CHECK(!lookup_case("open_parameter_class_not_resolved_for_logging",NULL,"filtered_out")); CHECK(!evidence.candidate[0].parameter.class_observed);
 lookup_reset(); stream[1]=&duplicate; stream[2]=&other; stream_count=3;
 CHECK(!lookup_case("duplicate_preserves_early_ambiguous_stop",NULL,"ambiguous"));
 CHECK(evidence.scan_count==2 && evidence.valid_matches==2 && !evidence.scan_completed && evidence.ambiguous);
 lookup_reset(); for(unsigned i=0;i<513;++i) stream[i]=&other; stream_count=513;
 CHECK(!lookup_case("cap_named0_is_incomplete_not_absent",NULL,"incomplete"));
 CHECK(evidence.scan_count==513 && evidence.evaluated_count==512 && evidence.capped && !evidence.scan_completed);
 lookup_reset(); for(unsigned i=0;i<513;++i) stream[i]=&other; stream[512]=&tag; stream_count=513;
 CHECK(!lookup_case("candidate_after_cap_not_peeked",NULL,"incomplete")); CHECK(evidence.named_count==0);
 lookup_reset(); for(unsigned i=0;i<512;++i) stream[i]=&other; stream[511]=&tag; stream_count=512;
 CHECK(!lookup_case("exact_match_at512_complete",&tag,"exact_match")); CHECK(evidence.scan_count==512 && evidence.scan_completed);
 lookup_reset(); duplicate.flags=METHOD_ATTRIBUTE_PRIVATE; for(unsigned i=0;i<4;++i) stream[i]=&duplicate;
 stream[4]=&tag; stream_count=5; CHECK(!lookup_case("capture_limit_does_not_limit_selection",&tag,"exact_match"));
 CHECK(evidence.named_count==5 && evidence.captured==3 && evidence.omitted==2 && evidence.valid_matches==1);
 lookup_reset(); lookup_string.name=NULL; CHECK(!lookup_case("missing_diagnostic_class_text_keeps_selection",&tag,"exact_match"));
 CHECK(evidence.candidate[0].parameter.klass.exact && !evidence.candidate[0].parameter.klass.name[0]);
 lookup_reset(); lookup_foreign=lookup_string; lookup_foreign.name="A_very_long_foreign_class_name_which_exceeds_the_fixed_evidence_buffer_123456789";
 lookup_wrong=lookup_string.type; lookup_wrong.klass=&lookup_foreign; tag.params[0]=&lookup_wrong;
 CHECK(!lookup_case("diagnostic_text_truncated_selection_unchanged",NULL,"filtered_out")); CHECK(evidence.candidate[0].parameter.klass.truncated);
 printf("RESULT cases=%u assertions=%u production_bp_find=YES contract=CompareTag target_metadata=UNVERIFIED\n",cases,assertions);
 return 0;
}

