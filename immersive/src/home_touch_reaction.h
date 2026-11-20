/* Unity objects are accessed only on the Home producer thread. An in-flight
   offset owns one short-lived handle and only removes its own last write. */
typedef struct {
    LONG gen;
    int id;
    uint32_t handle;
    ULONGLONG last_contact,last_step,released_at;
    Vec3 base,written,velocity,target,offset;
    float tilt_limit;
    Vec3 tilt,tilt_velocity,tilt_goal;
    Quat rotation_base,rotation_written;
    bool chest,applied,rotation_applied;
} ContactReaction;
/* One animation writer per movable region. */
static ContactReaction reactions[CONTACT_DRIVES];
static struct {
    bool bound;
    Il2CppObject* (*find)(int);
    Il2CppArray* (*bones)(Il2CppObject*);
    get_vec3_injected_fn local_get;
    set_vec3_injected_fn local_set;
    void (*rotation_get)(Il2CppObject*,Quat*);
    void (*rotation_set)(Il2CppObject*,const Quat*);
    Il2CppObject* (*parent)(Il2CppObject*);
    void (*inverse_vector)(Il2CppObject*,const Vec3*,Vec3*);
    Il2CppClass *rigid,*skin,*weight_type;
    Il2CppObject* (*shared_mesh)(Il2CppObject*);
    Il2CppArray* (*weights)(Il2CppObject*);
    int (*element_size)(Il2CppClass*);
    bool (*kinematic)(Il2CppObject*);
    void (*add_force)(Il2CppObject*,const Vec3*,int);
} reaction_api;

static void reaction_bind(void){
    if(reaction_api.bound)return;
    reaction_api.bound=true;
    reaction_api.find=(void*)p_resolve_icall("UnityEngine.Object::FindObjectFromInstanceID(System.Int32)");
    reaction_api.bones=(void*)p_resolve_icall("UnityEngine.SkinnedMeshRenderer::get_bones()");
    reaction_api.local_get=(void*)p_resolve_icall("UnityEngine.Transform::get_localPosition_Injected(UnityEngine.Vector3&)");
    reaction_api.local_set=(void*)p_resolve_icall("UnityEngine.Transform::set_localPosition_Injected(UnityEngine.Vector3&)");
    reaction_api.rotation_get=(void*)p_resolve_icall("UnityEngine.Transform::get_localRotation_Injected(UnityEngine.Quaternion&)");
    reaction_api.rotation_set=(void*)p_resolve_icall("UnityEngine.Transform::set_localRotation_Injected(UnityEngine.Quaternion&)");
    reaction_api.parent=(void*)p_resolve_icall("UnityEngine.Transform::GetParent()");
    reaction_api.inverse_vector=(void*)p_resolve_icall("UnityEngine.Transform::InverseTransformVector_Injected(UnityEngine.Vector3&,UnityEngine.Vector3&)");
    reaction_api.skin=p_class_from_name(story_geometry_core,"UnityEngine","SkinnedMeshRenderer");
    reaction_api.weight_type=p_class_from_name(story_geometry_core,"UnityEngine","BoneWeight");
    reaction_api.shared_mesh=(void*)p_resolve_icall("UnityEngine.SkinnedMeshRenderer::get_sharedMesh()");
    reaction_api.weights=(void*)p_resolve_icall("UnityEngine.Mesh::GetBoneWeightsImpl()");
    reaction_api.element_size=(void*)GetProcAddress(GetModuleHandleW(L"GameAssembly.dll"),"il2cpp_array_element_size");
    if(contact_physics)reaction_api.rigid=p_class_from_name(contact_physics,"UnityEngine","Rigidbody");
    reaction_api.kinematic=(void*)p_resolve_icall("UnityEngine.Rigidbody::get_isKinematic()");
    reaction_api.add_force=(void*)p_resolve_icall("UnityEngine.Rigidbody::AddForce_Injected(UnityEngine.Vector3&,UnityEngine.ForceMode)");
}
/* Unity BoneWeight[] uses four float weights followed by four bone indices.
   Match the array type and runtime stride before accessing its elements. */
