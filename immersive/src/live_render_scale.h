/* Unity main thread owns the scratch targets. XR owns the requested percentage.
   The transport pair and the XR projection keep their session dimensions. */
typedef struct ScaleTarget { uint32_t root; LONG percent; } ScaleTarget;
static ScaleTarget scale_targets[2];
static unsigned scale_slot;
static volatile LONG scale_requested, scale_native, scale_max_width, scale_max_height;
static volatile LONG scale_recommended_w, scale_recommended_h;
static volatile LONG64 scale_frame_stamp;
static LONG rendered_percent;
static const MethodInfo *scale_blit, *scale_get_active, *scale_set_active;

static void scale_bind(Il2CppClass* graphics, Il2CppClass* texture, Il2CppClass* rt) {
    RdApi* a = &request_dispatch_api;
    if (!graphics || !texture || !rt || !a->class_get_methods || !a->method_get_class ||
        !a->method_get_name || !a->method_get_param_count || !a->method_get_flags ||
        !a->method_get_return_type || !a->method_get_param || !a->type_get_type ||
        !a->class_from_type || !a->runtime_invoke || !a->gchandle_new ||
        !a->gchandle_free || !a->gchandle_get_target) return;
    for (unsigned group=0;group<2;++group) {
        Il2CppClass* owner=group?rt:graphics;
        void* it=NULL;
        const MethodInfo* method;
        for (unsigned i=0;i<512 && (method=a->class_get_methods(owner,&it));++i) {
            uint32_t impl=0;
            const char* name=a->method_get_name(method);
            if (!name || a->method_get_class(method)!=owner || !(a->method_get_flags(method,&impl)&0x10)) continue;
            unsigned argc=a->method_get_param_count(method);
            const Il2CppType* ret=a->method_get_return_type(method);
            if (!ret) continue;
            if (!group && !strcmp(name,"Blit") && argc==2 && a->type_get_type(ret)==1 &&
                a->class_from_type(a->method_get_param(method,0))==texture &&
                a->class_from_type(a->method_get_param(method,1))==rt) scale_blit=method;
            if (group && !strcmp(name,"get_active") && !argc && a->class_from_type(ret)==rt) scale_get_active=method;
            if (group && !strcmp(name,"set_active") && argc==1 && a->type_get_type(ret)==1 &&
                a->class_from_type(a->method_get_param(method,0))==rt) scale_set_active=method;
        }
    }
}
static void scale_free(ScaleTarget* target) {
    if (!target->root) return;
    Il2CppObject* o=request_dispatch_api.gchandle_get_target(target->root);
    if (o) ((destroy_object_fn)mi_object_destroy->methodPointer)(o,mi_object_destroy);
    request_dispatch_api.gchandle_free(target->root);
    *target=(ScaleTarget){0};
}
static Il2CppObject* scale_target(LONG percent) {
    if (percent==scale_native || percent<50 || percent>150 || !scale_blit ||
        !scale_get_active || !scale_set_active || !mi_object_destroy) return NULL;
    for(unsigned i=0;i<2;++i) if(scale_targets[i].root && scale_targets[i].percent==percent) {
        scale_slot=i;
        return request_dispatch_api.gchandle_get_target(scale_targets[i].root);
    }
    LONG w=(scale_recommended_w*percent+50)/100,h=(scale_recommended_h*percent+50)/100;
    if(w<=0 || h<=0 || w>scale_max_width || h>scale_max_height || w>8192 || h>8192) return NULL;
    Il2CppObject* o=il2cpp_object_new(rendertexture_class);
    if(!o) return NULL;
    ScaleTarget next={request_dispatch_api.gchandle_new(o,false),percent};
    if(!next.root) return NULL;
    ((rt_ctor_fn)mi_rt_ctor->methodPointer)(o,w,h,24,0,mi_rt_ctor);
    if(!((rt_create_fn)mi_rt_create->methodPointer)(o,mi_rt_create)) {scale_free(&next);return NULL;}
    scale_slot ^= 1;
    scale_free(&scale_targets[scale_slot]);
    scale_targets[scale_slot]=next;
    return o;
}
static BOOL scale_copy(Il2CppObject* from,Il2CppObject* to) {
    RdApi* a=&request_dispatch_api;
    Il2CppException* error=NULL;
    Il2CppObject* prior=a->runtime_invoke(scale_get_active,NULL,NULL,&error);
    if(error) return FALSE;
    void* args[]={from,to};
    a->runtime_invoke(scale_blit,NULL,args,&error);
    BOOL copied=error==NULL;
    error=NULL;
    void* restore[]={prior};
    a->runtime_invoke(scale_set_active,NULL,restore,&error);
    return copied && !error;
}
