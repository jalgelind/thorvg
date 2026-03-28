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
    return wgpuDeviceCreateRenderPipeline(device, &renderPipelineDesc);
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


void WgPipelines::initialize(WgContext& context)
{
    device_ = context.device;
    offscreenFormat_ = WGPUTextureFormat_RGBA8Unorm;

    // Vertex attributes (stored as members — referenced by vertex buffer layouts)
    vtxAttrPos_   = { .format = WGPUVertexFormat_Float32x2, .offset = 0, .shaderLocation = 0 };
    vtxAttrColor_ = { .format = WGPUVertexFormat_Float32x4, .offset = 0, .shaderLocation = 1 };
    vtxAttrTex_   = { .format = WGPUVertexFormat_Float32x2, .offset = 0, .shaderLocation = 1 };

    WGPUVertexBufferLayout vertexBufferLayoutPos { .stepMode = WGPUVertexStepMode_Vertex, .arrayStride = 8, .attributeCount = 1, .attributes = &vtxAttrPos_ };
    WGPUVertexBufferLayout vertexBufferLayoutColor { .stepMode = WGPUVertexStepMode_Instance, .arrayStride = 16, .attributeCount = 1, .attributes = &vtxAttrColor_ };
    WGPUVertexBufferLayout vertexBufferLayoutTex { .stepMode = WGPUVertexStepMode_Vertex, .arrayStride = 8, .attributeCount = 1, .attributes = &vtxAttrTex_ };
    // Store vertex buffer layouts for deferred pipeline creation
    vblSolid_[0] = vertexBufferLayoutPos; vblSolid_[1] = vertexBufferLayoutColor;
    vblShape_[0] = vertexBufferLayoutPos;
    vblImage_[0] = vertexBufferLayoutPos; vblImage_[1] = vertexBufferLayoutTex;

    // Multisample + blend states (stored for deferred creation)
    multisampleState_   = { .count = 4, .mask = 0xFFFFFFFF, .alphaToCoverageEnabled = false };
    multisampleStateX1_ = { .count = 1, .mask = 0xFFFFFFFF, .alphaToCoverageEnabled = false };

    WGPUBlendComponent blendComponentSrc { .operation = WGPUBlendOperation_Add, .srcFactor = WGPUBlendFactor_One, .dstFactor = WGPUBlendFactor_Zero };
    WGPUBlendComponent blendComponentNrm { .operation = WGPUBlendOperation_Add, .srcFactor = WGPUBlendFactor_One, .dstFactor = WGPUBlendFactor_OneMinusSrcAlpha };
    blendStateSrc_ = { .color = blendComponentSrc, .alpha = blendComponentSrc };
    blendStateNrm_ = { .color = blendComponentNrm, .alpha = blendComponentNrm };

    // Depth/stencil states (stored for deferred creation)
    depthStencilStateShape_ = makeDepthStencilState(WGPUCompareFunction_Always, WGPUOptionalBool_False, WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero);
    depthStencilStateScene_ = makeDepthStencilState(WGPUCompareFunction_Always, WGPUOptionalBool_False, WGPUCompareFunction_Always, WGPUStencilOperation_Zero);
    clipDepthStencilStates_[CopyStencilToDepth]      = makeDepthStencilState(WGPUCompareFunction_Always,  WGPUOptionalBool_True,  WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero);
    clipDepthStencilStates_[CopyStencilToDepthInterm] = makeDepthStencilState(WGPUCompareFunction_Greater, WGPUOptionalBool_True,  WGPUCompareFunction_NotEqual, WGPUStencilOperation_Zero);
    clipDepthStencilStates_[CopyDepthToStencil]       = makeDepthStencilState(WGPUCompareFunction_Equal,   WGPUOptionalBool_False, WGPUCompareFunction_Always,   WGPUStencilOperation_Replace);
    clipDepthStencilStates_[MergeDepthStencil]        = makeDepthStencilState(WGPUCompareFunction_Equal,   WGPUOptionalBool_True,  WGPUCompareFunction_Always,   WGPUStencilOperation_Keep);
    clipDepthStencilStates_[ClearDepth]               = makeDepthStencilState(WGPUCompareFunction_Always,  WGPUOptionalBool_True,  WGPUCompareFunction_Always,   WGPUStencilOperation_Keep);

    const WgBindGroupLayouts& layouts = context.layouts;
    const WGPUBindGroupLayout bindGroupLayoutsStencil[] { layouts.layoutBuffer1Un };
    const WGPUBindGroupLayout bindGroupLayoutsDepth[]   { layouts.layoutBuffer1Un, layouts.layoutBuffer1Un };
    const WGPUBindGroupLayout bindGroupLayoutsSolid[]    { layouts.layoutBuffer1Un };
    const WGPUBindGroupLayout bindGroupLayoutsGradient[] { layouts.layoutBuffer1Un, layouts.layoutBuffer1Un, layouts.layoutTexSampled };
    const WGPUBindGroupLayout bindGroupLayoutsImage[]    { layouts.layoutBuffer1Un, layouts.layoutBuffer1Un, layouts.layoutTexSampled };
    const WGPUBindGroupLayout bindGroupLayoutsScene[]    { layouts.layoutTexSampled, layouts.layoutBuffer1Un };
    const WGPUBindGroupLayout bindGroupLayoutsSolidBlend[]    { layouts.layoutBuffer1Un, layouts.layoutTexSampled };
    const WGPUBindGroupLayout bindGroupLayoutsGradientBlend[] { layouts.layoutBuffer1Un, layouts.layoutBuffer1Un, layouts.layoutTexSampled, layouts.layoutTexSampled };
    const WGPUBindGroupLayout bindGroupLayoutsImageBlend[]    { layouts.layoutBuffer1Un, layouts.layoutBuffer1Un, layouts.layoutTexSampled, layouts.layoutTexSampled };
    const WGPUBindGroupLayout bindGroupLayoutsSceneBlend[]    { layouts.layoutTexSampled, layouts.layoutTexSampled, layouts.layoutBuffer1Un };
    const WGPUBindGroupLayout bindGroupLayoutsSceneCompose[] { layouts.layoutTexSampled, layouts.layoutTexSampled };
    const WGPUBindGroupLayout bindGroupLayoutsBlit[] { layouts.layoutTexSampled };
    const WGPUBindGroupLayout bindGroupLayoutsShadow[] { layouts.layoutTexSampled, layouts.layoutTexSampled, layouts.layoutBuffer1Un };
    const WGPUBindGroupLayout bindGroupLayoutsEffects[] { layouts.layoutTexSampled, layouts.layoutBuffer1Un };

    // Shaders (cheap — no GPU compilation, just WGSL parsing)
    char shaderSourceBuff[16384]{};
    shader_stencil = createShaderModule(device_, "The shader stencil", cShaderSrc_Stencil);
    shader_depth   = createShaderModule(device_, "The shader depth", cShaderSrc_Depth);
    shader_solid  = createShaderModule(device_, "The shader solid",  cShaderSrc_Solid);
    shader_radial = createShaderModule(device_, "The shader radial", cShaderSrc_Radial);
    shader_linear = createShaderModule(device_, "The shader linear", cShaderSrc_Linear);
    shader_image  = createShaderModule(device_, "The shader image",  cShaderSrc_Image);
    shader_scene  = createShaderModule(device_, "The shader scene",  cShaderSrc_Scene);
    shader_solid_blend  = createShaderModule(device_, "The shader blend solid",  strcat(strcpy(shaderSourceBuff, cShaderSrc_Solid_Blend), cShaderSrc_BlendFuncs));
    shader_linear_blend = createShaderModule(device_, "The shader blend linear", strcat(strcpy(shaderSourceBuff, cShaderSrc_Linear_Blend), cShaderSrc_BlendFuncs));
    shader_radial_blend = createShaderModule(device_, "The shader blend radial", strcat(strcpy(shaderSourceBuff, cShaderSrc_Radial_Blend), cShaderSrc_BlendFuncs));
    shader_image_blend  = createShaderModule(device_, "The shader blend image",  strcat(strcpy(shaderSourceBuff, cShaderSrc_Image_Blend), cShaderSrc_BlendFuncs));
    shader_scene_blend  = createShaderModule(device_, "The shader blend scene",  strcat(strcpy(shaderSourceBuff, cShaderSrc_Scene_Blend), cShaderSrc_BlendFuncs));
    shader_scene_compose = createShaderModule(device_, "The shader scene composition", cShaderSrc_Scene_Compose);
    shader_blit = createShaderModule(device_, "The shader blit", cShaderSrc_Blit);
    shader_shadow = createShaderModule(device_, "The shader effects", cShaderSrc_Shadow);
    shader_effects = createShaderModule(device_, "The shader effects", cShaderSrc_Effects);

    // Layouts (cheap — no GPU compilation)
    layout_stencil = createPipelineLayout(device_, bindGroupLayoutsStencil, 1);
    layout_depth = createPipelineLayout(device_, bindGroupLayoutsDepth, 2);
    layout_solid    = createPipelineLayout(device_, bindGroupLayoutsSolid, 1);
    layout_gradient = createPipelineLayout(device_, bindGroupLayoutsGradient, 3);
    layout_image    = createPipelineLayout(device_, bindGroupLayoutsImage, 3);
    layout_scene    = createPipelineLayout(device_, bindGroupLayoutsScene, 2);
    layout_solid_blend    = createPipelineLayout(device_, bindGroupLayoutsSolidBlend, 2);
    layout_gradient_blend = createPipelineLayout(device_, bindGroupLayoutsGradientBlend, 4);
    layout_image_blend    = createPipelineLayout(device_, bindGroupLayoutsImageBlend, 4);
    layout_scene_blend    = createPipelineLayout(device_, bindGroupLayoutsSceneBlend, 3);
    layout_scene_compose = createPipelineLayout(device_, bindGroupLayoutsSceneCompose, 2);
    layout_blit = createPipelineLayout(device_, bindGroupLayoutsBlit, 1);
    layout_shadow = createPipelineLayout(device_, bindGroupLayoutsShadow, 3);
    layout_effects = createPipelineLayout(device_, bindGroupLayoutsEffects, 2);

    // ---- Eager pipelines (12 total — needed for first frame) ----
    const WGPUDepthStencilState depthStencilStateNonZero = makeDepthStencilState(WGPUCompareFunction_Always, WGPUOptionalBool_False, WGPUCompareFunction_Always, WGPUStencilOperation_IncrementWrap, WGPUCompareFunction_Always, WGPUStencilOperation_DecrementWrap);
    const WGPUDepthStencilState depthStencilStateEvenOdd = makeDepthStencilState(WGPUCompareFunction_Always, WGPUOptionalBool_False, WGPUCompareFunction_Always, WGPUStencilOperation_Invert);
    const WGPUDepthStencilState depthStencilStateDirect  = makeDepthStencilState(WGPUCompareFunction_Always, WGPUOptionalBool_False, WGPUCompareFunction_Always, WGPUStencilOperation_Replace);

    nonzero = createRenderPipeline(device_, "The render pipeline nonzero", shader_stencil, "vs_main", "fs_main", layout_stencil, vblShape_, 1, WGPUColorWriteMask_None, offscreenFormat_, blendStateSrc_, depthStencilStateNonZero, multisampleState_);
    evenodd = createRenderPipeline(device_, "The render pipeline even-odd", shader_stencil, "vs_main", "fs_main", layout_stencil, vblShape_, 1, WGPUColorWriteMask_None, offscreenFormat_, blendStateSrc_, depthStencilStateEvenOdd, multisampleState_);
    direct  = createRenderPipeline(device_, "The render pipeline direct", shader_stencil, "vs_main", "fs_main", layout_stencil, vblShape_, 1, WGPUColorWriteMask_None, offscreenFormat_, blendStateSrc_, depthStencilStateDirect, multisampleState_);
    solid      = createRenderPipeline(device_, "The render pipeline solid", shader_solid, "vs_main", "fs_main", layout_solid, vblSolid_, 2, WGPUColorWriteMask_All, offscreenFormat_, blendStateNrm_, depthStencilStateShape_, multisampleState_);
    radial     = createRenderPipeline(device_, "The render pipeline radial", shader_radial, "vs_main", "fs_main", layout_gradient, vblShape_, 1, WGPUColorWriteMask_All, offscreenFormat_, blendStateNrm_, depthStencilStateShape_, multisampleState_);
    linear     = createRenderPipeline(device_, "The render pipeline linear", shader_linear, "vs_main", "fs_main", layout_gradient, vblShape_, 1, WGPUColorWriteMask_All, offscreenFormat_, blendStateNrm_, depthStencilStateShape_, multisampleState_);
    solid_conv  = createRenderPipeline(device_, "The render pipeline solid convex", shader_solid, "vs_main", "fs_main", layout_solid, vblSolid_, 2, WGPUColorWriteMask_All, offscreenFormat_, blendStateNrm_, depthStencilStateScene_, multisampleState_);
    radial_conv = createRenderPipeline(device_, "The render pipeline radial convex", shader_radial, "vs_main", "fs_main", layout_gradient, vblShape_, 1, WGPUColorWriteMask_All, offscreenFormat_, blendStateNrm_, depthStencilStateScene_, multisampleState_);
    linear_conv = createRenderPipeline(device_, "The render pipeline linear convex", shader_linear, "vs_main", "fs_main", layout_gradient, vblShape_, 1, WGPUColorWriteMask_All, offscreenFormat_, blendStateNrm_, depthStencilStateScene_, multisampleState_);
    image  = createRenderPipeline(device_, "The render pipeline image", shader_image, "vs_main", "fs_main", layout_image, vblImage_, 2, WGPUColorWriteMask_All, offscreenFormat_, blendStateNrm_, depthStencilStateShape_, multisampleState_);
    scene  = createRenderPipeline(device_, "The render pipeline scene", shader_scene, "vs_main", "fs_main", layout_scene, vblImage_, 2, WGPUColorWriteMask_All, offscreenFormat_, blendStateNrm_, depthStencilStateScene_, multisampleState_);
    blit   = createRenderPipeline(device_, "The render pipeline blit", shader_blit, "vs_main", "fs_main", layout_blit, vblImage_, 2, WGPUColorWriteMask_All, context.format, blendStateSrc_, depthStencilStateScene_, multisampleStateX1_);

    // ---- Lazy pipelines (112 total — created on first use via accessors) ----
    // Blend[90], compose[11], clip[5], effects[6] are left zero-initialized.
}


