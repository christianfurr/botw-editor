#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <MetalFX/MetalFX.h>
#include <stdio.h>
#include <math.h>

static MTLTextureUsage reactive_usage(id<MTLFXTemporalScalerBase> scaler) {
#if defined(__MAC_27_0)
    if (@available(macOS 27.0, *)) return scaler.reactiveMaskTextureUsage;
#endif
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    return scaler.reactiveTextureUsage;
#pragma clang diagnostic pop
}

static id<MTLTexture> texture(id<MTLDevice> device, MTLPixelFormat format, NSUInteger w, NSUInteger h, MTLTextureUsage usage) {
    MTLTextureDescriptor *d=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:w height:h mipmapped:NO];
    d.usage=usage;d.storageMode=MTLStorageModeShared;
    return [device newTextureWithDescriptor:d];
}
static void fill(id<MTL4CommandBuffer> cb,id<MTLTexture> color,id<MTLTexture> depth,MTLClearColor value) {
    MTL4RenderPassDescriptor *d=[MTL4RenderPassDescriptor new];
    if(color){d.colorAttachments[0].texture=color;d.colorAttachments[0].loadAction=MTLLoadActionClear;d.colorAttachments[0].storeAction=MTLStoreActionStore;d.colorAttachments[0].clearColor=value;}
    if(depth){d.depthAttachment.texture=depth;d.depthAttachment.loadAction=MTLLoadActionClear;d.depthAttachment.storeAction=MTLStoreActionStore;d.depthAttachment.clearDepth=.5;}
    id<MTL4RenderCommandEncoder> e=[cb renderCommandEncoderWithDescriptor:d];[e endEncoding];
}
int main(void){@autoreleasepool{
    id<MTLDevice> device=MTLCreateSystemDefaultDevice();NSError *error=nil;
    if(!device||![MTLFXTemporalScalerDescriptor supportsMetal4FX:device]){fprintf(stderr,"SKIP: no Metal 4 temporal-capable device\n");return 77;}
    id<MTL4Compiler> compiler=[device newCompilerWithDescriptor:[MTL4CompilerDescriptor new] error:&error];
    id<MTL4CommandQueue> queue=[device newMTL4CommandQueue];id<MTL4CommandAllocator> allocator=[device newCommandAllocator];
    if(!compiler||!queue||!allocator){fprintf(stderr,"Metal 4 compiler/queue/allocator creation failed\n");return 1;}
    for(NSNumber *format in @[@(MTLPixelFormatRGB10A2Unorm),@(MTLPixelFormatRGBA16Float)]){
      for(NSNumber *scale in @[@(4.0/3.0),@1.5,@2.0]){
        MTLFXTemporalScalerDescriptor *d=[MTLFXTemporalScalerDescriptor new];
        d.inputWidth=192;d.inputHeight=108;d.outputWidth=(NSUInteger)lround(192*scale.doubleValue);d.outputHeight=(NSUInteger)lround(108*scale.doubleValue);
        d.colorTextureFormat=format.unsignedIntegerValue;d.depthTextureFormat=MTLPixelFormatDepth32Float;d.motionTextureFormat=MTLPixelFormatRG16Float;d.outputTextureFormat=MTLPixelFormatRGBA16Float;
        d.autoExposureEnabled=NO;d.reactiveMaskTextureEnabled=YES;d.reactiveMaskTextureFormat=MTLPixelFormatR16Float;d.requiresSynchronousInitialization=YES;
        id<MTL4FXTemporalScaler> sc=[d newTemporalScalerWithDevice:device compiler:compiler];
        if(!sc){fprintf(stderr,"format%lu refused descriptor at %.3fx\n",(unsigned long)d.colorTextureFormat,scale.doubleValue);return 2;}
        MTLTextureUsage rt=MTLTextureUsageRenderTarget;
        id<MTLTexture> color=texture(device,d.colorTextureFormat,192,108,sc.colorTextureUsage|rt),depth=texture(device,MTLPixelFormatDepth32Float,192,108,sc.depthTextureUsage|rt),motion=texture(device,MTLPixelFormatRG16Float,192,108,sc.motionTextureUsage|rt);
        id<MTLTexture> reactive=texture(device,MTLPixelFormatR16Float,192,108,reactive_usage(sc)|rt),output=texture(device,MTLPixelFormatRGBA16Float,d.outputWidth,d.outputHeight,sc.outputTextureUsage|MTLTextureUsageShaderRead);
        if(!color||!depth||!motion||!reactive||!output){fprintf(stderr,"format%lu failed textures\n",(unsigned long)d.colorTextureFormat);return 2;}
        id<MTLResidencySet> residency=[device newResidencySetWithDescriptor:[MTLResidencySetDescriptor new] error:&error];
        if(!residency){fprintf(stderr,"residency set creation failed\n");return 2;}
        for(id<MTLTexture> t in @[color,depth,motion,reactive,output])[residency addAllocation:t];[residency commit];
        for(int frame=0;frame<4;frame++){
            [allocator reset];id<MTL4CommandBuffer> cb=[device newCommandBuffer];[cb beginCommandBufferWithAllocator:allocator];[cb useResidencySet:residency];
            fill(cb,color,depth,MTLClearColorMake(.2,.4,.6,1));fill(cb,motion,nil,MTLClearColorMake(0,0,0,0));fill(cb,reactive,nil,MTLClearColorMake(0,0,0,0));
            sc.colorTexture=color;sc.depthTexture=depth;sc.motionTexture=motion;sc.reactiveMaskTexture=reactive;sc.outputTexture=output;
            sc.inputContentWidth=192;sc.inputContentHeight=108;sc.jitterOffsetX=0;sc.jitterOffsetY=0;sc.motionVectorScaleX=1;sc.motionVectorScaleY=1;sc.depthReversed=NO;sc.reset=frame==0;
            [sc encodeToCommandBuffer:cb];[cb endCommandBuffer];dispatch_semaphore_t done=dispatch_semaphore_create(0);__block NSError *gpuError=nil;
            MTL4CommitOptions *options=[MTL4CommitOptions new];[options addFeedbackHandler:^(id<MTL4CommitFeedback> feedback){gpuError=feedback?feedback.error:[NSError errorWithDomain:@"TemporalProbe" code:1 userInfo:nil];dispatch_semaphore_signal(done);}];
            [queue commit:&cb count:1 options:options];
            if(dispatch_semaphore_wait(done,dispatch_time(DISPATCH_TIME_NOW,10*NSEC_PER_SEC))){fprintf(stderr,"GPU completion timed out\n");return 3;}
            if(gpuError){fprintf(stderr,"format%lu GPUerror %s\n",(unsigned long)d.colorTextureFormat,gpuError.localizedDescription.UTF8String);return 3;}
        }
        __fp16 pixel[4]={0};[output getBytes:pixel bytesPerRow:8 fromRegion:MTLRegionMake2D(d.outputWidth/2,d.outputHeight/2,1,1) mipmapLevel:0];
        printf("format%lu MetalFX temporal 192x108->%lux%lu, 4 GPU-completed frames, RGBA %.5f %.5f %.5f %.5f\n",(unsigned long)d.colorTextureFormat,(unsigned long)d.outputWidth,(unsigned long)d.outputHeight,(double)pixel[0],(double)pixel[1],(double)pixel[2],(double)pixel[3]);
        if(!isfinite((float)pixel[0])||!isfinite((float)pixel[1])||!isfinite((float)pixel[2])||!isfinite((float)pixel[3])||fabs((float)pixel[0]-.2f)>.02f||fabs((float)pixel[1]-.4f)>.02f||fabs((float)pixel[2]-.6f)>.02f||fabs((float)pixel[3]-1.f)>.02f){fprintf(stderr,"unexpected colors\n");return 4;}
      }
    }
    return 0;
}}
