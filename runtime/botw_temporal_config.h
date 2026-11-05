#pragma once
/* One restart-required preference drives both NX160 render dimensions and its
 * MetalFX temporal path. Parsing is shared by the C guest adapter and renderer;
 * no process, file-system or game-data mutation is performed here. */
#include <stddef.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#define BOTW_TEMPORAL_SETTING_FILE "graphics-upscaler.txt"

typedef struct {
    const char *token;
    int valid, enabled;
    unsigned input_width, input_height, output_width, output_height;
} BotwTemporalConfig;

static inline int botw_temporal_blank(char c) {
    return c==' ' || c=='\t' || c=='\n' || c=='\r';
}
static inline BotwTemporalConfig botw_temporal_parse(const char *text) {
    static const BotwTemporalConfig modes[] = {
        {"off",1,0,0,0,0,0},
        {"quality-1440",1,1,1920,1080,2560,1440},
        {"balanced-1440",1,1,1280,720,2560,1440},
        {"quality-2160",1,1,2560,1440,3840,2160},
        {"balanced-2160",1,1,1920,1080,3840,2160}
    };
    BotwTemporalConfig invalid={"off",0,0,0,0,0,0};
    if(!text) return modes[0];
    size_t length=strnlen(text,64);
    if(length==64)return invalid;
    size_t start=0;
    while(start<length && botw_temporal_blank(text[start]))start++;
    while(length>start && botw_temporal_blank(text[length-1]))length--;
    text+=start;length-=start;
    for(size_t i=0;i<sizeof modes/sizeof *modes;i++)
        if(strlen(modes[i].token)==length && !memcmp(text,modes[i].token,length))return modes[i];
    return invalid;
}
static inline BotwTemporalConfig botw_temporal_load(int (*reader)(const char *,char *,size_t)) {
    char text[64]={0};
    if(!reader || !reader(BOTW_TEMPORAL_SETTING_FILE,text,sizeof text))return botw_temporal_parse(NULL);
    text[sizeof text-1]=0;
    /* A full read may be a truncated prefix. Never accept a known token when
       unread bytes could contain a second command or an unknown suffix. */
    if(strlen(text)==sizeof text-1)return botw_temporal_parse("");
    return botw_temporal_parse(text);
}
static inline int botw_temporal_cache_matches(unsigned input_width,unsigned input_height,unsigned output_width,unsigned output_height,
                                             unsigned expected_input_width,unsigned expected_input_height,unsigned expected_output_width,unsigned expected_output_height) {
    return input_width==expected_input_width && input_height==expected_input_height &&
        output_width==expected_output_width && output_height==expected_output_height;
}
static inline int botw_temporal_fullscreen_draw(int indexed,int primitive,int first,int count,int instances,int base_instance,int indirect) {
    /* Shape is only a bounded candidate. The actual indexed positions still
       have to prove full coverage below; indexed draws are not trusted by type. */
    return (indexed==0 || indexed==1) && first==0 &&
        ((primitive==4 && (count==3 || count==6)) || (primitive==5 && count==4)) &&
        instances>=0 && instances<=1 && base_instance==0 && !indirect;
}
static inline int botw_temporal_index(const unsigned char *bytes,size_t length,int type,unsigned ordinal,uint32_t *out) {
    unsigned size=type==0?1:type==1?2:type==2?4:0;
    if(!bytes || !out || !size || ordinal>=length/size)return 0;
    uint32_t value=0;memcpy(&value,bytes+(size_t)ordinal*size,size);
    /* Metal's strip restart marker is not a vertex, even when a negative base
       vertex would map that integer into a readable range. */
    if(value==(size==1?UINT8_MAX:size==2?UINT16_MAX:UINT32_MAX))return 0;
    *out=value;return 1;
}
static inline int botw_temporal_uv_matches(const float position[4],const float uv[4]) {
    /* Positive Metal viewport: the top pixel samples texture row zero. The
       opposite Y orientation would flip the original full-screen draw. */
    return position && uv && isfinite(position[0]) && isfinite(position[1]) &&
        uv[0]==(position[0]+1)*.5f && uv[1]==(1-position[1])*.5f;
}
static inline int botw_temporal_host_update(uint64_t address,size_t size,uint64_t begin,uint64_t end,int kind,const void *host,void *out) {
    /* Called newest-write first. A fully retained inline update supplies the
       exact bytes the GPU will read. Unknown/partial/GPU-copy overlap refuses;
       it must never silently fall back to stale pool contents. */
    if(!size || address>UINT64_MAX-size || end<begin)return -1;
    if(address>=end || address+size<=begin)return 0;
    if(address<begin || address+size>end || kind!=0 || !host || !out)return -1;
    memcpy(out,(const unsigned char *)host+(size_t)(address-begin),size);return 1;
}
static inline int botw_temporal_pending_live(uint64_t fence,uint64_t completed,uint64_t skipped_eye) {
    return fence>completed && (!skipped_eye || fence!=skipped_eye);
}
static inline int botw_temporal_gpu_write_overlaps(uint64_t address,size_t size,uint64_t begin,uint64_t end,
                                                  uint64_t fence,uint64_t completed,uint64_t skipped_eye) {
    if(!botw_temporal_pending_live(fence,completed,skipped_eye))return 0;
    if(!size || address>UINT64_MAX-size || end<begin)return 1;
    return address<end && address+size>begin;
}
static inline int botw_temporal_bound_uniform(uint64_t bound_gpu,uint64_t original_gpu,const unsigned char *bound_bytes,
                                              size_t bound_size,size_t offset,void *out,size_t size) {
    /* A renamed/snapshot arena is the actual immutable GPU binding, independent
       of later guest-pool writes. Raw original-pool pointers need separate GPU
       write checks and cannot take this route. */
    if(!bound_gpu || bound_gpu==original_gpu || !bound_bytes || !out || !size || offset>bound_size || size>bound_size-offset)return 0;
    memcpy(out,bound_bytes+offset,size);return 1;
}
static inline double botw_temporal_edge(const float *a,const float *b,double x,double y) {
    return ((double)b[0]-a[0])*(y-a[1])-((double)b[1]-a[1])*(x-a[0]);
}
static inline int botw_temporal_mesh_covers(const float (*p)[4],int n,int primitive,int cull,int front_ccw) {
    if(!p || !((primitive==4 && (n==3 || n==6)) || (primitive==5 && n==4)) || cull<0 || cull>2)return 0;
    for(int i=0;i<n;i++)if(!isfinite(p[i][0]) || !isfinite(p[i][1]) || !(p[i][2]>=0 && p[i][2]<=1) || p[i][3]!=1)return 0;
    unsigned masks[2]={0,0};int triangles=primitive==4?n/3:n-2;
    for(int t=0;t<triangles;t++) {
        int a=primitive==4?t*3:t,b=a+1,c=a+2;
        if(primitive==5 && (t&1)){int swap=b;b=c;c=swap;}
        double area=botw_temporal_edge(p[a],p[b],p[c][0],p[c][1]);
        int front=front_ccw?area>0:area<0;
        if(area==0 || (cull==1 && front) || (cull==2 && !front))return 0;
        int full=1;
        for(int corner=0;corner<4;corner++) {
            double x=(corner&1)?1:-1,y=(corner&2)?1:-1;
            double e0=botw_temporal_edge(p[a],p[b],x,y),e1=botw_temporal_edge(p[b],p[c],x,y),e2=botw_temporal_edge(p[c],p[a],x,y);
            if(area>0?(e0<0 || e1<0 || e2<0):(e0>0 || e1>0 || e2>0))full=0;
        }
        if(full)return 1; /* A giant triangle independently covers the canvas. */
        int ids[3]={a,b,c};
        for(int j=0;j<3;j++) {
            const float *v=p[ids[j]];
            if(!((v[0]==-1 || v[0]==1) && (v[1]==-1 || v[1]==1)))return 0;
            unsigned bit=1u<<((v[0]>0?1:0)|(v[1]>0?2:0));
            if(masks[t]&bit)return 0;masks[t]|=bit;
        }
    }
    if(triangles!=2 || (masks[0]|masks[1])!=15)return 0;
    unsigned shared=masks[0]&masks[1];
    /* Two non-culled corner triangles sharing either rectangle diagonal cover
       the entire square. Adjacent shared corners would overlap and leave a hole. */
    return shared==((1u<<0)|(1u<<3)) || shared==((1u<<1)|(1u<<2));
}
