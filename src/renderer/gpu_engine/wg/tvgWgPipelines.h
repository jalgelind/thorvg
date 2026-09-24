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

// js-seq: optional progress hook for WgPipelines::initialize(). Invoked once per
// created render pipeline with (created, total) so a host can draw a determinate
// loading bar through the cold-cache shader compile. Null by default — set it
// around WgCanvas::target() and clear it after. Not thread-safe: initialize()
// runs on the caller's thread and so does the callback.
// WG_PIPELINE_TOTAL is nominal; if it drifts from the real count the bar just
// saturates early, which is cosmetic. Keep it in step with initialize().
// 24 since the blend/compose variants went lazy (was 125): 3 stencil + 5 clip
// path + 8 normal blend + 2 blit + 6 effects.
constexpr uint32_t WG_PIPELINE_TOTAL = 24;
extern void (*wgPipelineProgressHook)(uint32_t created, uint32_t total);
extern uint32_t wgPipelineProgressCount;

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
    //js-seq: null unless the device has DualSourceBlending (optional feature).
    WGPUShaderModule shader_image_dualsrc{};
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
    // pipelines stencil markup
    WGPURenderPipeline nonzero{};
    WGPURenderPipeline evenodd{};
    WGPURenderPipeline direct{};
    // pipelines clip path markup
    WGPURenderPipeline copy_stencil_to_depth{};        // depth 0.50, clear stencil
    WGPURenderPipeline copy_stencil_to_depth_interm{}; // depth 0.75, clear stencil
    WGPURenderPipeline copy_depth_to_stencil{}; // depth 0.50 and 0.75, update stencil
    WGPURenderPipeline merge_depth_stencil{};   // depth 0.75, update stencil
    WGPURenderPipeline clear_depth{}; // depth 1.00, clear ctencil
    // pipelines normal blend
    WGPURenderPipeline solid{};
    WGPURenderPipeline radial{};
    WGPURenderPipeline linear{};
    WGPURenderPipeline solid_conv{};  // convex geometry (no stencil)
    WGPURenderPipeline radial_conv{}; // convex geometry (no stencil)
    WGPURenderPipeline linear_conv{}; // convex geometry (no stencil)
    WGPURenderPipeline image{};
    // js-seq: `image` with fixed-function Multiply / Add instead of the read-back blend
    // shader — see the blend states in tvgWgPipelines.cpp for the equivalence and its
    // opaque-destination precondition.
    WGPURenderPipeline image_mul_hw{};
    WGPURenderPipeline image_add_hw{};
    //js-seq: null unless the device has WGPUFeatureName_DualSourceBlending. Callers
    //MUST null-check and fall back to the image_mul_hw/image_add_hw pair.
    WGPURenderPipeline image_dualsrc{};
    WGPURenderPipeline scene{};
    // pipelines custom blend
    WGPURenderPipeline solid_blend[18]{};
    WGPURenderPipeline radial_blend[18]{};
    WGPURenderPipeline linear_blend[18]{};
    WGPURenderPipeline image_blend[18]{};
    WGPURenderPipeline scene_blend[18]{};
    // pipelines compose
    WGPURenderPipeline scene_compose[11]{};
    // pipeline blit
    WGPURenderPipeline blit{};
    WGPURenderPipeline blit_unpremultiplied{};
    // effects
    WGPURenderPipeline gaussian_vert{};
    WGPURenderPipeline gaussian_horz{};
    WGPURenderPipeline dropshadow{};
    WGPURenderPipeline fill_effect{};
    WGPURenderPipeline tint_effect{};
    WGPURenderPipeline tritone_effect{};
private:
    void releaseGraphicHandles(WgContext& context);
    WGPUShaderModule createShaderModule(WGPUDevice device, const char* label, const char* code);
    WGPUPipelineLayout createPipelineLayout(WGPUDevice device, const WGPUBindGroupLayout* bindGroupLayouts, const uint32_t bindGroupLayoutsCount);
    WGPURenderPipeline createRenderPipeline(
        WGPUDevice device, const char* pipelineLabel,
        const WGPUShaderModule shaderModule, const char* vsEntryPoint, const char* fsEntryPoint,
        const WGPUPipelineLayout pipelineLayout,
        const WGPUVertexBufferLayout *vertexBufferLayouts, const uint32_t vertexBufferLayoutsCount,
        const WGPUColorWriteMask writeMask, const WGPUTextureFormat colorTargetFormat, const WGPUBlendState blendState,
        const WGPUDepthStencilState depthStencilState, const WGPUMultisampleState multisampleState,
        bool withDepth = true);   // js-seq: false for a pass with no depth attachment (the screen blit)
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
    // js-seq: shared by initialize() and the lazy accessors so they cannot drift.
    WGPUDepthStencilState depthStencilShape();
    WGPUDepthStencilState depthStencilScene();
public:
    void initialize(WgContext& context);
    void release(WgContext& context);

    // js-seq: lazy accessors for the blend/compose variants. The arrays above stay
    // public and are still the storage, but read them through these — a raw
    // `solid_blend[i]` is null until someone has asked for that variant.
    // idx is a raw (uint32_t)BlendMethod / (uint32_t)CompositeMethod cast from the
    // caller with no bounds check — same as the direct array indexing this
    // replaced, so no new hazard, but the accessors assert rather than inherit a
    // silent OOB if an enum ever grows past its table.
    WGPURenderPipeline blendSolid(WgContext& context, uint32_t idx);
    WGPURenderPipeline blendRadial(WgContext& context, uint32_t idx);
    WGPURenderPipeline blendLinear(WgContext& context, uint32_t idx);
    WGPURenderPipeline blendImage(WgContext& context, uint32_t idx);
    WGPURenderPipeline blendScene(WgContext& context, uint32_t idx);
    WGPURenderPipeline sceneCompose(WgContext& context, uint32_t idx);
};

#endif // _TVG_WG_PIPELINES_H_
