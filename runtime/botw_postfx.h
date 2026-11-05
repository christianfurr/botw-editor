#pragma once
/* Optional BOTW 1.6.0 Mac output filters. Off keeps the original presentation
 * pipeline. This grades the composited picture (including the game's HUD),
 * never guest shaders or memory; the runtime's FPS overlays use their old PSO. */
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <TargetConditionals.h>
#include <pthread.h>
#include <sys/stat.h>
#include <math.h>
#include <unistd.h>

#if defined(GAME_BOTW) && !defined(GAME_BOTW190) && TARGET_OS_OSX
static pthread_mutex_t bpf_lock = PTHREAD_MUTEX_INITIALIZER;
static struct {
    double next_poll, revision;
    int requested, applied, ready, configured;
    float strength, sharpness;
    NSString *signature, *custom, *error, *stage;
    id<MTLRenderPipelineState> pipelines[4];
    MTLPixelFormat formats[4];
} bpf;
#ifdef BOTW_POSTFX_TEST
static NSString *bpf_test_directory;
#endif
static NSString *bpf_directory(void) {
#ifdef BOTW_POSTFX_TEST
    if (bpf_test_directory) return bpf_test_directory;
#endif
    return [NSHomeDirectory() stringByAppendingPathComponent:@"Library/Application Support/BOTWEditor/shaders"];
}
static NSString *bpf_name(int n) {
    return n == 1 ? @"natural" : n == 2 ? @"vivid" : n == 3 ? @"cinematic" : n == 4 ? @"custom" : @"off";
}
static int bpf_number(id value, double low, double high) {
    return [value isKindOfClass:NSNumber.class] && CFGetTypeID((__bridge CFTypeRef)value) != CFBooleanGetTypeID() &&
        isfinite([value doubleValue]) && [value doubleValue] >= low && [value doubleValue] <= high;
}
static NSString *bpf_stamp(NSString *path) {
    struct stat s;
    if (stat(path.fileSystemRepresentation, &s)) return @"missing";
    return [NSString stringWithFormat:@"%lld:%lld:%ld:%lld",(long long)s.st_ino,(long long)s.st_mtimespec.tv_sec,s.st_mtimespec.tv_nsec,(long long)s.st_size];
}
static void bpf_status(void) {
    NSString *directory = bpf_directory();
    [[NSFileManager defaultManager] createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions:@0700} error:nil];
    NSDictionary *state = @{@"schema":@1,@"version":@"1.6.0",@"supported":@YES,@"revision":@(bpf.revision),
        @"requested_preset":bpf_name(bpf.requested),@"applied_preset":bpf_name(bpf.applied),
        @"strength":@(bpf.strength),@"sharpness":@(bpf.sharpness),@"error":bpf.error ?: @"",
        @"stage":bpf.stage ?: @"off",@"pid":@((int)getpid()),@"updated_at":@([NSDate date].timeIntervalSince1970)};
    NSData *json = [NSJSONSerialization dataWithJSONObject:state options:0 error:nil];
    NSString *path = [directory stringByAppendingPathComponent:@"status.json"];
    if ([json writeToFile:path options:NSDataWritingAtomic error:nil])
        [[NSFileManager defaultManager] setAttributes:@{NSFilePosixPermissions:@0600} ofItemAtPath:path error:nil];
}
static void bpf_poll(void) {
    double now = [NSDate timeIntervalSinceReferenceDate];
    if (now < bpf.next_poll) return;
    bpf.next_poll = now + 1;
    NSString *directory = bpf_directory(), *config = [directory stringByAppendingPathComponent:@"settings.json"];
    NSString *custom = [directory stringByAppendingPathComponent:@"custom.metal"];
    NSString *signature = [NSString stringWithFormat:@"%@|%@",bpf_stamp(config),bpf_stamp(custom)];
    if ([signature isEqualToString:bpf.signature]) { bpf_status(); return; }
    bpf.signature = signature; bpf.applied = 0; bpf.ready = 0; bpf.error = @""; bpf.stage = @"off";
    bpf.requested = 0; bpf.strength = 0; bpf.sharpness = 0; bpf.revision = 0; bpf.custom = nil;
    for (int i = 0; i < 4; i++) bpf.pipelines[i] = nil;
    if ([[NSFileManager defaultManager] fileExistsAtPath:config]) {
        NSDictionary *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:config error:nil];
        NSData *data = [attributes fileSize] <= 4096 ? [NSData dataWithContentsOfFile:config] : nil;
        id value = data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
        NSDictionary *d = [value isKindOfClass:NSDictionary.class] ? value : nil;
        NSArray *names = @[@"off",@"natural",@"vivid",@"cinematic",@"custom"];
        NSUInteger index = [d[@"preset"] isKindOfClass:NSString.class] ? [names indexOfObject:d[@"preset"]] : NSNotFound;
        if (bpf_number(d[@"revision"],0,9007199254740991.0) && floor([d[@"revision"] doubleValue]) == [d[@"revision"] doubleValue]) bpf.revision = [d[@"revision"] doubleValue];
        if (!bpf_number(d[@"schema"],1,1) || index == NSNotFound || !bpf_number(d[@"strength"],0,1) ||
            !bpf_number(d[@"sharpness"],0,1) || !bpf_number(d[@"revision"],0,9007199254740991.0) || floor([d[@"revision"] doubleValue]) != [d[@"revision"] doubleValue]) {
            bpf.error = @"Invalid shader settings. The original picture is unchanged."; bpf.stage = @"error";
        } else {
            bpf.requested = (int)index; bpf.strength = [d[@"strength"] floatValue]; bpf.sharpness = [d[@"sharpness"] floatValue];
            if (index == 4) {
                NSDictionary *a = [[NSFileManager defaultManager] attributesOfItemAtPath:custom error:nil];
                if (a && [a fileSize] <= 65536) bpf.custom = [NSString stringWithContentsOfFile:custom encoding:NSUTF8StringEncoding error:nil];
                if (!bpf.custom.length) { bpf.error = @"Custom shader missing, unreadable, or over 64 KB. The original picture is unchanged."; bpf.stage = @"error"; }
            }
            if (!bpf.error.length) { bpf.ready = 1; bpf.stage = bpf.requested ? @"ready" : @"off"; }
        }
    } else { bpf.ready = 1; }
    bpf.configured = 1;
    bpf_status();
}
static NSString *bpf_source(void) {
    NSString *custom = bpf.requested == 4 ? bpf.custom : @"float3 customGrade(float3 color,float2 uv){return color;}";
    return [@"#include <metal_stdlib>\nusing namespace metal;\n"
        "struct V{float4 p [[position]];float2 uv;};\n"
        "vertex V bpf_vs(uint id [[vertex_id]]){float2 u=float2((id<<1)&2,id&2);return{float4(u*float2(2,-2)+float2(-1,1),0,1),u};}\n"
        "struct Crop{float4 rect;float4 edge;float4 post;};\n"
        stringByAppendingFormat:@"%@\n"
        "fragment float4 bpf_fs(V in [[stage_in]],texture2d<float> t [[texture(0)]],sampler s [[sampler(0)]],constant Crop& c [[buffer(0)]]){\n"
        "float2 uv=clamp(in.uv*c.rect.zw+c.rect.xy,c.edge.xy,c.edge.zw);float4 base=t.sample(s,uv);float3 rgb=base.rgb;\n"
        "if(c.post.y>0){float2 px=1.0/float2(t.get_width(),t.get_height());\n"
        "float3 n=t.sample(s,clamp(uv+float2(0,px.y),c.edge.xy,c.edge.zw)).rgb;float3 a=t.sample(s,clamp(uv-float2(0,px.y),c.edge.xy,c.edge.zw)).rgb;\n"
        "float3 e=t.sample(s,clamp(uv+float2(px.x,0),c.edge.xy,c.edge.zw)).rgb;float3 w=t.sample(s,clamp(uv-float2(px.x,0),c.edge.xy,c.edge.zw)).rgb;\n"
        "float3 lo=min(rgb,min(min(n,a),min(e,w))),hi=max(rgb,max(max(n,a),max(e,w)));rgb=clamp(rgb+(rgb-(n+a+e+w)*0.25)*c.post.y*0.5,lo,hi);}\n"
        "float3 grade=rgb;int mode=int(c.post.z);\n"
        "if(mode==1){float y=dot(rgb,float3(0.2126,0.7152,0.0722));grade=(mix(float3(y),rgb,1.06)-0.5)*1.035+0.5;}\n"
        "else if(mode==2){float y=dot(rgb,float3(0.2126,0.7152,0.0722));grade=(mix(float3(y),rgb,1.18)-0.5)*1.07+0.5;}\n"
        "else if(mode==3){float y=dot(rgb,float3(0.2126,0.7152,0.0722));grade=((mix(float3(y),rgb,0.94)-0.5)*1.055+0.5)*float3(1.025,1.0,0.975);}\n"
        "else if(mode==4){grade=customGrade(rgb,uv);}\n"
        "grade=select(grade,rgb,!isfinite(grade));return float4(clamp(mix(rgb,grade,c.post.x),0.0,1.0),base.a);}\n",custom];
}
/* Called under the presenter lock below. Compilation occurs once per changed
 * config/custom file, never every frame. Invalid user MSL cannot take down play. */
