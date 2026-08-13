/*
 * Copyright (c) 2023 - 2026 ThorVG project. All rights reserved.

 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "tvgWgShaderSrc.h"
#include "tvgWgPipelines.h"
#include <cstring>
#include <cassert>

// js-seq: pipeline state that does not depend on WgContext, hoisted out of
// initialize() so the eager pipelines there and the lazily-created blend/compose
// ones below share ONE definition. Duplicating these into the lazy getters would
// let a blend pipeline silently drift from its non-blend sibling the first time
// someone edits a vertex layout upstream.
namespace {

const WGPUVertexAttribute vertexAttributePos { .format = WGPUVertexFormat_Float32x2, .offset = 0, .shaderLocation = 0 };
const WGPUVertexAttribute vertexAttributeColor { .format = WGPUVertexFormat_Float32x4, .offset = 0, .shaderLocation = 1 };
const WGPUVertexAttribute vertexAttributeTex { .format = WGPUVertexFormat_Float32x2, .offset = 0, .shaderLocation = 1 };
const WGPUVertexAttribute vertexAttributesPos[] { vertexAttributePos };
const WGPUVertexAttribute vertexAttributesColor[] { vertexAttributeColor };
const WGPUVertexAttribute vertexAttributesTex[] { vertexAttributeTex };
const WGPUVertexBufferLayout vertexBufferLayoutPos { .stepMode = WGPUVertexStepMode_Vertex, .arrayStride = 8, .attributeCount = 1, .attributes = vertexAttributesPos };
// Solid path: one vec4 color per draw from an instance-rate aux vertex slot.
const WGPUVertexBufferLayout vertexBufferLayoutColor { .stepMode = WGPUVertexStepMode_Instance, .arrayStride = 16, .attributeCount = 1, .attributes = vertexAttributesColor };
const WGPUVertexBufferLayout vertexBufferLayoutTex { .stepMode = WGPUVertexStepMode_Vertex, .arrayStride = 8, .attributeCount = 1, .attributes = vertexAttributesTex };
const WGPUVertexBufferLayout vertexBufferLayoutsSolid[] { vertexBufferLayoutPos, vertexBufferLayoutColor };
const WGPUVertexBufferLayout vertexBufferLayoutsShape[] { vertexBufferLayoutPos };
const WGPUVertexBufferLayout vertexBufferLayoutsImage[] { vertexBufferLayoutPos, vertexBufferLayoutTex };
const WGPUMultisampleState multisampleState   { .count = 4, .mask = 0xFFFFFFFF, .alphaToCoverageEnabled = false };
const WGPUMultisampleState multisampleStateX1 { .count = 1, .mask = 0xFFFFFFFF, .alphaToCoverageEnabled = false };
const WGPUTextureFormat offscreenTargetFormat = WGPUTextureFormat_RGBA8Unorm;

const WGPUBlendComponent blendComponentSrc { .operation = WGPUBlendOperation_Add, .srcFactor = WGPUBlendFactor_One, .dstFactor = WGPUBlendFactor_Zero };
const WGPUBlendComponent blendComponentNrm { .operation = WGPUBlendOperation_Add, .srcFactor = WGPUBlendFactor_One, .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha };
const WGPUBlendState blendStateSrc { .color = blendComponentSrc, .alpha = blendComponentSrc };
const WGPUBlendState blendStateNrm { .color = blendComponentNrm, .alpha = blendComponentNrm };

// js-seq: FIXED-FUNCTION equivalents of the two blend modes RGB-subpixel text uses, so
// those paints stop taking the read-back path in WgCompositor::blendImage — which per
// paint ends the render pass, copies the WHOLE render target to targetTemp0, and starts a
// new pass (and every pass end resolves the 4x MSAA attachment). Measured in the editor at
// 984 such paints per frame: 26.0 ms of encode against 1.1 ms with them gone.
//
// Equivalence, from the shaders these replace (tvgWgShaderSrc.cpp), whose pipeline is
// blendStateSrc — i.e. the shader's output IS the result:
//   fs_main_Multiply: Rc = mix(Sc, Sc * Dc/Da, Da), written as vec4(Rc, 1.0)
//   fs_main_Add:      Rc = min(One, Sc + Dc),       written as vec4(Rc, 1.0)
// With an OPAQUE destination (Da == 1) Multiply collapses to Sc*Dc, which is exactly
// (srcFactor=Dst, dstFactor=Zero); Add is (One, One) and RGBA8Unorm clamps for free, which
// is the min(). Alpha keeps the destination's (Zero, One) rather than writing the shader's
// literal 1.0 — identical whenever Da == 1, and it cannot punch a hole if it is not.
//
// Da == 1 IS THE PRECONDITION and it is not checked here (the renderer cannot see it). It
// holds because the only caller is the subpixel text pair, which canvas.cpp's
// subpixelEligible() already refuses unless targetOpaque — the same S0 caveat documented
// there. Restricted to IMAGE paints for that reason; shapes keep the read-back path.
const WGPUBlendComponent blendComponentMulHw  { .operation = WGPUBlendOperation_Add, .srcFactor = WGPUBlendFactor_Dst,  .dstFactor = WGPUBlendFactor_Zero };
const WGPUBlendComponent blendComponentAddHw  { .operation = WGPUBlendOperation_Add, .srcFactor = WGPUBlendFactor_One,  .dstFactor = WGPUBlendFactor_One };
const WGPUBlendComponent blendComponentKeepDa { .operation = WGPUBlendOperation_Add, .srcFactor = WGPUBlendFactor_Zero, .dstFactor = WGPUBlendFactor_One };
const WGPUBlendState blendStateMulHw { .color = blendComponentMulHw, .alpha = blendComponentKeepDa };
const WGPUBlendState blendStateAddHw { .color = blendComponentAddHw, .alpha = blendComponentKeepDa };
//js-seq: the one-draw form of that Multiply/Add pair. The fragment emits both operands
//(src0 = col*a3, src1 = a3) and this combines them as dst = src0 + dst*(1 - src1) --
//the per-channel lerp RGB-subpixel text needs, which a single premultiplied alpha
//cannot express. Alpha keeps Da for the same reason the pair does.
const WGPUBlendComponent blendComponentDualSrc { .operation = WGPUBlendOperation_Add, .srcFactor = WGPUBlendFactor_One, .dstFactor = WGPUBlendFactor_OneMinusSrc1 };
const WGPUBlendState blendStateDualSrc { .color = blendComponentDualSrc, .alpha = blendComponentKeepDa };

// blend shader names
const char* shaderBlendNames[] {
    "fs_main_Normal",
    "fs_main_Multiply",
    "fs_main_Screen",
    "fs_main_Overlay",
    "fs_main_Darken",
    "fs_main_Lighten",
    "fs_main_ColorDodge",
    "fs_main_ColorBurn",
    "fs_main_HardLight",
    "fs_main_SoftLight",
    "fs_main_Difference",
    "fs_main_Exclusion",
    "fs_main_Hue",
    "fs_main_Saturation",
    "fs_main_Color",
    "fs_main_Luminosity",
    "fs_main_Add",
    "fs_main_Normal"  //TODO: a padding for reserved Hardmix.
};

// compose shader names
const char* shaderComposeNames[] {
    "fs_main_None",
    "fs_main_AlphaMask",
    "fs_main_InvAlphaMask",
    "fs_main_LumaMask",
    "fs_main_InvLumaMask",
    "fs_main_AddMask",
    "fs_main_SubtractMask",
    "fs_main_IntersectMask",
    "fs_main_DifferenceMask",
    "fs_main_LightenMask",
    "fs_main_DarkenMask"
};

// compose shader blend states
const WGPUBlendState composeBlends[] {
    blendStateNrm, // None
    blendStateNrm, // AlphaMask
    blendStateNrm, // InvAlphaMask
    blendStateNrm, // LumaMask
    blendStateNrm, // InvLumaMask
    blendStateSrc, // AddMask
    blendStateSrc, // SubtractMask
    blendStateSrc, // IntersectMask
    blendStateSrc, // DifferenceMask
    blendStateSrc, // LightenMask
    blendStateSrc  // DarkenMask
};

} // namespace

WGPUShaderModule WgPipelines::createShaderModule(WGPUDevice device, const char* label, const char* code)
{
    WGPUShaderSourceWGSL shaderSourceWGSL {
        .chain = { .sType = WGPUSType_ShaderSourceWGSL },
        .code = { .data = code, .length = WGPU_STRLEN }
    };
    const WGPUShaderModuleDescriptor shaderModuleDesc {
        .nextInChain = &shaderSourceWGSL.chain,
        .label = { .data = label, .length = WGPU_STRLEN }
    };
    return wgpuDeviceCreateShaderModule(device, &shaderModuleDesc);
}


WGPUPipelineLayout WgPipelines::createPipelineLayout(WGPUDevice device, const WGPUBindGroupLayout* bindGroupLayouts, const uint32_t bindGroupLayoutsCount)
{
    const WGPUPipelineLayoutDescriptor pipelineLayoutDesc { .bindGroupLayoutCount = bindGroupLayoutsCount, .bindGroupLayouts = bindGroupLayouts };
    return wgpuDeviceCreatePipelineLayout(device, &pipelineLayoutDesc);
}


WGPURenderPipeline WgPipelines::createRenderPipeline(
    WGPUDevice device, const char* pipelineLabel,
    const WGPUShaderModule shaderModule, const char* vsEntryPoint, const char* fsEntryPoint,
    const WGPUPipelineLayout pipelineLayout,
    const WGPUVertexBufferLayout *vertexBufferLayouts, const uint32_t vertexBufferLayoutsCount,
    const WGPUColorWriteMask writeMask, const WGPUTextureFormat colorTargetFormat, const WGPUBlendState blendState,
    const WGPUDepthStencilState depthStencilState, const WGPUMultisampleState multisampleState)
{
    const WGPUColorTargetState colorTargetState { .format = colorTargetFormat, .blend = &blendState, .writeMask = writeMask };
    const WGPUColorTargetState colorTargetStates[] { colorTargetState };
    const WGPUPrimitiveState primitiveState { .topology = WGPUPrimitiveTopology_TriangleList };
    const WGPUVertexState   vertexState   { .module = shaderModule, .entryPoint = { .data = vsEntryPoint, .length = WGPU_STRLEN }, .bufferCount = vertexBufferLayoutsCount, .buffers = vertexBufferLayouts };
    const WGPUFragmentState fragmentState { .module = shaderModule, .entryPoint = { .data = fsEntryPoint, .length = WGPU_STRLEN }, .targetCount = 1, .targets = colorTargetStates };
    const WGPURenderPipelineDescriptor renderPipelineDesc {
        .label = { .data = pipelineLabel, .length = WGPU_STRLEN },
        .layout = pipelineLayout,
        .vertex = vertexState,
        .primitive = primitiveState,
        .depthStencil = &depthStencilState,
        .multisample = multisampleState,
        .fragment = &fragmentState
    };
    auto pipeline = wgpuDeviceCreateRenderPipeline(device, &renderPipelineDesc);
    // js-seq: report progress. Every pipeline funnels through here, and on a cold
    // shader cache each one is a blocking MSL compile — initialize() can take
    // ~10s in a single call the caller's thread cannot otherwise interrupt, so
    // without this a host's loading screen has no way to move. See
    // docs/TODO_wg_cold_start.md in the js-seq repo.
    if (wgPipelineProgressHook) wgPipelineProgressHook(++wgPipelineProgressCount, WG_PIPELINE_TOTAL);
    return pipeline;
}


WGPUComputePipeline WgPipelines::createComputePipeline(
        WGPUDevice device, const char* pipelineLabel,
        const WGPUShaderModule shaderModule, const char* entryPoint,
        const WGPUPipelineLayout pipelineLayout)
{
    const WGPUComputePipelineDescriptor computePipelineDesc{
        .label = { .data = pipelineLabel, .length = WGPU_STRLEN },
        .layout = pipelineLayout,
        .compute = { 
            .module = shaderModule,
            .entryPoint = { .data = entryPoint, .length = WGPU_STRLEN }
        }
    };
    return wgpuDeviceCreateComputePipeline(device, &computePipelineDesc);
}


void WgPipelines::releaseComputePipeline(WGPUComputePipeline& computePipeline)
{
    if (computePipeline) {
        wgpuComputePipelineRelease(computePipeline);
        computePipeline = nullptr;
    }
}


void WgPipelines::releaseRenderPipeline(WGPURenderPipeline& renderPipeline)
{
    if (renderPipeline) {
        wgpuRenderPipelineRelease(renderPipeline);
        renderPipeline = nullptr;
    }
}


void WgPipelines::releasePipelineLayout(WGPUPipelineLayout& pipelineLayout)
{
    if (pipelineLayout) {
        wgpuPipelineLayoutRelease(pipelineLayout);
        pipelineLayout = nullptr;
    }
}


void WgPipelines::releaseShaderModule(WGPUShaderModule& shaderModule)
{
    if (shaderModule) {
        wgpuShaderModuleRelease(shaderModule);
        shaderModule = nullptr;
    }
}


WGPUDepthStencilState WgPipelines::makeDepthStencilState(
    const WGPUCompareFunction depthCompare, WGPUOptionalBool depthWriteEnabled,
    const WGPUCompareFunction stencilFunction, const WGPUStencilOperation stencilOperation)
{
    return makeDepthStencilState(depthCompare, depthWriteEnabled, stencilFunction, stencilOperation, stencilFunction, stencilOperation);
}


WGPUDepthStencilState WgPipelines::makeDepthStencilState(
    const WGPUCompareFunction depthCompare, WGPUOptionalBool depthWriteEnabled,
    const WGPUCompareFunction stencilFunctionFrnt, const WGPUStencilOperation stencilOperationFrnt,
    const WGPUCompareFunction stencilFunctionBack, const WGPUStencilOperation stencilOperationBack)
{
    const WGPUDepthStencilState depthStencilState {
        .format = WGPUTextureFormat_Depth24PlusStencil8, .depthWriteEnabled = depthWriteEnabled, .depthCompare = depthCompare,
        .stencilFront = { .compare = stencilFunctionFrnt, .failOp = stencilOperationFrnt, .depthFailOp = WGPUStencilOperation_Zero, .passOp = stencilOperationFrnt },
        .stencilBack =  { .compare = stencilFunctionBack, .failOp = stencilOperationBack, .depthFailOp = WGPUStencilOperation_Zero, .passOp = stencilOperationBack },
        .stencilReadMask = 0xFFFFFFFF, .stencilWriteMask = 0xFFFFFFFF
    };
    return depthStencilState;
}


// js-seq: see tvgWgPipelines.h
void (*wgPipelineProgressHook)(uint32_t created, uint32_t total) = nullptr;
uint32_t wgPipelineProgressCount = 0;


void WgPipelines::initialize(WgContext& context)
{
    wgPipelineProgressCount = 0;   // js-seq: restart the progress count per init

    // js-seq: vertex layouts, multisample/blend states and the blend/compose name
    // tables now live at file scope — see the note there.

    const WgBindGroupLayouts& layouts = context.layouts;
    // bind group layouts helpers
    const WGPUBindGroupLayout bindGroupLayoutsStencil[] { layouts.layoutBuffer1Un };
    const WGPUBindGroupLayout bindGroupLayoutsDepth[]   { layouts.layoutBuffer1Un, layouts.layoutBuffer1Un };
    // bind group layouts normal blend
    const WGPUBindGroupLayout bindGroupLayoutsSolid[]    { layouts.layoutBuffer1Un };
    const WGPUBindGroupLayout bindGroupLayoutsGradient[] { layouts.layoutBuffer1Un, layouts.layoutBuffer1Un, layouts.layoutTexSampled };
    const WGPUBindGroupLayout bindGroupLayoutsImage[]    { layouts.layoutBuffer1Un, layouts.layoutBuffer1Un, layouts.layoutTexSampled };
    const WGPUBindGroupLayout bindGroupLayoutsScene[]    { layouts.layoutTexSampled, layouts.layoutBuffer1Un };
    // bind group layouts custom blend
    const WGPUBindGroupLayout bindGroupLayoutsSolidBlend[]    { layouts.layoutBuffer1Un, layouts.layoutTexSampled };
    const WGPUBindGroupLayout bindGroupLayoutsGradientBlend[] { layouts.layoutBuffer1Un, layouts.layoutBuffer1Un, layouts.layoutTexSampled, layouts.layoutTexSampled };
    const WGPUBindGroupLayout bindGroupLayoutsImageBlend[]    { layouts.layoutBuffer1Un, layouts.layoutBuffer1Un, layouts.layoutTexSampled, layouts.layoutTexSampled };
    const WGPUBindGroupLayout bindGroupLayoutsSceneBlend[]    { layouts.layoutTexSampled, layouts.layoutTexSampled, layouts.layoutBuffer1Un };
    // bind group layouts scene compose
    const WGPUBindGroupLayout bindGroupLayoutsSceneCompose[] { layouts.layoutTexSampled, layouts.layoutTexSampled };
    // bind group layouts blit
    const WGPUBindGroupLayout bindGroupLayoutsBlit[] { layouts.layoutTexSampled };
    // bind group layouts effects
    const WGPUBindGroupLayout bindGroupLayoutsShadow[] { layouts.layoutTexSampled, layouts.layoutTexSampled, layouts.layoutBuffer1Un };
    const WGPUBindGroupLayout bindGroupLayoutsEffects[] { layouts.layoutTexSampled, layouts.layoutBuffer1Un };

    // depth stencil state markup
    const WGPUDepthStencilState depthStencilStateNonZero = makeDepthStencilState(WGPUCompareFunction_Always, WGPUOptionalBool_False, WGPUCompareFunction_Always, WGPUStencilOperation_IncrementWrap, WGPUCompareFunction_Always, WGPUStencilOperation_DecrementWrap);
    const WGPUDepthStencilState depthStencilStateEvenOdd = makeDepthStencilState(WGPUCompareFunction_Always, WGPUOptionalBool_False, WGPUCompareFunction_Always, WGPUStencilOperation_Invert);
    const WGPUDepthStencilState depthStencilStateDirect  = makeDepthStencilState(WGPUCompareFunction_Always, WGPUOptionalBool_False, WGPUCompareFunction_Always, WGPUStencilOperation_Replace);
    // depth stencil state clip path
    const WGPUDepthStencilState depthStencilStateCopyStencilToDepth    = makeDepthStencilState(WGPUCompareFunction_Always,  WGPUOptionalBool_True,  WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero);
    const WGPUDepthStencilState depthStencilStateCopyStencilToDepthInt = makeDepthStencilState(WGPUCompareFunction_Greater, WGPUOptionalBool_True,  WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero);
    const WGPUDepthStencilState depthStencilStateCopyDepthToStencil    = makeDepthStencilState(WGPUCompareFunction_Equal,   WGPUOptionalBool_False, WGPUCompareFunction_Always,   WGPUStencilOperation_Replace);
    const WGPUDepthStencilState depthStencilStateMergeDepthStencil     = makeDepthStencilState(WGPUCompareFunction_Equal,   WGPUOptionalBool_True,  WGPUCompareFunction_Always,   WGPUStencilOperation_Keep);
    const WGPUDepthStencilState depthStencilStateClearDepth            = makeDepthStencilState(WGPUCompareFunction_Always,  WGPUOptionalBool_True,  WGPUCompareFunction_Always,   WGPUStencilOperation_Keep);
    // depth stencil state blend, compose and blit
    // js-seq: via the accessors, so the lazy blend/compose pipelines below cannot
    // drift from the eager ones that use the same state.
    const WGPUDepthStencilState depthStencilStateShape = depthStencilShape();
    const WGPUDepthStencilState depthStencilStateScene = depthStencilScene();
    // shaders
    char shaderSourceBuff[16384]{};
    shader_stencil = createShaderModule(context.device, "The shader stencil", cShaderSrc_Stencil);
    shader_depth   = createShaderModule(context.device, "The shader depth", cShaderSrc_Depth);
    // shader normal blend
    shader_solid  = createShaderModule(context.device, "The shader solid",  cShaderSrc_Solid);
    shader_radial = createShaderModule(context.device, "The shader radial", cShaderSrc_Radial);
    shader_linear = createShaderModule(context.device, "The shader linear", cShaderSrc_Linear);
    shader_image  = createShaderModule(context.device, "The shader image",  cShaderSrc_Image);
    shader_scene  = createShaderModule(context.device, "The shader scene",  cShaderSrc_Scene);
    // shader custom blend
    shader_solid_blend  = createShaderModule(context.device, "The shader blend solid",  strcat(strcpy(shaderSourceBuff, cShaderSrc_Solid_Blend), cShaderSrc_BlendFuncs));
    shader_linear_blend = createShaderModule(context.device, "The shader blend linear", strcat(strcpy(shaderSourceBuff, cShaderSrc_Linear_Blend), cShaderSrc_BlendFuncs));
    shader_radial_blend = createShaderModule(context.device, "The shader blend radial", strcat(strcpy(shaderSourceBuff, cShaderSrc_Radial_Blend), cShaderSrc_BlendFuncs));
    shader_image_blend  = createShaderModule(context.device, "The shader blend image",  strcat(strcpy(shaderSourceBuff, cShaderSrc_Image_Blend), cShaderSrc_BlendFuncs));
    shader_scene_blend  = createShaderModule(context.device, "The shader blend scene",  strcat(strcpy(shaderSourceBuff, cShaderSrc_Scene_Blend), cShaderSrc_BlendFuncs));
    // shader compose
    shader_scene_compose = createShaderModule(context.device, "The shader scene composition", cShaderSrc_Scene_Compose);
    // shader blit
    shader_blit = createShaderModule(context.device, "The shader blit", cShaderSrc_Blit);
    // shader effects
    shader_shadow = createShaderModule(context.device, "The shader effects", cShaderSrc_Shadow);
    shader_effects = createShaderModule(context.device, "The shader effects", cShaderSrc_Effects);

    // layouts
    layout_stencil = createPipelineLayout(context.device, bindGroupLayoutsStencil, 1);
    layout_depth = createPipelineLayout(context.device, bindGroupLayoutsDepth, 2);
    // layouts normal blend
    layout_solid    = createPipelineLayout(context.device, bindGroupLayoutsSolid, 1);
    layout_gradient = createPipelineLayout(context.device, bindGroupLayoutsGradient, 3);
    layout_image    = createPipelineLayout(context.device, bindGroupLayoutsImage, 3);
    layout_scene    = createPipelineLayout(context.device, bindGroupLayoutsScene, 2);
    // layouts custom blend
    layout_solid_blend    = createPipelineLayout(context.device, bindGroupLayoutsSolidBlend, 2);
    layout_gradient_blend = createPipelineLayout(context.device, bindGroupLayoutsGradientBlend, 4);
    layout_image_blend    = createPipelineLayout(context.device, bindGroupLayoutsImageBlend, 4);
    layout_scene_blend    = createPipelineLayout(context.device, bindGroupLayoutsSceneBlend, 3);
    // layout compose
    layout_scene_compose = createPipelineLayout(context.device, bindGroupLayoutsSceneCompose, 2);
    // layout blit
    layout_blit = createPipelineLayout(context.device, bindGroupLayoutsBlit, 1);
    // layout effects
    layout_shadow = createPipelineLayout(context.device, bindGroupLayoutsShadow, 3);
    layout_effects = createPipelineLayout(context.device, bindGroupLayoutsEffects, 2);

    // render pipeline nonzero
    nonzero = createRenderPipeline(
        context.device, "The render pipeline nonzero",
        shader_stencil, "vs_main", "fs_main",
        layout_stencil, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc,
        depthStencilStateNonZero, multisampleState);
    // render pipeline even-odd
    evenodd = createRenderPipeline(
        context.device, "The render pipeline even-odd",
        shader_stencil, "vs_main", "fs_main",
        layout_stencil, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc,
        depthStencilStateEvenOdd, multisampleState);
    // render pipeline direct
    direct = createRenderPipeline(
        context.device, "The render pipeline direct",
        shader_stencil, "vs_main", "fs_main",
        layout_stencil, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc,
        depthStencilStateDirect, multisampleState);

    // render pipeline copy stencil to depth (front)
    copy_stencil_to_depth = createRenderPipeline(
        context.device, "The render pipeline copy stencil to depth front",
        shader_depth, "vs_main", "fs_main",
        layout_depth, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc,
        depthStencilStateCopyStencilToDepth , multisampleState);
    // render pipeline copy stencil to depth (intermediate)
    copy_stencil_to_depth_interm = createRenderPipeline(
        context.device, "The render pipeline copy stencil to depth intermediate",
        shader_depth, "vs_main", "fs_main",
        layout_depth, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc,
        depthStencilStateCopyStencilToDepthInt, multisampleState);
    // render pipeline depth to stencil
    copy_depth_to_stencil = createRenderPipeline(
        context.device, "The render pipeline depth to stencil",
        shader_depth, "vs_main", "fs_main",
        layout_depth, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc,
        depthStencilStateCopyDepthToStencil, multisampleState);
    // render pipeline merge depth with stencil
    merge_depth_stencil = createRenderPipeline(
        context.device, "The render pipeline merge depth with stencil",
        shader_depth, "vs_main", "fs_main",
        layout_depth, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc,
        depthStencilStateMergeDepthStencil, multisampleState);
    // render pipeline clear depth
    clear_depth = createRenderPipeline(
        context.device, "The render pipeline clear depth",
        shader_depth, "vs_main", "fs_main",
        layout_depth, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_None, offscreenTargetFormat, blendStateSrc,
        depthStencilStateClearDepth, multisampleState);

    // render pipeline solid
    solid = createRenderPipeline(
        context.device, "The render pipeline solid",
        shader_solid, "vs_main", "fs_main",
        layout_solid, vertexBufferLayoutsSolid, 2,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm,
        depthStencilStateShape, multisampleState);
    // render pipeline radial
    radial = createRenderPipeline(
        context.device, "The render pipeline radial",
        shader_radial, "vs_main", "fs_main",
        layout_gradient, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm,
        depthStencilStateShape, multisampleState);
    // render pipeline linear
    linear = createRenderPipeline(
        context.device, "The render pipeline linear",
        shader_linear, "vs_main", "fs_main",
        layout_gradient, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm,
        depthStencilStateShape, multisampleState);
    // render pipeline solid (no stencil)
    solid_conv = createRenderPipeline(
        context.device, "The render pipeline solid",
        shader_solid, "vs_main", "fs_main",
        layout_solid, vertexBufferLayoutsSolid, 2,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm,
        depthStencilStateScene, multisampleState);
    // render pipeline radial (no stencil)
    radial_conv = createRenderPipeline(
        context.device, "The render pipeline radial",
        shader_radial, "vs_main", "fs_main",
        layout_gradient, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm,
        depthStencilStateScene, multisampleState);
    // render pipeline linear (no stencil)
    linear_conv = createRenderPipeline(
        context.device, "The render pipeline linear",
        shader_linear, "vs_main", "fs_main",
        layout_gradient, vertexBufferLayoutsShape, 1,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm,
        depthStencilStateScene, multisampleState);
    // render pipeline image
    image = createRenderPipeline(
        context.device, "The render pipeline image",
        shader_image, "vs_main", "fs_main",
        layout_image, vertexBufferLayoutsImage, 2,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm,
        depthStencilStateShape, multisampleState);
    // js-seq: same shader and layout as `image`; only the fixed-function blend differs.
    image_mul_hw = createRenderPipeline(
        context.device, "The render pipeline image multiply (hw blend)",
        shader_image, "vs_main", "fs_main",
        layout_image, vertexBufferLayoutsImage, 2,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateMulHw,
        depthStencilStateShape, multisampleState);
    image_add_hw = createRenderPipeline(
        context.device, "The render pipeline image add (hw blend)",
        shader_image, "vs_main", "fs_main",
        layout_image, vertexBufferLayoutsImage, 2,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateAddHw,
        depthStencilStateShape, multisampleState);
    //js-seq: DualSourceBlending is an OPTIONAL WebGPU feature. Creating the shader
    //module at all would raise a validation error on a device without it, so both the
    //module and the pipeline stay null there and the caller falls back to the pair.
    if (wgpuDeviceHasFeature(context.device, WGPUFeatureName_DualSourceBlending)) {
        shader_image_dualsrc = createShaderModule(context.device, "The shader image dual-source", cShaderSrc_ImageDualSrc);
        image_dualsrc = createRenderPipeline(
            context.device, "The render pipeline image dual-source (subpixel text)",
            shader_image_dualsrc, "vs_main", "fs_main",
            layout_image, vertexBufferLayoutsImage, 2,
            WGPUColorWriteMask_All, offscreenTargetFormat, blendStateDualSrc,
            depthStencilStateShape, multisampleState);
    }
    if (getenv("NSEQ_WG_DEBUG")) fprintf(stderr, "[wg] js-seq dual-source subpixel pipeline: %s\n",
        image_dualsrc ? "created" : "unavailable (device lacks DualSourceBlending)");
    // render pipeline scene
    scene = createRenderPipeline(
        context.device, "The render pipeline scene",
        shader_scene, "vs_main", "fs_main",
        layout_scene, vertexBufferLayoutsImage, 2,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateNrm,
        depthStencilStateScene, multisampleState);

    // js-seq: the 90 blend and 11 compose pipelines are NOT created here any more.
    // They were 101 of the 125 eager pipelines, and each one is a blocking
    // MSL/DXIL/SPIR-V compile on the main thread — ~10s of cold start on macOS for
    // variants a given app almost never draws (a typical UI needs Normal blend and
    // None compose). They are created on first use by the blendXxx()/sceneCompose()
    // accessors below instead. release() already loops the whole array and skips
    // nulls, so teardown is unchanged.

    // render pipeline blit
    blit = createRenderPipeline(
        context.device, "The render pipeline blit",
        shader_blit, "vs_main", "fs_main",
        layout_blit, vertexBufferLayoutsImage, 2,
        WGPUColorWriteMask_All, context.format, blendStateSrc,  // must be preferred screen pixel format
        depthStencilStateScene, multisampleStateX1);

    // TODO: either premultiplied blit or unpremultplied bit used.
    blit_unpremultiplied = createRenderPipeline(
        context.device, "The render pipeline blit unpremultiplied",
        shader_blit, "vs_main", "fs_main_unpremultiplied",
        layout_blit, vertexBufferLayoutsImage, 2,
        WGPUColorWriteMask_All, context.format, blendStateSrc,  // must be preferred screen pixel format
        depthStencilStateScene, multisampleStateX1);

    // effects
    dropshadow = createRenderPipeline(
        context.device, "The render pipeline drop shadow",
        shader_shadow, "vs_main", "fs_main_shadow",
        layout_shadow, vertexBufferLayoutsImage, 2,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc,
        depthStencilStateScene, multisampleStateX1);

    gaussian_vert = createRenderPipeline(
        context.device, "The render pipeline gaussian vert",
        shader_effects, "vs_main", "fs_main_vert",
        layout_effects, vertexBufferLayoutsImage, 2,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc,
        depthStencilStateScene, multisampleStateX1);

    gaussian_horz = createRenderPipeline(
        context.device, "The render pipeline gaussian horz",
        shader_effects, "vs_main", "fs_main_horz",
        layout_effects, vertexBufferLayoutsImage, 2,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc,
        depthStencilStateScene, multisampleStateX1);

    fill_effect = createRenderPipeline(
        context.device, "The render pipeline fill effect",
        shader_effects, "vs_main", "fs_main_fill",
        layout_effects, vertexBufferLayoutsImage, 2,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc,
        depthStencilStateScene, multisampleStateX1);

    tint_effect = createRenderPipeline(
        context.device, "The render pipeline tint effect",
        shader_effects, "vs_main", "fs_main_tint",
        layout_effects, vertexBufferLayoutsImage, 2,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc,
        depthStencilStateScene, multisampleStateX1);

    tritone_effect = createRenderPipeline(
        context.device, "The render pipeline tritone effect",
        shader_effects, "vs_main", "fs_main_tritone",
        layout_effects, vertexBufferLayoutsImage, 2,
        WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc,
        depthStencilStateScene, multisampleStateX1);

}

// js-seq: lazy blend/compose pipelines.
//
// Each accessor compiles its variant on first use and memoises it in the same
// array initialize() used to fill eagerly, so callers see identical handles and
// release() is untouched. The cost moves from startup to the first frame that
// actually draws with that blend mode — one compile, once per process, and only
// for modes the content really uses.
//
// NOT thread-safe, deliberately: every caller is WgCompositor on the render
// thread, which is where initialize() ran too. If a second thread ever renders,
// these need a lock (or a pre-warm pass) — a torn read here would hand WebGPU a
// half-built pipeline.
WGPURenderPipeline WgPipelines::blendSolid(WgContext& context, uint32_t idx)
{
    assert(idx < 18);
    if (!solid_blend[idx]) {
        solid_blend[idx] = createRenderPipeline(
            context.device, "The render pipeline solid blend",
            shader_solid_blend, "vs_main", shaderBlendNames[idx],
            layout_solid_blend, vertexBufferLayoutsSolid, 2,
            WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc,
            depthStencilShape(), multisampleState);
    }
    return solid_blend[idx];
}


WGPURenderPipeline WgPipelines::blendRadial(WgContext& context, uint32_t idx)
{
    assert(idx < 18);
    if (!radial_blend[idx]) {
        radial_blend[idx] = createRenderPipeline(
            context.device, "The render pipeline radial blend",
            shader_radial_blend, "vs_main", shaderBlendNames[idx],
            layout_gradient_blend, vertexBufferLayoutsShape, 1,
            WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc,
            depthStencilShape(), multisampleState);
    }
    return radial_blend[idx];
}


WGPURenderPipeline WgPipelines::blendLinear(WgContext& context, uint32_t idx)
{
    assert(idx < 18);
    if (!linear_blend[idx]) {
        linear_blend[idx] = createRenderPipeline(
            context.device, "The render pipeline linear blend",
            shader_linear_blend, "vs_main", shaderBlendNames[idx],
            layout_gradient_blend, vertexBufferLayoutsShape, 1,
            WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc,
            depthStencilShape(), multisampleState);
    }
    return linear_blend[idx];
}


WGPURenderPipeline WgPipelines::blendImage(WgContext& context, uint32_t idx)
{
    assert(idx < 18);
    if (!image_blend[idx]) {
        image_blend[idx] = createRenderPipeline(
            context.device, "The render pipeline image blend",
            shader_image_blend, "vs_main", shaderBlendNames[idx],
            layout_image_blend, vertexBufferLayoutsImage, 2,
            WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc,
            depthStencilShape(), multisampleState);
    }
    return image_blend[idx];
}


WGPURenderPipeline WgPipelines::blendScene(WgContext& context, uint32_t idx)
{
    assert(idx < 18);
    if (!scene_blend[idx]) {
        scene_blend[idx] = createRenderPipeline(
            context.device, "The render pipeline scene blend",
            shader_scene_blend, "vs_main", shaderBlendNames[idx],
            layout_scene_blend, vertexBufferLayoutsImage, 2,
            WGPUColorWriteMask_All, offscreenTargetFormat, blendStateSrc,
            depthStencilScene(), multisampleState);
    }
    return scene_blend[idx];
}


WGPURenderPipeline WgPipelines::sceneCompose(WgContext& context, uint32_t idx)
{
    assert(idx < 11);
    if (!scene_compose[idx]) {
        scene_compose[idx] = createRenderPipeline(
            context.device, "The render pipeline scene composition",
            shader_scene_compose, "vs_main", shaderComposeNames[idx],
            layout_scene_compose, vertexBufferLayoutsImage, 2,
            WGPUColorWriteMask_All, offscreenTargetFormat, composeBlends[idx],
            depthStencilScene(), multisampleState);
    }
    return scene_compose[idx];
}


WGPUDepthStencilState WgPipelines::depthStencilShape()
{
    return makeDepthStencilState(WGPUCompareFunction_Always, WGPUOptionalBool_False, WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero);
}


WGPUDepthStencilState WgPipelines::depthStencilScene()
{
    return makeDepthStencilState(WGPUCompareFunction_Always, WGPUOptionalBool_False, WGPUCompareFunction_Always, WGPUStencilOperation_Zero);
}


void WgPipelines::releaseGraphicHandles(WgContext& context)
{
    // pipeline effects
    releaseRenderPipeline(tritone_effect);
    releaseRenderPipeline(tint_effect);
    releaseRenderPipeline(fill_effect);
    releaseRenderPipeline(gaussian_horz);
    releaseRenderPipeline(gaussian_vert);
    releaseRenderPipeline(dropshadow);
    // pipeline blit
    releaseRenderPipeline(blit_unpremultiplied);
    releaseRenderPipeline(blit);
    // pipelines compose
    for (uint32_t i = 0; i < 11; i++)
        releaseRenderPipeline(scene_compose[i]);
    // pipelines custom blend
    for (uint32_t i = 0; i < 18; i++) {
        releaseRenderPipeline(scene_blend[i]);
        releaseRenderPipeline(image_blend[i]);
        releaseRenderPipeline(linear_blend[i]);
        releaseRenderPipeline(radial_blend[i]);
        releaseRenderPipeline(solid_blend[i]);
    }
    // pipelines normal blend
    releaseRenderPipeline(scene);
    releaseRenderPipeline(image_dualsrc);
    releaseShaderModule(shader_image_dualsrc);
    releaseRenderPipeline(image_add_hw);
    releaseRenderPipeline(image_mul_hw);
    releaseRenderPipeline(image);
    releaseRenderPipeline(linear_conv);
    releaseRenderPipeline(radial_conv);
    releaseRenderPipeline(solid_conv);
    releaseRenderPipeline(linear);
    releaseRenderPipeline(radial);
    releaseRenderPipeline(solid);
    // pipelines clip path markup
    releaseRenderPipeline(clear_depth);
    releaseRenderPipeline(merge_depth_stencil);
    releaseRenderPipeline(copy_depth_to_stencil);
    releaseRenderPipeline(copy_stencil_to_depth_interm);
    releaseRenderPipeline(copy_stencil_to_depth);
    // pipelines stencil markup
    releaseRenderPipeline(direct);
    releaseRenderPipeline(evenodd);
    releaseRenderPipeline(nonzero);
    // layouts
    releasePipelineLayout(layout_effects);
    releasePipelineLayout(layout_shadow);
    releasePipelineLayout(layout_blit);
    releasePipelineLayout(layout_scene_compose);
    releasePipelineLayout(layout_scene_blend);
    releasePipelineLayout(layout_image_blend);
    releasePipelineLayout(layout_gradient_blend);
    releasePipelineLayout(layout_solid_blend);
    releasePipelineLayout(layout_scene);
    releasePipelineLayout(layout_image);
    releasePipelineLayout(layout_gradient);
    releasePipelineLayout(layout_solid);
    releasePipelineLayout(layout_depth);
    releasePipelineLayout(layout_stencil);
    // shaders
    releaseShaderModule(shader_effects);
    releaseShaderModule(shader_shadow);
    releaseShaderModule(shader_blit);
    releaseShaderModule(shader_scene_compose);
    releaseShaderModule(shader_scene_blend);
    releaseShaderModule(shader_image_blend);
    releaseShaderModule(shader_linear_blend);
    releaseShaderModule(shader_radial_blend);
    releaseShaderModule(shader_solid_blend);
    releaseShaderModule(shader_scene);
    releaseShaderModule(shader_image);
    releaseShaderModule(shader_linear);
    releaseShaderModule(shader_radial);
    releaseShaderModule(shader_solid);
    releaseShaderModule(shader_depth);
    releaseShaderModule(shader_stencil);
}


void WgPipelines::release(WgContext& context)
{
    releaseGraphicHandles(context);
}
