/* wgpu-native GPU backend for std/nori_ui. Renders a ComputedOverlay's PaintPrimitives:
   - rounded-rect fill + border via an SDF fragment shader (matches ui_cpu's AA rounded rects)
   - text as textured quads sampling an R8 glyph atlas (std/font)
   Two targets: offscreen (render to a texture, read pixels back, used for headless tests) and
   window (present to an X11 surface). Bound from Nori via bare `extern fn` (all handles as Int).
   (wgpu-native v29 C API; descriptors built here in C, per the Nori FFI convention.) */
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "webgpu.h"
#include "wgpu.h"

static const char* SHADER =
"struct U { vp: vec2<f32>, atlas: vec2<f32> };\n"
"@group(0) @binding(0) var<uniform> u: U;\n"
"var<private> CORNERS: array<vec2<f32>,6> = array<vec2<f32>,6>(\n"
"  vec2<f32>(0.,0.), vec2<f32>(1.,0.), vec2<f32>(0.,1.),\n"
"  vec2<f32>(0.,1.), vec2<f32>(1.,0.), vec2<f32>(1.,1.));\n"
/* Rect instances carry the same optional two-colour linear gradient the glyph instances do, in the
   same per-instance-data style: `gto` is the "to" colour (the "from" colour is the instance's own
   `color`, which is what PaintPrimitive stores too) and `gprm` is (axis_origin, axis_span,
   horizontal?1:0, enabled?1:0). Evaluated per fragment from the screen position, as
   text_fs does and as paint_cpu's fill_rect_grad_cl does, so a gradient rect and a gradient
   label with the same parameters land on the same t, and so do the two backends. Still one draw
   call: a gradient rect batches with every other rect in the frame. */
"struct RIn { @location(0) rect: vec4<f32>, @location(1) color: vec4<f32>, @location(2) prm: vec2<f32>,\n"
"             @location(3) clip: vec4<f32>, @location(4) gto: vec4<f32>, @location(5) gprm: vec4<f32> };\n"
"struct RVO { @builtin(position) pos: vec4<f32>, @location(0) local: vec2<f32>,\n"
"             @location(1) half: vec2<f32>, @location(2) color: vec4<f32>, @location(3) prm: vec2<f32>,\n"
"             @location(4) clip: vec4<f32>, @location(5) gto: vec4<f32>, @location(6) gprm: vec4<f32> };\n"
"@vertex fn rect_vs(@builtin(vertex_index) vi: u32, in: RIn) -> RVO {\n"
"  let c = CORNERS[vi];\n"
"  let px = in.rect.xy + c * in.rect.zw;\n"
"  let ndc = vec2<f32>(px.x / u.vp.x * 2. - 1., 1. - px.y / u.vp.y * 2.);\n"
"  var o: RVO; o.pos = vec4<f32>(ndc, 0., 1.);\n"
"  o.half = in.rect.zw * 0.5;\n"
"  o.local = (c - vec2<f32>(0.5, 0.5)) * in.rect.zw;\n"
"  o.color = in.color; o.prm = in.prm; o.clip = in.clip; o.gto = in.gto; o.gprm = in.gprm; return o;\n"
"}\n"
"fn sd_round(p: vec2<f32>, b: vec2<f32>, r: f32) -> f32 {\n"
"  let q = abs(p) - b + vec2<f32>(r, r);\n"
"  return min(max(q.x, q.y), 0.) + length(max(q, vec2<f32>(0., 0.))) - r;\n"
"}\n"
/* per-instance analytic scissor: `cl` is an integer, half-open pixel box [x0,x1) x [y0,y1) in
   framebuffer coords -- exactly ui_cpu's `Clip`. @builtin(position) has pixel centers at +0.5, so
   floor() recovers the integer pixel index and the comparison is the CPU's test, pixel for pixel.
   Returning 0 coverage (rather than `discard`) keeps it a plain multiply: with src-alpha blending a
   zero-alpha fragment leaves the destination untouched. An unclipped instance carries a huge box. */
"fn clip_mask(pos: vec2<f32>, cl: vec4<f32>) -> f32 {\n"
"  let p = floor(pos);\n"
"  if (p.x < cl.x || p.y < cl.y || p.x >= cl.z || p.y >= cl.w) { return 0.0; }\n"
"  return 1.0;\n"
"}\n"
"@fragment fn rect_fs(in: RVO) -> @location(0) vec4<f32> {\n"
"  let r = min(in.prm.x, min(in.half.x, in.half.y));\n"
"  let d = sd_round(in.local, in.half, r);\n"
"  var cov: f32;\n"
"  if (in.prm.y > 0.5) {\n"
"    let dd = abs(d + in.prm.y * 0.5) - in.prm.y * 0.5;\n"
"    cov = 1.0 - smoothstep(-0.5, 0.5, dd);\n"
"  } else {\n"
"    cov = 1.0 - smoothstep(-0.5, 0.5, d);\n"
"  }\n"
"  cov = cov * clip_mask(in.pos.xy, in.clip);\n"
"  var col = in.color;\n"
"  if (in.gprm.w > 0.5) {\n"
"    var q = in.pos.y;\n"
"    if (in.gprm.z > 0.5) { q = in.pos.x; }\n"
"    let t = clamp((q - in.gprm.x) / max(in.gprm.y, 1.0), 0.0, 1.0);\n"
"    col = mix(in.color, in.gto, t);\n"
"  }\n"
"  return vec4<f32>(col.rgb, col.a * cov);\n"
"}\n"
"@group(1) @binding(0) var atlas_tex: texture_2d<f32>;\n"
"@group(1) @binding(1) var atlas_samp: sampler;\n"
/* Glyph instances carry an optional linear gradient alongside the clip box, in the same
   per-instance-data style: `gto` is the "to" colour (the "from" colour is the instance's own `color`,
   which is also what the primitive stores) and `gprm` is
   (axis_origin, axis_span, horizontal?1:0, enabled?1:0). Evaluated per fragment from the screen
   position, as paint_cpu's draw_line_g
   does, so the same document lands on the same t in both painters. Still one draw call: a gradient
   run batches with every other glyph in the frame. */
"struct GIn { @location(0) rect: vec4<f32>, @location(1) uv: vec4<f32>, @location(2) color: vec4<f32>,\n"
"             @location(3) clip: vec4<f32>, @location(4) gto: vec4<f32>, @location(5) gprm: vec4<f32> };\n"
"struct GVO { @builtin(position) pos: vec4<f32>, @location(0) uv: vec2<f32>, @location(1) color: vec4<f32>,\n"
"             @location(2) clip: vec4<f32>, @location(3) gto: vec4<f32>, @location(4) gprm: vec4<f32> };\n"
"@vertex fn text_vs(@builtin(vertex_index) vi: u32, in: GIn) -> GVO {\n"
"  let c = CORNERS[vi];\n"
"  let px = in.rect.xy + c * in.rect.zw;\n"
"  let ndc = vec2<f32>(px.x / u.vp.x * 2. - 1., 1. - px.y / u.vp.y * 2.);\n"
"  var o: GVO; o.pos = vec4<f32>(ndc, 0., 1.);\n"
"  o.uv = (in.uv.xy + c * in.uv.zw) / u.atlas;\n"
"  o.color = in.color; o.clip = in.clip; o.gto = in.gto; o.gprm = in.gprm; return o;\n"
"}\n"
/* a colour glyph (an emoji) is drawn from the RGBA plane in its own colours, at the text's alpha:
   gprm.w = 2 marks it (1 is a gradient, 0 a plain glyph). Both textures are sampled before the
   branch; textureSample must stay in uniform control flow. */
"@group(1) @binding(2) var color_tex: texture_2d<f32>;\n"
"@fragment fn text_fs(in: GVO) -> @location(0) vec4<f32> {\n"
"  let m = clip_mask(in.pos.xy, in.clip);\n"
"  let cov = textureSample(atlas_tex, atlas_samp, in.uv).r * m;\n"
"  let rgba = textureSample(color_tex, atlas_samp, in.uv);\n"
"  if (in.gprm.w > 1.5) {\n"
"    if (rgba.a <= 0.0) { return vec4<f32>(0., 0., 0., 0.); }\n"
"    return vec4<f32>(rgba.rgb / rgba.a, rgba.a * in.color.a * m);\n"
"  }\n"
"  var col = in.color;\n"
"  if (in.gprm.w > 0.5) {\n"
"    var q = in.pos.y;\n"
"    if (in.gprm.z > 0.5) { q = in.pos.x; }\n"
"    let t = clamp((q - in.gprm.x) / max(in.gprm.y, 1.0), 0.0, 1.0);\n"
"    col = mix(in.color, in.gto, t);\n"
"  }\n"
"  return vec4<f32>(col.rgb, col.a * cov);\n"
"}\n"
/* image: sample the bound texture as full RGBA (uv = corner 0..1), alpha-modulated by the tint */
"struct IIn { @location(0) rect: vec4<f32>, @location(1) tint: vec4<f32>, @location(2) clip: vec4<f32>,\n"
"             @location(3) prm: vec4<f32> };\n"
"struct IVO { @builtin(position) pos: vec4<f32>, @location(0) uv: vec2<f32>, @location(1) tint: vec4<f32>,\n"
"             @location(2) clip: vec4<f32>, @location(3) prm: vec4<f32> };\n"
"@vertex fn img_vs(@builtin(vertex_index) vi: u32, in: IIn) -> IVO {\n"
"  let c = CORNERS[vi];\n"
"  let px = in.rect.xy + c * in.rect.zw;\n"
"  let ndc = vec2<f32>(px.x / u.vp.x * 2. - 1., 1. - px.y / u.vp.y * 2.);\n"
"  var uv = c;\n"
"  if (in.prm.x > 0.5) { uv = vec2<f32>(c.x, 1. - c.y); }\n"
"  var o: IVO; o.pos = vec4<f32>(ndc, 0., 1.); o.uv = uv; o.tint = in.tint; o.clip = in.clip; o.prm = in.prm; return o;\n"
"}\n"
"@fragment fn img_fs(in: IVO) -> @location(0) vec4<f32> {\n"
"  let t = textureSample(atlas_tex, atlas_samp, in.uv);\n"
"  var a = t.a;\n"
"  if (in.prm.y > 0.5) { a = 1.0; }\n"
"  return vec4<f32>(t.rgb, a * in.tint.a * clip_mask(in.pos.xy, in.clip));\n"
"}\n";