typedef struct {float weight[4];int index[4];} ContactSkinWeight;
static Vec3 reaction_scale(Vec3 a,float k){return (Vec3){a.x*k,a.y*k,a.z*k};}
static Vec3 reaction_unit(Vec3 a){
    float length=sqrtf(contact_dot(a,a));
    return length>0 && isfinite(length)?reaction_scale(a,1/length):(Vec3){0};
}
/* The triangle supplies the push direction. A lateral stroke carries the
   surface with the hand, while an off-centre press deflects toward the joint. */
static Vec3 reaction_displacement(Vec3 surface,Vec3 pivot,Vec3 face_normal,Vec3 hand_velocity){
    Vec3 radial=contact_sub(surface,pivot),normal=reaction_unit(face_normal);
    if(contact_dot(normal,normal)<.5f)normal=reaction_unit(radial);
    if(contact_dot(normal,radial)<0)normal=reaction_scale(normal,-1);
    float speed=sqrtf(contact_dot(hand_velocity,hand_velocity));
    if(!isfinite(speed))return (Vec3){0};
    if(speed>1.5f)hand_velocity=reaction_scale(hand_velocity,1.5f/speed);
    float approach=contact_dot(hand_velocity,normal);
    Vec3 swipe=contact_sub(hand_velocity,reaction_scale(normal,approach));
    Vec3 lever=contact_sub(radial,reaction_scale(normal,contact_dot(radial,normal)));
    Vec3 push=reaction_scale(normal,-(.009f+.009f*fmaxf(-approach,0)));
    Vec3 result={push.x+.024f*swipe.x-.12f*lever.x,push.y+.024f*swipe.y-.12f*lever.y,push.z+.024f*swipe.z-.12f*lever.z};
    float length=sqrtf(contact_dot(result,result));
    return length>.027f?reaction_scale(result,.027f/length):result;
}
static bool reaction_physics(Il2CppObject* joint,Vec3 direction){
    if(!reaction_api.rigid || !reaction_api.kinematic || !reaction_api.add_force)return false;
    Il2CppObject* go=component_get_game_object(joint);
    Il2CppArray* parts=go?gameobject_get_components(go,component_reflection_type,0,0,1,0,NULL):NULL;
    if(!parts || parts->max_length>32)return false;
    for(uintptr_t i=0;i<parts->max_length;i++){
        Il2CppObject* rb=parts->vector[i];
        if(rb && bp.api.object_get_class(rb)==reaction_api.rigid && !reaction_api.kinematic(rb)){
            /* ForceMode.VelocityChange, a one-shot input to the existing solver. */
            Vec3 velocity=reaction_scale(direction,6.25f);reaction_api.add_force(rb,&velocity,2);return true;
        }
    }
    return false;
}
/* Position and velocity are relative to the current animation pose. An exact
   damped oscillator step keeps the response consistent across frame rates. */
static void reaction_spring(Vec3* offset,Vec3* velocity,Vec3 goal,float dt,float damping,float frequency){
    float decay=expf(-damping*dt),c=cosf(frequency*dt),s=sinf(frequency*dt)/frequency;
    Vec3 x=contact_sub(*offset,goal),v=*velocity;
    float p=decay*(c+damping*s),q=decay*s;
    float u=-(damping*damping+frequency*frequency)*q,w=decay*(c-damping*s);
    *offset=(Vec3){goal.x+p*x.x+q*v.x,goal.y+p*x.y+q*v.y,goal.z+p*x.z+q*v.z};
    *velocity=(Vec3){u*x.x+w*v.x,u*x.y+w*v.y,u*x.z+w*v.z};
}
static Vec3 reaction_pressure(Vec3 hand,Vec3 surface,Vec3 pivot,Vec3 normal,Vec3 velocity,bool chest){
    Vec3 n=reaction_unit(normal),radial=contact_sub(surface,pivot);
    if(contact_dot(n,n)<.5f)n=reaction_unit(radial);
    if(contact_dot(n,radial)<0)n=reaction_scale(n,-1);
    float depth=fmaxf(0.f,contact_dot(contact_sub(surface,hand),n));
    Vec3 touch=reaction_scale(reaction_displacement(surface,pivot,n,velocity),chest?.62f:.45f);
    return contact_sub(touch,reaction_scale(n,fminf(chest?.048f:.035f,(chest?.95f:.7f)*depth)));
}
/* Depth is measured independently on each side. Fade in tangential motion
   near first contact so a grazing hand cannot trigger a full entry kick. */
