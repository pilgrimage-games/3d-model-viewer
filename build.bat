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
set compiler_flags=-std:c17 -DWINDOWS -DUNICODE -DSIMD_X86 -W4 -Zi -Oi -EHa- -GR- -Fdbuild\ -Fobuild\ -diagnostics:column -nologo
if defined release (
    rem -O2: Enable performance optimizations
    rem -MT: Statically link MSVC CRT
    set compiler_flags=!compiler_flags! -O2 -MT
) else (
    rem -MTd: Statically link debug MSVC CRT
    rem -sdl: Enable additional security checks
    rem -fsanitize: Enable address sanitizer
    set compiler_flags=!compiler_flags! -DDEBUG -MTd -sdl -fsanitize=address
)
rem -link: Set linker flags
rem -DEBUG: Generate full debug info
rem -INCREMENTAL: Disable incremental linking
rem -OPT: Remove unreferenced functions and data, disable identical COMDAT folding
set linker_flags=-link -DEBUG:FULL -INCREMENTAL:NO -OPT:REF,NOICF

rem -I: Set include dir path
set compiler_flags=!compiler_flags! -I..\game-development\common-library\ -I%VULKAN_SDK%\Include\
rem -LIBPATH: Set library dir path
set linker_flags=!linker_flags! -LIBPATH:..\game-development\common-library\imgui\deps\ -LIBPATH:%VULKAN_SDK%\Lib\

rem -OUT: Set output file path
cl 3d_model_viewer.c build\icon.res !compiler_flags! !linker_flags! -OUT:build\3d_model_viewer.exe

if defined all (
    rem Shader Compilation

    if not exist build\shaders\ (
        mkdir build\shaders\
    )

    set shader=model
    set dxc=!VULKAN_SDK!\Bin\dxc

    rem -Zi: Enable debug info
    rem -Zpr: Pack matrices in row-major
    rem -Ges: Enable strict mode
    set compiler_flags=-Zi -Zpr -Ges
    if defined release (
        rem -O3: Enable optimizations
        rem -Qstrip_debug: Strip debug data
        rem -Qstrip_priv: Strip private data
        set compiler_flags=!compiler_flags! -O3 -Qstrip_debug -Qstrip_priv
    ) else (
        rem -Od: Disable optimizations
        set compiler_flags=!compiler_flags! -Od
    )
    rem -T: Set shader target profile
    rem -Fo: Set output file path
    rem -Qstrip_reflect: Strip reflection data
    rem -spirv: Output SPIR-V (OpenGL/Vulkan)
    rem -vkbr a b c d: Assign HLSL register (a) in space (b) to binding (c) in descriptor set (d) (Vulkan)
    fxc !shader!_vs.hlsl !compiler_flags! -T vs_5_0 -Fo build\shaders\d3d11_!shader!_vs.dxbc -Qstrip_reflect > nul
    fxc !shader!_ps.hlsl !compiler_flags! -T ps_5_0 -Fo build\shaders\d3d11_!shader!_ps.dxbc -Qstrip_reflect > nul
    fxc !shader!_vs.hlsl !compiler_flags! -T vs_5_1 -Fo build\shaders\d3d12_!shader!_vs.dxbc -Qstrip_reflect > nul
    fxc !shader!_ps.hlsl !compiler_flags! -T ps_5_1 -Fo build\shaders\d3d12_!shader!_ps.dxbc -Qstrip_reflect > nul
    !dxc! !shader!_vs.hlsl !compiler_flags! -T vs_6_0 -Fo build\shaders\gl_!shader!_vs.spv -spirv
    !dxc! !shader!_ps.hlsl !compiler_flags! -T ps_6_0 -Fo build\shaders\gl_!shader!_ps.spv -spirv
    !dxc! !shader!_vs.hlsl !compiler_flags! -T vs_6_0 -Fo build\shaders\vk_!shader!_vs.spv -spirv -vkbr b0 0 0 0 -vkbr b1 0 0 1
    !dxc! !shader!_ps.hlsl !compiler_flags! -T ps_6_0 -Fo build\shaders\vk_!shader!_ps.spv -spirv -vkbr b2 0 0 2 -vkbr t0 0 1 2 -vkbr s0 0 1 2

    rem Asset Compilation
    asset_compiler

    rem Resource Compilation
    rem -fo: Set RES file path
    rc -fo build\icon.res -nologo icon.rc
)

endlocal