#define RECT_FLOATS 22   /* rect(4) color(4) radius,border(2) clip(4) gradient-to(4) gradient-params(4) */
#define GLYPH_FLOATS 24  /* rect(4) uv(4) color(4) clip(4) gradient-to(4) gradient-params(4) */
#define IMG_FLOATS 16    /* rect(4) tint(4) clip(4) prm(4: flip_v, opaque, 0, 0) */
/* "no clip" sentinel box: every real framebuffer pixel is inside it. */
#define CLIP_NONE_LO (-1.0e9f)
#define CLIP_NONE_HI ( 1.0e9f)

/* per-image GPU resources (one texture+bind group per drawn image, released after the frame) */
/* `owned` is 0 for a borrowed view handed in by ui_gpu_image_view: free_images releases the bind
   group it made either way, but must not release a texture/view somebody else still owns. */
typedef struct { WGPUTexture tex; WGPUTextureView view; WGPUBindGroup bg; int owned; int rectAt; int glyphAt; } ImgRes;

/* A CUT is a point in the frame where the two instanced batches have to be flushed before going on:
   everything queued up to (rectAt, glyphAt) is drawn, then whatever the cut is for.
   `img >= 0` cuts for an image (its own texture bind group); `img < 0` is a plain batch break. */
typedef struct { int rectAt; int glyphAt; int img; } Cut;

typedef struct {
    WGPUInstance instance; WGPUAdapter adapter; WGPUDevice device; WGPUQueue queue;
    int offscreen; WGPUSurface surface; WGPUTextureFormat format;
    WGPUTexture target; WGPUTextureView targetView;   /* offscreen render target */
    WGPUBuffer readbuf; int rb_bpr;
    /* ---- the pipelined readback, for a frame path rather than a test.
       `ui_gpu_readback` submits a copy and then blocks the CPU on `wgpuDevicePoll(.., true)` until
       the GPU has finished it, which is a full sync every frame. That is right for a screenshot and
       wrong for a video stream: measured on a 3090, streaming cost 2.34 ms a frame at 720p and
       4.53 ms at 1080p, almost all of it that wait.
       Two buffers, used alternately: frame N submits its copy into one and reads what frame N-1 put
       in the other, which the GPU finished long ago and which therefore maps without waiting. The
       price is one frame of latency, which a stream already has many of. */
    WGPUBuffer rbuf2;
    int rb_cur;          /* which of the two this frame writes into */
    int rb_pending;      /* is there a mapped-or-mapping buffer from last frame to collect? */
    /* forward-declared: the resize path settles an in-flight map long before the readback that
       creates one is defined. */
    int w, h;
    WGPURenderPipeline rectPipe, textPipe, imagePipe;
    WGPUBindGroupLayout ubgl, tbgl;
    WGPUBindGroupLayout tbgl3;  /* the text pipeline's group 1: coverage atlas, sampler, colour atlas */
    WGPUBuffer ubo; WGPUBindGroup ubg;
    WGPUSampler samp;       /* Nearest: glyph fidelity */
    WGPUSampler sampLinear; /* Linear: a scene blit into a viewport rect */
    float* rectData; int rectCount, rectCap;
    float* glyphData; int glyphCount, glyphCap;
    WGPUBuffer rectBuf; int rectBufCap;
    WGPUBuffer glyphBuf; int glyphBufCap;
    WGPUTexture atlasTex; WGPUTextureView atlasView; WGPUBindGroup atlasBG; int atlasW, atlasH;
    int atlasGen;   /* last-uploaded atlas glyph count; lets Nori skip re-pack when unchanged */
    WGPUTexture colorTex; WGPUTextureView colorView; int colorW, colorH;   /* RGBA8 colour glyphs */
    float* imgData; int imgCount, imgCap;       /* IMG_FLOATS each: rect+tint instances */
    ImgRes* imgRes;                              /* parallel to imgData: per-image texture+bind group */
    Cut* cuts; int cutCount, cutCap;             /* batch breaks, in emission order */
    int cutGlyph;                                /* glyphCount at the last cut, see push_cut */
    WGPUBuffer imgBuf; int imgBufCap;
    double clearR, clearG, clearB;
    /* current clip box, copied into every instance emitted while it is set.
       Volatile: the C is optimised together with the Nori code, and with a plain float[4] the stores
       in ui_gpu_clip/ui_gpu_clip_none can be eliminated entirely, so every instance would carry the
       "no clip" sentinel and nothing would be clipped. __attribute__((noinline)) does not prevent
       it; giving the setters an observable side effect (an fprintf) or marking the field volatile
       both do. Four volatile float loads per emitted instance is noise. */
    volatile float clip[4];
} Ctx;

static void rb_settle(Ctx* c);


static WGPUStringView sv(const char* s) { WGPUStringView v; v.data = s; v.length = s ? strlen(s) : 0; return v; }
static void free_images(Ctx* c);   /* release the per-image textures queued this frame */
void ui_gpu_clip_none(void* ctx);  /* reset the frame clip box to "everything" */

static void on_adapter(WGPURequestAdapterStatus s, WGPUAdapter a, WGPUStringView m, void* u1, void* u2) {
    (void)u2; if (s == WGPURequestAdapterStatus_Success) *(WGPUAdapter*)u1 = a;
    else fprintf(stderr, "ui_gpu: adapter failed: %.*s\n", (int)m.length, m.data);
}
static void on_device(WGPURequestDeviceStatus s, WGPUDevice d, WGPUStringView m, void* u1, void* u2) {
    (void)u2; if (s == WGPURequestDeviceStatus_Success) *(WGPUDevice*)u1 = d;
    else fprintf(stderr, "ui_gpu: device failed: %.*s\n", (int)m.length, m.data);
}
static volatile int g_mapped; static WGPUMapAsyncStatus g_mapst;
static void on_map(WGPUMapAsyncStatus s, WGPUStringView m, void* u1, void* u2) { (void)m;(void)u1;(void)u2; g_mapst = s; g_mapped = 1; }

