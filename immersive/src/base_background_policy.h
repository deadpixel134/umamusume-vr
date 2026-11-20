/* Unity Color is sequential r,g,b,a float32, passed by boxed runtime invoke. */
typedef struct BpColor { float r,g,b,a; } BpColor;
_Static_assert(sizeof(BpColor)==16,"Unity Color RGBA layout");
typedef struct BpBackground {
 BpColor color; int32_t source_flags,own_flags; unsigned configured,observed;
 bool captured;
} BpBackground;
static bool bp_background_policy(int32_t flags,BpColor color,int32_t* own) {
 if(!own || flags<1 || flags>4 || !isfinite(color.r) || !isfinite(color.g) ||
  !isfinite(color.b) || !isfinite(color.a)) return false;
 /* Depth/Nothing depend on another camera's color. Own RT has no base. */
 *own=flags<=2?flags:2;return true;
}
static bool bp_background_equal(BpColor a,BpColor b) {
 return isfinite(b.r) && isfinite(b.g) && isfinite(b.b) && isfinite(b.a) &&
  fabsf(a.r-b.r)<=0.0001f && fabsf(a.g-b.g)<=0.0001f &&
  fabsf(a.b-b.b)<=0.0001f && fabsf(a.a-b.a)<=0.0001f;
}