static Vec3 reaction_chest_load(Vec3 surface,Vec3 pivot,Vec3 normal,Vec3 velocity,float penetration,float radius){
    if(!(penetration>0 && radius>0))return (Vec3){0};
    Vec3 radial=contact_sub(surface,pivot),n=reaction_unit(normal);
    if(contact_dot(n,n)<.5f)n=reaction_unit(radial);
    if(contact_dot(n,radial)<0)n=reaction_scale(n,-1);
    float u=fminf(1.f,penetration/radius);
    float gain=u*u*(3.f-2.f*u);
    Vec3 stroke=reaction_displacement(surface,pivot,n,velocity);
    stroke=contact_sub(stroke,reaction_scale(n,contact_dot(stroke,n)));
    return contact_sub(reaction_scale(stroke,.62f*gain),reaction_scale(n,fminf(.048f,.95f*penetration)*gain));
}
static Vec3 reaction_tilt_goal(Il2CppObject* parent,Vec3 surface,Vec3 pivot,Vec3 local_push,float limit){
    Vec3 radial=contact_sub(surface,pivot),lever;
    reaction_api.inverse_vector(parent,&radial,&lever);
    float length=sqrtf(contact_dot(lever,lever));
    if(!(length>.001f) || !isfinite(length))return (Vec3){0};
    Vec3 axis={lever.y*local_push.z-lever.z*local_push.y,lever.z*local_push.x-lever.x*local_push.z,
               lever.x*local_push.y-lever.y*local_push.x};
    float torque=sqrtf(contact_dot(axis,axis));
    if(!(torque>0) || !isfinite(torque))return (Vec3){0};
    return reaction_scale(axis,fminf(limit,atan2f(torque,length*length))/torque);
}
/* A release is a single edge for a region, after its last pressing hand
   unloads. The return impulse scales with the actual stored deflection. */
static void reaction_release(ContactReaction* r,ULONGLONG now){
    if(!r->handle || !r->chest || r->released_at)return;
    r->released_at=now;
    r->velocity=contact_sub(r->velocity,reaction_scale(r->offset,4.f));
    r->tilt_velocity=contact_sub(r->tilt_velocity,reaction_scale(r->tilt,4.f));
}
static void reaction_tick(ContactReaction* r,bool active,ULONGLONG now){
    if(!r->handle)return;
    Il2CppObject* joint=bp.api.gchandle_get_target(r->handle);
    if(!joint || reaction_api.find(r->id)!=joint)joint=NULL;
    if(joint){
        Vec3 current;reaction_api.local_get(joint,&current);
        /* Animation or a game spring may already have supplied a fresh pose. */
        if(r->applied && memcmp(&current,&r->written,sizeof current)==0){
            current=r->base;reaction_api.local_set(joint,&current);
        }
        Quat rotation={0};
        bool rotate=reaction_api.rotation_get && reaction_api.rotation_set;
        if(rotate){
            reaction_api.rotation_get(joint,&rotation);
            if(r->rotation_applied && !memcmp(&rotation,&r->rotation_written,sizeof rotation)){
                rotation=r->rotation_base;reaction_api.rotation_set(joint,&rotation);
            }
            r->rotation_applied=false;
        }
        if(active && (r->released_at?now-r->released_at<(ULONGLONG)(1450*contact_duration):now-r->last_contact<(ULONGLONG)((r->chest?1150:750)*contact_duration))){
            float dt=(float)(now-r->last_step)*.001f/contact_duration;
            bool pressed=!r->released_at && now-r->last_contact<=110;
            bool recovering=r->chest && !pressed;
            float damping=recovering?5.5f:12.f,frequency=recovering?16.f:23.f;
            Vec3 goal=pressed?r->target:(Vec3){0};
            reaction_spring(&r->offset,&r->velocity,goal,dt,damping,frequency);
            if(rotate){
                Vec3 goal_angle=pressed?reaction_scale(r->tilt_goal,contact_tilt/100.f):(Vec3){0};
                reaction_spring(&r->tilt,&r->tilt_velocity,goal_angle,dt,damping,frequency);
                float angle=sqrtf(contact_dot(r->tilt,r->tilt));
                if(isfinite(angle) && angle>1e-5f){
                    float half=.5f*fminf(angle,r->tilt_limit);
                    Vec3 a=reaction_scale(r->tilt,sinf(half)/angle);
                    r->rotation_base=rotation;
                    r->rotation_written=quat_multiply((Quat){a.x,a.y,a.z,cosf(half)},rotation);
                    reaction_api.rotation_set(joint,&r->rotation_written);
                    r->rotation_applied=true;
                }
            }
            r->last_step=now;
            r->base=current;
            r->written=(Vec3){current.x+r->offset.x,current.y+r->offset.y,current.z+r->offset.z};
            if(isfinite(r->written.x) && isfinite(r->written.y) && isfinite(r->written.z)){
                reaction_api.local_set(joint,&r->written);r->applied=true;return;
            }
        }
    }
    bp.api.gchandle_free(r->handle);*r=(ContactReaction){0};
}
/* Position and rotation can have different owners in the same frame. Undo
   only exact writes still owned by this reaction, independently per channel. */