/* build the shader module, uniform+sampler, and the two pipelines. Returns 1 on success. */
static int build_pipelines(Ctx* c) {
    WGPUShaderSourceWGSL wsl; memset(&wsl, 0, sizeof(wsl));
    wsl.chain.sType = WGPUSType_ShaderSourceWGSL; wsl.code = sv(SHADER);
    WGPUShaderModuleDescriptor smd; memset(&smd, 0, sizeof(smd));
    smd.nextInChain = (WGPUChainedStruct*)&wsl;
    WGPUShaderModule sm = wgpuDeviceCreateShaderModule(c->device, &smd);
    if (!sm) { fprintf(stderr, "ui_gpu: shader compile failed\n"); return 0; }

    /* uniform bind group layout (group 0): one uniform buffer, visible to VS+FS */
    WGPUBindGroupLayoutEntry ue; memset(&ue, 0, sizeof(ue));
    ue.binding = 0; ue.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    ue.buffer.type = WGPUBufferBindingType_Uniform;
    WGPUBindGroupLayoutDescriptor ubld; memset(&ubld, 0, sizeof(ubld));
    ubld.entryCount = 1; ubld.entries = &ue;
    c->ubgl = wgpuDeviceCreateBindGroupLayout(c->device, &ubld);

    /* texture bind group layout (group 1): texture + sampler, FS only */
    WGPUBindGroupLayoutEntry te[2]; memset(te, 0, sizeof(te));
    te[0].binding = 0; te[0].visibility = WGPUShaderStage_Fragment;
    te[0].texture.sampleType = WGPUTextureSampleType_Float; te[0].texture.viewDimension = WGPUTextureViewDimension_2D;
    te[1].binding = 1; te[1].visibility = WGPUShaderStage_Fragment;
    te[1].sampler.type = WGPUSamplerBindingType_Filtering;
    WGPUBindGroupLayoutDescriptor tbld; memset(&tbld, 0, sizeof(tbld));
    tbld.entryCount = 2; tbld.entries = te;
    c->tbgl = wgpuDeviceCreateBindGroupLayout(c->device, &tbld);
    /* the text pipeline's group 1 adds the colour-glyph atlas at binding 2 (images keep tbgl) */
    WGPUBindGroupLayoutEntry te3[3]; memset(te3, 0, sizeof(te3));
    te3[0] = te[0]; te3[1] = te[1];
    te3[2].binding = 2; te3[2].visibility = WGPUShaderStage_Fragment;
    te3[2].texture.sampleType = WGPUTextureSampleType_Float; te3[2].texture.viewDimension = WGPUTextureViewDimension_2D;
    WGPUBindGroupLayoutDescriptor tbld3; memset(&tbld3, 0, sizeof(tbld3));
    tbld3.entryCount = 3; tbld3.entries = te3;
    c->tbgl3 = wgpuDeviceCreateBindGroupLayout(c->device, &tbld3);

    /* uniform buffer (16 bytes: vp.xy, atlas.xy) + its bind group */
    WGPUBufferDescriptor ubd; memset(&ubd, 0, sizeof(ubd));
    ubd.usage = WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst; ubd.size = 16;
    c->ubo = wgpuDeviceCreateBuffer(c->device, &ubd);
    WGPUBindGroupEntry ube; memset(&ube, 0, sizeof(ube));
    ube.binding = 0; ube.buffer = c->ubo; ube.size = 16;
    WGPUBindGroupDescriptor ubgd; memset(&ubgd, 0, sizeof(ubgd));
    ubgd.layout = c->ubgl; ubgd.entryCount = 1; ubgd.entries = &ube;
    c->ubg = wgpuDeviceCreateBindGroup(c->device, &ubgd);

    /* sampler (linear, clamp) */
    WGPUSamplerDescriptor sd; memset(&sd, 0, sizeof(sd));
    sd.addressModeU = WGPUAddressMode_ClampToEdge; sd.addressModeV = WGPUAddressMode_ClampToEdge;
    sd.addressModeW = WGPUAddressMode_ClampToEdge;
    /* Nearest, not Linear: glyph quads map 1:1 to atlas texels, so nearest samples the exact coverage
       the CPU renderer rasterizes -> the two backends produce identical text (no atlas-filter blur). */
    sd.magFilter = WGPUFilterMode_Nearest; sd.minFilter = WGPUFilterMode_Nearest;
    sd.mipmapFilter = WGPUMipmapFilterMode_Nearest; sd.maxAnisotropy = 1;
    c->samp = wgpuDeviceCreateSampler(c->device, &sd);
    /* the same descriptor, filtered: ui_gpu_image_view(linear=1) picks this one. The tbgl entry is
       already declared WGPUSamplerBindingType_Filtering, so no layout change is needed. */
    sd.magFilter = WGPUFilterMode_Linear; sd.minFilter = WGPUFilterMode_Linear;
    sd.mipmapFilter = WGPUMipmapFilterMode_Linear;
    c->sampLinear = wgpuDeviceCreateSampler(c->device, &sd);

    /* premultiplied-over alpha blend */
    WGPUBlendState blend; memset(&blend, 0, sizeof(blend));
    blend.color.operation = WGPUBlendOperation_Add; blend.color.srcFactor = WGPUBlendFactor_SrcAlpha;
    blend.color.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    blend.alpha.operation = WGPUBlendOperation_Add; blend.alpha.srcFactor = WGPUBlendFactor_One;
    blend.alpha.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;
    WGPUColorTargetState cts; memset(&cts, 0, sizeof(cts));
    cts.format = c->format; cts.blend = &blend; cts.writeMask = WGPUColorWriteMask_All;

    /* ---- rect pipeline ---- */
    WGPUVertexAttribute ra[6]; memset(ra, 0, sizeof(ra));
    ra[0].format = WGPUVertexFormat_Float32x4; ra[0].offset = 0;  ra[0].shaderLocation = 0;
    ra[1].format = WGPUVertexFormat_Float32x4; ra[1].offset = 16; ra[1].shaderLocation = 1;
    ra[2].format = WGPUVertexFormat_Float32x2; ra[2].offset = 32; ra[2].shaderLocation = 2;
    ra[3].format = WGPUVertexFormat_Float32x4; ra[3].offset = 40; ra[3].shaderLocation = 3;
    ra[4].format = WGPUVertexFormat_Float32x4; ra[4].offset = 56; ra[4].shaderLocation = 4;
    ra[5].format = WGPUVertexFormat_Float32x4; ra[5].offset = 72; ra[5].shaderLocation = 5;
    WGPUVertexBufferLayout rvb; memset(&rvb, 0, sizeof(rvb));
    rvb.stepMode = WGPUVertexStepMode_Instance; rvb.arrayStride = RECT_FLOATS * 4;
    rvb.attributeCount = 6; rvb.attributes = ra;
    WGPUPipelineLayoutDescriptor rpl; memset(&rpl, 0, sizeof(rpl));
    rpl.bindGroupLayoutCount = 1; rpl.bindGroupLayouts = &c->ubgl;
    WGPUPipelineLayout rlayout = wgpuDeviceCreatePipelineLayout(c->device, &rpl);
    WGPURenderPipelineDescriptor rpd; memset(&rpd, 0, sizeof(rpd));
    rpd.layout = rlayout;
    rpd.vertex.module = sm; rpd.vertex.entryPoint = sv("rect_vs");
    rpd.vertex.bufferCount = 1; rpd.vertex.buffers = &rvb;
    rpd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    rpd.multisample.count = 1; rpd.multisample.mask = 0xFFFFFFFF;
    WGPUFragmentState rfs; memset(&rfs, 0, sizeof(rfs));
    rfs.module = sm; rfs.entryPoint = sv("rect_fs"); rfs.targetCount = 1; rfs.targets = &cts;
    rpd.fragment = &rfs;
    c->rectPipe = wgpuDeviceCreateRenderPipeline(c->device, &rpd);

    /* ---- text pipeline ---- */
    WGPUVertexAttribute ga[6]; memset(ga, 0, sizeof(ga));
    ga[0].format = WGPUVertexFormat_Float32x4; ga[0].offset = 0;  ga[0].shaderLocation = 0;
    ga[1].format = WGPUVertexFormat_Float32x4; ga[1].offset = 16; ga[1].shaderLocation = 1;
    ga[2].format = WGPUVertexFormat_Float32x4; ga[2].offset = 32; ga[2].shaderLocation = 2;
    ga[3].format = WGPUVertexFormat_Float32x4; ga[3].offset = 48; ga[3].shaderLocation = 3;
    ga[4].format = WGPUVertexFormat_Float32x4; ga[4].offset = 64; ga[4].shaderLocation = 4;
    ga[5].format = WGPUVertexFormat_Float32x4; ga[5].offset = 80; ga[5].shaderLocation = 5;
    WGPUVertexBufferLayout gvb; memset(&gvb, 0, sizeof(gvb));
    gvb.stepMode = WGPUVertexStepMode_Instance; gvb.arrayStride = GLYPH_FLOATS * 4;
    gvb.attributeCount = 6; gvb.attributes = ga;
    WGPUBindGroupLayout tbls[2] = { c->ubgl, c->tbgl };
    WGPUPipelineLayoutDescriptor tpl; memset(&tpl, 0, sizeof(tpl));
    tpl.bindGroupLayoutCount = 2; tpl.bindGroupLayouts = tbls;
    WGPUBindGroupLayout tbls3[2] = { c->ubgl, c->tbgl3 };
    WGPUPipelineLayoutDescriptor tpl3; memset(&tpl3, 0, sizeof(tpl3));
    tpl3.bindGroupLayoutCount = 2; tpl3.bindGroupLayouts = tbls3;
    WGPUPipelineLayout tlayout = wgpuDeviceCreatePipelineLayout(c->device, &tpl3);
    WGPURenderPipelineDescriptor tpd; memset(&tpd, 0, sizeof(tpd));
    tpd.layout = tlayout;
    tpd.vertex.module = sm; tpd.vertex.entryPoint = sv("text_vs");
    tpd.vertex.bufferCount = 1; tpd.vertex.buffers = &gvb;
    tpd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    tpd.multisample.count = 1; tpd.multisample.mask = 0xFFFFFFFF;
    WGPUFragmentState tfs; memset(&tfs, 0, sizeof(tfs));
    tfs.module = sm; tfs.entryPoint = sv("text_fs"); tfs.targetCount = 1; tfs.targets = &cts;
    tpd.fragment = &tfs;
    c->textPipe = wgpuDeviceCreateRenderPipeline(c->device, &tpd);

    /* ---- image pipeline (rect + RGBA texture sample) ---- */
    WGPUVertexAttribute ia[4]; memset(ia, 0, sizeof(ia));
    ia[0].format = WGPUVertexFormat_Float32x4; ia[0].offset = 0;  ia[0].shaderLocation = 0;
    ia[1].format = WGPUVertexFormat_Float32x4; ia[1].offset = 16; ia[1].shaderLocation = 1;
    ia[2].format = WGPUVertexFormat_Float32x4; ia[2].offset = 32; ia[2].shaderLocation = 2;
    ia[3].format = WGPUVertexFormat_Float32x4; ia[3].offset = 48; ia[3].shaderLocation = 3;
    WGPUVertexBufferLayout ivb; memset(&ivb, 0, sizeof(ivb));
    ivb.stepMode = WGPUVertexStepMode_Instance; ivb.arrayStride = IMG_FLOATS * 4;
    ivb.attributeCount = 4; ivb.attributes = ia;
    WGPUPipelineLayout ilayout = wgpuDeviceCreatePipelineLayout(c->device, &tpl);   /* ubgl+tbgl */
    WGPURenderPipelineDescriptor ipd; memset(&ipd, 0, sizeof(ipd));
    ipd.layout = ilayout;
    ipd.vertex.module = sm; ipd.vertex.entryPoint = sv("img_vs");
    ipd.vertex.bufferCount = 1; ipd.vertex.buffers = &ivb;
    ipd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    ipd.multisample.count = 1; ipd.multisample.mask = 0xFFFFFFFF;
    WGPUFragmentState ifs; memset(&ifs, 0, sizeof(ifs));
    ifs.module = sm; ifs.entryPoint = sv("img_fs"); ifs.targetCount = 1; ifs.targets = &cts;
    ipd.fragment = &ifs;
    c->imagePipe = wgpuDeviceCreateRenderPipeline(c->device, &ipd);

    wgpuShaderModuleRelease(sm);
    return c->rectPipe && c->textPipe && c->imagePipe;
}

