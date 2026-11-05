#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static id<MTLDevice> g_dev;
#define GAME_BOTW 1
#define BOTW_POSTFX_TEST 1
#include "../runtime/botw_postfx.h"

static int checks;
static void check(int condition, const char *label) {
    checks++;
    if (!condition) { fprintf(stderr,"Post-processing GPU check failed: %s\n",label); exit(1); }
}
static NSString *fixture;
static double revision;
static void configure(NSString *preset, double strength, double sharpness) {
    NSDictionary *d = @{@"schema":@1,@"revision":@(++revision),@"preset":preset,@"strength":@(strength),@"sharpness":@(sharpness)};
    NSData *data = [NSJSONSerialization dataWithJSONObject:d options:0 error:nil];
    check([data writeToFile:[fixture stringByAppendingPathComponent:@"settings.json"] options:NSDataWritingAtomic error:nil],"write independent test settings");
    bpf.next_poll = 0;
}
static NSDictionary *status(void) {
    NSData *data = [NSData dataWithContentsOfFile:[fixture stringByAppendingPathComponent:@"status.json"]];
    return data ? [NSJSONSerialization JSONObjectWithData:data options:0 error:nil] : nil;
}
static id<MTLRenderPipelineState> original_pipeline(void) {
    NSString *source = @"#include <metal_stdlib>\nusing namespace metal;\n"
        "struct V{float4 p [[position]];float2 uv;};\n"
        "vertex V vs(uint i [[vertex_id]]){float2 uv=float2((i<<1)&2,i&2);return{float4(uv*float2(2,-2)+float2(-1,1),0,1),uv};}\n"
        "struct Crop{float4 rect;float4 edge;};\n"
        "fragment float4 fs(V in [[stage_in]],texture2d<float> t [[texture(0)]],sampler s [[sampler(0)]],constant Crop& c [[buffer(0)]]){return t.sample(s,clamp(in.uv*c.rect.zw+c.rect.xy,c.edge.xy,c.edge.zw));}\n";
    NSError *error = nil;
    id<MTLLibrary> library = [g_dev newLibraryWithSource:source options:nil error:&error];
    check(library != nil,"compile original copy shader");
    MTLRenderPipelineDescriptor *d = [MTLRenderPipelineDescriptor new];
    d.vertexFunction = [library newFunctionWithName:@"vs"]; d.fragmentFunction = [library newFunctionWithName:@"fs"];
    d.colorAttachments[0].pixelFormat = MTLPixelFormatRGBA8Unorm;
    id<MTLRenderPipelineState> p = [g_dev newRenderPipelineStateWithDescriptor:d error:&error];
    check(p != nil,"compile original copy pipeline"); return p;
}
static void render(id<MTLRenderPipelineState> pipeline, id<MTLTexture> input, int x, int y, int width, int height, uint8_t *output, float *post) {
    MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:width height:height mipmapped:NO];
    d.storageMode = MTLStorageModeShared; d.usage = MTLTextureUsageRenderTarget;
    id<MTLTexture> target = [g_dev newTextureWithDescriptor:d];
    id<MTLCommandQueue> queue = [g_dev newCommandQueue]; id<MTLCommandBuffer> command = [queue commandBuffer];
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = target; pass.colorAttachments[0].loadAction = MTLLoadActionClear; pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    id<MTLRenderCommandEncoder> encoder = [command renderCommandEncoderWithDescriptor:pass];
    [encoder setViewport:(MTLViewport){0,0,width,height,0,1}]; [encoder setRenderPipelineState:pipeline];
    MTLSamplerDescriptor *sd = [MTLSamplerDescriptor new]; sd.minFilter = sd.magFilter = MTLSamplerMinMagFilterLinear;
    sd.sAddressMode = sd.tAddressMode = MTLSamplerAddressModeClampToEdge;
    [encoder setFragmentTexture:input atIndex:0]; [encoder setFragmentSamplerState:[g_dev newSamplerStateWithDescriptor:sd] atIndex:0];
    float uv[12] = {(float)x/input.width,(float)y/input.height,(float)width/input.width,(float)height/input.height,
        (x+0.5f)/input.width,(y+0.5f)/input.height,(x+width-0.5f)/input.width,(y+height-0.5f)/input.height,0,0,0,0};
    if (post) memcpy(uv+8,post,4*sizeof(float));
    [encoder setFragmentBytes:uv length:sizeof uv atIndex:0]; [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    [encoder endEncoding]; [command commit]; [command waitUntilCompleted];
    check(command.status == MTLCommandBufferStatusCompleted,"complete GPU filter render without errors");
    [target getBytes:output bytesPerRow:width*4 fromRegion:MTLRegionMake2D(0,0,width,height) mipmapLevel:0];
}
static int near_byte(float expected, uint8_t actual) { return abs((int)lroundf(fminf(fmaxf(expected,0),1)*255) - actual) <= 1; }

