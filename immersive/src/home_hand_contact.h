/* Unity queries are read-only and run on the Home producer's attached thread.
   Controller poses and pulses cross threads by value; no Unity handles cross. */
static SRWLOCK contact_lock=SRWLOCK_INIT;
enum { CONTACT_MAX_TARGETS=7, CONTACT_MOVABLE=4, CONTACT_DRIVES=2*CONTACT_MOVABLE };
typedef struct { XrVector3f hand; LONG gen, pulse_gen, surface_gen; unsigned surface_mask; XrVector3f surface_local[CONTACT_MAX_TARGETS]; ULONGLONG tick, pulse_tick, surface_tick; } HandContact;
static HandContact contact[2];
/* World-space landmarks on the visible character. No collider or component is
   added to the game. Only numeric samples leave the game thread. */
/* Keep the IDs already present in Home contact telemetry. */
typedef enum { CONTACT_GENERIC=0, CONTACT_CHEST_LEFT=3, CONTACT_CHEST_RIGHT=4, CONTACT_BUTT_LEFT, CONTACT_BUTT_RIGHT, CONTACT_THIGH_LEFT, CONTACT_THIGH_RIGHT, CONTACT_FACE } ContactRegion;

typedef struct {XrVector3f points[CONTACT_MAX_TARGETS];int count,body;LONG gen;ULONGLONG tick;} ContactTargets;
static ContactTargets targets[2];
static const Il2CppImage* contact_physics;
/* Read-only Unity 2022 player entry points. Managed getter wrappers may
   be stripped even when the underlying engine entry points are present. */
typedef struct { Vec3 center,extent; } ContactBounds;
_Static_assert(sizeof(ContactBounds)==24 && sizeof(Scene)==4,"Unity value layout");
static struct {
    bool attempted;
    Il2CppString* (*name)(Il2CppObject*);
    bool (*visible)(Il2CppObject*);
    void (*scene)(Il2CppObject*,Scene*);
    void (*bounds)(Il2CppObject*,ContactBounds*);
} contact_native;
static void contact_bind_native(void){
    if(contact_native.attempted)return;
    contact_native.attempted=true;
    contact_native.name=(void*)p_resolve_icall("UnityEngine.Object::GetName(UnityEngine.Object)");
    contact_native.visible=(void*)p_resolve_icall("UnityEngine.Renderer::get_isVisible()");
    contact_native.scene=(void*)p_resolve_icall("UnityEngine.GameObject::get_scene_Injected(UnityEngine.SceneManagement.Scene&)");
    contact_native.bounds=(void*)p_resolve_icall("UnityEngine.Renderer::get_bounds_Injected(UnityEngine.Bounds&)");
}
static Il2CppString* contact_name(const MethodInfo* getter,Il2CppObject* obj){
    if(!obj)return NULL;
    if(!getter)return contact_native.name?contact_native.name(obj):NULL;
    Il2CppException* err=NULL;
    Il2CppString* name=(Il2CppString*)bp.api.runtime_invoke(getter,obj,NULL,&err);
    return err?NULL:name;
}
static bool contact_scene(const MethodInfo* getter,Il2CppObject* go,Il2CppClass* type,Scene* scene){
    if(!go)return false;
    if(getter)return geometry_read(getter,go,NULL,type,scene,sizeof *scene);
    if(!contact_native.scene)return false;
    contact_native.scene(go,scene);
    return true;
}
static bool contact_visible(const MethodInfo* getter,Il2CppObject* renderer){
    uint8_t visible=0;
    if(getter)return geometry_read(getter,renderer,NULL,bp.types[BT_BOOL],&visible,sizeof visible) && visible;
    return contact_native.visible && contact_native.visible(renderer);
}
static int contact_part_name(Il2CppObject* obj,const MethodInfo* get_name){
    Il2CppString* name=contact_name(get_name,obj);
    if(!name)return -1;
    return il2cpp_string_equals_ascii(name,"M_Face")?0:il2cpp_string_equals_ascii(name,"M_Body")?1:-1;
}
/* Use numeric anchors only. Bone indices belong to this renderer/generation;
   no Unity object or managed array survives the owner-thread query. */
