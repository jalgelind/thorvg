source_file = [
    'tvgWgBindGroups.h',
    'tvgWgCommon.h',
    'tvgWgCompositor.h',
    'tvgWgMesh.h',
    'tvgWgPipelines.h',
    'tvgWgRenderData.h',
    'tvgWgRenderer.h',
    'tvgWgRenderTarget.h',
    'tvgWgRenderTask.h',
    'tvgWgShaderSrc.h',
    'tvgWgShaderTypes.h',
    'tvgWgSolidBatch.h',
    'tvgWgStencilBatch.h',
    'tvgWgTextureMgr.h',
    'tvgWgTessellator.h',
    'tvgWgBindGroups.cpp',
    'tvgWgCommon.cpp',
    'tvgWgCompositor.cpp',
    'tvgWgMesh.cpp',
    'tvgWgPipelines.cpp',
    'tvgWgRenderData.cpp',
    'tvgWgRenderer.cpp',
    'tvgWgRenderTarget.cpp',
    'tvgWgRenderTask.cpp',
    'tvgWgShaderSrc.cpp',
    'tvgWgShaderTypes.cpp',
    'tvgWgSolidBatch.cpp',
    'tvgWgStencilBatch.cpp',
    'tvgWgTextureMgr.cpp',
    'tvgWgTessellator.cpp'
]

wgpu_dep = []
if host_machine.system() != 'emscripten'
    # js-seq: optional — we build the WG backend against Dawn (webgpu.h provided via
    # -I<DAWN_INCLUDE_DIRS>), not wgpu_native, so this dependency must not be required.
    wgpu_dep = dependency('wgpu_native', required: false)
endif

engine_dep += [declare_dependency(
    dependencies        : wgpu_dep,
    include_directories : include_directories('.'),
    sources : source_file
)]