static unsigned reaction_sample_pose(ContactReaction* r,unsigned suspended,bool restore){
    Il2CppObject* joint=bp.api.gchandle_get_target(r->handle);
    if(!joint || reaction_api.find(r->id)!=joint)return 0;
    unsigned changed=0;
    if(restore?(suspended&1u)!=0:r->applied){
        Vec3 actual;reaction_api.local_get(joint,&actual);
        const Vec3* expected=restore?&r->base:&r->written;
        if(!memcmp(&actual,expected,sizeof actual)){
            reaction_api.local_set(joint,restore?&r->written:&r->base);
            changed|=1u;
        }
    }
    if(restore?(suspended&2u)!=0:r->rotation_applied){
        Quat actual;reaction_api.rotation_get(joint,&actual);
        const Quat* expected=restore?&r->rotation_base:&r->rotation_written;
        if(!memcmp(&actual,expected,sizeof actual)){
            reaction_api.rotation_set(joint,restore?&r->rotation_written:&r->rotation_base);
            changed|=2u;
        }
    }
    return changed;
}
static void contact_reaction_step(LONG gen){
    if(GetCurrentThreadId()!=bp.owner_thread)return;
    ULONGLONG now=GetTickCount64();
    LONG eligible[2]={0};
    AcquireSRWLockShared(&contact_lock);
    for(int i=0;i<2;i++)if(now-contact[i].tick<=160)eligible[i]=contact[i].gen;
    ReleaseSRWLockShared(&contact_lock);
    for(int i=0;i<CONTACT_DRIVES;i++)reaction_tick(&reactions[i],gen>0 && gen==reactions[i].gen && (eligible[0]==gen || eligible[1]==gen),now);
}
static void contact_reaction_follow(unsigned side,LONG gen,int joint_id,Vec3 hand,Vec3 surface,Vec3 normal,Vec3 velocity,float penetration,ULONGLONG now);
static void contact_reaction_start(unsigned side,LONG gen,int renderer_id,ContactRegion region,int joint_id,Vec3 hand,Vec3 surface,Vec3 normal,Vec3 velocity,float penetration){
    reaction_bind();
    if(!reaction_api.find || !reaction_api.bones || !reaction_api.skin)return;
    Il2CppObject* renderer=reaction_api.find(renderer_id);
    if(!renderer || bp.api.object_get_class(renderer)!=reaction_api.skin)return;
    Il2CppObject* joint=reaction_api.find(joint_id);
    const char* route="joint_unavailable";
    if(joint && bp.api.object_get_class(joint)!=bp.types[BT_TRANSFORM])return;
    if(joint){
        int id=0;ULONGLONG now=GetTickCount64();
        if(!geometry_read(bp.method[BM_INSTANCE],joint,NULL,bp.types[BT_INT],&id,sizeof id) || !id)return;
        ContactReaction* prior=&reactions[side];
        if(prior->handle && prior->released_at && prior->gen==gen && prior->id==id){
            contact_reaction_follow(side,gen,id,hand,surface,normal,velocity,penetration,now);
            return;
        }
        reaction_tick(prior,false,now);
        for(unsigned i=0;i<CONTACT_DRIVES;i++)if(i!=side && reactions[i].id==id)reaction_tick(&reactions[i],false,now);
        Vec3 pivot;transform_get_position(joint,&pivot);
        bool chest=region==CONTACT_CHEST_LEFT || region==CONTACT_CHEST_RIGHT;
        Vec3 direction=chest?reaction_chest_load(surface,pivot,normal,velocity,penetration,(hand_radius_mm/1000.f)*(contact_reach/100.f)/contact_scale):
                             reaction_displacement(surface,pivot,normal,velocity);
        if(reaction_physics(joint,reaction_scale(direction,contact_strength/100.f)))route="rigidbody";
        else if(reaction_api.local_get && reaction_api.local_set && reaction_api.parent && reaction_api.inverse_vector){
            Il2CppObject* parent=reaction_api.parent(joint);
            Vec3 local,goal;
            if(parent){
                Vec3 pressure=chest?direction:reaction_pressure(hand,surface,pivot,normal,velocity,chest);
                pressure=reaction_scale(pressure,contact_strength/100.f);
                direction=reaction_scale(direction,contact_strength/100.f);
                reaction_api.inverse_vector(parent,&pressure,&goal);
                reaction_api.inverse_vector(parent,&direction,&local);
                if(isfinite(contact_dot(goal,goal)) && isfinite(contact_dot(local,local))){
                    uint32_t handle=bp.api.gchandle_new(joint,false);
                    if(handle){float limit=chest?.279253f:.10472f; /* 16 or 6 degrees */
                        reactions[side]=(ContactReaction){.handle=handle,.id=id,.gen=gen,.last_contact=now,.last_step=now,.velocity=reaction_scale(local,16.f),.target=goal,.chest=chest,.tilt_limit=limit,.tilt_goal=reaction_tilt_goal(parent,surface,pivot,goal,limit)};route="bone_spring";}
                }
            }
        }else route="motion_api_unavailable";
    }
    static LONG logged_gen;static unsigned reports;
    if(logged_gen!=gen){logged_gen=gen;reports=0;}
    if(reports<24){
        char msg[240];snprintf(msg,sizeof msg,"\"generation\":%ld,\"stage\":\"%d_%s\",\"region\":%d,\"body_id\":%d,\"route\":\"%s\",\"joint_id\":%d",gen,(int)region,route,(int)region,renderer_id,route,joint_id);
        write_record("CONTACT","home_touch_reaction",msg);reports++;
    }
}

static void contact_reaction_follow(unsigned side,LONG gen,int joint_id,Vec3 hand,Vec3 surface,Vec3 normal,Vec3 velocity,float penetration,ULONGLONG now){
    ContactReaction* r=&reactions[side];
    if(!r->handle || r->gen!=gen || r->id!=joint_id)return;
    Il2CppObject* joint=bp.api.gchandle_get_target(r->handle);
    if(!joint || reaction_api.find(joint_id)!=joint)return;
    Il2CppObject* parent=reaction_api.parent(joint);
    if(!parent)return;
    Vec3 pivot,local;
    transform_get_position(joint,&pivot);
    Vec3 push=r->chest?reaction_chest_load(surface,pivot,normal,velocity,penetration,(hand_radius_mm/1000.f)*(contact_reach/100.f)/contact_scale):reaction_pressure(hand,surface,pivot,normal,velocity,r->chest);
    push=reaction_scale(push,contact_strength/100.f);
    reaction_api.inverse_vector(parent,&push,&local);
    if(isfinite(contact_dot(local,local))){r->target=local;r->tilt_goal=reaction_tilt_goal(parent,surface,pivot,local,r->tilt_limit);r->last_contact=now;r->released_at=0;}
}
