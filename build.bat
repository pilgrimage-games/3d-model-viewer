@echo off

setlocal enabledelayedexpansion

set all=
set release=
:args
if "%1" neq "" (
    if "%1"=="-a" (
        set all=1
    )
    if "%1"=="-r" (
        set release=1
    )
    if "%1"=="-ar" (
        set all=1
        set release=1
    )
    if "%1"=="-ra" (
        set all=1
        set release=1
    )
    SHIFT
    GOTO :args
)

if not exist build\ (
	mkdir build\
)

if defined all (
    rem Resource Compilation
    rem -fo: Set RES file path
    rc -fo build\icon.res -nologo icon.rc
)

rem -std: Set language standard to C17
rem -D: Set preprocessor macro
rem -W4: Enable warnings up to level 4
rem -Zi: Generate debug info (PDB)
rem -Oi: Generate intrinsic functions
rem -EHa-: Disable exceptions (C++)
rem -GR-: Disable run-time type information (RTTI) (C++)
rem -Fd: Set PDB file path
rem -Fo: Set OBJ file path
rem -diagnostics: Show column number in error/warning display info
rem -nologo: Suppress startup banner
set cl_flags=-std:c17 -DWINDOWS -DUNICODE -DSIMD_X86 -W4 -Zi -Oi -EHa- -GR- -Fdbuild\ -Fobuild\ -diagnostics:column -nologo
if defined release (
    rem -O2: Enable performance optimizations
    rem -MT: Statically link MSVC CRT
    set cl_flags=!cl_flags! -O2 -MT
) else (
    rem -MTd: Statically link debug MSVC CRT
    rem -sdl: Enable additional security checks
    rem -wd4100: Disable "unreferenced formal parameter" warning
    rem -wd4189: Disable "local variable initialized but not referenced" warning
    rem -fsanitize: Enable address sanitizer
    set cl_flags=!cl_flags! -DDEBUG -MTd -sdl -wd4100 -wd4189
    set cl_flags=!cl_flags! -fsanitize=address
)
rem -DEBUG: Generate full debug info
rem -INCREMENTAL: Disable incremental linking
rem -OPT: Remove unreferenced functions and data, disable identical COMDAT folding
set link_flags=-DEBUG:FULL -INCREMENTAL:NO -OPT:REF,NOICF

rem -I: Set include dir path
set cl_flags=!cl_flags! -I..\game-development\common-library\ -I%VULKAN_SDK%\Include\
rem -LIBPATH: Set library dir path
set link_flags=!link_flags! -LIBPATH:..\game-development\common-library\imgui\deps\ -LIBPATH:%VULKAN_SDK%\Lib\

rem -link: Set linker flags
rem -OUT: Set output file path
cl 3d_model_viewer.c build\icon.res !cl_flags! -link !link_flags! -OUT:build\3d_model_viewer.exe

if defined all (
    rem Shader Compilation

    if not exist build\shaders\ (
        mkdir build\shaders\
    )

    set dxc=!VULKAN_SDK!\Bin\dxc

    rem -Zpr: Pack matrices in row-major
    rem -Ges: Enable strict mode
    rem -enable_unbounded_descriptor_tables: Enable use of unbounded arrays (D3D12 only)
    set fxc_flags=-Zpr -Ges -enable_unbounded_descriptor_tables
    if defined release (
        rem -O3: Enable optimizations
        rem -Qstrip_debug: Strip debug data
        rem -Qstrip_priv: Strip private data
        rem -Qstrip_reflect: Strip reflection data
        rem -all_resources_bound: Optimize out unbound resource checks (D3D12 only)
        set fxc_flags=!fxc_flags! -O3 -Qstrip_debug -Qstrip_priv -Qstrip_reflect -all_resources_bound
    ) else (
        rem -Od: Disable optimizations
        rem -Zi: Enable debug info
        set fxc_flags=!fxc_flags! -Od -Zi
    )
    rem -D: Set preprocessor macro
    rem -E: Set entrypoint name
    rem -T: Set shader target profile
    rem -Fo: Set output file path
    fxc shaders.hlsl !fxc_flags! -DD3D11 -E vs -T vs_5_0 -Fo build\shaders\d3d11_vs.dxbc > nul
    fxc shaders.hlsl !fxc_flags! -DD3D11 -E ps -T ps_5_0 -Fo build\shaders\d3d11_ps.dxbc > nul
    fxc shaders.hlsl !fxc_flags! -DD3D12 -E vs -T vs_5_1 -Fo build\shaders\d3d12_vs.dxbc > nul
    fxc shaders.hlsl !fxc_flags! -DD3D12 -E ps -T ps_5_1 -Fo build\shaders\d3d12_ps.dxbc > nul

    rem -Zpr: Pack matrices in row-major
    rem -Ges: Enable strict mode
    rem -spirv: Output SPIR-V (OpenGL/Vulkan)
    rem -fvk-use-dx-layout: Use DirectX packing rules for uniform and storage buffers
    set dxc_flags=-Zpr -Ges -spirv -fvk-use-dx-layout
    if defined release (
        rem -O3: Enable optimizations
        rem -Qstrip_debug: Strip debug data
        rem -Qstrip_priv: Strip private data
        rem -Qstrip_reflect: Strip reflection data (NOTE: not supported with -spirv)
        rem -all-resources-bound: Optimize out unbound resource checks
        set dxc_flags=!dxc_flags! -O3 -Qstrip_debug -Qstrip_priv -all-resources-bound
    ) else (
        rem -Od: Disable optimizations
        rem -Zi: Enable debug info
        rem -fspv-debug=vulkan-with-source: Enable source-level debugging
        set dxc_flags=!dxc_flags! -Od -Zi -fspv-debug=vulkan-with-source
    )
    rem -D: Set preprocessor macro
    rem -E: Set entrypoint name
    rem -T: Set shader target profile
    rem -Fo: Set output file path
    rem -vkbr a b c d: Assign HLSL register (a) in space (b) to binding (c) in descriptor set (d)
    !dxc! shaders.hlsl !dxc_flags! -DOPENGL -E vs -T vs_6_0 -Fo build\shaders\gl_vs.spv -vkbr b0 0 0 0 -vkbr b1 0 1 0 -vkbr t0 0 0 0 -vkbr t1 0 1 0 -vkbr t2 0 2 0 -vkbr t3 0 3 0 -vkbr s0 0 3 0
    !dxc! shaders.hlsl !dxc_flags! -DOPENGL -E ps -T ps_6_0 -Fo build\shaders\gl_ps.spv -vkbr b0 0 0 0 -vkbr b1 0 1 0 -vkbr t0 0 0 0 -vkbr t1 0 1 0 -vkbr t2 0 2 0 -vkbr t3 0 3 0 -vkbr s0 0 3 0
    !dxc! shaders.hlsl !dxc_flags! -DVULKAN -E vs -T vs_6_0 -Fo build\shaders\vk_vs.spv -vkbr b0 0 0 0 -vkbr b1 0 1 0 -vkbr t0 0 2 0 -vkbr t1 0 3 0 -vkbr t2 0 4 0 -vkbr t3 1 6 0 -vkbr s0 0 5 0
    !dxc! shaders.hlsl !dxc_flags! -DVULKAN -E ps -T ps_6_0 -Fo build\shaders\vk_ps.spv -vkbr b0 0 0 0 -vkbr b1 0 1 0 -vkbr t0 0 2 0 -vkbr t1 0 3 0 -vkbr t2 0 4 0 -vkbr t3 1 6 0 -vkbr s0 0 5 0

    rem Asset Compilation
    asset_compiler
)

endlocal