static Vec3 contact_sub(Vec3 a,Vec3 b){return (Vec3){a.x-b.x,a.y-b.y,a.z-b.z};}
static float contact_dot(Vec3 a,Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
/* Contact context belongs to the Unity thread. Both eye producers allocate
   generation IDs from source_generation. Only numeric samples cross to XR. */
static LONG contact_generation;
static bool contact_live;
static float contact_scale;
static uint32_t live_contact_mesh;
static uint32_t contact_mesh_handle(void){return contact_live?live_contact_mesh:bp.contact_mesh;}
static bool contact_sample_mesh(Il2CppObject* renderer,Il2CppArray** vertices,Matrix4x4* matrix){
    static void (*create)(Il2CppObject*),(*bake)(Il2CppObject*,Il2CppObject*,bool);
    static int (*vertex_count)(Il2CppObject*);
    static Il2CppArray* (*positions)(Il2CppObject*,int,int,int);
    static get_matrix4x4_injected_fn matrix_get;
    static Il2CppClass* mesh_type;
    static bool bound;
    if(!bound){
        create=(void*)p_resolve_icall("UnityEngine.Mesh::Internal_Create(UnityEngine.Mesh)");
        bake=(void*)p_resolve_icall("UnityEngine.SkinnedMeshRenderer::BakeMesh(UnityEngine.Mesh,System.Boolean)");
        vertex_count=(void*)p_resolve_icall("UnityEngine.Mesh::get_vertexCount()");
        positions=(void*)p_resolve_icall("UnityEngine.Mesh::GetAllocArrayFromChannelImpl(UnityEngine.Rendering.VertexAttribute,UnityEngine.Rendering.VertexAttributeFormat,System.Int32)");
        matrix_get=(void*)p_resolve_icall("UnityEngine.Transform::get_localToWorldMatrix_Injected(UnityEngine.Matrix4x4&)");
        mesh_type=p_class_from_name(story_geometry_core,"UnityEngine","Mesh");bound=true;
    }
    if(!renderer || !create || !bake || !positions || !matrix_get || !vertex_count || !mesh_type)return false;
    Il2CppObject* transform=component_get_transform(renderer);
    if(!transform)return false;
    if(!contact_mesh_handle()){
        if(!contact_live && bp.root_count>=24)return false;
        Il2CppObject* mesh=bp.object_new(mesh_type);
        uint32_t handle=mesh?bp.api.gchandle_new(mesh,false):0;
        if(!handle)return false;
        if(contact_live)live_contact_mesh=handle;
        else {bp.roots[bp.root_count++]=handle;bp.contact_mesh=handle;}
        create(mesh);
    }
    Il2CppObject* mesh=bp.api.gchandle_get_target(contact_mesh_handle());if(!mesh)return false;
    bake(renderer,mesh,false);
    int count=vertex_count(mesh);if(count<=0 || count>65536)return false;
    Il2CppArray* verts=positions(mesh,0,0,3);
    if(!verts || verts->max_length!=(uintptr_t)count)return false;
    Il2CppClass* array=bp.api.object_get_class((Il2CppObject*)verts);
    if(bp.api.class_get_element_class(array)!=bp.types[BT_V3])return false;
    matrix_get(transform,matrix);
    *vertices=verts;return true;
}

static Vec3 contact_world_vertex(Vec3 v,const Matrix4x4* matrix){
    const float* m=matrix->m;
    return (Vec3){m[0]*v.x+m[4]*v.y+m[8]*v.z+m[12],
                  m[1]*v.x+m[5]*v.y+m[9]*v.z+m[13],
                  m[2]*v.x+m[6]*v.y+m[10]*v.z+m[14]};
}
#include "home_touch_reaction.h"
#include "home_chest_surface.h"
typedef struct {int id;Vec3 center,extent;} ContactFace;
static ContactFace face_target;
static bool contact_same_rig(Il2CppObject* body,Il2CppObject* face){
    if(!reaction_api.bones || !reaction_api.parent)return false;
    /* Some face and body meshes have parallel skeletons under the same model. */
    Il2CppObject* bt=component_get_transform(body),*ft=component_get_transform(face);
    Il2CppObject* parent=bt?reaction_api.parent(bt):NULL;
    if(parent && ft && parent==reaction_api.parent(ft))return true;
    Il2CppArray* b=reaction_api.bones(body),*f=reaction_api.bones(face);
    if(!b || !f || b->max_length>512 || f->max_length>512)return false;
    /* Face rigs may start below Head. Match a concrete body skin bone. */
    for(uintptr_t i=0;i<f->max_length;i++){
        Il2CppObject* bone=f->vector[i];
        for(int depth=0;bone && depth<16;depth++,bone=reaction_api.parent(bone))
            for(uintptr_t j=0;j<b->max_length;j++)if(bone==b->vector[j])return true;
    }
    return false;
}
typedef struct {
    int id;
    ContactBounds bounds;
    ContactFace face;
    bool face_checked;
} ContactActor;
static struct {
    ContactActor *actors;
    ContactFace *faces;
    size_t capacity,count,face_count;
    LONG gen;
    ULONGLONG next;
} roster;

static void contact_clear_roster(void){
    free(roster.actors);free(roster.faces);
    memset(&roster,0,sizeof roster);
}

static void contact_refresh_roster(Scene scene,LONG gen,ULONGLONG now){
    if(roster.gen==gen && now<roster.next)return;
    roster.gen=gen;roster.next=now+180;roster.count=roster.face_count=0;
    static const MethodInfo *bounds_get,*visible_get,*scene_get,*name_get,*mesh_get,*layer_get,*viewport;
    static Il2CppClass *bounds_class,*scene_class;
    static Il2CppReflectionType* skin_type;
    static bool attempted;
    if(!attempted && story_geometry_core){
        attempted=true;
        Il2CppClass* skin=p_class_from_name(story_geometry_core,"UnityEngine","SkinnedMeshRenderer");
        Il2CppClass* renderer=p_class_from_name(story_geometry_core,"UnityEngine","Renderer");
        bounds_class=p_class_from_name(story_geometry_core,"UnityEngine","Bounds");
        scene_class=p_class_from_name(story_geometry_core,"UnityEngine.SceneManagement","Scene");
        if(!skin || !renderer || !bounds_class || !scene_class)return;
        Il2CppClass* mesh=p_class_from_name(story_geometry_core,"UnityEngine","Mesh");
        if(mesh)mesh_get=geometry_method(skin,"get_sharedMesh",mesh,NULL);
        layer_get=geometry_method(bp.types[BT_GO],"get_layer",bp.types[BT_INT],NULL);
        viewport=geometry_method(bp.types[BT_CAMERA],"WorldToViewportPoint",bp.types[BT_V3],bp.types[BT_V3]);
        bounds_get=geometry_method(renderer,"get_bounds",bounds_class,NULL);
        visible_get=geometry_method(renderer,"get_isVisible",bp.types[BT_BOOL],NULL);
        scene_get=geometry_method(bp.types[BT_GO],"get_scene",scene_class,NULL);
        name_get=geometry_method(bp.types[BT_OBJECT],"get_name",bp.types[BT_STRING],NULL);
        if((bounds_get || contact_native.bounds) && (visible_get || contact_native.visible) && (scene_get || contact_native.scene) && (name_get || contact_native.name))skin_type=p_type_get_object(p_class_get_type(skin));
    }
    if(!skin_type){
        static LONG reported;if(reported!=gen){reported=gen;
            char msg[192];snprintf(msg,sizeof msg,"\"stage\":\"metadata_unavailable\",\"bounds\":%d,\"visible\":%d,\"scene\":%d,\"name\":%d",
                !!(bounds_get || contact_native.bounds),!!(visible_get || contact_native.visible),!!(scene_get || contact_native.scene),!!(name_get || contact_native.name));
            write_record("CONTACT","home_contact_selection",msg);}
        return;
    }
    Il2CppArray* list=unity_find_objects(skin_type,0,0);
    if(!list)return;
    if(list->max_length>roster.capacity){
        size_t count=list->max_length;
        if(count>SIZE_MAX/sizeof(ContactActor) || count>SIZE_MAX/sizeof(ContactFace))return;
        ContactActor* actors=malloc(count*sizeof *actors);
        ContactFace* faces=malloc(count*sizeof *faces);
        if(!actors || !faces){free(actors);free(faces);return;}
        free(roster.actors);free(roster.faces);
        roster.actors=actors;roster.faces=faces;roster.capacity=count;
    }
    for(uintptr_t i=0;i<list->max_length;i++){
        Il2CppObject* mesh=list->vector[i];Il2CppObject* go=mesh?component_get_game_object(mesh):NULL;
        Scene s={0};
        if(!go || !contact_scene(scene_get,go,scene_class,&s) ||
           !contact_visible(visible_get,mesh))continue;
        int k=contact_part_name(go,name_get);
        if(k<0 && mesh_get){
            Il2CppException* err=NULL;
            Il2CppObject* asset=bp.api.runtime_invoke(mesh_get,mesh,NULL,&err);
            if(!err)k=contact_part_name(asset,name_get);
        }
        if(k<0)continue;
        ContactBounds b={0};
        if(bounds_get){
            if(!geometry_read(bounds_get,mesh,NULL,bounds_class,&b,sizeof b))continue;
        }else contact_native.bounds(mesh,&b);
        if(!(b.extent.x>0 && b.extent.x<4 && b.extent.y>0 && b.extent.y<4 && b.extent.z>0 && b.extent.z<4))continue;
        if(k==0){
            int id=chest_instance(mesh);
            if(id)roster.faces[roster.face_count++]=(ContactFace){id,b.center,b.extent};
            continue;
        }
        if(contact_live){
            int32_t mask=0,layer=-1;
            if(!geometry_read(bp.method[BM_CULL_GET],current_authored_owner,NULL,bp.types[BT_INT],&mask,sizeof mask) ||
               !geometry_read(layer_get,go,NULL,bp.types[BT_INT],&layer,sizeof layer) ||
               layer<0 || layer>=32 || !((uint32_t)mask&(1u<<layer)))continue;
            Vec3 relative=quat_rotate(quat_conjugate(live_contact_view.rotation),contact_sub(b.center,live_contact_view.center));
            if(relative.z+sqrtf(contact_dot(b.extent,b.extent))<=0)continue;
        }else if(s.handle!=scene.handle){
            int32_t layer=-1;
            if(!geometry_read(layer_get,go,NULL,bp.types[BT_INT],&layer,sizeof layer) ||
               layer<0 || layer>=32 || !((uint32_t)bp.eye_mask&(1u<<layer)))continue;
            BOOL inside=TRUE;
            for(int e=0;e<2;e++){
                Vec3 v;void* args[]={&b.center};
                Il2CppObject* eye=bp.api.gchandle_get_target(bp.eye[e].camera);
                if(!geometry_read(viewport,eye,args,bp.types[BT_V3],&v,sizeof v) ||
                   !(v.z>0 && v.x>=0 && v.x<=1 && v.y>=0 && v.y<=1)){inside=FALSE;break;}
            }
            if(!inside)continue;
        }
        int id=chest_instance(mesh);
        if(id)roster.actors[roster.count++]=(ContactActor){.id=id,.bounds=b};
    }
}
static void contact_bind_face(ContactActor* actor){
    if(actor->face_checked)return;
    actor->face_checked=true;
    reaction_bind();
    if(!reaction_api.find)return;
    Il2CppObject* body=reaction_api.find(actor->id);
    if(!body || bp.api.object_get_class(body)!=reaction_api.skin)return;
    for(size_t i=0;i<roster.face_count;i++){
        Il2CppObject* face=reaction_api.find(roster.faces[i].id);
        if(face && bp.api.object_get_class(face)==reaction_api.skin && contact_same_rig(body,face)){
            actor->face=roster.faces[i];return;
        }
    }
}

static float contact_bounds_distance(Vec3 hand,ContactBounds bounds){
    Vec3 d=contact_sub(hand,bounds.center);
    d=(Vec3){fmaxf(0,fabsf(d.x)-bounds.extent.x),fmaxf(0,fabsf(d.y)-bounds.extent.y),
             fmaxf(0,fabsf(d.z)-bounds.extent.z)};
    return contact_dot(d,d);
}
/* Bake the selected actor's face only while a hand is near its bounds.
   Numeric topology survives a pose update; both hands share the posed vertices. */
static bool contact_face_hit(Vec3 hand,float reach,LONG gen,ULONGLONG now,Vec3* nearest,float* distance){
    static struct {int mesh,renderer,vertices,indices;Vec3 center;LONG gen;ULONGLONG next;
        uint16_t index[196608];Vec3 posed[CHEST_VERTICES];} cache;
    Vec3 delta=contact_sub(hand,face_target.center),ext=face_target.extent;
    if(!face_target.id || fabsf(delta.x)>ext.x+reach || fabsf(delta.y)>ext.y+reach ||
       fabsf(delta.z)>ext.z+reach || !reaction_api.find || !reaction_api.shared_mesh)return false;
    if(gen!=cache.gen || face_target.id!=cache.renderer || now>=cache.next){
        Il2CppObject* face=reaction_api.find(face_target.id);
        if(!face || bp.api.object_get_class(face)!=reaction_api.skin)return false;
        int mesh=chest_instance(reaction_api.shared_mesh(face));Il2CppArray* verts;Matrix4x4 matrix;
        if(!mesh || !contact_sample_mesh(face,&verts,&matrix))return false;
        int n=(int)verts->max_length;
        if(cache.gen!=gen || cache.renderer!=face_target.id || cache.mesh!=mesh || cache.vertices!=n){
            int (*subs)(Il2CppObject*)=(void*)p_resolve_icall("UnityEngine.Mesh::get_subMeshCount()");
            int (*topology)(Il2CppObject*,int)=(void*)p_resolve_icall("UnityEngine.Mesh::GetTopologyImpl(System.Int32)");
            Il2CppArray* (*indices)(Il2CppObject*,int,bool)=(void*)p_resolve_icall("UnityEngine.Mesh::GetTrianglesImpl(System.Int32,System.Boolean)");
            Il2CppObject* baked=bp.api.gchandle_get_target(contact_mesh_handle());
            if(!baked || !subs || !topology || !indices)return false;
            int count=subs(baked),used=0;
            if(count<=0 || count>64)return false;
            cache.mesh=0;
            for(int j=0;j<count;j++){
                if(topology(baked,j)!=0)continue;
                Il2CppArray* a=indices(baked,j,true);
                if(!a || a->max_length%3 || a->max_length>(uintptr_t)(196608-used) ||
                    bp.api.class_get_element_class(bp.api.object_get_class((Il2CppObject*)a))!=bp.types[BT_INT])return false;
                const int* idx=(const int*)(const void*)a->vector;
                for(uintptr_t h=0;h<a->max_length;h++){
                    if(idx[h]<0 || idx[h]>=n)return false;
                    cache.index[used++]=(uint16_t)idx[h];
                }
            }
            cache.indices=used;cache.mesh=mesh;cache.vertices=n;cache.renderer=face_target.id;cache.gen=gen;
        }
        const Vec3* v=(const Vec3*)(const void*)verts->vector;
        Vec3 lo={INFINITY,INFINITY,INFINITY},hi={-INFINITY,-INFINITY,-INFINITY};
        for(int j=0;j<n;j++){
            Vec3 q=contact_world_vertex(v[j],&matrix);cache.posed[j]=q;
            if(!isfinite(q.x) || !isfinite(q.y) || !isfinite(q.z))return false;
            lo=(Vec3){fminf(lo.x,q.x),fminf(lo.y,q.y),fminf(lo.z,q.z)};
            hi=(Vec3){fmaxf(hi.x,q.x),fmaxf(hi.y,q.y),fmaxf(hi.z,q.z)};
        }
        cache.center=chest_mix(lo,hi,.5f);
        cache.next=now+48;
    }
    float best=INFINITY;bool inside=false;
    for(int j=0;j<cache.indices;j+=3){
        ChestTriangle t={cache.posed[cache.index[j]],cache.posed[cache.index[j+1]],cache.posed[cache.index[j+2]]};
        Vec3 q=chest_closest(&t,hand);float d=chest_distance(q,hand);
        if(d<best){*nearest=q;best=d;}
        if(!inside)inside=chest_contains(&t,cache.center,hand);
    }
    if(!isfinite(best) || (!inside && best>reach*reach))return false;
    *distance=best;return true;
}
static Vec3 contact_world_hand(XrVector3f hand,XrVector3f head,Vec3 center,Quat basis,float scale){
    Vec3 v=quat_rotate(basis,(Vec3){hand.x-head.x,hand.y-head.y,head.z-hand.z});
    return (Vec3){center.x+v.x/scale,center.y+v.y/scale,center.z+v.z/scale};
}
static XrVector3f contact_local_point(XrVector3f head,Vec3 center,Vec3 surface,Quat basis,float scale){
    Vec3 v=quat_rotate(quat_conjugate(basis),contact_sub(surface,center));
    return (XrVector3f){head.x+v.x*scale,head.y+v.y*scale,head.z-v.z*scale};
}
typedef struct {
    Vec3 surface,normal;
    float penetration;
    int joint;
} ContactHit;
typedef struct {
    int body;
    unsigned mask;
    ULONGLONG measured_tick[CONTACT_MAX_TARGETS],tick;
    Vec3 measured_point[CONTACT_MAX_TARGETS],point,velocity;
    ContactHit hits[CONTACT_MAX_TARGETS];
    LONG gen;
} RegionContacts;
static RegionContacts hand_regions[2];

static bool contact_prefer_actor(unsigned mask,float score,int id,unsigned chosen_mask,float chosen_score,
                                 int chosen_id,int held_id,bool nearest){
    if(!mask)return !chosen_mask && (nearest || !chosen_id);
    if(!chosen_mask || score<chosen_score)return true;
    if(score!=chosen_score)return false;
    return id==held_id || (chosen_id!=held_id && id<chosen_id);
}

static unsigned contact_measure(Vec3 point,XrVector3f head,Vec3 view_center,Quat basis,LONG gen,
                                ULONGLONG now,unsigned previous,ContactHit hits[CONTACT_MAX_TARGETS],
                                ContactTargets* display,float* score){
    unsigned measured=0;
    for(int k=0;k<BODY_PATCHES;k++){
        ChestPatch* patch=&active_chest->patches[k];
        if(!patch->count)continue;
        display->points[display->count++]=contact_local_point(head,view_center,patch->marker,basis,contact_scale);
        unsigned bit=1u<<k;
        float reach=(previous&bit?.021f:.015f)/contact_scale;
        float distance;bool inside=false;
        if(k>=4)reach+=.018f/contact_scale;
        const float radius=(hand_radius_mm/1000.f)/contact_scale; /* The displayed XR hand sphere. */
        if(k<2)reach=(previous&bit)?fmaxf(reach,radius):radius;
        else reach+=(hand_radius_mm-13.f)*.001f/contact_scale;
        reach*=contact_reach/100.f;
        if(!chest_hit(patch,point,reach,k<2?.0005f/contact_scale:reach,
                      &hits[k].surface,&distance,&hits[k].normal,&inside,&hits[k].joint))continue;
        if(k<2){
            float signed_distance=sqrtf(distance)*(inside?-1.f:1.f);
            hits[k].penetration=fmaxf(0.f,radius*contact_reach/100.f-signed_distance);
            if(!(previous&bit) && hits[k].penetration<=0)continue;
            /* At an outer edge the sphere pushes toward its closest point.
               Within the tissue, keep using the inward skin normal. */
            if(!inside && distance>1e-10f)
                hits[k].normal=reaction_unit(contact_sub(point,hits[k].surface));
        }

        *score=fminf(*score,distance);
        measured|=bit;
    }
    if(face_target.id){
        const int k=CONTACT_FACE-CONTACT_CHEST_LEFT;
        float distance,reach=(previous&(1u<<k)?.038f:.032f)/contact_scale;
        reach+=(hand_radius_mm-13.f)*.001f/contact_scale;
        bool face=contact_face_hit(point,reach*contact_reach/100.f,gen,now,&hits[k].surface,&distance);
        if(face){measured|=1u<<k;*score=fminf(*score,distance);}
        display->points[display->count++]=contact_local_point(head,view_center,face?hits[k].surface:face_target.center,basis,contact_scale);
    }
    return measured;
}

static void contact_hand_step(Scene scene,unsigned side) {
    static ULONGLONG next[2];
    static LONG previous_gen[2];
    RegionContacts* held=&hand_regions[side];
    static XrVector3f previous_hand[2];
    static ULONGLONG previous_tick[2];
    HandContact* slot=&contact[side];
    ULONGLONG now=GetTickCount64();
    if(now<next[side] || GetCurrentThreadId()!=bp.owner_thread || !TryAcquireSRWLockShared(&contact_lock))return;
    XrVector3f hand=slot->hand;LONG gen=slot->gen;ULONGLONG tick=slot->tick;
    ReleaseSRWLockShared(&contact_lock);
    next[side]=now+48;
    if(gen!=previous_gen[side]){held->mask=0;previous_tick[side]=0;previous_gen[side]=gen;}
    if(gen<=0 || now-tick>160 || gen!=contact_generation){held->mask=0;return;}
    static LONG tracked[2];
    if(!contact_live && bp.source_kind==BP_SOURCE_DRESS)
        outfit_stage(side?"right_hand":"left_hand",gen,&tracked[side]);
    EyeOptics optics;
    Vec3 view_center,point;
    Quat view_rotation;
    if(contact_live){
        optics=live_contact_view.optics;
        view_center=live_contact_view.center;
        view_rotation=live_contact_view.rotation;
    }else{
        Il2CppObject* eyes[2];
        Vec3 pos[2];
        for(int i=0;i<2;i++){
            if(!bp.eye[i].transform || !(eyes[i]=bp.api.gchandle_get_target(bp.eye[i].transform))){held->mask=0;return;}
            transform_get_position(eyes[i],&pos[i]);
        }
        view_center=reaction_scale((Vec3){pos[0].x+pos[1].x,pos[0].y+pos[1].y,pos[0].z+pos[1].z},.5f);
        transform_get_rotation(eyes[0],&view_rotation);
        if(!snapshot_eye_optics(&optics) || !optics.origin_valid){held->mask=0;return;}
    }
    Quat basis=quat_multiply(view_rotation,quat_conjugate(xr_to_unity_quat(optics.current_center.orientation)));
    XrVector3f head=optics.current_center.position;
    point=contact_world_hand(hand,head,view_center,basis,contact_scale);
    Vec3 velocity={0};
    if(previous_tick[side] && tick>previous_tick[side] && tick-previous_tick[side]<=120){
        XrVector3f old=previous_hand[side];
        Vec3 travel={hand.x-old.x,hand.y-old.y,old.z-hand.z};
        if(contact_dot(travel,travel)<.25f*.25f)
            velocity=quat_rotate(basis,reaction_scale(travel,1000.f/(float)(tick-previous_tick[side])));
    }
    previous_tick[side]=tick;previous_hand[side]=hand;
    contact_bind_native();
    contact_refresh_roster(scene,gen,now);
    int body_id=0;
    unsigned previous=0,measured=0;
    ContactTargets display={.gen=gen,.tick=now};
    ContactHit hits[CONTACT_MAX_TARGETS]={0};
    float best_score=INFINITY,best_bounds=INFINITY;
    size_t nearest=0;
    for(size_t i=0;i<roster.count;i++){
        float d=contact_bounds_distance(point,roster.actors[i].bounds);
        if(d<best_bounds){best_bounds=d;nearest=i;}
    }
    for(size_t i=0;i<roster.count;i++){
        ContactActor* actor=&roster.actors[i];
        float d=contact_bounds_distance(point,actor->bounds);
        /* Accommodate the head above the body mesh and motion between
           broad-phase refreshes. Always keep one landmark set at a distance. */
        float margin=.35f/contact_scale;
        if(i!=nearest && d>margin*margin && actor->id!=held->body)continue;
        contact_bind_face(actor);
        face_target=actor->face;
        chest_refresh(actor->id,gen,now,point);
        unsigned prior=held->gen==gen && held->body==actor->id?held->mask:0;
        ContactTargets marks={.gen=gen,.tick=now};
        ContactHit candidate[CONTACT_MAX_TARGETS]={0};
        float score=INFINITY;
        unsigned mask=contact_measure(point,head,view_center,basis,gen,now,prior,candidate,&marks,&score);
        if(!contact_prefer_actor(mask,score,actor->id,measured,best_score,body_id,held->body,i==nearest))continue;
        measured=mask;best_score=score;previous=prior;body_id=actor->id;display=marks;
        memcpy(hits,candidate,sizeof hits);
    }
    if(!TryAcquireSRWLockExclusive(&contact_lock))return;
    if(slot->gen!=gen || GetTickCount64()-slot->tick>160){
        held->mask=0;ReleaseSRWLockExclusive(&contact_lock);return;
    }
    unsigned active=measured;
    for(int k=0;k<CONTACT_MAX_TARGETS;k++){
        unsigned bit=1u<<k;
        if(measured&bit){
            held->measured_tick[k]=now;held->measured_point[k]=point;held->hits[k]=hits[k];
        }else if((previous&bit) && now-held->measured_tick[k]<=(ULONGLONG)contact_hold_ms &&
                 chest_distance(point,held->measured_point[k])*contact_scale*contact_scale<=.02f*.02f){
            active|=bit;
        }
        if(active&bit)slot->surface_local[k]=contact_local_point(head,view_center,held->hits[k].surface,basis,contact_scale);
    }
    unsigned entered=active&~previous;
    if(entered){slot->pulse_gen=gen;slot->pulse_tick=now;}
    display.body=body_id;targets[side]=display;slot->surface_gen=gen;slot->surface_tick=now;slot->surface_mask=active;
    ReleaseSRWLockExclusive(&contact_lock);
    held->gen=gen;held->body=body_id;held->mask=active;held->point=point;held->tick=now;held->velocity=velocity;
    static LONG report_gen;static unsigned reports;
    if(report_gen!=gen){report_gen=gen;reports=0;}
    if(previous!=active && reports<32){
        char detail[144];snprintf(detail,sizeof detail,"\"generation\":%ld,\"hand\":%u,\"active_mask\":%u,\"entered_mask\":%u,\"left_mask\":%u",
                                  gen,side,active,entered,previous&~active);
        write_record("CONTACT","home_region_contacts",detail);reports++;
    }
}

/* Exactly one writer per movable region even when both hands touch its joint.
   Keep the current contributing hand until it lets go. */
static void contact_drive_regions(void){
    static struct {int joint,body,hand;LONG gen;ULONGLONG sample;} drives[CONTACT_DRIVES];
    LONG gen=contact_generation;
    ULONGLONG now=GetTickCount64();
    for(int k=0;k<CONTACT_MOVABLE;k++){
        int hand_for[2]={-1,-1},joint[2]={0,0};
        for(int h=0;h<2;h++){
            RegionContacts* c=&hand_regions[h];
            if(c->gen==gen && now-c->tick<=160 && (c->mask&(1u<<k)) &&
               (k>=2 || c->hits[k].penetration>0))joint[h]=c->hits[k].joint;
        }
        /* Preserve the writer on handover. Two different joints of the
           same anatomical region may each have a writer. */
        for(int n=0;n<2;n++){
            int slot=2*k+n,h=drives[slot].hand;
            if(!drives[slot].joint || drives[slot].gen!=gen)continue;
            if(joint[h]!=drives[slot].joint)h=1-h;
            if(joint[h]==drives[slot].joint){
                hand_for[n]=h;
                if(joint[1-h]==joint[h])joint[1-h]=0;
                joint[h]=0;
            }
        }
        for(int n=0;n<2;n++)if(hand_for[n]<0 && (joint[0] || joint[1])){
            int h=joint[0]?0:1;
            hand_for[n]=h;
            if(joint[1-h]==joint[h])joint[1-h]=0;
            joint[h]=0;
        }
        for(int n=0;n<2;n++){
            int slot=2*k+n,side=hand_for[n];
            if(side<0){
                if(drives[slot].joint && drives[slot].gen==gen)reaction_release(&reactions[slot],now);
                drives[slot].joint=0;continue;
            }
            RegionContacts* h=&hand_regions[side];
            ContactHit* hit=&h->hits[k];
            bool start=drives[slot].joint!=hit->joint || drives[slot].gen!=gen || drives[slot].body!=h->body;
            if(start || drives[slot].hand!=side || drives[slot].sample!=h->measured_tick[k]){
                if(start)contact_reaction_start(slot,gen,h->body,(ContactRegion)(CONTACT_CHEST_LEFT+k),hit->joint,h->point,hit->surface,hit->normal,h->velocity,hit->penetration);
                else contact_reaction_follow(slot,gen,hit->joint,h->point,hit->surface,hit->normal,h->velocity,hit->penetration,now);
                drives[slot].joint=hit->joint;drives[slot].gen=gen;drives[slot].body=h->body;
                drives[slot].sample=h->measured_tick[k];drives[slot].hand=side;
            }
        }
    }
}
static void contact_retire_live(void){
    if(GetCurrentThreadId()!=bp.owner_thread)return;
    contact_generation=0;
    contact_reaction_step(0);
    if(live_contact_mesh){
        Il2CppObject* mesh=bp.api.gchandle_get_target(live_contact_mesh);
        if(mesh)((destroy_object_fn)mi_object_destroy->methodPointer)(mesh,mi_object_destroy);
        bp.api.gchandle_free(live_contact_mesh);
        live_contact_mesh=0;
    }
}
static void contact_scene_step(Scene scene,bool live) {
    static LONG preview;
    if(!live && bp.source_kind==BP_SOURCE_DRESS && bp.c.identity.scene==scene.handle)
        outfit_stage(!contact_consent?"preview_no_consent":!contact_enabled?"preview_disabled":"preview",
                     (LONG)bp.c.identity.generation,&preview);
    if(!contact_consent || !contact_enabled)return;
    if(!bp.bound || GetCurrentThreadId()!=bp.owner_thread)return;
    LONG gen=0;
    if(live && current_owner_kind==AUTHORED_OWNER_LIVE && current_scene_handle==scene.handle)
        gen=current_authored_generation;
    if(!live && bp.c.phase==BP_ACTIVE && !bp.c.retiring && !bp.failed && bp.c.identity.scene==scene.handle &&
       bp.c.identity.generation==(uint64_t)InterlockedCompareExchange(&bp_published_generation,0,0))
        gen=(LONG)bp.c.identity.generation;
    contact_live=live;
    contact_scale=live?world_scale:bp.scale;
    contact_generation=gen;
    contact_reaction_step(gen);
    if(gen<=0 || !(contact_scale>=WORLD_SCALE_MIN)){
        if(roster.capacity)contact_clear_roster();
        return;
    }
    contact_hand_step(scene,0);
    contact_hand_step(scene,1);
    if(contact_motion && contact_strength>0)contact_drive_regions();
}