static int acquire_device(Ctx* c) {
    WGPURequestAdapterOptions opt; memset(&opt, 0, sizeof(opt));
    if (c->surface) opt.compatibleSurface = c->surface;
    WGPURequestAdapterCallbackInfo aci; memset(&aci, 0, sizeof(aci));
    aci.mode = WGPUCallbackMode_AllowProcessEvents; aci.callback = on_adapter; aci.userdata1 = &c->adapter;
    wgpuInstanceRequestAdapter(c->instance, &opt, aci);
    if (!c->adapter) return 0;
    WGPURequestDeviceCallbackInfo dci; memset(&dci, 0, sizeof(dci));
    dci.mode = WGPUCallbackMode_AllowProcessEvents; dci.callback = on_device; dci.userdata1 = &c->device;
    /* Ask for everything the adapter can do, rather than taking WebGPU's conservative defaults.
       A NULL descriptor gives `wgpu::Limits::default()`, the portable web baseline where
       maxStorageBuffersPerShaderStage is 8, and any pipeline that needs more is rejected at
       creation time with no way to widen the limit afterwards, because limits are fixed when the
       device is made. A ray marcher that binds ten storage buffers in one fragment stage is such
       a case. Raising a limit only ever accepts more; every pipeline that builds against the
       defaults builds unchanged. If the adapter will not report its limits we fall back to a NULL
       request rather than failing to get a device. */
    /* And TEXTURE_ADAPTER_SPECIFIC_FORMAT_FEATURES when the adapter offers it, for the same reason
       as the limits above: a capability refused at device creation can never be recovered
       afterwards. This one makes rgba16float usable with read-write storage access, which a
       compute pass that reads and rewrites an HDR render target needs; without it the bind group
       layout is rejected outright.
       Requested only when the adapter lists it, so an adapter without it still gets a device.
       A caller trying to detect this later should know that neither `wgpuDeviceHasFeature` nor
       `wgpuDeviceGetFeatures` reports a wgpu-native feature, even on a device created with one.
       The only reliable test is to build the thing inside an error scope. */
    WGPUFeatureName want_rw = (WGPUFeatureName)WGPUNativeFeature_TextureAdapterSpecificFormatFeatures;
    int have_rw = 0;
    {
        WGPUSupportedFeatures sf; memset(&sf, 0, sizeof(sf));
        wgpuAdapterGetFeatures(c->adapter, &sf);
        for (size_t i = 0; i < sf.featureCount; i++)
            if (sf.features[i] == want_rw) have_rw = 1;
    }
    WGPULimits alim; memset(&alim, 0, sizeof(alim));
    if (wgpuAdapterGetLimits(c->adapter, &alim) == WGPUStatus_Success) {
        WGPUDeviceDescriptor dd; memset(&dd, 0, sizeof(dd));
        dd.requiredLimits = &alim;
        if (have_rw) { dd.requiredFeatureCount = 1; dd.requiredFeatures = &want_rw; }
        wgpuAdapterRequestDevice(c->adapter, &dd, dci);
    }
    if (!c->device) wgpuAdapterRequestDevice(c->adapter, NULL, dci);
    if (!c->device) return 0;
    c->queue = wgpuDeviceGetQueue(c->device);
    return 1;
}

/* (re)create the offscreen render target + readback buffer at c->w x c->h */
static void make_target(Ctx* c) {
    if (c->targetView) { wgpuTextureViewRelease(c->targetView); c->targetView = NULL; }
    if (c->target) { wgpuTextureRelease(c->target); c->target = NULL; }
    rb_settle(c);
    if (c->readbuf) { wgpuBufferRelease(c->readbuf); c->readbuf = NULL; }
    if (c->rbuf2)   { wgpuBufferRelease(c->rbuf2);   c->rbuf2 = NULL; }
    WGPUTextureDescriptor td; memset(&td, 0, sizeof(td));
    td.usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc;
    td.dimension = WGPUTextureDimension_2D; td.size.width = c->w; td.size.height = c->h; td.size.depthOrArrayLayers = 1;
    td.format = c->format; td.mipLevelCount = 1; td.sampleCount = 1;
    c->target = wgpuDeviceCreateTexture(c->device, &td);
    c->targetView = wgpuTextureCreateView(c->target, NULL);
    c->rb_bpr = ((c->w * 4) + 255) & ~255;
    WGPUBufferDescriptor bd; memset(&bd, 0, sizeof(bd));
    bd.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead; bd.size = (size_t)c->rb_bpr * c->h;
    c->readbuf = wgpuDeviceCreateBuffer(c->device, &bd);
    /* the pipelined path's second buffer. Allocated with the first so a resize replaces both. */
    if (c->rbuf2) { wgpuBufferRelease(c->rbuf2); c->rbuf2 = NULL; }
    c->rbuf2 = wgpuDeviceCreateBuffer(c->device, &bd);
    c->rb_cur = 0;
    c->rb_pending = 0;
}

void* ui_gpu_new_offscreen(int w, int h) {
    Ctx* c = (Ctx*)calloc(1, sizeof(Ctx));
    ui_gpu_clip_none(c);
    c->offscreen = 1; c->w = w; c->h = h; c->format = WGPUTextureFormat_RGBA8Unorm;
    c->instance = wgpuCreateInstance(NULL);
    if (!acquire_device(c)) return c;
    if (!build_pipelines(c)) return c;
    make_target(c);
    return c;
}

/* prefer a straight (non-sRGB) 8-bit UNORM surface format: the shader outputs straight 0..1 color
   (same 0..255 space the CPU renderer blends in) and the CPU's wl_shm buffer is straight XRGB8888,
   so a non-sRGB surface makes the GPU output match the CPU exactly. An sRGB surface would gamma-encode
   the output on write, shifting AA edges (text looks soft / low-contrast). */
static WGPUTextureFormat pick_surface_format(WGPUSurfaceCapabilities* caps) {
    for (size_t i = 0; i < caps->formatCount; i++) {
        if (caps->formats[i] == WGPUTextureFormat_BGRA8Unorm || caps->formats[i] == WGPUTextureFormat_RGBA8Unorm) return caps->formats[i];
    }
    return caps->formatCount > 0 ? caps->formats[0] : WGPUTextureFormat_BGRA8Unorm;
}

/* The window handle is 64 bits wide, and `unsigned long` is not that on Windows.
 *
 * LLP64 gives `long` four bytes, so an HWND passed through a `long` parameter would be truncated
 * and sign-extended. On Linux `unsigned long` is eight bytes and an XID is 32, so that spelling
 * would work there. The Nori side passes an Int, which is 64 bits everywhere, so the parameter is
 * `unsigned long long`. */
void* ui_gpu_new_window(void* x11_display, unsigned long long native_window, int w, int h) {
    Ctx* c = (Ctx*)calloc(1, sizeof(Ctx));
    ui_gpu_clip_none(c);
    c->offscreen = 0; c->w = w; c->h = h;
    c->instance = wgpuCreateInstance(NULL);
    WGPUSurfaceDescriptor sfd; memset(&sfd, 0, sizeof(sfd));
#ifdef _WIN32
    /* Windows has no display to point at. Its backend answers 0 for `nori_win_display`, and wgpu's
     * Vulkan path takes that as an Xlib display it was not given: "Display pointer is not set."
     * A window here is an HWND plus the module it belongs to. */
    (void)x11_display;
    WGPUSurfaceSourceWindowsHWND src; memset(&src, 0, sizeof(src));
    src.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    src.hinstance = (void*)GetModuleHandleW(NULL);
    src.hwnd = (void*)(uintptr_t)native_window;
#else
    WGPUSurfaceSourceXlibWindow src; memset(&src, 0, sizeof(src));
    src.chain.sType = WGPUSType_SurfaceSourceXlibWindow; src.display = x11_display; src.window = (unsigned long)native_window;
#endif
    sfd.nextInChain = (WGPUChainedStruct*)&src;
    c->surface = wgpuInstanceCreateSurface(c->instance, &sfd);
    if (!acquire_device(c)) return c;
    WGPUSurfaceCapabilities caps; memset(&caps, 0, sizeof(caps));
    wgpuSurfaceGetCapabilities(c->surface, c->adapter, &caps);
    c->format = pick_surface_format(&caps);
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    if (!build_pipelines(c)) return c;
    WGPUSurfaceConfiguration cfg; memset(&cfg, 0, sizeof(cfg));
    cfg.device = c->device; cfg.format = c->format; cfg.usage = WGPUTextureUsage_RenderAttachment;
    cfg.width = w; cfg.height = h; cfg.presentMode = WGPUPresentMode_Fifo; cfg.alphaMode = WGPUCompositeAlphaMode_Auto;
    wgpuSurfaceConfigure(c->surface, &cfg);
    return c;
}

/* finish setup once c->surface exists (adapter/device/pipelines/configure), shared by X11 + Wayland */
static void* finish_surface_init(Ctx* c, int w, int h) {
    if (!acquire_device(c)) return c;
    WGPUSurfaceCapabilities caps; memset(&caps, 0, sizeof(caps));
    wgpuSurfaceGetCapabilities(c->surface, c->adapter, &caps);
    c->format = pick_surface_format(&caps);
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    if (!build_pipelines(c)) return c;
    WGPUSurfaceConfiguration cfg; memset(&cfg, 0, sizeof(cfg));
    cfg.device = c->device; cfg.format = c->format; cfg.usage = WGPUTextureUsage_RenderAttachment;
    cfg.width = w; cfg.height = h; cfg.presentMode = WGPUPresentMode_Fifo; cfg.alphaMode = WGPUCompositeAlphaMode_Auto;
    wgpuSurfaceConfigure(c->surface, &cfg);
    return c;
}

