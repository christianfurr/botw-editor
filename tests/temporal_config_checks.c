#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "botw_temporal_config.h"

static unsigned checks;
#define CHECK(condition) do { checks++; if(!(condition)) { fprintf(stderr,"line %d: %s\n",__LINE__,#condition); exit(1); } } while(0)

static void mode(const char *text,const char *token,int valid,int enabled,unsigned iw,unsigned ih,unsigned ow,unsigned oh) {
    BotwTemporalConfig value=botw_temporal_parse(text);
    CHECK(!strcmp(value.token,token));CHECK(value.valid==valid);CHECK(value.enabled==enabled);
    CHECK(value.input_width==iw);CHECK(value.input_height==ih);
    CHECK(value.output_width==ow);CHECK(value.output_height==oh);
}
static const char *reader_text;
static unsigned reads;
static int reader(const char *name,char *text,size_t capacity) {
    CHECK(!strcmp(name,"graphics-upscaler.txt"));CHECK(capacity==64);reads++;
    if(!reader_text)return 0;
    size_t size=strlen(reader_text);if(size>=capacity)size=capacity-1;
    memcpy(text,reader_text,size);text[size]=0;return 1;
}
int main(void) {
    mode(NULL,"off",1,0,0,0,0,0);mode("off","off",1,0,0,0,0,0);
    mode("quality-1440","quality-1440",1,1,1920,1080,2560,1440);
    mode("balanced-1440","balanced-1440",1,1,1280,720,2560,1440);
    mode("quality-2160","quality-2160",1,1,2560,1440,3840,2160);
    mode("balanced-2160","balanced-2160",1,1,1920,1080,3840,2160);
    mode(" \t\nquality-2160\r\n ","quality-2160",1,1,2560,1440,3840,2160);
    const char *bad[]={""," ","QUALITY-1440","quality-1440 junk","quality-1440\noff","quality-144","quality-14400","../quality-1440","quality-1440;off","quality-1440\v"};
    for(size_t i=0;i<sizeof bad/sizeof *bad;i++)mode(bad[i],"off",0,0,0,0,0,0);
    char long_text[96];memset(long_text,' ',sizeof long_text);long_text[95]=0;
    mode(long_text,"off",0,0,0,0,0,0);
    char unterminated[64];memset(unterminated,' ',sizeof unterminated);
    mode(unterminated,"off",0,0,0,0,0,0); /* ASan proves the bounded whitespace scan. */
    BotwTemporalConfig loaded=botw_temporal_load(NULL);CHECK(loaded.valid&&!loaded.enabled);CHECK(reads==0);
    reader_text=NULL;loaded=botw_temporal_load(reader);CHECK(loaded.valid&&!loaded.enabled);CHECK(reads==1);
    reader_text="balanced-2160\n";loaded=botw_temporal_load(reader);CHECK(loaded.enabled&&loaded.input_width==1920&&loaded.output_width==3840);
    memcpy(long_text,"quality-1440",12);reader_text=long_text;loaded=botw_temporal_load(reader);CHECK(!loaded.valid&&!loaded.enabled);
    CHECK(botw_temporal_cache_matches(1920,1080,2560,1440,1920,1080,2560,1440));
    CHECK(!botw_temporal_cache_matches(1920,1080,2560,1440,1920,1080,3840,2160)); /* Same input, wrong output must withhold jitter. */
    CHECK(!botw_temporal_cache_matches(1280,720,2560,1440,1920,1080,2560,1440));
    CHECK(!botw_temporal_cache_matches(1920,1080,2560,1440,1920,720,2560,1440));
    CHECK(!botw_temporal_cache_matches(1920,1080,2560,1440,1920,1080,2560,2160));
    CHECK(botw_temporal_fullscreen_draw(0,4,0,3,0,0,0));
    CHECK(botw_temporal_fullscreen_draw(0,4,0,3,1,0,0));
    CHECK(botw_temporal_fullscreen_draw(1,4,0,3,1,0,0));
    CHECK(botw_temporal_fullscreen_draw(1,4,0,6,1,0,0));
    CHECK(botw_temporal_fullscreen_draw(1,5,0,4,1,0,0));
    CHECK(!botw_temporal_fullscreen_draw(0,5,0,3,1,0,0));
    CHECK(!botw_temporal_fullscreen_draw(0,4,1,3,1,0,0));
    CHECK(!botw_temporal_fullscreen_draw(0,4,0,9,1,0,0));
    CHECK(!botw_temporal_fullscreen_draw(0,4,0,3,2,0,0));
    CHECK(!botw_temporal_fullscreen_draw(0,4,0,3,-1,0,0));
    CHECK(!botw_temporal_fullscreen_draw(0,4,0,3,1,1,0));
    CHECK(!botw_temporal_fullscreen_draw(0,4,0,3,1,0,1));
    unsigned char i8[]={0,2,1,2,3,1};uint16_t i16[]={0,2,1,2,3,1};uint32_t i32[]={0,2,1,2,3,1},value;
    for(unsigned i=0;i<6;i++) {
        CHECK(botw_temporal_index(i8,sizeof i8,0,i,&value)&&value==i8[i]);
        CHECK(botw_temporal_index((unsigned char *)i16,sizeof i16,1,i,&value)&&value==i8[i]);
        CHECK(botw_temporal_index((unsigned char *)i32,sizeof i32,2,i,&value)&&value==i8[i]);
    }
    CHECK(!botw_temporal_index(i8,sizeof i8,0,6,&value));
    CHECK(!botw_temporal_index((unsigned char *)i16,sizeof i16-1,1,5,&value));
    CHECK(!botw_temporal_index((unsigned char *)i32,sizeof i32-1,2,5,&value));
    CHECK(!botw_temporal_index(i8,sizeof i8,3,0,&value));
    CHECK(!botw_temporal_index(NULL,sizeof i8,0,0,&value));
    CHECK(!botw_temporal_index(i8,sizeof i8,0,0,NULL));
    unsigned char restart8=UINT8_MAX;uint16_t restart16=UINT16_MAX;uint32_t restart32=UINT32_MAX;
    CHECK(!botw_temporal_index(&restart8,sizeof restart8,0,0,&value));
    CHECK(!botw_temporal_index((unsigned char *)&restart16,sizeof restart16,1,0,&value));
    CHECK(!botw_temporal_index((unsigned char *)&restart32,sizeof restart32,2,0,&value));
    float top_left[4]={-1,1,.5,1},top_uv[4]={0,0,0,0},bottom_left[4]={-1,-1,.5,1},bottom_uv[4]={0,1,0,0};
    CHECK(botw_temporal_uv_matches(top_left,top_uv));
    CHECK(botw_temporal_uv_matches(bottom_left,bottom_uv));
    CHECK(!botw_temporal_uv_matches(top_left,bottom_uv));
    CHECK(!botw_temporal_uv_matches(bottom_left,top_uv));
    top_uv[0]=1;CHECK(!botw_temporal_uv_matches(top_left,top_uv));top_uv[0]=0;
    top_uv[1]=NAN;CHECK(!botw_temporal_uv_matches(top_left,top_uv));top_uv[1]=0;
    CHECK(!botw_temporal_uv_matches(NULL,top_uv));
    CHECK(!botw_temporal_uv_matches(top_left,NULL));
    unsigned char host_bytes[16];for(unsigned i=0;i<sizeof host_bytes;i++)host_bytes[i]=(unsigned char)(20+i);
    unsigned char resolved[4]={0};
    CHECK(botw_temporal_host_update(104,4,100,116,0,host_bytes,resolved)==1);
    CHECK(!memcmp(resolved,host_bytes+4,4));
    memset(resolved,0,sizeof resolved);
    CHECK(botw_temporal_host_update(116,4,100,116,0,host_bytes,resolved)==0);
    CHECK(botw_temporal_host_update(96,4,100,116,0,host_bytes,resolved)==0);
    CHECK(botw_temporal_host_update(98,4,100,116,0,host_bytes,resolved)==-1);
    CHECK(botw_temporal_host_update(114,4,100,116,0,host_bytes,resolved)==-1);
    CHECK(botw_temporal_host_update(104,4,100,116,1,host_bytes,resolved)==-1);
    CHECK(botw_temporal_host_update(104,4,100,116,0,NULL,resolved)==-1);
    CHECK(botw_temporal_host_update(104,4,100,116,0,host_bytes,NULL)==-1);
    CHECK(botw_temporal_host_update(UINT64_MAX-1,4,100,116,0,host_bytes,resolved)==-1);
    CHECK(botw_temporal_host_update(104,4,116,100,0,host_bytes,resolved)==-1);
    CHECK(botw_temporal_host_update(104,0,100,116,0,host_bytes,resolved)==-1);
    CHECK(botw_temporal_pending_live(12,11,0));
    CHECK(!botw_temporal_pending_live(11,11,0));
    CHECK(!botw_temporal_pending_live(10,11,0));
    CHECK(!botw_temporal_pending_live(12,11,12));
    CHECK(botw_temporal_pending_live(12,11,13));
    CHECK(botw_temporal_gpu_write_overlaps(104,4,100,116,12,11,0));
    CHECK(!botw_temporal_gpu_write_overlaps(104,4,100,116,11,11,0));
    CHECK(!botw_temporal_gpu_write_overlaps(104,4,100,116,12,11,12));
    CHECK(!botw_temporal_gpu_write_overlaps(116,4,100,116,12,11,0));
    CHECK(botw_temporal_gpu_write_overlaps(98,4,100,116,12,11,0));
    CHECK(botw_temporal_gpu_write_overlaps(UINT64_MAX-1,4,100,116,12,11,0));
    CHECK(botw_temporal_gpu_write_overlaps(104,4,116,100,12,11,0));
    unsigned char immutable_binding[128]={0};float known_depth=.25f,actual_depth=-1;
    memcpy(immutable_binding+84,&known_depth,sizeof known_depth);
    CHECK(botw_temporal_bound_uniform(2000,1000,immutable_binding,sizeof immutable_binding,84,&actual_depth,sizeof actual_depth));
    CHECK(actual_depth==known_depth);
    actual_depth=-1;
    CHECK(!botw_temporal_bound_uniform(1000,1000,immutable_binding,sizeof immutable_binding,84,&actual_depth,sizeof actual_depth));
    CHECK(actual_depth==-1); /* Original-pool bytes may still have a GPU write. */
    CHECK(!botw_temporal_bound_uniform(0,1000,immutable_binding,sizeof immutable_binding,84,&actual_depth,sizeof actual_depth));
    CHECK(!botw_temporal_bound_uniform(2000,1000,NULL,sizeof immutable_binding,84,&actual_depth,sizeof actual_depth));
    CHECK(!botw_temporal_bound_uniform(2000,1000,immutable_binding,sizeof immutable_binding,127,&actual_depth,sizeof actual_depth));
    CHECK(!botw_temporal_bound_uniform(2000,1000,immutable_binding,sizeof immutable_binding,SIZE_MAX,&actual_depth,sizeof actual_depth));
    CHECK(!botw_temporal_bound_uniform(2000,1000,immutable_binding,sizeof immutable_binding,84,NULL,sizeof actual_depth));
    CHECK(!botw_temporal_bound_uniform(2000,1000,immutable_binding,sizeof immutable_binding,84,&actual_depth,0));
    float giant[3][4]={{-1,-1,.5,1},{3,-1,.5,1},{-1,3,.5,1}};
    CHECK(botw_temporal_mesh_covers(giant,3,4,0,1));
    CHECK(botw_temporal_mesh_covers(giant,3,4,2,1));
    CHECK(!botw_temporal_mesh_covers(giant,3,4,1,1));
    CHECK(!botw_temporal_mesh_covers(giant,3,4,3,1));
    float strip[4][4]={{-1,-1,.5,1},{1,-1,.5,1},{-1,1,.5,1},{1,1,.5,1}};
    CHECK(botw_temporal_mesh_covers(strip,4,5,0,1));
    CHECK(botw_temporal_mesh_covers(strip,4,5,2,1));
    CHECK(!botw_temporal_mesh_covers(strip,4,5,1,1));
    float triangles[6][4];
    for(int i=0;i<6;i++)memcpy(triangles[i],strip[i8[i]],sizeof triangles[i]);
    CHECK(botw_temporal_mesh_covers(triangles,6,4,0,1));
    CHECK(botw_temporal_mesh_covers(triangles,6,4,2,0));
    CHECK(!botw_temporal_mesh_covers(triangles,6,4,2,1));
    /* Half a rectangle, overlapping halves, a gap and a culled half must all refuse. */
    CHECK(!botw_temporal_mesh_covers(strip,3,4,0,1));
    memcpy(triangles[3],triangles[0],sizeof triangles[0]);
    memcpy(triangles[4],triangles[1],sizeof triangles[0]);
    memcpy(triangles[5],triangles[2],sizeof triangles[0]);
    CHECK(!botw_temporal_mesh_covers(triangles,6,4,0,1));
    for(int i=0;i<6;i++)memcpy(triangles[i],strip[i8[i]],sizeof triangles[i]);
    triangles[5][0]=.9f;CHECK(!botw_temporal_mesh_covers(triangles,6,4,0,1));
    strip[3][3]=2;CHECK(!botw_temporal_mesh_covers(strip,4,5,0,1));strip[3][3]=1;
    strip[3][2]=2;CHECK(!botw_temporal_mesh_covers(strip,4,5,0,1));strip[3][2]=.5f;
    strip[3][0]=NAN;CHECK(!botw_temporal_mesh_covers(strip,4,5,0,1));strip[3][0]=1;
    CHECK(!botw_temporal_mesh_covers(strip,4,4,0,1));
    CHECK(!botw_temporal_mesh_covers(NULL,4,5,0,1));
    printf("temporal configuration/cache/draw guards: %u checks passed\n",checks);
    return 0;
}
