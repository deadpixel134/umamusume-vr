/* Explicit supported owner shapes, shared with focused selection fixtures.
   Home keeps its existing collector. Story requires one exact component of
   each identity on the same GO; no background-type or scene-name-only gate. */
typedef enum BpSourceKind { BP_SOURCE_NONE, BP_SOURCE_HOME, BP_SOURCE_STORY, BP_SOURCE_RACE, BP_SOURCE_DRESS } BpSourceKind;
enum { BP_STORY_CAMERA=1, BP_STORY_EFFECT=2, BP_STORY_DATA=4, BP_STORY_URP=8,
 BP_STORY_REQUIRED=15 };
static bool bp_story_component_set(unsigned mask,unsigned duplicate,bool same_go,bool camera_same,
 bool complete,bool relationships) {
 return mask==BP_STORY_REQUIRED && !duplicate && same_go && camera_same && complete && relationships;
}