/* windowed canvas on a native Wayland surface (wl_display + wl_surface, e.g. from std/window_wayland) */
void* ui_gpu_new_window_wl(void* wl_display, void* wl_surface, int w, int h) {
    Ctx* c = (Ctx*)calloc(1, sizeof(Ctx));
    ui_gpu_clip_none(c);
    c->offscreen = 0; c->w = w; c->h = h;
    c->instance = wgpuCreateInstance(NULL);
    WGPUSurfaceSourceWaylandSurface src; memset(&src, 0, sizeof(src));
    src.chain.sType = WGPUSType_SurfaceSourceWaylandSurface; src.display = wl_display; src.surface = wl_surface;
    WGPUSurfaceDescriptor sfd; memset(&sfd, 0, sizeof(sfd)); sfd.nextInChain = (WGPUChainedStruct*)&src;
    c->surface = wgpuInstanceCreateSurface(c->instance, &sfd);
    return finish_surface_init(c, w, h);
}

int ui_gpu_ok(void* ctx) { Ctx* c = (Ctx*)ctx; return (c->device && c->rectPipe && c->textPipe && c->imagePipe) ? 1 : 0; }

void ui_gpu_resize(void* ctx, int w, int h) {
    Ctx* c = (Ctx*)ctx; if (w <= 0 || h <= 0 || (w == c->w && h == c->h)) return;
    c->w = w; c->h = h;
    if (c->offscreen) { make_target(c); }
    else {
        WGPUSurfaceConfiguration cfg; memset(&cfg, 0, sizeof(cfg));
        cfg.device = c->device; cfg.format = c->format; cfg.usage = WGPUTextureUsage_RenderAttachment;
        cfg.width = w; cfg.height = h; cfg.presentMode = WGPUPresentMode_Fifo; cfg.alphaMode = WGPUCompositeAlphaMode_Auto;
        wgpuSurfaceConfigure(c->surface, &cfg);
    }
}

/* Per-primitive clipping (ui_cpu's PaintPrimitive.has_clip/clip). This is frame state, not a
   per-call argument: set it, emit any number of rects/glyphs/images, clear it. Each emitted instance
   copies the box into its own instance data, so one batched draw still carries many different clip
   boxes. wgpuRenderPassEncoderSetScissorRect is the alternative, but a scissor is
   per-draw-call state: honouring it would mean splitting the instance batch at every clip change, so
   a document with N scroll viewports would cost N+1 draws per pipeline instead of 1. Clipping
   analytically in the fragment shader keeps the batching intact; see clip_mask().
   The box is a half-open pixel range [x0,x1) x [y0,y1), already snapped to whole pixels. */
void ui_gpu_clip(void* ctx, double x0, double y0, double x1, double y1) {
    Ctx* c = (Ctx*)ctx;
    c->clip[0] = (float)x0; c->clip[1] = (float)y0; c->clip[2] = (float)x1; c->clip[3] = (float)y1;
}
void ui_gpu_clip_none(void* ctx) {
    Ctx* c = (Ctx*)ctx;
    c->clip[0] = CLIP_NONE_LO; c->clip[1] = CLIP_NONE_LO; c->clip[2] = CLIP_NONE_HI; c->clip[3] = CLIP_NONE_HI;
}

/* Record a cut at the current position, if one is needed.
 *
 * Rects and glyphs are each one instanced draw, and rects are drawn first. That is correct only
 * while every rect in the frame belongs underneath every glyph in it. When a caller draws a panel
 * over something already drawn (a menu dropdown over the dock, a popup over a document), its
 * background is a rect and the text underneath it is a glyph, so without a break the text would
 * come out on top of the panel that is supposed to cover it.
 *
 * So a rect queued after any glyph forces a break. `cutGlyph` keeps this cheap: consecutive
 * rects with no glyphs between them share one run, which is the ordinary case (a widget emits its
 * backgrounds together, then its text), so a document that never interleaves costs just the two
 * draws. */
static void push_cut(Ctx* c, int img) {
    if (c->cutCount + 1 > c->cutCap) {
        c->cutCap = c->cutCap ? c->cutCap * 2 : 16;
        c->cuts = (Cut*)realloc(c->cuts, (size_t)c->cutCap * sizeof(Cut));
    }
    c->cuts[c->cutCount].rectAt = c->rectCount;
    c->cuts[c->cutCount].glyphAt = c->glyphCount;
    c->cuts[c->cutCount].img = img;
    c->cutCount++;
    c->cutGlyph = c->glyphCount;
}

void ui_gpu_begin(void* ctx, double r, double g, double b) {
    Ctx* c = (Ctx*)ctx;
    free_images(c);   /* release the previous frame's image textures: the GPU has finished sampling
                         them by now (present/poll happened), which avoids the use-after-free that
                         freeing them right after present would cause on the windowed (async) path. */
    c->rectCount = 0; c->glyphCount = 0; c->clearR = r; c->clearG = g; c->clearB = b;
    c->cutCount = 0; c->cutGlyph = 0;
    ui_gpu_clip_none(c);
}

/* One rect instance. Like ui_gpu_glyph, the gradient travels with the call (gtr..gta, gax, gaxd,
   ghoriz, gon) rather than through a setter that stores it on the Ctx: a C entry point whose only
   observable effect is a store into an opaque context can have that store optimised away, because
   the C and the Nori code are optimised together. One call, one instance, no extra draw. */
void ui_gpu_rect(void* ctx, double x, double y, double w, double h,
                 double r, double g, double b, double a, double radius, double border,
                 double gtr, double gtg, double gtb, double gta,
                 double gax, double gaxd, double ghoriz, double gon) {
    Ctx* c = (Ctx*)ctx;
    /* a rect that comes after text must be drawn after it, not folded into the batch that goes first */
    if (c->glyphCount > c->cutGlyph) push_cut(c, -1);
    if (c->rectCount + 1 > c->rectCap) {
        c->rectCap = c->rectCap ? c->rectCap * 2 : 256;
        c->rectData = (float*)realloc(c->rectData, (size_t)c->rectCap * RECT_FLOATS * sizeof(float));
    }
    float* p = c->rectData + (size_t)c->rectCount * RECT_FLOATS;
    p[0]=(float)x; p[1]=(float)y; p[2]=(float)w; p[3]=(float)h;
    p[4]=(float)r; p[5]=(float)g; p[6]=(float)b; p[7]=(float)a;
    p[8]=(float)radius; p[9]=(float)border;
    p[10]=c->clip[0]; p[11]=c->clip[1]; p[12]=c->clip[2]; p[13]=c->clip[3];
    p[14]=(float)gtr; p[15]=(float)gtg; p[16]=(float)gtb; p[17]=(float)gta;
    p[18]=(float)gax; p[19]=(float)gaxd; p[20]=(float)ghoriz; p[21]=(float)gon;
    c->rectCount++;
}

/* One glyph instance. The gradient travels with the call (gtr..gta, gax, gaxd, ghoriz, gon) rather
   than through a setter that stores it on the Ctx: a C entry point whose only observable effect is a
   store into an opaque context can have that store optimised away, because the C and the Nori code
   are optimised together. `clip` survives only because the field is volatile; a new one-call-one-effect
   parameter has no such hazard and needs no workaround. */
void ui_gpu_glyph(void* ctx, double x, double y, double w, double h,
                  double ux, double uy, double uw, double uh,
                  double r, double g, double b, double a,
                  double gtr, double gtg, double gtb, double gta,
                  double gax, double gaxd, double ghoriz, double gon) {
    Ctx* c = (Ctx*)ctx;
    if (c->glyphCount + 1 > c->glyphCap) {
        c->glyphCap = c->glyphCap ? c->glyphCap * 2 : 512;
        c->glyphData = (float*)realloc(c->glyphData, (size_t)c->glyphCap * GLYPH_FLOATS * sizeof(float));
    }
    float* p = c->glyphData + (size_t)c->glyphCount * GLYPH_FLOATS;
    p[0]=(float)x; p[1]=(float)y; p[2]=(float)w; p[3]=(float)h;
    p[4]=(float)ux; p[5]=(float)uy; p[6]=(float)uw; p[7]=(float)uh;
    p[8]=(float)r; p[9]=(float)g; p[10]=(float)b; p[11]=(float)a;
    p[12]=c->clip[0]; p[13]=c->clip[1]; p[14]=c->clip[2]; p[15]=c->clip[3];
    p[16]=(float)gtr; p[17]=(float)gtg; p[18]=(float)gtb; p[19]=(float)gta;
    p[20]=(float)gax; p[21]=(float)gaxd; p[22]=(float)ghoriz; p[23]=(float)gon;
    c->glyphCount++;
}

/* last-uploaded atlas glyph count (init -1); Nori compares to font::atlas_count to skip re-uploads */
int ui_gpu_atlas_gen(void* ctx) { Ctx* c = (Ctx*)ctx; return c->atlasTex ? c->atlasGen : -1; }

static WGPUTexture rgba_texture(Ctx* c, int w, int h) {
    WGPUTextureDescriptor td; memset(&td, 0, sizeof(td));
    td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    td.dimension = WGPUTextureDimension_2D; td.size.width = w; td.size.height = h; td.size.depthOrArrayLayers = 1;
    td.format = WGPUTextureFormat_RGBA8Unorm; td.mipLevelCount = 1; td.sampleCount = 1;
    return wgpuDeviceCreateTexture(c->device, &td);
}
static void write_rgba(Ctx* c, WGPUTexture t, const void* bytes, int w, int h) {
    WGPUTexelCopyTextureInfo dst; memset(&dst, 0, sizeof(dst));
    dst.texture = t; dst.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferLayout lay; memset(&lay, 0, sizeof(lay));
    lay.bytesPerRow = w * 4; lay.rowsPerImage = h;
    WGPUExtent3D ext; memset(&ext, 0, sizeof(ext)); ext.width = w; ext.height = h; ext.depthOrArrayLayers = 1;
    wgpuQueueWriteTexture(c->queue, &dst, bytes, (size_t)w * h * 4, &lay, &ext);
}
/* the text pipeline's bind group: coverage atlas, sampler, colour atlas (a 1x1 transparent texture
   until the first colour glyph arrives, so the group is always complete) */