// --- Lazy pipeline accessors ---

static const char* sBlendShaderNames[] {
    "fs_main_Normal", "fs_main_Multiply", "fs_main_Screen", "fs_main_Overlay",
    "fs_main_Darken", "fs_main_Lighten", "fs_main_ColorDodge", "fs_main_ColorBurn",
    "fs_main_HardLight", "fs_main_SoftLight", "fs_main_Difference", "fs_main_Exclusion",
    "fs_main_Hue", "fs_main_Saturation", "fs_main_Color", "fs_main_Luminosity",
    "fs_main_Add", "fs_main_Normal" // padding for reserved Hardmix
};

WGPURenderPipeline WgPipelines::getBlendPipeline(uint32_t fillType, uint32_t blendIdx)
{
    auto& slot = blendPipelines_[fillType][blendIdx];
    if (!slot) {
        WGPUShaderModule shader{};
        WGPUPipelineLayout layout{};
        const WGPUVertexBufferLayout* vbl{};
        uint32_t vblCount{};
        switch (fillType) {
            case Solid:  shader = shader_solid_blend;  layout = layout_solid_blend;    vbl = vblSolid_; vblCount = 2; break;
            case Linear: shader = shader_linear_blend; layout = layout_gradient_blend; vbl = vblShape_; vblCount = 1; break;
            case Radial: shader = shader_radial_blend; layout = layout_gradient_blend; vbl = vblShape_; vblCount = 1; break;
            case Image:  shader = shader_image_blend;  layout = layout_image_blend;    vbl = vblImage_; vblCount = 2; break;
            default:     shader = shader_scene_blend;  layout = layout_scene_blend;    vbl = vblImage_; vblCount = 2; break;
        }
        const auto& ds = (fillType == Scene) ? depthStencilStateScene_ : depthStencilStateShape_;
        slot = createRenderPipeline(device_, "lazy blend", shader, "vs_main", sBlendShaderNames[blendIdx], layout, vbl, vblCount, WGPUColorWriteMask_All, offscreenFormat_, blendStateSrc_, ds, multisampleState_);
    }
    return slot;
}

