/* Numeric topology is bound to a Home generation, renderer, mesh and rig.
   The owner thread refreshes posed triangles once for both hands. */
enum { CHEST_VERTICES=65536, CHEST_FACES=8192, BODY_PATCHES=6 };
typedef struct {Vec3 a,b,c;} ChestTriangle;
typedef struct {uint16_t index[3];bool use_child;float margin[3];} ChestFace;
typedef struct {
    int joint_id,root_index,child_id,child_index,faces,count;
    bool child_triangle[CHEST_FACES*2];
    ChestFace topology[CHEST_FACES];
    ChestTriangle triangles[CHEST_FACES*2];
    Vec3 lo,hi,marker,core;
    bool core_valid;
} ChestPatch;
typedef struct {
    LONG generation;int renderer,mesh,vertex_count;
    ChestPatch patches[BODY_PATCHES];bool ready;
    ULONGLONG next;
} ChestCache;
/* Four recent rigs share posed triangles across both hands. Eviction changes
   bake cost only, never which characters are eligible for collision. */
static ChestCache chest_cache[4];
static ChestCache* active_chest;

static Vec3 chest_mix(Vec3 a,Vec3 b,float t){
    return (Vec3){a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};
}
static Vec3 chest_segment(Vec3 a,Vec3 b,Vec3 p){
    Vec3 edge=contact_sub(b,a);
    float length=contact_dot(edge,edge);
    float t=length>0?contact_dot(contact_sub(p,a),edge)/length:0;
    return chest_mix(a,b,fmaxf(0,fminf(1,t)));
}
static float chest_distance(Vec3 a,Vec3 b){
    Vec3 d=contact_sub(a,b);return contact_dot(d,d);
}
static Vec3 chest_closest(const ChestTriangle* tri,Vec3 p){
    Vec3 a=tri->a,b=tri->b,c=tri->c;
    Vec3 u=contact_sub(b,a),v=contact_sub(c,a),w=contact_sub(p,a);
    float uu=contact_dot(u,u),uv=contact_dot(u,v),vv=contact_dot(v,v);
    float wu=contact_dot(w,u),wv=contact_dot(w,v),den=uu*vv-uv*uv;
    if(den>1e-8f*uu*vv){
        float x=(vv*wu-uv*wv)/den,y=(uu*wv-uv*wu)/den;
        if(x>=0 && y>=0 && x+y<=1)
            return (Vec3){a.x+x*u.x+y*v.x,a.y+x*u.y+y*v.y,a.z+x*u.z+y*v.z};
    }
    Vec3 q=chest_segment(a,b,p),r=chest_segment(b,c,p),s=chest_segment(c,a,p);
    if(chest_distance(r,p)<chest_distance(q,p))q=r;
    return chest_distance(s,p)<chest_distance(q,p)?s:q;
}
static Vec3 chest_cross(Vec3 a,Vec3 b){
    return (Vec3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
/* Each weighted surface triangle and the current regional bone origin bound
   a finite tetrahedron. The visible skin is an open patch, not a closed mesh. */
static bool chest_contains(const ChestTriangle* tri,Vec3 core,Vec3 hand){
    Vec3 a=contact_sub(tri->a,core),b=contact_sub(tri->b,core),c=contact_sub(tri->c,core);
    Vec3 h=contact_sub(hand,core),bc=chest_cross(b,c);
    float det=contact_dot(a,bc);
    float scale2=contact_dot(a,a)*contact_dot(b,b)*contact_dot(c,c);
    if(!(det*det>1e-12f*scale2))return false;
    float x=contact_dot(h,bc)/det;
    float y=contact_dot(h,chest_cross(c,a))/det;
    float z=contact_dot(h,chest_cross(a,b))/det;
    return x>=-1e-6f && y>=-1e-6f && z>=-1e-6f && x+y+z<=1.000001f;
}
/* Project from the bone-depth plane toward the posed surface. Unlike the
   radial tetrahedra, this retains the width of a face at depth. */
static bool chest_under_face(const ChestTriangle* tri,Vec3 hand,Vec3 axis,float inset,float reach){
    Vec3 ab=contact_sub(tri->b,tri->a),ac=contact_sub(tri->c,tri->a);
    Vec3 normal=chest_cross(ab,ac);
    float denom=contact_dot(axis,normal);
    if(denom*denom<=1e-8f*contact_dot(normal,normal))return false;
    float depth=contact_dot(contact_sub(tri->a,hand),normal)/denom;
    if(depth<0 || inset < -reach)return false;
    Vec3 on_face={hand.x+axis.x*depth,hand.y+axis.y*depth,hand.z+axis.z*depth};
    return chest_distance(chest_closest(tri,on_face),on_face)<=reach*reach;
}
static bool chest_hit(const ChestPatch* patch,Vec3 hand,float reach,float interior_margin,Vec3* nearest,float* distance,Vec3* normal,bool* interior,int* driver){
    bool volume=interior && patch->core_valid,inside=false;
    Vec3 axis=volume?reaction_unit(contact_sub(patch->marker,patch->core)):(Vec3){0};
    float inset=contact_dot(contact_sub(hand,patch->core),axis);
    Vec3 lo=patch->lo,hi=patch->hi;
    if(volume){
        Vec3 c=patch->core;
        lo=(Vec3){fminf(lo.x,c.x),fminf(lo.y,c.y),fminf(lo.z,c.z)};
        hi=(Vec3){fmaxf(hi.x,c.x),fmaxf(hi.y,c.y),fmaxf(hi.z,c.z)};
    }
    if(!patch->count || hand.x<lo.x-reach || hand.x>hi.x+reach ||
       hand.y<lo.y-reach || hand.y>hi.y+reach ||
       hand.z<lo.z-reach || hand.z>hi.z+reach)return false;
    float best=INFINITY;int face=-1;
    for(int i=0;i<patch->count;i++){
        const ChestTriangle* tri=&patch->triangles[i];
        if(volume && !inside)inside=chest_contains(tri,patch->core,hand) || chest_under_face(tri,hand,axis,inset,interior_margin);
        Vec3 q=chest_closest(tri,hand);
        float d=chest_distance(q,hand);
        if(d<best){best=d;*nearest=q;face=i;}
    }
    if(face<0 || (!inside && best>reach*reach))return false;
    if(driver)*driver=patch->child_triangle[face]?patch->child_id:patch->joint_id;
    *distance=best;
    if(interior)*interior=inside;
    if(normal){
        const ChestTriangle* t=&patch->triangles[face];
        Vec3 a=contact_sub(t->b,t->a),b=contact_sub(t->c,t->a);
        *normal=(Vec3){a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
    }
    return true;
}
static int chest_clip(const Vec3 corners[3],const float margin[3],Vec3 out[4]){
    int n=0;
    for(int i=0,j=2;i<3;j=i++){
        if((margin[j]<0 && margin[i]>0) || (margin[j]>0 && margin[i]<0))
            out[n++]=chest_mix(corners[j],corners[i],margin[j]/(margin[j]-margin[i]));
        if(margin[i]>=0)out[n++]=corners[i];
    }
    return n;
}
static bool chest_name(Il2CppString* name,const char* lower){
    if(!name || name->length!=(int)strlen(lower))return false;
    for(int i=0;i<name->length;i++){
        unsigned c=name->chars[i];if(c>='A' && c<='Z')c+=32;
        if(c!=(unsigned char)lower[i])return false;
    }
    return true;
}
static int chest_instance(Il2CppObject* o){
    int id=0;
    return o && geometry_read(bp.method[BM_INSTANCE],o,NULL,bp.types[BT_INT],&id,sizeof id)?id:0;
}
static bool chest_bind_topology(Il2CppObject* mesh,Il2CppArray* bones,int nverts){
    static int (*submeshes)(Il2CppObject*),(*topology)(Il2CppObject*,int);
    static Il2CppArray* (*indices)(Il2CppObject*,int,bool);
    static bool attempted;
    if(!attempted){
        attempted=true;
        submeshes=(void*)p_resolve_icall("UnityEngine.Mesh::get_subMeshCount()");
        topology=(void*)p_resolve_icall("UnityEngine.Mesh::GetTopologyImpl(System.Int32)");
        indices=(void*)p_resolve_icall("UnityEngine.Mesh::GetTrianglesImpl(System.Int32,System.Boolean)");
    }
    if(!indices || !submeshes || !topology || !reaction_api.weights || !reaction_api.element_size ||
       !reaction_api.weight_type || !reaction_api.parent || !bones || !bones->max_length || bones->max_length>512)return false;
    const MethodInfo* name_get=geometry_method(bp.types[BT_OBJECT],"get_name",bp.types[BT_STRING],NULL);
    const char* roots[][8]={{"sp_ch_bust0_l_00","bust_l","breast_l","leftbreast"},
                            {"sp_ch_bust0_r_00","bust_r","breast_r","rightbreast"},
                            {"sp_ch_hip_l_00","sp_ch_hip0_l_00","butt_l","butt_l_back01","buttock_l","sp_ch_butt_l_00","sp_ch_hip_l","hipsoft_l"},
                            {"sp_ch_hip_r_00","sp_ch_hip0_r_00","butt_r","butt_r_back01","buttock_r","sp_ch_butt_r_00","sp_ch_hip_r","hipsoft_r"},
                            {"thigh_l","upperleg_l","legup_l","leftupperleg","hip_l","upleg_l","upper_leg_l","leg_l_upper"},
                            {"thigh_r","upperleg_r","legup_r","rightupperleg","hip_r","upleg_r","upper_leg_r","leg_r_upper"}};
    int chest_index=-1,child_index[2]={-1,-1};
    bool preview=!contact_live && bp.source_kind==BP_SOURCE_DRESS;
    int rank[BODY_PATCHES]={4,4,8,8,8,8};
    for(int side=0;side<BODY_PATCHES;side++){active_chest->patches[side].joint_id=0;active_chest->patches[side].faces=0;active_chest->patches[side].child_id=0;}
    for(uintptr_t i=0;i<bones->max_length;i++){
        Il2CppString* label=contact_name(name_get,bones->vector[i]);
        if(preview){
            if(chest_name(label,"chest"))chest_index=(int)i;
            if(chest_name(label,"sp_ch_bust0_l_01"))child_index[0]=(int)i;
            if(chest_name(label,"sp_ch_bust0_r_01"))child_index[1]=(int)i;
        }
        for(int k=0;k<BODY_PATCHES;k++)for(int j=0;j<rank[k];j++)if(chest_name(label,roots[k][j])){
            active_chest->patches[k].root_index=(int)i;active_chest->patches[k].joint_id=chest_instance(bones->vector[i]);rank[k]=j;break;
        }
    }

    int branch[512];
    for(uintptr_t i=0;i<bones->max_length;i++){
        branch[i]=-1;
        Il2CppObject* bone=bones->vector[i];
        for(int depth=0;bone && depth<64;depth++,bone=reaction_api.parent(bone)){
            for(int k=0;k<BODY_PATCHES;k++)if((k<4 || depth==0) && active_chest->patches[k].joint_id && bone==bones->vector[active_chest->patches[k].root_index])branch[i]=k;
            if(branch[i]>=0)break;
        }
    }
    Il2CppArray* weights=reaction_api.weights(mesh);
    if(!weights || weights->max_length!=(uintptr_t)nverts)return false;
    Il2CppClass* type=bp.api.object_get_class((Il2CppObject*)weights);
    if(bp.api.class_get_element_class(type)!=reaction_api.weight_type ||
       reaction_api.element_size(type)!=(int)sizeof(ContactSkinWeight))return false;
    const ContactSkinWeight* skin=(const ContactSkinWeight*)(const void*)weights->vector;
    for(int k=0;k<2;k++)if(chest_index>=0 && child_index[k]>=0 && branch[child_index[k]]==k){
        ChestPatch* patch=&active_chest->patches[k];
        patch->child_index=child_index[k];
        patch->child_id=chest_instance(bones->vector[child_index[k]]);
    }
    static bool child_skin[CHEST_VERTICES][2];
    static float strength[CHEST_VERTICES][BODY_PATCHES];
    for(int i=0;i<nverts;i++){
        float w[BODY_PATCHES]={0},torso=0,child[2]={0};
        for(int j=0;j<4;j++){
            float value=skin[i].weight[j];if(value==0)continue;
            int index=skin[i].index[j];
            if(!(value>0 && value<=1.001f) || index<0 || (uintptr_t)index>=bones->max_length)return false;
            if(branch[index]>=0)w[branch[index]]+=value;
            if(index==chest_index)torso+=value;
            for(int k=0;k<2;k++)if(index==child_index[k])child[k]+=value;
        }
        /* A majority weight keeps the two sides disjoint across the seam. */
        for(int k=0;k<BODY_PATCHES;k++)strength[i][k]=k>=4?fminf(w[k]-.20f,w[k]-w[k^1]):w[k]-.5f;
        for(int k=0;k<2;k++){
            child_skin[i][k]=active_chest->patches[k].child_id && torso+child[k]>=.9999f;
            if(child_skin[i][k] && torso>0 && child[k]>0)
                strength[i][k]=child[k]-.30f;
        }
    }
    Il2CppObject* baked=bp.api.gchandle_get_target(contact_mesh_handle());
    int subs=baked?submeshes(baked):0;
    if(subs<1 || subs>64)return false;
    size_t total=0;
    for(int sub=0;sub<subs;sub++){
        if(topology(baked,sub)!=0)continue; /* MeshTopology.Triangles */
        Il2CppArray* data=indices(baked,sub,true);
        if(!data || data->max_length%3 || data->max_length>786432-total ||
           bp.api.class_get_element_class(bp.api.object_get_class((Il2CppObject*)data))!=bp.types[BT_INT])return false;
        total+=data->max_length;
        const int* idx=(const int*)(const void*)data->vector;
        for(uintptr_t i=0;i<data->max_length;i+=3){
            for(int j=0;j<3;j++)if(idx[i+j]<0 || idx[i+j]>=nverts)return false;
            for(int k=0;k<BODY_PATCHES;k++){
                ChestPatch* p=&active_chest->patches[k];
                if(!p->joint_id)continue;
                ChestFace f={0};
                f.use_child=k<2 && child_skin[idx[i]][k] && child_skin[idx[i+1]][k] && child_skin[idx[i+2]][k];
                for(int j=0;j<3;j++){f.index[j]=(uint16_t)idx[i+j];f.margin[j]=strength[idx[i+j]][k];}
                float max_margin=fmaxf(f.margin[0],fmaxf(f.margin[1],f.margin[2]));
                if(max_margin+(f.use_child?.15f:0.f)<=0)continue;
                if(p->faces==CHEST_FACES){p->faces=0;p->joint_id=0;continue;}
                p->topology[p->faces++]=f;
            }
        }
    }
    for(int k=0;k<BODY_PATCHES;k++)if(active_chest->patches[k].faces)return true;
    return false;
}
static void chest_pose(ChestPatch* p,const Vec3* vertices,const Matrix4x4* matrix){
    p->count=0;p->lo=(Vec3){INFINITY,INFINITY,INFINITY};p->hi=(Vec3){-INFINITY,-INFINITY,-INFINITY};
    for(int i=0;i<p->faces;i++){
        const ChestFace* f=&p->topology[i];Vec3 corners[3],clipped[4];
        for(int j=0;j<3;j++)corners[j]=contact_world_vertex(vertices[f->index[j]],matrix);
        float margin[3];
        for(int j=0;j<3;j++){
            margin[j]=f->margin[j];
            if(f->use_child && p->core_valid){
                float below_m=(p->core.y-corners[j].y)*contact_scale;
                margin[j]+=.15f*fminf(1.f,fmaxf(0.f,below_m/.03f));
            }
        }
        int n=chest_clip(corners,margin,clipped);
        for(int j=0;j<n;j++){
            Vec3 q=clipped[j];if(!isfinite(q.x)||!isfinite(q.y)||!isfinite(q.z)){p->count=0;return;}
            p->lo=(Vec3){fminf(p->lo.x,q.x),fminf(p->lo.y,q.y),fminf(p->lo.z,q.z)};
            p->hi=(Vec3){fmaxf(p->hi.x,q.x),fmaxf(p->hi.y,q.y),fmaxf(p->hi.z,q.z)};
        }
        for(int j=1;j+1<n;j++){
            p->child_triangle[p->count]=f->use_child;
            p->triangles[p->count++]=(ChestTriangle){clipped[0],clipped[j],clipped[j+1]};
        }
    }
    if(p->count){
        float dist;Vec3 center=chest_mix(p->lo,p->hi,.5f);
        chest_hit(p,center,sqrtf(chest_distance(p->lo,p->hi))+.001f,0,&p->marker,&dist,NULL,NULL,NULL);
    }
}
/* Preview-only evidence reserve, independent of the shared session quotas. */
static void outfit_record(const char* detail){
    static unsigned attempts;
    if(attempts>=64)return;
    attempts++;
    WCHAR path[MAX_PATH];
    DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",path,MAX_PATH);
    if(!n || n>=MAX_PATH || swprintf(path+n,MAX_PATH-n,L"\\UmaVR\\outfit-%lu-%llu.jsonl",
        (unsigned long)telemetry_pid,telemetry_attach)<0)return;
    char line[1200];
    int bytes=telemetry_snprintf(line,sizeof line,"{\"pid\":%lu,\"attach\":%llu,\"build\":\"%s\",%s}\r\n",
        (unsigned long)telemetry_pid,telemetry_attach,UMAVR_BUILD_ID,detail);
    if(bytes<=0 || bytes>=(int)sizeof line)return;
    HANDLE file=CreateFileW(path,FILE_APPEND_DATA,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file!=INVALID_HANDLE_VALUE){DWORD written;WriteFile(file,line,(DWORD)bytes,&written,NULL);CloseHandle(file);}
}
static void outfit_stage(const char* stage,LONG generation,LONG* previous){
    if(!generation || generation==*previous)return;
    *previous=generation;
    char msg[128];telemetry_snprintf(msg,sizeof msg,"\"stage\":\"%s\",\"generation\":%ld",stage,generation);
    outfit_record(msg);
}
/* Bounded preview observation on an already baked pose. Compare the skin
   nearest the hand with the region and driver selected by the product. */
static void chest_observe_outfit(Il2CppObject* mesh,Il2CppArray* bones,const Vec3* vertices,
                                 const Matrix4x4* matrix,Vec3 hand,LONG gen,ULONGLONG now){
    static LONG generation;
    static ULONGLONG next;
    static unsigned samples[2],attempts;static bool gap_reported;
    if(contact_live || bp.source_kind!=BP_SOURCE_DRESS)return;
    if(generation!=gen){generation=gen;next=0;samples[0]=samples[1]=attempts=0;gap_reported=false;}
    if(now<next || attempts>=48 || !active_chest->ready)return;
    ChestPatch* p=NULL;float nearest=INFINITY;
    for(int k=0;k<2;k++){
        ChestPatch* q=&active_chest->patches[k];
        if(!q->count || !q->core_valid)continue;
        float d=chest_distance(q->marker,hand);
        if(d<nearest){p=q;nearest=d;}
    }
    static LONG posed;
    if(p)outfit_stage("weighted_patch_ready",gen,&posed);
    if(!p || nearest*contact_scale*contact_scale>.25f*.25f)return;
    bool inside=false;Vec3 surface;float distance;
    float reach=hand_radius_mm*.001f/contact_scale*contact_reach*.01f;
    int driver=p->joint_id;
    unsigned hit=chest_hit(p,hand,reach,.0005f/contact_scale,&surface,&distance,NULL,&inside,&driver);
    if(samples[hit]>=3)return;
    next=now+300;attempts++;
    int vertex=-1;float best=INFINITY;Vec3 closest={0};
    for(int i=0;i<active_chest->vertex_count;i++){
        Vec3 q=contact_world_vertex(vertices[i],matrix);
        float d=chest_distance(q,hand);
        if(d<best){best=d;vertex=i;closest=q;}
    }
    if(vertex<0)return;
    bool gap=best*contact_scale*contact_scale>.045f*.045f;
    if(gap && gap_reported)return;
    Il2CppArray* weights=reaction_api.weights(mesh);
    if(!weights || weights->max_length!=(uintptr_t)active_chest->vertex_count)return;
    Il2CppClass* type=bp.api.object_get_class((Il2CppObject*)weights);
    if(bp.api.class_get_element_class(type)!=reaction_api.weight_type ||
       reaction_api.element_size(type)!=(int)sizeof(ContactSkinWeight))return;
    const ContactSkinWeight* w=&((const ContactSkinWeight*)(const void*)weights->vector)[vertex];
    char labels[4][48]={{0}};
    for(int j=0;j<4;j++){
        int index=w->index[j];
        if(!(w->weight[j]>0) || index<0 || (uintptr_t)index>=bones->max_length)continue;
        Il2CppString* name=contact_name(NULL,bones->vector[index]);
        for(int i=0;name && i<name->length && i<47;i++){
            unsigned ch=name->chars[i];
            labels[j][i]=(ch>=32 && ch<127 && ch!=34 && ch!=92)?(char)ch:'_';
        }
    }
    char msg[768];
    telemetry_snprintf(msg,sizeof msg,"\"stage\":\"outfit_%u_%u\",\"generation\":%ld,\"renderer\":%d,\"mesh\":%d,\"driver\":%d,\"root_index\":%d,\"vertex\":%d,\"vertex_gap_m\":%.4f,\"marker_minus_vertex_y_m\":%.4f,\"pivot_minus_vertex_y_m\":%.4f,\"bones\":[\"%s\",\"%s\",\"%s\",\"%s\"],\"weights\":[%.3f,%.3f,%.3f,%.3f]",
        hit,gap?0:++samples[hit],gen,active_chest->renderer,active_chest->mesh,driver,p->root_index,vertex,
        sqrtf(best)*contact_scale,(p->marker.y-closest.y)*contact_scale,(p->core.y-closest.y)*contact_scale,
        labels[0],labels[1],labels[2],labels[3],w->weight[0],w->weight[1],w->weight[2],w->weight[3]);
    if(gap)gap_reported=true;
    outfit_record(msg);
    write_record("CONTACT","home_contact_selection",msg);
}
static void chest_refresh_pose(int id,LONG gen,ULONGLONG now,bool changed,Vec3 hand){
    active_chest->next=now+48;
    for(int k=0;k<BODY_PATCHES;k++)active_chest->patches[k].count=0;
    if(changed){active_chest->ready=false;active_chest->generation=gen;active_chest->renderer=id;active_chest->mesh=0;}
    if(!id)return;
    reaction_bind();
    if(!reaction_api.find || !reaction_api.bones || !reaction_api.shared_mesh)return;
    Il2CppObject* renderer=reaction_api.find(id);
    if(!renderer || bp.api.object_get_class(renderer)!=reaction_api.skin)return;
    Il2CppObject* mesh=reaction_api.shared_mesh(renderer);
    int mesh_id=chest_instance(mesh);
    if(!mesh_id)return;
    Il2CppArray* verts=NULL;Matrix4x4 matrix;
    if(!contact_sample_mesh(renderer,&verts,&matrix))return;
    int n=(int)verts->max_length;
    Il2CppArray* bones=reaction_api.bones(renderer);
    if(!bones)return;
    bool rebind=changed || !active_chest->ready || mesh_id!=active_chest->mesh || n!=active_chest->vertex_count;
    for(int k=0;!rebind && k<BODY_PATCHES;k++){
        ChestPatch* p=&active_chest->patches[k];
        if(p->child_id && ((uintptr_t)p->child_index>=bones->max_length ||
           chest_instance(bones->vector[p->child_index])!=p->child_id))rebind=true;
        if(p->joint_id && ((uintptr_t)p->root_index>=bones->max_length ||
           chest_instance(bones->vector[p->root_index])!=p->joint_id))rebind=true;
    }
    if(rebind){
        active_chest->mesh=mesh_id;active_chest->vertex_count=n;
        active_chest->ready=chest_bind_topology(mesh,bones,n);
        static LONG log_gen;static unsigned reports;
        if(log_gen!=gen){log_gen=gen;reports=0;}
        if(reports<32){
            char msg[180];snprintf(msg,sizeof msg,"\"generation\":%ld,\"stage\":\"%s\",\"renderer\":%d,\"left_faces\":%d,\"right_faces\":%d,\"butt\":[%d,%d],\"thigh\":[%d,%d]",
                gen,active_chest->ready?"weighted_chest_surface":"chest_surface_unavailable",id,active_chest->patches[0].faces,active_chest->patches[1].faces,active_chest->patches[2].faces,active_chest->patches[3].faces,active_chest->patches[4].faces,active_chest->patches[5].faces);
            write_record("CONTACT","home_contact_selection",msg);
            reports++;
        }
        if(!active_chest->ready){active_chest->next=now+1000;return;}
    }
    for(int k=0;k<BODY_PATCHES;k++){
        ChestPatch* p=&active_chest->patches[k];
        p->core_valid=false;
        if(p->joint_id && (uintptr_t)p->root_index<bones->max_length && bones->vector[p->root_index]){
            transform_get_position(bones->vector[p->root_index],&p->core);
            p->core_valid=isfinite(p->core.x) && isfinite(p->core.y) && isfinite(p->core.z);
        }
        chest_pose(p,(const Vec3*)(const void*)verts->vector,&matrix);
    }
    chest_observe_outfit(mesh,bones,(const Vec3*)(const void*)verts->vector,&matrix,hand,gen,now);
}

/* BakeMesh reads the active bone transforms. Sample the animated pose with
   our own still-present offsets removed, then restore the visible pose before
   returning to the game. No animation/Unity object is retained by this scope. */
static void chest_refresh(int id,LONG gen,ULONGLONG now,Vec3 hand){
    static uint64_t visits,last_used[4];
    unsigned slot=0;
    for(unsigned i=0;i<4;i++){
        if(chest_cache[i].generation==gen && chest_cache[i].renderer==id){slot=i;break;}
        if(last_used[i]<last_used[slot])slot=i;
    }
    active_chest=&chest_cache[slot];last_used[slot]=++visits;
    bool changed=active_chest->generation!=gen || active_chest->renderer!=id;
    if(!changed && now<active_chest->next)return;
    unsigned suspended[CONTACT_DRIVES]={0};
    for(int k=0;k<CONTACT_DRIVES;k++){
        ContactReaction* r=&reactions[k];
        if(r->handle && r->gen==gen)suspended[k]=reaction_sample_pose(r,0,false);
    }
    chest_refresh_pose(id,gen,now,changed,hand);
    for(int k=CONTACT_DRIVES-1;k>=0;k--)
        if(suspended[k])reaction_sample_pose(&reactions[k],suspended[k],true);
}
