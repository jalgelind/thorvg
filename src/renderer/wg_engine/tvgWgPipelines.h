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

#ifndef _TVG_WG_PIPELINES_H_
#define _TVG_WG_PIPELINES_H_

#include "tvgWgCommon.h"

class WgPipelines {
private:
    // shaders helpers
    WGPUShaderModule shader_stencil{};
    WGPUShaderModule shader_depth{};
    // shaders normal blend
    WGPUShaderModule shader_solid{};
    WGPUShaderModule shader_radial{};
    WGPUShaderModule shader_linear{};
    WGPUShaderModule shader_image{};
    WGPUShaderModule shader_scene{};
    // shaders custom blend
    WGPUShaderModule shader_solid_blend{};
    WGPUShaderModule shader_radial_blend{};
    WGPUShaderModule shader_linear_blend{};
    WGPUShaderModule shader_image_blend{};
    WGPUShaderModule shader_scene_blend{};
    // shader scene compose
    WGPUShaderModule shader_scene_compose{};
    // shader blit
    WGPUShaderModule shader_blit{};
    // shader effects
    WGPUShaderModule shader_shadow;
    WGPUShaderModule shader_effects;

    // layouts helpers
    WGPUPipelineLayout layout_stencil{};
    WGPUPipelineLayout layout_depth{};
    // layouts normal blend
    WGPUPipelineLayout layout_solid{};
    WGPUPipelineLayout layout_gradient{};
    WGPUPipelineLayout layout_image{};
    WGPUPipelineLayout layout_scene{};
    // layouts custom blend
    WGPUPipelineLayout layout_solid_blend{};
    WGPUPipelineLayout layout_gradient_blend{};
    WGPUPipelineLayout layout_image_blend{};
    WGPUPipelineLayout layout_scene_blend{};
    // layouts scene compose
    WGPUPipelineLayout layout_scene_compose{};
    // layouts blit
    WGPUPipelineLayout layout_blit{};
    // layouts effects
    WGPUPipelineLayout layout_shadow{};
    WGPUPipelineLayout layout_effects{};
public:
    // pipelines stencil markup (eager — created at init)
    WGPURenderPipeline nonzero{};
    WGPURenderPipeline evenodd{};
    WGPURenderPipeline direct{};
    // pipelines normal blend (eager)
    WGPURenderPipeline solid{};
    WGPURenderPipeline radial{};
    WGPURenderPipeline linear{};
    WGPURenderPipeline solid_conv{};  // convex geometry (no stencil)
    WGPURenderPipeline radial_conv{}; // convex geometry (no stencil)
    WGPURenderPipeline linear_conv{}; // convex geometry (no stencil)
    WGPURenderPipeline image{};
    WGPURenderPipeline scene{};
    // pipeline blit (eager)
    WGPURenderPipeline blit{};

    // Lazy pipeline accessors — created on first use
    WGPURenderPipeline getBlendPipeline(uint32_t fillType, uint32_t blendIdx);
    WGPURenderPipeline getComposePipeline(uint32_t idx);
    WGPURenderPipeline getClipPipeline(uint32_t op);
    WGPURenderPipeline getEffectPipeline(uint32_t type);

    // Clip pipeline operation indices
    enum ClipOp : uint32_t {
        CopyStencilToDepth = 0,
        CopyStencilToDepthInterm = 1,
        CopyDepthToStencil = 2,
        MergeDepthStencil = 3,
        ClearDepth = 4
    };
    // Effect pipeline type indices
    enum EffectOp : uint32_t {
        GaussianVert = 0, GaussianHorz = 1, DropShadow = 2,
        FillEffect = 3, TintEffect = 4, TritoneEffect = 5
    };
    // Blend fill type indices
    enum BlendFill : uint32_t {
        Solid = 0, Linear = 1, Radial = 2, Image = 3, Scene = 4
    };

private:
    // Lazy pipelines (created on first use via accessors above)
    WGPURenderPipeline clipPipelines_[5]{};
    WGPURenderPipeline blendPipelines_[5][18]{};   // [fillType][blendMode]
    WGPURenderPipeline composePipelines_[11]{};
    WGPURenderPipeline effectPipelines_[6]{};

    // Stored creation params for deferred pipeline creation
    WGPUDevice device_{};
    WGPUTextureFormat offscreenFormat_{};
    WGPUMultisampleState multisampleState_{};
    WGPUMultisampleState multisampleStateX1_{};
    WGPUBlendState blendStateSrc_{};
    WGPUBlendState blendStateNrm_{};
    WGPUDepthStencilState depthStencilStateShape_{};
    WGPUDepthStencilState depthStencilStateScene_{};
    WGPUDepthStencilState clipDepthStencilStates_[5]{};

    // Vertex buffer layout storage (must persist for deferred creation)
    WGPUVertexAttribute vtxAttrPos_{};
    WGPUVertexAttribute vtxAttrColor_{};
    WGPUVertexAttribute vtxAttrTex_{};
    WGPUVertexBufferLayout vblSolid_[2]{};
    WGPUVertexBufferLayout vblShape_[1]{};
    WGPUVertexBufferLayout vblImage_[2]{};

    void releaseGraphicHandles(WgContext& context);
    WGPUShaderModule createShaderModule(WGPUDevice device, const char* label, const char* code);
    WGPUPipelineLayout createPipelineLayout(WGPUDevice device, const WGPUBindGroupLayout* bindGroupLayouts, const uint32_t bindGroupLayoutsCount);
    WGPURenderPipeline createRenderPipeline(
        WGPUDevice device, const char* pipelineLabel,
        const WGPUShaderModule shaderModule, const char* vsEntryPoint, const char* fsEntryPoint,
        const WGPUPipelineLayout pipelineLayout,
        const WGPUVertexBufferLayout *vertexBufferLayouts, const uint32_t vertexBufferLayoutsCount,
        const WGPUColorWriteMask writeMask, const WGPUTextureFormat colorTargetFormat, const WGPUBlendState blendState,
        const WGPUDepthStencilState depthStencilState, const WGPUMultisampleState multisampleState);
    WGPUComputePipeline createComputePipeline(
        WGPUDevice device, const char* pipelineLabel,
        const WGPUShaderModule shaderModule, const char* entryPoint,
        const WGPUPipelineLayout pipelineLayout);
    void releaseComputePipeline(WGPUComputePipeline& computePipeline);
    void releaseRenderPipeline(WGPURenderPipeline& renderPipeline);
    void releasePipelineLayout(WGPUPipelineLayout& pipelineLayout);
    void releaseShaderModule(WGPUShaderModule& shaderModule);

    WGPUDepthStencilState makeDepthStencilState(
        const WGPUCompareFunction depthCompare, WGPUOptionalBool depthWriteEnabled,
        const WGPUCompareFunction stencilFunctionFrnt, const WGPUStencilOperation stencilOperationFrnt);
    WGPUDepthStencilState makeDepthStencilState(
        const WGPUCompareFunction depthCompare, WGPUOptionalBool depthWriteEnabled,
        const WGPUCompareFunction stencilFunctionFrnt, const WGPUStencilOperation stencilOperationFrnt,
        const WGPUCompareFunction stencilFunctionBack, const WGPUStencilOperation stencilOperationBack);
public:
    void initialize(WgContext& context);
    void release(WgContext& context);
};

#endif // _TVG_WG_PIPELINES_H_
