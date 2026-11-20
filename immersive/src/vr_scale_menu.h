#include <ctype.h>
#include <float.h>
/* XR-thread modal. The Settings GUI already refuses writes while the game runs. */
static struct ScaleMenu {
    BOOL open, primary, back, held, swallow;
    XrTime press;
    LONG generation, selected, saved, pending, label_key;
    int page; BOOL home, effects;
    ULONGLONG deadline;
    BOOL failed, prior_aux, tracked, hit;
    int hover;
    float u,v;
    XrPosef pose;
    ID3D11ShaderResourceView* label;
} scale_menu;

static BOOL write_menu_setting(LONG percent,const char* bool_key,BOOL old_value,const float* numeric,float before) {
    WCHAR path[MAX_PATH], temp[MAX_PATH];
    DWORD n=GetModuleFileNameW(NULL,path,MAX_PATH);
    if(!n || n>=MAX_PATH) return FALSE;
    WCHAR* tail=wcsrchr(path,L'\\');
    if(!tail || wcscpy_s(tail+1,MAX_PATH-(size_t)(tail+1-path),L"vrmod\\config\\settings.json")) return FALSE;
    if(swprintf_s(temp,MAX_PATH,L"%ls.%lu.tmp",path,(unsigned long)GetCurrentProcessId())<=0) return FALSE;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
    if(file==INVALID_HANDLE_VALUE) return FALSE;
    char buf[16384]; DWORD got=0; LARGE_INTEGER size;
    BOOL read=GetFileSizeEx(file,&size)&&size.QuadPart>0&&size.QuadPart<(LONG64)sizeof buf &&
        ReadFile(file,buf,(DWORD)size.QuadPart,&got,NULL)&&got==(DWORD)size.QuadPart;
    CloseHandle(file);
    if(!read) return FALSE;
    buf[got]=0;
    char needle[80],value[100];
    snprintf(needle,sizeof needle,"\"%s\"",bool_key?bool_key:"eyeRenderScale");
    const char* key=strstr(buf,needle);
    const char* start;
    char* end=NULL;
    int len=0;
    BOOL is_mask=bool_key && !strcmp(bool_key,"homeVfxDisabled");
    if(is_mask){
        LONG previous;
        if(!read_home_vfx_mask(buf,&previous) || previous!=old_value)return FALSE;
    }
    if(key){
        const char* colon=strchr(key,':');
        if(!colon)return FALSE;
        start=colon+1;
        while(*start==' '||*start=='\t'||*start=='\r'||*start=='\n') ++start;
        if(is_mask){
            (void)strtol(start,&end,10);
            len=snprintf(value,sizeof value,"%ld",percent);
        }else if(numeric){
            float current;
            if(!parse_json_float_setting(buf,bool_key,&current) ||
               fabsf(current-before)>1e-5f*fmaxf(1.f,fabsf(before)))return FALSE;
            (void)strtod(start,&end);
            len=snprintf(value,sizeof value,"%.9g",(double)*numeric);
        }else if(bool_key){
            BOOL previous;
            if(!parse_json_bool_setting(buf,bool_key,&previous) || previous!=old_value)return FALSE;
            end=(char*)start+(previous?4:5);
            len=snprintf(value,sizeof value,"%s",old_value?"false":"true");
        }else{
            float old_scale=0;
            if(!parse_json_float_setting(buf,"eyeRenderScale",&old_scale) ||
                lroundf(old_scale*100)!=scale_menu.saved)return FALSE;
            (void)strtod(start,&end);
            len=snprintf(value,sizeof value,"%ld.%02ld",percent/100,percent%100);
        }
    }else{
        float version;
        if(!bool_key || !parse_json_float_setting(buf,"schemaVersion",&version))return FALSE;
        end=strrchr(buf,'}');
        if(!end || !strchr(buf,'{') || end<strchr(buf,'{'))return FALSE;
        for(char* tail=end+1;*tail;++tail)if(!isspace((unsigned char)*tail))return FALSE;
        start=end;
        if(numeric)len=snprintf(value,sizeof value,",\n  %s: %.9g\n",needle,(double)*numeric);
        else if(is_mask)len=snprintf(value,sizeof value,",\n  %s: %ld\n",needle,percent);
        else len=snprintf(value,sizeof value,",\n  %s: %s\n",needle,old_value?"false":"true");
    }
    if(len<=0 || (size_t)len>=sizeof value)return FALSE;
    file=CreateFileW(temp,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
    if(file==INVALID_HANDLE_VALUE) return FALSE;
    DWORD written=0, prefix=(DWORD)(start-buf), suffix=got-(DWORD)(end-buf);
    BOOL ok=WriteFile(file,buf,prefix,&written,NULL)&&written==prefix &&
        WriteFile(file,value,(DWORD)len,&written,NULL)&&written==(DWORD)len &&
        WriteFile(file,end,suffix,&written,NULL)&&written==suffix && FlushFileBuffers(file);
    CloseHandle(file);
    if(ok) ok=ReplaceFileW(path,temp,NULL,0,NULL,NULL);
    if(!ok) DeleteFileW(temp);
    return ok;
}

static BOOL save_menu_setting(LONG percent,const char* key,BOOL old_value){
    return write_menu_setting(percent,key,old_value,NULL,0);
}
static BOOL saved_hand_swap,saved_start_follow;
static float saved_world_scale,saved_snap_degrees;
typedef struct {
    const char* key;
    BOOL* value;
    const WCHAR *ko,*en,*ja;
} VrMenuSwitch;
static const VrMenuSwitch menu_switches[] = {
    {"navigationHandsSwapped",&saved_hand_swap,L"패널·포인터 손 교환 (재시작)",L"Swap panel / pointer (restart)",L"パネル・ポインター交換（再起動）"},
    {"liveCameraFollow",&saved_start_follow,L"Live 시작 시 카메라 추종",L"Follow at Live startup",L"起動時のライブ追従"},
    {"swapMovementSticks",&swap_move_sticks,L"이동·회전 스틱 교환",L"Swap movement sticks",L"移動・回転スティック交換"},
    {"grabTracksPanelHand",&grab_panel_hand,L"월드 이동: 패널 손 추적",L"Grab tracks panel hand",L"パネル側の手で移動"},
    {"grabUsesTrigger",&grab_with_trigger,L"트리거로 월드 잡기",L"Grab with trigger",L"トリガーでワールド移動"},
    {"showControllerHands",&show_hands,L"양손 위치",L"Controller positions",L"両手の位置"},
    {"showContactLocations",&show_contact,L"접촉 지점",L"Contact location",L"接触点"},
    {"locomotionEnabled",&nav_locomotion_enabled,L"스틱 이동",L"Stick movement",L"スティック移動"},
    {"snapTurnEnabled",&nav_snap_turn_enabled,L"스냅 턴",L"Snap turn",L"スナップターン"},
    {"locomotionScaleCompensationEnabled",&nav_locomotion_scale_compensation_enabled,L"이동 속도 배율 보정",L"Scale movement speed",L"移動速度の倍率補正"}
};
enum { FOLLOW_PAGE = 1 + sizeof(menu_switches)/sizeof(menu_switches[0]) };
enum { VFX_PAGE=FOLLOW_PAGE+1, FIRST_NUMBER };
typedef struct {const char* key;float* value;float step,min,max;const WCHAR *ko,*en,*ja;} MenuNumber;
static const MenuNumber numbers[]={
    {"worldScale",&saved_world_scale,.05f,WORLD_SCALE_MIN,FLT_MAX,L"체감 세계 크기 (재시작)",L"World size (restart)",L"体感サイズ（再起動）"},
    {"locomotionSpeed",&nav_locomotion_speed_mps,.25f,0,FLT_MAX,L"이동 속도",L"Movement speed",L"移動速度"},
    {"snapTurnAngleDegrees",&saved_snap_degrees,5,15,90,L"스냅 회전 각도 (°)",L"Snap angle (°)",L"スナップ角度 (°)"},
    {"movementDeadzone",&move_deadzone,.05f,0,.9f,L"스틱 데드존",L"Stick deadzone",L"スティックのデッドゾーン"}
};
enum { NUMBER_COUNT=sizeof(numbers)/sizeof(numbers[0]), NUM_BUTTON=200 };
static const WCHAR* vfx_group_names[5][3]={
    {L"전체",L"General",L"全体"},{L"초점",L"Focus",L"焦点"},
    {L"조명",L"Lighting",L"照明"},{L"색상",L"Color",L"色"},
    {L"화면",L"Screen",L"画面"}
};
static const WCHAR* effect_name(const VfxOption* item,BOOL ko,BOOL ja){
    return ko?item->ko:ja?item->ja:item->en;
}
static LONG home_eye_menu_fields(void){
    LONG gen=InterlockedCompareExchange(&home_vfx_generation,0,0);
    return gen>0 && gen==xr_cache_telemetry.generation && gen==InterlockedCompareExchange(&bp_published_generation,0,0)
        ? InterlockedCompareExchange(&home_vfx_fields,0,0):0;
}
enum { MENU_W=960,MENU_H=720,MENU_ROWS=6, BUTTON_PREV=100,BUTTON_NEXT,BUTTON_BACK,BUTTON_CLOSE,BUTTON_MINUS,BUTTON_PLUS };
typedef struct {int items[VFX_COUNT],count,pages; LONG fields,mask,switches; BOOL master,follow;} MenuRows;
static MenuRows menu_rows(void){
    MenuRows rows={0};
    rows.fields=scale_menu.home?home_eye_menu_fields():0;
    rows.master=InterlockedCompareExchange(&home_vfx_allowed,0,0)!=0;
    rows.follow=InterlockedCompareExchange(&follow_display_state,0,0)!=0;
    rows.mask=scale_menu.home?~InterlockedCompareExchange(&home_vfx_disabled,0,0):InterlockedCompareExchange(&live_vfx_mask,0,0);
    for(int i=1;i<FOLLOW_PAGE;i++)if(*menu_switches[i-1].value)rows.switches|=1L<<i;
    if(scale_menu.effects){
        for(int i=0;i<VFX_COUNT;i++)if(!scale_menu.home || !i || (rows.fields&(1L<<i)))rows.items[rows.count++]=i;
    }else{
        rows.items[rows.count++]=VFX_PAGE;
        for(int i=0;i<=FOLLOW_PAGE;i++){
            if(i>0 && i<FOLLOW_PAGE && menu_switches[i-1].value==&show_contact && !contact_consent)continue;
            if(!scale_menu.home || i!=FOLLOW_PAGE)rows.items[rows.count++]=i;
        }
    }
    if(!scale_menu.effects)for(int i=0;i<NUMBER_COUNT;i++)rows.items[rows.count++]=FIRST_NUMBER+i;
    rows.pages=(rows.count+MENU_ROWS-1)/MENU_ROWS;
    if(scale_menu.page>=rows.pages)scale_menu.page=rows.pages-1;
    return rows;
}
static RECT menu_rect(int x,int y,int w,int h){return (RECT){x,y,x+w,y+h};}
static RECT row_rect(int line){return menu_rect(24,108+line*76,912,64);}
static RECT footer_rect(int col){return menu_rect(24+col*228,631,216,60);}
static RECT scale_rect(int col,int line){return menu_rect(553+col*128,112+line*76,120,56);}
static int menu_hit(const MenuRows* rows,float u,float v){
    if(!(u>=0 && u<1 && v>=0 && v<1))return -1;
    POINT pt={(LONG)(u*MENU_W),(LONG)(v*MENU_H)};
    for(int i=0;i<MENU_ROWS && i+scale_menu.page*MENU_ROWS<rows->count;i++){
        int item=rows->items[scale_menu.page*MENU_ROWS+i];
        RECT rect=row_rect(i);
        if(!PtInRect(&rect,pt))continue;
        if(!scale_menu.effects && item>=FIRST_NUMBER){
            for(int k=0;k<2;k++){RECT b=scale_rect(k+1,i);if(PtInRect(&b,pt))return NUM_BUTTON+2*(item-FIRST_NUMBER)+k;}
            return -1;
        }
        if(!scale_menu.effects && !item){
            for(int k=0;k<3;k++){rect=scale_rect(k,i);if(PtInRect(&rect,pt))return k==0?BUTTON_MINUS:k==1?BUTTON_PLUS:0;}
            return -1;
        }
        return scale_menu.effects && !live_vfx_options[item].key?-1:item;
    }
    for(int c=0;c<4;c++){
        RECT rect=footer_rect(c);if(!PtInRect(&rect,pt))continue;
        if((c==0 && !scale_menu.page)||(c==1 && scale_menu.page+1>=rows->pages)||(c==2 && !scale_menu.effects))return -1;
        return BUTTON_PREV+c;
    }
    return -1;
}
static void menu_box(HDC dc,RECT r,COLORREF color){
    SetDCBrushColor(dc,color);FillRect(dc,&r,(HBRUSH)GetStockObject(DC_BRUSH));
}
static void menu_text(HDC dc,RECT r,const WCHAR* text,COLORREF color,UINT align){
    r.left+=12;r.right-=12;SetTextColor(dc,color);
    DrawTextW(dc,text,-1,&r,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|align);
}
static BOOL scale_menu_label(void){
    int W=MENU_W,H=MENU_H;
    MenuRows rows=menu_rows();
    LONG key=scale_menu.page+8*scale_menu.effects+16*scale_menu.home+32*scale_menu.selected+8192*scale_menu.failed+
        16384*(scale_menu.pending!=0)+32768*rows.master+65536*rows.follow+131072*(scale_menu.hover+1);
    LONG status=scale_menu.home?InterlockedCompareExchange(&home_vfx_failed,0,0):0;
    LONG source=scale_menu.home?InterlockedCompareExchange(&home_source_post_status,0,0):0;
    LONG observed=InterlockedCompareExchange(&home_eye_post_status,0,0);
    LONG active=InterlockedCompareExchange(&home_active_fields,0,0);
    static LONG old_mask,old_fields,old_switches,old_status,old_source,old_active,old_observed;
    if(scale_menu.label && scale_menu.label_key==key && rows.mask==old_mask && rows.fields==old_fields &&
       observed==old_observed && active==old_active && rows.switches==old_switches && status==old_status && source==old_source)return TRUE;
    BITMAPINFO bitmap={0}; bitmap.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    bitmap.bmiHeader.biWidth=W; bitmap.bmiHeader.biHeight=-H;
    bitmap.bmiHeader.biPlanes=1; bitmap.bmiHeader.biBitCount=32;
    HDC dc=CreateCompatibleDC(NULL); if(!dc) return FALSE;
    void* pixels=NULL;
    HBITMAP dib=CreateDIBSection(dc,&bitmap,DIB_RGB_COLORS,&pixels,NULL,0);
    if(!dib || !pixels){if(dib)DeleteObject(dib);DeleteDC(dc);return FALSE;}
    HGDIOBJ prior=SelectObject(dc,dib);
    memset(pixels,20,W*H*4);
    HFONT font=CreateFontW(-26,0,0,0,FW_MEDIUM,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,ANTIALIASED_QUALITY,0,L"Segoe UI");
    HGDIOBJ old_font=font?SelectObject(dc,font):NULL;
    SetTextColor(dc,RGB(246,246,246)); SetBkMode(dc,TRANSPARENT);
    WCHAR language[16]=L"";
    WCHAR pref[MAX_PATH]; DWORD count=GetEnvironmentVariableW(L"LOCALAPPDATA",pref,MAX_PATH);
    if(count && count<MAX_PATH && !wcscat_s(pref,MAX_PATH,L"\\UmaVR\\ui-language.txt")){
        HANDLE lang=CreateFileW(pref,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_EXISTING,0,NULL);
        if(lang!=INVALID_HANDLE_VALUE){
            char code[12]={0};DWORD bytes=0;
            if(ReadFile(lang,code,sizeof(code)-1,&bytes,NULL)) MultiByteToWideChar(CP_UTF8,0,code,-1,language,16);
            CloseHandle(lang);
        }
    }
    LANGID lang=GetUserDefaultUILanguage();
    BOOL ko=wcsstr(language,L"ko")!=NULL || (!*language && PRIMARYLANGID(lang)!=LANG_ENGLISH && PRIMARYLANGID(lang)!=LANG_JAPANESE);
    BOOL ja=wcsstr(language,L"ja")!=NULL || (!*language && PRIMARYLANGID(lang)==LANG_JAPANESE);
    const COLORREF ink=RGB(241,246,252),accent=RGB(118,234,209),muted=RGB(165,182,205);
    menu_box(dc,menu_rect(0,0,W,H),RGB(16,26,42));
    menu_box(dc,menu_rect(0,0,W,6),RGB(46,177,154));
    WCHAR title[128];
    swprintf_s(title,96,L"%ls   /   %ls       %d / %d",scale_menu.home?L"Home":L"Live",
        scale_menu.effects?L"VFX":ko?L"VR 설정":ja?L"VR 設定":L"VR settings",scale_menu.page+1,rows.pages);
    menu_text(dc,menu_rect(18,14,920,64),title,ink,DT_LEFT);
    for(int line=0;line<MENU_ROWS && scale_menu.page*MENU_ROWS+line<rows.count;line++){
        int id=rows.items[scale_menu.page*MENU_ROWS+line];RECT rect=row_rect(line);
        menu_box(dc,rect,scale_menu.hover==id?RGB(31,92,95):RGB(32,49,72));
        const WCHAR* name;const WCHAR* value;
        if(scale_menu.effects){
            const VfxOption* option=&live_vfx_options[id];swprintf_s(title,128,L"%ls   ·   %ls",vfx_group_names[option->group][ko?0:ja?2:1],effect_name(option,ko,ja));name=title;
            value=!option->key?(ko?L"조회":ja?L"読取専用":L"Read only"):
                ((scale_menu.home && !id)?rows.master:!!(rows.mask&(1L<<id)))?(ko?L"원본":ja?L"標準":L"Default"):L"OFF";
            if(scale_menu.home && !id){name=ko?L"양안 후처리":ja?L"両眼の後処理":L"Stereo post-processing";value=rows.master?L"ON":L"OFF";}
            if(scale_menu.home && id && (rows.mask&(1L<<id)) && !(active&(1L<<id)))
                value=ko?L"원본 · 대기":ja?L"標準 · 待機":L"Default · idle";
            if(id>=4 && id<=6 && !(rows.mask&(1L<<3)))value=ko?L"묶음 OFF":ja?L"複合 OFF":L"Group OFF";
        }else if(id==VFX_PAGE){name=ko?L"시각 효과":ja?L"視覚効果":L"Visual effects";value=L"VFX   ›";}
        else if(id==FOLLOW_PAGE){name=ko?L"현재 Live 카메라 추종":ja?L"カメラ追従":L"Camera follow";value=rows.follow?L"ON":L"OFF";}
        else if(id>=FIRST_NUMBER){
            const MenuNumber* num=&numbers[id-FIRST_NUMBER];
            swprintf_s(title,128,L"%ls    %.2f",ko?num->ko:ja?num->ja:num->en,(double)*num->value);
            name=title;value=L"";
            for(int k=0;k<2;k++){RECT b=scale_rect(k+1,line);
                menu_box(dc,b,scale_menu.hover==NUM_BUTTON+2*(id-FIRST_NUMBER)+k?RGB(31,115,110):RGB(49,70,96));
                menu_text(dc,b,k?L"+":L"−",ink,DT_CENTER);}
        }
        else if(id){const VrMenuSwitch* opt=&menu_switches[id-1];name=ko?opt->ko:ja?opt->ja:opt->en;value=*opt->value?L"ON":L"OFF";}
        else{
            const WCHAR* label=scale_menu.home?(ko?L"눈 해상도 (재시작)":ja?L"描画倍率（再起動）":L"Eye resolution (restart)"):(ko?L"눈 해상도 (Live)":ja?L"描画倍率（ライブ）":L"Eye resolution (Live)");
            swprintf_s(title,96,L"%ls    %ld%%",label,scale_menu.selected);
            name=title;value=L"";
            for(int k=0;k<3;k++){RECT b=scale_rect(k,line);int code=k==0?BUTTON_MINUS:k==1?BUTTON_PLUS:0;
                menu_box(dc,b,scale_menu.hover==code?RGB(31,115,110):RGB(49,70,96));
                menu_text(dc,b,k==0?L"−":k==1?L"+":scale_menu.home?(ko?L"저장":ja?L"保存":L"Save"):ko?L"적용":ja?L"適用":L"Apply",ink,DT_CENTER);}
        }
        RECT left=rect;left.right=550;menu_text(dc,left,name,ink,DT_LEFT);
        if(*value){rect.left=565;menu_text(dc,rect,value,accent,DT_RIGHT);}
    }
    const WCHAR* hint=scale_menu.failed || status?(ko?L"적용 실패 · 이전 상태 확인":ja?L"適用失敗":L"Apply failed"):
        scale_menu.pending?(ko?L"적용 중…":ja?L"適用中…":L"Applying…"):
        scale_menu.effects && scale_menu.home && !rows.master?(ko?L"Home 전체 OFF · 개별 설정은 저장됨":ja?L"ホーム全体 OFF · 個別設定は保存済み":L"Home master OFF · individual choices saved"):
        scale_menu.effects && !scale_menu.home && !(rows.mask&1)?(ko?L"Live 전체 후처리 OFF":ja?L"ライブ後処理 OFF":L"Live master OFF"):
        scale_menu.home && scale_menu.effects && rows.count==1?(ko?L"현재 양안에서 확인된 개별 효과 없음":ja?L"両眼の個別効果なし":L"No individual stereo effect detected"):
        scale_menu.effects?(ko?L"선택 즉시 저장·반영 · 원본은 장면 값을 사용":ja?L"選択時に保存・反映 · 標準はシーン設定":L"Changes save immediately · Default follows the scene"):
        ko?L"레이로 가리킨 뒤 트리거로 선택":ja?L"レイで指し、トリガーで選択":L"Point and squeeze the trigger to select";
    if(scale_menu.home && scale_menu.effects){
        const WCHAR* state=observed<0?L"?":observed==3?L"ON":observed==0?L"OFF":ko?L"좌우 불일치":ja?L"左右不一致":L"L/R mismatch";
        swprintf_s(title,128,ko?L"VR 카메라 %ls  ·  게임 원본(참고) %ls":ja?L"VRカメラ %ls  ·  ゲーム側（参考）%ls":L"VR cameras %ls  ·  Original game (info) %ls",
            state,source<0?L"?":source?L"ON":L"OFF");
        menu_text(dc,menu_rect(16,62,934,42),title,accent,DT_LEFT);
    }
    menu_text(dc,menu_rect(16,562,934,62),hint,accent,DT_LEFT);
    const WCHAR* footer[]={ko?L"이전":ja?L"前へ":L"Previous",ko?L"다음":ja?L"次へ":L"Next",ko?L"설정 목록":ja?L"設定一覧":L"Settings",ko?L"닫기":ja?L"閉じる":L"Close"};
    for(int i=0;i<4;i++){RECT b=footer_rect(i);
        BOOL enabled=(i!=0 || scale_menu.page>0)&&(i!=1 || scale_menu.page+1<rows.pages)&&(i!=2 || scale_menu.effects);
        menu_box(dc,b,scale_menu.hover==BUTTON_PREV+i?RGB(31,115,110):RGB(41,61,86));
        menu_text(dc,b,footer[i],enabled?ink:muted,DT_CENTER);
    }
    GdiFlush();
    D3D11_TEXTURE2D_DESC desc={0};desc.Width=W;desc.Height=H;desc.MipLevels=1;desc.ArraySize=1;
    desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_B8G8R8A8_UNORM;desc.Usage=D3D11_USAGE_IMMUTABLE;
    desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA data={pixels,W*4,0};ID3D11Texture2D* tex=NULL;
    ID3D11ShaderResourceView* srv=NULL;
    if(SUCCEEDED(ID3D11Device_CreateTexture2D(probe_device,&desc,&data,&tex))){
        ID3D11Device_CreateShaderResourceView(probe_device,(ID3D11Resource*)tex,NULL,&srv);
        ID3D11Texture2D_Release(tex);
    }
    if(font){SelectObject(dc,old_font);DeleteObject(font);}
    SelectObject(dc,prior);DeleteObject(dib);DeleteDC(dc);
    if(!srv)return FALSE;
    if(scale_menu.label) ID3D11ShaderResourceView_Release(scale_menu.label);
    old_observed=observed;old_active=active;old_fields=rows.fields;old_mask=rows.mask;old_switches=rows.switches;old_status=status;old_source=source;
    scale_menu.label=srv;scale_menu.label_key=key;return TRUE;
}

static void scale_cancel(void) {
    if(scale_menu.pending) InterlockedExchange(&scale_requested,scale_menu.saved);
    if(scale_menu.open){g_aux_panel_visible=scale_menu.prior_aux;scale_menu.swallow=TRUE;}
    scale_menu.pending=0;scale_menu.page=0;scale_menu.tracked=FALSE;scale_menu.hit=FALSE;scale_menu.effects=FALSE;scale_menu.open=FALSE;scale_menu.press=0;
}
static void scale_menu_input(XrTime time,BOOL immersive,BOOL synced) {
    BOOL active=FALSE;float value=0;
    if(!immersive || !synced || !game_window_is_foreground() ||
       !read_float_action_for_path(g_panel_grip_value_action,g_panel_hand_path,&active,&value) || !active){
        scale_cancel();scale_menu.held=TRUE;return;
    }
    if(scale_menu.generation!=xr_cache_telemetry.generation || scale_menu.home!=!!xr_cache_telemetry.owned){
        scale_menu.home=!!xr_cache_telemetry.owned;scale_menu.page=0;scale_menu.hover=-1;
        scale_cancel();scale_menu.held=TRUE;scale_menu.generation=xr_cache_telemetry.generation;
    }
    if(value<.35f){scale_menu.press=0;scale_menu.held=FALSE;}
    if(value>=.55f && !scale_menu.held && !scale_menu.open) {
        if(!scale_menu.press){scale_menu.press=time;scale_menu.prior_aux=g_aux_panel_visible;}
        if(time-scale_menu.press>=750000000){
            if(!scale_menu.saved)scale_menu.saved=scale_native;
            scale_menu.selected=scale_menu.saved;scale_menu.failed=FALSE;
            scale_menu.open=scale_menu_label();scale_menu.held=TRUE;
            if(scale_menu.open){release_synthetic_input();scale_menu.primary=scale_menu.back=TRUE;}
        }
    }
    if(!scale_menu.open) return;
    if(scale_menu.pending && GetTickCount64()>scale_menu.deadline){
        InterlockedExchange(&scale_requested,scale_menu.saved);
        scale_menu.pending=0;scale_menu.failed=TRUE;scale_menu.selected=scale_menu.saved;
    }
    XrSpaceLocation aim={.type=XR_TYPE_SPACE_LOCATION};
    XrSpaceLocationFlags valid=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
    scale_menu.tracked=panel_hand_mount(time,&scale_menu.pose);
    scale_menu.hit=scale_menu.tracked && XR_SUCCEEDED(pxrLocateSpace(g_aim_space,g_space,time,&aim)) && (aim.locationFlags&valid)==valid &&
        panel_ray_hit_pose(&aim.pose,&scale_menu.pose,AUX_PANEL_HEIGHT_M*MENU_W/MENU_H,AUX_PANEL_HEIGHT_M,&scale_menu.u,&scale_menu.v);
    MenuRows rows=menu_rows();
    scale_menu.hover=scale_menu.hit?menu_hit(&rows,scale_menu.u,scale_menu.v):-1;
    BOOL active_face=FALSE,primary=FALSE,back=FALSE,active_trigger=FALSE;float trigger=0;
    read_boolean_action(g_primary_face_action,&active_face,&primary);primary&=active_face;
    read_boolean_action(g_secondary_face_action,&active_face,&back);back&=active_face;
    read_float_action(g_trigger_action,&active_trigger,&trigger);
    BOOL pressed=primary || (active_trigger && trigger>=(scale_menu.primary?.30f:.65f));
    int id=scale_menu.hover;
    if(back && !scale_menu.back)id=scale_menu.effects?BUTTON_BACK:BUTTON_CLOSE;
    else if(!pressed || scale_menu.primary)id=-1;
    scale_menu.primary=pressed;scale_menu.back=back;
    if(id<0)return;
    scale_menu.failed=FALSE;
    if(id==BUTTON_CLOSE){scale_cancel();return;}
    if(id==BUTTON_BACK){scale_menu.effects=FALSE;scale_menu.page=0;return;}
    if(id==BUTTON_NEXT || id==BUTTON_PREV){scale_menu.page+=id==BUTTON_NEXT?1:-1;return;}
    if(scale_menu.pending)return;
    if(id>=NUM_BUTTON && id<NUM_BUTTON+2*NUMBER_COUNT){
        int index=(id-NUM_BUTTON)/2;const MenuNumber* num=&numbers[index];
        float old=*num->value,newval=fminf(num->max,fmaxf(num->min,old+((id-NUM_BUTTON)%2?num->step:-num->step)));
        scale_menu.failed=!write_menu_setting(0,num->key,FALSE,&newval,old);
        if(!scale_menu.failed){
            *num->value=newval;
            if(num->value==&saved_snap_degrees)nav_snap_angle_rad=newval*.01745329252f;
            scale_menu.label_key=-1;
        }
        return;
    }
    if(scale_menu.effects){
        LONG bit=1L<<id;
        if(scale_menu.home && id>0){
            LONG old=InterlockedCompareExchange(&home_vfx_disabled,0,0);
            scale_menu.failed=!(home_eye_menu_fields()&bit) || !save_menu_setting(old^bit,"homeVfxDisabled",old);
            if(!scale_menu.failed)InterlockedExchange(&home_vfx_disabled,old^bit);
        }else{
            const char* key=scale_menu.home?"homePostProcessingEnabled":live_vfx_options[id].key;
            if(!key)return;
            BOOL old=scale_menu.home?rows.master:!!(rows.mask&bit);
            scale_menu.failed=!save_menu_setting(0,key,old);
            if(!scale_menu.failed){if(scale_menu.home)InterlockedExchange(&home_vfx_allowed,!old);else InterlockedXor(&live_vfx_mask,bit);}
        }
    }else if(id==VFX_PAGE){scale_menu.effects=TRUE;scale_menu.page=0;}
    else if(id==FOLLOW_PAGE)(void)toggle_camera_follow(InterlockedCompareExchange(&follow_display_state,0,0));
    else if(id==BUTTON_MINUS || id==BUTTON_PLUS){
        LONG pct=scale_menu.selected+(id==BUTTON_PLUS?5:-5);
        scale_menu.selected=pct<50?50:pct>150?150:pct;
    }else if(!id){
        if(scale_menu.home){
            if(scale_menu.selected!=scale_menu.saved){
                scale_menu.failed=!save_menu_setting(scale_menu.selected,NULL,FALSE);
                if(!scale_menu.failed)scale_menu.saved=scale_menu.selected;
            }
        }else if(scale_menu.selected!=scale_menu.saved || xr_cache_telemetry.render_percent!=scale_menu.selected){scale_menu.pending=scale_menu.selected;scale_menu.deadline=GetTickCount64()+3000;InterlockedExchange(&scale_requested,scale_menu.pending);}
    }else{
        const VrMenuSwitch* opt=&menu_switches[id-1];BOOL old=*opt->value;
        scale_menu.failed=!save_menu_setting(0,opt->key,old);if(!scale_menu.failed)*opt->value=!old;
    }
}

static void scale_menu_accept(void) {
    if(!scale_menu.pending || xr_cache_telemetry.owned ||
       xr_cache_telemetry.generation!=scale_menu.generation ||
       xr_cache_telemetry.render_percent!=scale_menu.pending) return;
    if(save_menu_setting(scale_menu.pending,NULL,FALSE)) scale_menu.saved=scale_menu.selected;
    else {InterlockedExchange(&scale_requested,scale_menu.saved);scale_menu.selected=scale_menu.saved;scale_menu.failed=TRUE;}
    scale_menu.pending=0;
}