static void atlas_bind_group(Ctx* c) {
    if (c->atlasBG) { wgpuBindGroupRelease(c->atlasBG); c->atlasBG = NULL; }
    if (!c->atlasView) return;
    if (!c->colorTex) {
        static const unsigned char z[4] = { 0, 0, 0, 0 };
        c->colorTex = rgba_texture(c, 1, 1);
        c->colorView = wgpuTextureCreateView(c->colorTex, NULL);
        c->colorW = 1; c->colorH = 1;
        write_rgba(c, c->colorTex, z, 1, 1);
    }
    WGPUBindGroupEntry be[3]; memset(be, 0, sizeof(be));
    be[0].binding = 0; be[0].textureView = c->atlasView;
    be[1].binding = 1; be[1].sampler = c->samp;
    be[2].binding = 2; be[2].textureView = c->colorView;
    WGPUBindGroupDescriptor bgd; memset(&bgd, 0, sizeof(bgd));
    bgd.layout = c->tbgl3; bgd.entryCount = 3; bgd.entries = be;
    c->atlasBG = wgpuDeviceCreateBindGroup(c->device, &bgd);
}
/* upload the atlas's colour-glyph plane: aw*ah premultiplied RGBA8 texels, laid out like the
   coverage atlas (a colour glyph's texels sit where its coverage does) */
void ui_gpu_atlas_color(void* ctx, void* bytes, int aw, int ah) {
    Ctx* c = (Ctx*)ctx;
    if (aw <= 0 || ah <= 0 || !c->device) return;
    if (!c->colorTex || c->colorW != aw || c->colorH != ah) {
        if (c->colorView) { wgpuTextureViewRelease(c->colorView); c->colorView = NULL; }
        if (c->colorTex) { wgpuTextureRelease(c->colorTex); c->colorTex = NULL; }
        c->colorTex = rgba_texture(c, aw, ah);
        c->colorView = wgpuTextureCreateView(c->colorTex, NULL);
        c->colorW = aw; c->colorH = ah;
        atlas_bind_group(c);
    }
    write_rgba(c, c->colorTex, bytes, aw, ah);
}

/* upload the R8 glyph atlas (aw*ah bytes at `bytes`); (re)creates texture + bind group on size change */
void ui_gpu_atlas(void* ctx, void* bytes, int aw, int ah, int gen) {
    Ctx* c = (Ctx*)ctx;
    if (aw <= 0 || ah <= 0) return;
    c->atlasGen = gen;
    if (!c->atlasTex || aw != c->atlasW || ah != c->atlasH) {
        if (c->atlasBG) { wgpuBindGroupRelease(c->atlasBG); c->atlasBG = NULL; }
        if (c->atlasView) { wgpuTextureViewRelease(c->atlasView); c->atlasView = NULL; }
        if (c->atlasTex) { wgpuTextureRelease(c->atlasTex); c->atlasTex = NULL; }
        WGPUTextureDescriptor td; memset(&td, 0, sizeof(td));
        td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
        td.dimension = WGPUTextureDimension_2D; td.size.width = aw; td.size.height = ah; td.size.depthOrArrayLayers = 1;
        td.format = WGPUTextureFormat_R8Unorm; td.mipLevelCount = 1; td.sampleCount = 1;
        c->atlasTex = wgpuDeviceCreateTexture(c->device, &td);
        c->atlasView = wgpuTextureCreateView(c->atlasTex, NULL);
        c->atlasW = aw; c->atlasH = ah;
        atlas_bind_group(c);
    }
    WGPUTexelCopyTextureInfo dst; memset(&dst, 0, sizeof(dst));
    dst.texture = c->atlasTex; dst.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferLayout lay; memset(&lay, 0, sizeof(lay));
    lay.bytesPerRow = aw; lay.rowsPerImage = ah;
    WGPUExtent3D ext; memset(&ext, 0, sizeof(ext)); ext.width = aw; ext.height = ah; ext.depthOrArrayLayers = 1;
    wgpuQueueWriteTexture(c->queue, &dst, bytes, (size_t)aw * ah, &lay, &ext);
}

/* queue a straight-RGBA8 image (iw*ih*4 bytes at `rgba`) to be drawn into `(x,y,w,h)` this frame,
   alpha-modulated by `alpha`. Creates a texture + bind group per image; released after the frame. */
void ui_gpu_image(void* ctx, void* rgba, int iw, int ih, double x, double y, double w, double h, double alpha) {
    Ctx* c = (Ctx*)ctx;
    if (iw <= 0 || ih <= 0 || !c->device) return;
    if (c->imgCount + 1 > c->imgCap) {
        c->imgCap = c->imgCap ? c->imgCap * 2 : 8;
        c->imgData = (float*)realloc(c->imgData, (size_t)c->imgCap * IMG_FLOATS * sizeof(float));
        c->imgRes = (ImgRes*)realloc(c->imgRes, (size_t)c->imgCap * sizeof(ImgRes));
    }
    WGPUTextureDescriptor td; memset(&td, 0, sizeof(td));
    td.usage = WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    td.dimension = WGPUTextureDimension_2D; td.size.width = iw; td.size.height = ih; td.size.depthOrArrayLayers = 1;
    td.format = WGPUTextureFormat_RGBA8Unorm; td.mipLevelCount = 1; td.sampleCount = 1;
    WGPUTexture tex = wgpuDeviceCreateTexture(c->device, &td);
    WGPUTextureView view = wgpuTextureCreateView(tex, NULL);
    WGPUTexelCopyTextureInfo dst; memset(&dst, 0, sizeof(dst)); dst.texture = tex; dst.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferLayout lay; memset(&lay, 0, sizeof(lay)); lay.bytesPerRow = iw * 4; lay.rowsPerImage = ih;
    WGPUExtent3D ext; memset(&ext, 0, sizeof(ext)); ext.width = iw; ext.height = ih; ext.depthOrArrayLayers = 1;
    wgpuQueueWriteTexture(c->queue, &dst, rgba, (size_t)iw * ih * 4, &lay, &ext);
    WGPUBindGroupEntry be[2]; memset(be, 0, sizeof(be));
    be[0].binding = 0; be[0].textureView = view; be[1].binding = 1; be[1].sampler = c->samp;
    WGPUBindGroupDescriptor bgd; memset(&bgd, 0, sizeof(bgd)); bgd.layout = c->tbgl; bgd.entryCount = 2; bgd.entries = be;
    WGPUBindGroup bg = wgpuDeviceCreateBindGroup(c->device, &bgd);
    ImgRes* R = &c->imgRes[c->imgCount]; R->tex = tex; R->view = view; R->bg = bg; R->owned = 1;
    R->rectAt = c->rectCount; R->glyphAt = c->glyphCount;
    push_cut(c, c->imgCount);
    float* p = c->imgData + (size_t)c->imgCount * IMG_FLOATS;
    p[0]=(float)x; p[1]=(float)y; p[2]=(float)w; p[3]=(float)h;
    p[4]=1.f; p[5]=1.f; p[6]=1.f; p[7]=(float)alpha;
    p[8]=c->clip[0]; p[9]=c->clip[1]; p[10]=c->clip[2]; p[11]=c->clip[3];
    p[12]=0.f; p[13]=0.f; p[14]=0.f; p[15]=0.f;   /* no v-flip, honour the image's own alpha */
    c->imgCount++;
}

/* ---- the three exports a host needs to fill a `surface` hole with a GPU render ----
   The scene renderer records and submits its own encoder into its own offscreen texture on the
   device borrowed below, and then hands the resulting texture view here; ui_gpu draws it as one more
   image instance. `ui_gpu`'s one-encoder/one-pass/one-submit invariant is untouched, and nothing
   about the UI's encoder or colour target is exposed.

   Everything travels as a parameter. There is no `ui_gpu_set_view(...)` followed by a later
   `ui_gpu_draw_view(...)`: the C and the Nori code are optimised together, and a setter whose only
   observable effect is a store into an opaque context can have that store eliminated (see the
   `volatile float clip[4]` note on Ctx). One call, one instance. */

/* The WGPUDevice this context owns. Borrowed: the caller must not release it, and must not use it
   after ui_gpu_free. NULL when the backend did not come up (ui_gpu_ok(ctx) == 0). */
void* ui_gpu_device(void* ctx) { Ctx* c = (Ctx*)ctx; if (!c || !c->device || !c->rectPipe) return NULL; return (void*)c->device; }
/* The WGPUQueue, on the same terms. */
void* ui_gpu_queue(void* ctx) { Ctx* c = (Ctx*)ctx; if (!c || !c->queue || !c->rectPipe) return NULL; return (void*)c->queue; }
/* The colour format this context renders into, as a WGPUTextureFormat enum value. */
int ui_gpu_target_format(void* ctx) { Ctx* c = (Ctx*)ctx; return c ? (int)c->format : 0; }
/* The WGPUAdapter, borrowed, for limit/feature queries without a second instance. */
void* ui_gpu_adapter(void* ctx) { Ctx* c = (Ctx*)ctx; return c ? (void*)c->adapter : NULL; }
/* The WGPUInstance, borrowed. */
void* ui_gpu_instance(void* ctx) { Ctx* c = (Ctx*)ctx; return c ? (void*)c->instance : NULL; }