static const char* sComposeShaderNames[] {
    "fs_main_None", "fs_main_AlphaMask", "fs_main_InvAlphaMask", "fs_main_LumaMask",
    "fs_main_InvLumaMask", "fs_main_AddMask", "fs_main_SubtractMask", "fs_main_IntersectMask",
    "fs_main_DifferenceMask", "fs_main_LightenMask", "fs_main_DarkenMask"
};

static const bool sComposeUsesSrc[] { false, false, false, false, false, true, true, true, true, true, true };

WGPURenderPipeline WgPipelines::getComposePipeline(uint32_t idx)
{
    auto& slot = composePipelines_[idx];
    if (!slot) {
        const auto& bs = sComposeUsesSrc[idx] ? blendStateSrc_ : blendStateNrm_;
        slot = createRenderPipeline(device_, "lazy compose", shader_scene_compose, "vs_main", sComposeShaderNames[idx], layout_scene_compose, vblImage_, 2, WGPUColorWriteMask_All, offscreenFormat_, bs, depthStencilStateScene_, multisampleState_);
    }
    return slot;
}

WGPURenderPipeline WgPipelines::getClipPipeline(uint32_t op)
{
    auto& slot = clipPipelines_[op];
    if (!slot) {
        slot = createRenderPipeline(device_, "lazy clip", shader_depth, "vs_main", "fs_main", layout_depth, vblShape_, 1, WGPUColorWriteMask_None, offscreenFormat_, blendStateSrc_, clipDepthStencilStates_[op], multisampleState_);
    }
    return slot;
}