int main(int argc, const char **argv) {
    @autoreleasepool {
        check(argc == 2,"fixture parent argument supplied");
        fixture = [[NSString stringWithUTF8String:argv[1]] stringByAppendingPathComponent:[NSString stringWithFormat:@"postfx-%@",NSUUID.UUID.UUIDString]];
        check([[NSFileManager defaultManager] createDirectoryAtPath:fixture withIntermediateDirectories:YES attributes:@{NSFilePosixPermissions:@0700} error:nil],"create isolated build fixture");
        bpf_test_directory = fixture;
        g_dev = MTLCreateSystemDefaultDevice();
        if (!g_dev) {
            NSString *source = [[NSString stringWithUTF8String:argv[1]] stringByAppendingPathComponent:@"postfx-compile-only.metal"];
            check([bpf_source() writeToFile:source atomically:YES encoding:NSUTF8StringEncoding error:nil],"write compile-only shader when no GPU exists");
            [[NSFileManager defaultManager] removeItemAtPath:fixture error:nil];
            fprintf(stderr,"No Metal device is available; GPU assertions were not run.\n");
            return 77;
        }
        check(g_dev != nil,"native Metal device available");
        float controls[4] = {0};
        check(bpf_pipeline(MTLPixelFormatRGBA8Unorm,controls) == nil,"missing config keeps original pipeline");
        check([status()[@"applied_preset"] isEqual:@"off"] && [status()[@"version"] isEqual:@"1.6.0"],"report supported version and original output");
        check([status()[@"pid"] intValue] == getpid(),"report actual process identity");
        double firstHeartbeat = [status()[@"updated_at"] doubleValue];
        bpf.next_poll = 0; (void)bpf_pipeline(MTLPixelFormatRGBA8Unorm,controls);
        check([status()[@"updated_at"] doubleValue] >= firstHeartbeat,"refresh unchanged-state heartbeat");
        configure(@"off",1,1); check(bpf_pipeline(MTLPixelFormatRGBA8Unorm,controls) == nil,"explicit Off bypasses all processing");
        configure(@"natural",0,0); check(bpf_pipeline(MTLPixelFormatRGBA8Unorm,controls) == nil,"zero strength and sharpness bypass original output");

        uint8_t pixels[8*8*4];
        for (int y=0;y<8;y++) for(int x=0;x<8;x++) {
            uint8_t *p=pixels+(y*8+x)*4; p[0]=30+x*17; p[1]=40+y*15; p[2]=65+x*4+y*3; p[3]=95+x+y;
        }
        MTLTextureDescriptor *td=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm width:8 height:8 mipmapped:NO];
        td.storageMode=MTLStorageModeShared;td.usage=MTLTextureUsageShaderRead;
        id<MTLTexture> input=[g_dev newTextureWithDescriptor:td];[input replaceRegion:MTLRegionMake2D(0,0,8,8) mipmapLevel:0 withBytes:pixels bytesPerRow:32];
        id<MTLRenderPipelineState> original=original_pipeline(); uint8_t baseline[4*4*4],output[4*4*4];
        render(original,input,2,2,4,4,baseline,NULL);
        for(int y=0;y<4;y++)for(int x=0;x<4;x++)check(!memcmp(baseline+(y*4+x)*4,pixels+((y+2)*8+x+2)*4,4),"original crop is texel exact");
        NSString *customPath=[fixture stringByAppendingPathComponent:@"custom.metal"];
        check([@"float3 customGrade(float3 color,float2 uv){return color;}" writeToFile:customPath atomically:YES encoding:NSUTF8StringEncoding error:nil],"write original custom identity function");
        configure(@"custom",1,0);id<MTLRenderPipelineState> pipeline=bpf_pipeline(MTLPixelFormatRGBA8Unorm,controls);
        check(pipeline != nil,"compile real custom shader");render(pipeline,input,2,2,4,4,output,controls);bpf_encoded();
        check(!memcmp(output,baseline,sizeof output),"custom identity shader exactly preserves cropped RGBA pixels");
        check([status()[@"stage"] isEqual:@"applied"] && [status()[@"revision"] doubleValue] == revision,"acknowledge matching encoded revision");
        for(NSString *preset in @[@"natural",@"vivid",@"cinematic"]) {
            configure(preset,1,0);pipeline=bpf_pipeline(MTLPixelFormatRGBA8Unorm,controls);check(pipeline != nil,"compile preset shader");
            render(pipeline,input,2,2,4,4,output,controls);
            check(memcmp(output,baseline,sizeof output) != 0,"preset changes actual GPU output");
            for(int i=0;i<16;i++) {
                float r=baseline[i*4]/255.0f,g=baseline[i*4+1]/255.0f,b=baseline[i*4+2]/255.0f,y=r*.2126f+g*.7152f+b*.0722f;
                float saturation=[preset isEqual:@"natural"]?1.06f:[preset isEqual:@"vivid"]?1.18f:.94f;
                float contrast=[preset isEqual:@"natural"]?1.035f:[preset isEqual:@"vivid"]?1.07f:1.055f;
                float values[3]={r,g,b};
                for(int c=0;c<3;c++) {
                    float expected=((y+(values[c]-y)*saturation)-.5f)*contrast+.5f;
                    if([preset isEqual:@"cinematic"])expected*=c==0?1.025f:c==2?.975f:1;
                    check(near_byte(expected,output[i*4+c]),"preset GPU result matches independent CPU formula");
                }
                check(output[i*4+3]==baseline[i*4+3],"preset preserves alpha");
            }
        }
        configure(@"natural",0,1);pipeline=bpf_pipeline(MTLPixelFormatRGBA8Unorm,controls);render(pipeline,input,2,2,4,4,output,controls);
        for(int y=0;y<4;y++)for(int x=0;x<4;x++)for(int c=0;c<3;c++) {
            int i=(y*4+x)*4+c;float center=baseline[i]/255.0f;
            float n=baseline[((y<3?y+1:y)*4+x)*4+c]/255.0f,a=baseline[((y>0?y-1:y)*4+x)*4+c]/255.0f;
            float e=baseline[(y*4+(x<3?x+1:x))*4+c]/255.0f,w=baseline[(y*4+(x>0?x-1:x))*4+c]/255.0f;
            float lo=fminf(center,fminf(fminf(n,a),fminf(e,w))),hi=fmaxf(center,fmaxf(fmaxf(n,a),fmaxf(e,w)));
            float expected=fminf(fmaxf(center+(center-(n+a+e+w)*.25f)*.5f,lo),hi);
            check(near_byte(expected,output[i]),"sharpening clamps every neighbor to crop and matches CPU formula");
        }
        check([@"float3 customGrade(float3 color,float2 uv){return color+float3(0.1,0,0);}" writeToFile:customPath atomically:YES encoding:NSUTF8StringEncoding error:nil],"write custom color shader");
        configure(@"custom",.5,0);pipeline=bpf_pipeline(MTLPixelFormatRGBA8Unorm,controls);render(pipeline,input,2,2,4,4,output,controls);
        for(int i=0;i<16;i++)check(near_byte(baseline[i*4]/255.0f+.05f,output[i*4]),"custom strength blends actual output");
        check([@"this is not valid Metal" writeToFile:customPath atomically:YES encoding:NSUTF8StringEncoding error:nil],"write invalid custom fixture");
        bpf.next_poll=0;check(bpf_pipeline(MTLPixelFormatRGBA8Unorm,controls)==nil,"compile failure falls back to original pipeline");
        check([status()[@"error"] length]>0 && [status()[@"applied_preset"] isEqual:@"off"],"compile failure reports original output and actionable error");
        check([@"float3 customGrade(float3 color,float2 uv){return float3(NAN,INFINITY,-INFINITY);}" writeToFile:customPath atomically:YES encoding:NSUTF8StringEncoding error:nil],"write nonfinite custom fixture");
        bpf.next_poll=0;pipeline=bpf_pipeline(MTLPixelFormatRGBA8Unorm,controls);check(pipeline!=nil,"compile nonfinite handling fixture");render(pipeline,input,2,2,4,4,output,controls);
        check(!memcmp(output,baseline,sizeof output),"nonfinite custom channels preserve source pixels");
        configure(@"vivid",1.01,0);check(bpf_pipeline(MTLPixelFormatRGBA8Unorm,controls)==nil,"out-of-range strength keeps original pipeline");
        configure(@"vivid",1,-.01);check(bpf_pipeline(MTLPixelFormatRGBA8Unorm,controls)==nil,"negative sharpness keeps original pipeline");
        configure(@"unknown",1,0);check(bpf_pipeline(MTLPixelFormatRGBA8Unorm,controls)==nil,"unknown preset keeps original pipeline");
        configure(@"natural",1,0);check(bpf_pipeline(MTLPixelFormatRGBA16Float,controls)==nil,"unsupported HDR output keeps original pipeline");
        check([[NSFileManager defaultManager] removeItemAtPath:fixture error:nil],"remove independent GPU fixture");
        printf("Post-processing GPU checks passed: %d assertions; exact identity/crop/alpha, preset math, sharpening bounds, custom source, invalid settings and error fallback.\n",checks);
    }
    return 0;
}