/* Queue an already-resident texture view to be drawn into (x,y,w,h) this frame, alpha-modulated by
   `alpha`. `view` must be a WGPUTextureView that is TextureBinding-usable, sampled-float, 2D, and
   alive until ui_gpu_end returns. Unlike ui_gpu_image the view is not owned and is never released
   here. `linear` picks the filtering sampler (1) or the UI's nearest one (0). `flip_v` (1) samples
   with v inverted, for a renderer whose target y runs the other way.
   The texture's own alpha is ignored (a colour target cleared with alpha 0 would otherwise blit
   as nothing at all), so the blit is opaque, scaled by `alpha` alone. */
void ui_gpu_image_view(void* ctx, void* view, double x, double y, double w, double h, double alpha, int linear, int flip_v) {
    Ctx* c = (Ctx*)ctx;
    if (!c || !c->device || !view || w <= 0.0 || h <= 0.0) return;
    if (c->imgCount + 1 > c->imgCap) {
        c->imgCap = c->imgCap ? c->imgCap * 2 : 8;
        c->imgData = (float*)realloc(c->imgData, (size_t)c->imgCap * IMG_FLOATS * sizeof(float));
        c->imgRes = (ImgRes*)realloc(c->imgRes, (size_t)c->imgCap * sizeof(ImgRes));
    }
    WGPUBindGroupEntry be[2]; memset(be, 0, sizeof(be));
    be[0].binding = 0; be[0].textureView = (WGPUTextureView)view;
    be[1].binding = 1; be[1].sampler = linear ? c->sampLinear : c->samp;
    WGPUBindGroupDescriptor bgd; memset(&bgd, 0, sizeof(bgd)); bgd.layout = c->tbgl; bgd.entryCount = 2; bgd.entries = be;
    WGPUBindGroup bg = wgpuDeviceCreateBindGroup(c->device, &bgd);
    ImgRes* R = &c->imgRes[c->imgCount]; R->tex = NULL; R->view = NULL; R->bg = bg; R->owned = 0;
    R->rectAt = c->rectCount; R->glyphAt = c->glyphCount;
    push_cut(c, c->imgCount);
    float* p = c->imgData + (size_t)c->imgCount * IMG_FLOATS;
    p[0]=(float)x; p[1]=(float)y; p[2]=(float)w; p[3]=(float)h;
    p[4]=1.f; p[5]=1.f; p[6]=1.f; p[7]=(float)alpha;
    p[8]=c->clip[0]; p[9]=c->clip[1]; p[10]=c->clip[2]; p[11]=c->clip[3];
    p[12]=flip_v ? 1.f : 0.f; p[13]=1.f; p[14]=0.f; p[15]=0.f;
    c->imgCount++;
}

/* release this frame's per-image textures/bind groups (called after the draw is submitted) */
static void free_images(Ctx* c) {
    for (int i = 0; i < c->imgCount; i++) {
        if (c->imgRes[i].bg) wgpuBindGroupRelease(c->imgRes[i].bg);   /* always ours: made here */
        if (c->imgRes[i].owned) {
            if (c->imgRes[i].view) wgpuTextureViewRelease(c->imgRes[i].view);
            if (c->imgRes[i].tex) wgpuTextureRelease(c->imgRes[i].tex);
        }
    }
    c->imgCount = 0;
}

/* ensure a device buffer of at least `bytes`, (re)creating on growth. Returns the buffer. */
static WGPUBuffer ensure_buf(Ctx* c, WGPUBuffer* slot, int* cap, size_t bytes) {
    if (!*slot || (size_t)*cap < bytes) {
        if (*slot) wgpuBufferRelease(*slot);
        size_t nc = bytes < 4096 ? 4096 : bytes;
        WGPUBufferDescriptor bd; memset(&bd, 0, sizeof(bd));
        bd.usage = WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst; bd.size = nc;
        *slot = wgpuDeviceCreateBuffer(c->device, &bd); *cap = (int)nc;
    }
    return *slot;
}

/* one run of rect instances, [a, b). A run, not the whole batch: see encode_frame. */
static void draw_rect_run(Ctx* c, WGPURenderPassEncoder pass, int a, int b) {
    if (b <= a) return;
    size_t stride = RECT_FLOATS * sizeof(float);
    wgpuRenderPassEncoderSetPipeline(pass, c->rectPipe);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, c->ubg, 0, NULL);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, c->rectBuf, (size_t)a * stride, (size_t)(b - a) * stride);
    wgpuRenderPassEncoderDraw(pass, 6, b - a, 0, 0);
}
/* one run of glyph instances, [a, b). */
static void draw_glyph_run(Ctx* c, WGPURenderPassEncoder pass, int a, int b) {
    if (b <= a || !c->atlasBG) return;
    size_t stride = GLYPH_FLOATS * sizeof(float);
    wgpuRenderPassEncoderSetPipeline(pass, c->textPipe);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, c->ubg, 0, NULL);
    wgpuRenderPassEncoderSetBindGroup(pass, 1, c->atlasBG, 0, NULL);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, c->glyphBuf, (size_t)a * stride, (size_t)(b - a) * stride);
    wgpuRenderPassEncoderDraw(pass, 6, b - a, 0, 0);
}

/* record the draw into `view` (either the offscreen target or a surface texture view) */
static void encode_frame(Ctx* c, WGPUTextureView view) {
    float uni[4] = { (float)c->w, (float)c->h, (float)(c->atlasW ? c->atlasW : 1), (float)(c->atlasH ? c->atlasH : 1) };
    wgpuQueueWriteBuffer(c->queue, c->ubo, 0, uni, sizeof(uni));
    if (c->rectCount) {
        size_t nb = (size_t)c->rectCount * RECT_FLOATS * sizeof(float);
        WGPUBuffer b = ensure_buf(c, &c->rectBuf, &c->rectBufCap, nb);
        wgpuQueueWriteBuffer(c->queue, b, 0, c->rectData, nb);
    }
    if (c->glyphCount) {
        size_t nb = (size_t)c->glyphCount * GLYPH_FLOATS * sizeof(float);
        WGPUBuffer b = ensure_buf(c, &c->glyphBuf, &c->glyphBufCap, nb);
        wgpuQueueWriteBuffer(c->queue, b, 0, c->glyphData, nb);
    }
    if (c->imgCount) {
        size_t nb = (size_t)c->imgCount * IMG_FLOATS * sizeof(float);
        WGPUBuffer b = ensure_buf(c, &c->imgBuf, &c->imgBufCap, nb);
        wgpuQueueWriteBuffer(c->queue, b, 0, c->imgData, nb);
    }
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(c->device, NULL);
    WGPURenderPassColorAttachment ca; memset(&ca, 0, sizeof(ca));
    ca.view = view; ca.loadOp = WGPULoadOp_Clear; ca.storeOp = WGPUStoreOp_Store; ca.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    ca.clearValue.r = c->clearR; ca.clearValue.g = c->clearG; ca.clearValue.b = c->clearB; ca.clearValue.a = 1.0;
    WGPURenderPassDescriptor rp; memset(&rp, 0, sizeof(rp)); rp.colorAttachmentCount = 1; rp.colorAttachments = &ca;
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &rp);
    /* Submission order.
       Rects and glyphs are each one instanced draw whenever nothing sits between them, and within a
       run rects go first, which is the order both painters use. That is correct only while every
       rect belongs under every glyph, and two cases break that:

         * an image cannot join either batch (it has its own texture bind group), and drawing images
           last would put one over every rect and glyph in the frame whatever order the caller asked
           for. A host that blits a scene into a `surface` hole and draws chrome over it would lose
           the chrome;
         * a rect queued after a glyph would still be folded into the batch that goes first, so
           anything drawn over existing text would come out under it. A menu dropdown painted last,
           over the dock and over every panel, would have the panels' tab titles and button labels
           showing through it.

       So both are `Cut`s in one list, in emission order: everything queued up to the cut, then the
       image if it is one, then on. With no images and no rect-after-text this is just the two
       draws, and consecutive rects still share a run, so a widget emitting its backgrounds together
       and then its text costs nothing. Interleaving costs a pair of draws per alternation, which is
       the price of getting the stacking order right. */
    int rdone = 0, gdone = 0;
    size_t istride = IMG_FLOATS * sizeof(float);
    for (int i = 0; i < c->cutCount; i++) {
        draw_rect_run(c, pass, rdone, c->cuts[i].rectAt);   rdone = c->cuts[i].rectAt;
        draw_glyph_run(c, pass, gdone, c->cuts[i].glyphAt); gdone = c->cuts[i].glyphAt;
        int im = c->cuts[i].img;
        if (im < 0) continue;               /* a plain break: the flush above is all it does */
        /* each image: its own texture bind group + its slice of the instance buffer (offset, not
           firstInstance; a non-zero base instance is unreliable here). */
        wgpuRenderPassEncoderSetPipeline(pass, c->imagePipe);
        wgpuRenderPassEncoderSetBindGroup(pass, 0, c->ubg, 0, NULL);
        wgpuRenderPassEncoderSetBindGroup(pass, 1, c->imgRes[im].bg, 0, NULL);
        wgpuRenderPassEncoderSetVertexBuffer(pass, 0, c->imgBuf, (size_t)im * istride, istride);
        wgpuRenderPassEncoderDraw(pass, 6, 1, 0, 0);
    }
    draw_rect_run(c, pass, rdone, c->rectCount);
    draw_glyph_run(c, pass, gdone, c->glyphCount);
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, NULL);
    wgpuQueueSubmit(c->queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
}