static id<MTLRenderPipelineState> bpf_pipeline(MTLPixelFormat format, float *values) {
    pthread_mutex_lock(&bpf_lock);
    bpf_poll();
    values[0] = bpf.strength; values[1] = bpf.sharpness; values[2] = (float)bpf.requested; values[3] = 0;
    if (!bpf.ready || !bpf.requested) { pthread_mutex_unlock(&bpf_lock); return nil; }
    if (bpf.strength == 0 && bpf.sharpness == 0) {
        if (bpf.applied != bpf.requested) { bpf.applied = bpf.requested; bpf.stage = @"applied"; bpf_status(); }
        pthread_mutex_unlock(&bpf_lock); return nil;
    }
    if (format != MTLPixelFormatBGRA8Unorm && format != MTLPixelFormatRGBA8Unorm && format != MTLPixelFormatBGRA8Unorm_sRGB && format != MTLPixelFormatRGBA8Unorm_sRGB) {
        if (![bpf.stage isEqualToString:@"error"]) { bpf.applied = 0; bpf.error = @"This filter requires an SDR output surface. The original picture is unchanged."; bpf.stage = @"error"; bpf_status(); }
        pthread_mutex_unlock(&bpf_lock); return nil;
    }
    int slot = -1;
    for (int i = 0; i < 4; i++) {
        if (bpf.pipelines[i] && bpf.formats[i] == format) { id<MTLRenderPipelineState> p = bpf.pipelines[i]; pthread_mutex_unlock(&bpf_lock); return p; }
        if (!bpf.pipelines[i] && slot < 0) slot = i;
    }
    NSError *error = nil;
    MTLCompileOptions *options = [MTLCompileOptions new]; options.mathMode = MTLMathModeSafe;
    id<MTLLibrary> library = [g_dev newLibraryWithSource:bpf_source() options:options error:&error];
    MTLRenderPipelineDescriptor *descriptor = [MTLRenderPipelineDescriptor new];
    descriptor.vertexFunction = [library newFunctionWithName:@"bpf_vs"]; descriptor.fragmentFunction = [library newFunctionWithName:@"bpf_fs"];
    descriptor.colorAttachments[0].pixelFormat = format;
    id<MTLRenderPipelineState> pipeline = library ? [g_dev newRenderPipelineStateWithDescriptor:descriptor error:&error] : nil;
    if (!pipeline || slot < 0) {
        bpf.ready = 0; bpf.applied = 0; bpf.error = error.localizedDescription ?: @"Could not create the custom shader pipeline.";
        if (bpf.error.length > 2048) bpf.error = [bpf.error substringToIndex:2048];
        bpf.stage = @"error"; bpf_status(); pthread_mutex_unlock(&bpf_lock); return nil;
    }
    bpf.pipelines[slot] = pipeline; bpf.formats[slot] = format;
    pthread_mutex_unlock(&bpf_lock); return pipeline;
}
static void bpf_encoded(void) {
    pthread_mutex_lock(&bpf_lock);
    if (bpf.applied != bpf.requested || ![bpf.stage isEqualToString:@"applied"]) { bpf.applied = bpf.requested; bpf.stage = @"applied"; bpf_status(); }
    pthread_mutex_unlock(&bpf_lock);
}
#else
static id<MTLRenderPipelineState> bpf_pipeline(MTLPixelFormat format, float *values) { (void)format; (void)values; return nil; }
static void bpf_encoded(void) {}
#endif