static const char* sEffectEntryPoints[] { "fs_main_vert", "fs_main_horz", "fs_main_shadow", "fs_main_fill", "fs_main_tint", "fs_main_tritone" };

WGPURenderPipeline WgPipelines::getEffectPipeline(uint32_t type)
{
    auto& slot = effectPipelines_[type];
    if (!slot) {
        WGPUShaderModule shader = (type == DropShadow) ? shader_shadow : shader_effects;
        WGPUPipelineLayout layout = (type == DropShadow) ? layout_shadow : layout_effects;
        slot = createRenderPipeline(device_, "lazy effect", shader, "vs_main", sEffectEntryPoints[type], layout, vblImage_, 2, WGPUColorWriteMask_All, offscreenFormat_, blendStateSrc_, depthStencilStateScene_, multisampleStateX1_);
    }
    return slot;
}

void WgPipelines::releaseGraphicHandles(WgContext& context)
{
    // lazy effect pipelines
    for (uint32_t i = 0; i < 6; i++)
        releaseRenderPipeline(effectPipelines_[i]);
    // pipeline blit (eager)
    releaseRenderPipeline(blit);
    // lazy compose pipelines
    for (uint32_t i = 0; i < 11; i++)
        releaseRenderPipeline(composePipelines_[i]);
    // lazy blend pipelines
    for (uint32_t f = 0; f < 5; f++)
        for (uint32_t i = 0; i < 18; i++)
            releaseRenderPipeline(blendPipelines_[f][i]);
    // eager normal blend pipelines
    releaseRenderPipeline(scene);
    releaseRenderPipeline(image);
    releaseRenderPipeline(linear_conv);
    releaseRenderPipeline(radial_conv);
    releaseRenderPipeline(solid_conv);
    releaseRenderPipeline(linear);
    releaseRenderPipeline(radial);
    releaseRenderPipeline(solid);
    // lazy clip path pipelines
    for (uint32_t i = 0; i < 5; i++)
        releaseRenderPipeline(clipPipelines_[i]);
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