void ui_gpu_end(void* ctx) {
    Ctx* c = (Ctx*)ctx;
    if (!c->device) return;
    if (c->offscreen) {
        encode_frame(c, c->targetView);
        wgpuDevicePoll(c->device, 1, NULL);
    } else {
        WGPUSurfaceTexture st; memset(&st, 0, sizeof(st));
        wgpuSurfaceGetCurrentTexture(c->surface, &st);
        if (st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
            st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) return;
        WGPUTextureView view = wgpuTextureCreateView(st.texture, NULL);
        encode_frame(c, view);
        wgpuSurfacePresent(c->surface);
        wgpuTextureViewRelease(view);
        wgpuTextureRelease(st.texture);
    }
    /* image textures are freed at the next ui_gpu_begin (deferred one frame), not here: releasing
       them right after an async present would free them before the GPU samples them. */
}

/* offscreen: read the rendered frame into `out` as w*h ints (0x00RRGGBB), matching ui_cpu's canvas */
int ui_gpu_readback(void* ctx, void* out) {
    Ctx* c = (Ctx*)ctx;
    if (!c->offscreen || !c->device) return 0;
    /* The pipelined path may have left `readbuf` mapping; this one is about to map it again, and
       mapping a buffer that is already mapped corrupts the state until the next submit dies with
       "Buffer with '' label is still mapped", nowhere near the cause. Settling first makes the two
       readbacks safe to interleave, which matters when a program mixes the two readback paths. */
    rb_settle(c);
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(c->device, NULL);
    WGPUTexelCopyTextureInfo src; memset(&src, 0, sizeof(src));
    src.texture = c->target; src.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferInfo dst; memset(&dst, 0, sizeof(dst));
    dst.buffer = c->readbuf; dst.layout.bytesPerRow = c->rb_bpr; dst.layout.rowsPerImage = c->h;
    WGPUExtent3D ext; memset(&ext, 0, sizeof(ext)); ext.width = c->w; ext.height = c->h; ext.depthOrArrayLayers = 1;
    wgpuCommandEncoderCopyTextureToBuffer(enc, &src, &dst, &ext);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, NULL);
    wgpuQueueSubmit(c->queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd); wgpuCommandEncoderRelease(enc);
    g_mapped = 0;
    WGPUBufferMapCallbackInfo mci; memset(&mci, 0, sizeof(mci));
    mci.mode = WGPUCallbackMode_AllowProcessEvents; mci.callback = on_map;
    size_t total = (size_t)c->rb_bpr * c->h;
    wgpuBufferMapAsync(c->readbuf, WGPUMapMode_Read, 0, total, mci);
    for (int i = 0; i < 100000 && !g_mapped; i++) wgpuDevicePoll(c->device, 1, NULL);
    if (!g_mapped || g_mapst != WGPUMapAsyncStatus_Success) return 0;
    const unsigned char* p = (const unsigned char*)wgpuBufferGetConstMappedRange(c->readbuf, 0, total);
    unsigned int* o = (unsigned int*)out;
    for (int y = 0; y < c->h; y++) {
        const unsigned char* row = p + (size_t)y * c->rb_bpr;
        for (int x = 0; x < c->w; x++) {
            const unsigned char* px = row + x * 4;   /* RGBA8 */
            o[(size_t)y * c->w + x] = ((unsigned)px[0] << 16) | ((unsigned)px[1] << 8) | (unsigned)px[2];
        }
    }
    wgpuBufferUnmap(c->readbuf);
    return 1;
}

/* Collect and unmap whatever the pipelined readback left in flight.
 *
 * The last call of a run always leaves one buffer mapping (that is the point of the
 * pipeline: the copy is collected next frame), and there is no next frame at teardown. Releasing a
 * buffer that is still mapped aborts the process from inside wgpu with "Buffer with '' label is
 * still mapped", which is a teardown crash reported nowhere near the readback. So both the resize
 * path and `ui_gpu_free` finish it first. */
static void rb_settle(Ctx* c) {
    if (!c || !c->rb_pending) return;
    WGPUBuffer inflight = c->rb_cur ? c->readbuf : c->rbuf2;
    for (int i = 0; i < 100000 && !g_mapped; i++) wgpuDevicePoll(c->device, 1, NULL);
    if (inflight && g_mapped && g_mapst == WGPUMapAsyncStatus_Success) wgpuBufferUnmap(inflight);
    c->rb_pending = 0;
}

/* Pipelined readback: hand back the previous frame's pixels and start this frame's copy.
 *
 * Returns 1 when `out` was filled (so: 0 on the very first call, which has no previous frame), and
 * the caller skips that frame rather than treating it as an error.
 *
 * `out` receives raw RGBA8 rows, w*4 bytes each, tightly packed, not the `0x00RRGGBB` words
 * `ui_gpu_readback` produces. That repack is a per-pixel scalar loop, two million iterations at
 * 1080p, and a video encoder wants bytes it can be handed directly. The 256-byte row padding wgpu
 * requires is still removed here, which is what the per-row memcpy is for.
 */
int ui_gpu_readback_pipelined(void* ctx, void* out) {
    Ctx* c = (Ctx*)ctx;
    if (!c->offscreen || !c->device || !c->readbuf || !c->rbuf2) return 0;
    size_t total = (size_t)c->rb_bpr * c->h;
    WGPUBuffer writing = c->rb_cur ? c->rbuf2 : c->readbuf;   /* this frame copies into here */
    WGPUBuffer reading = c->rb_cur ? c->readbuf : c->rbuf2;   /* last frame copied into here */

    int got = 0;
    if (c->rb_pending) {
        /* Finish last frame's map. In the steady state the copy completed frames ago and the
           callback only needs the queue pumped, so this poll does not stall; the bound is here for
           a device that never answers, not for the normal case. */
        for (int i = 0; i < 100000 && !g_mapped; i++) wgpuDevicePoll(c->device, 1, NULL);
        if (g_mapped && g_mapst == WGPUMapAsyncStatus_Success) {
            const unsigned char* p = (const unsigned char*)wgpuBufferGetConstMappedRange(reading, 0, total);
            if (p) {
                unsigned char* o = (unsigned char*)out;
                size_t row_bytes = (size_t)c->w * 4;
                for (int y = 0; y < c->h; y++)
                    memcpy(o + (size_t)y * row_bytes, p + (size_t)y * c->rb_bpr, row_bytes);
                got = 1;
            }
            wgpuBufferUnmap(reading);
        }
        c->rb_pending = 0;
    }

    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(c->device, NULL);
    WGPUTexelCopyTextureInfo src; memset(&src, 0, sizeof(src));
    src.texture = c->target; src.aspect = WGPUTextureAspect_All;
    WGPUTexelCopyBufferInfo dst; memset(&dst, 0, sizeof(dst));
    dst.buffer = writing; dst.layout.bytesPerRow = c->rb_bpr; dst.layout.rowsPerImage = c->h;
    WGPUExtent3D ext; memset(&ext, 0, sizeof(ext)); ext.width = c->w; ext.height = c->h; ext.depthOrArrayLayers = 1;
    wgpuCommandEncoderCopyTextureToBuffer(enc, &src, &dst, &ext);
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, NULL);
    wgpuQueueSubmit(c->queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd); wgpuCommandEncoderRelease(enc);

    g_mapped = 0;
    WGPUBufferMapCallbackInfo mci; memset(&mci, 0, sizeof(mci));
    mci.mode = WGPUCallbackMode_AllowProcessEvents; mci.callback = on_map;
    wgpuBufferMapAsync(writing, WGPUMapMode_Read, 0, total, mci);
    c->rb_pending = 1;
    c->rb_cur = c->rb_cur ? 0 : 1;
    return got;
}

void ui_gpu_free(void* ctx) {
    Ctx* c = (Ctx*)ctx; if (!c) return;
    free_images(c); free(c->imgData); free(c->imgRes);
    if (c->imgBuf) wgpuBufferRelease(c->imgBuf);
    free(c->rectData); free(c->glyphData);
    if (c->rectBuf) wgpuBufferRelease(c->rectBuf);
    if (c->glyphBuf) wgpuBufferRelease(c->glyphBuf);
    if (c->imagePipe) wgpuRenderPipelineRelease(c->imagePipe);
    if (c->atlasBG) wgpuBindGroupRelease(c->atlasBG);
    if (c->atlasView) wgpuTextureViewRelease(c->atlasView);
    if (c->atlasTex) wgpuTextureRelease(c->atlasTex);
    if (c->colorView) wgpuTextureViewRelease(c->colorView);
    if (c->colorTex) wgpuTextureRelease(c->colorTex);
    if (c->tbgl3) wgpuBindGroupLayoutRelease(c->tbgl3);
    if (c->samp) wgpuSamplerRelease(c->samp);
    if (c->sampLinear) wgpuSamplerRelease(c->sampLinear);
    if (c->ubg) wgpuBindGroupRelease(c->ubg);
    if (c->ubo) wgpuBufferRelease(c->ubo);
    if (c->rectPipe) wgpuRenderPipelineRelease(c->rectPipe);
    if (c->textPipe) wgpuRenderPipelineRelease(c->textPipe);
    if (c->ubgl) wgpuBindGroupLayoutRelease(c->ubgl);
    if (c->tbgl) wgpuBindGroupLayoutRelease(c->tbgl);
    if (c->targetView) wgpuTextureViewRelease(c->targetView);
    if (c->target) wgpuTextureRelease(c->target);
    rb_settle(c);
    if (c->readbuf) wgpuBufferRelease(c->readbuf);
    if (c->rbuf2) wgpuBufferRelease(c->rbuf2);
    if (c->surface) wgpuSurfaceRelease(c->surface);
    if (c->device) wgpuDeviceRelease(c->device);
    if (c->adapter) wgpuAdapterRelease(c->adapter);
    if (c->instance) wgpuInstanceRelease(c->instance);
    free(c);
}
