cmake_minimum_required(VERSION 3.20)

# CLANG IS REQUIRED, and this must be set before project() or CMake has already
# locked in the default compiler.
#
# Not a preference: XenonRecomp's ppc_context.h defines
#   #define PPC_FUNC_PROLOGUE() __builtin_assume(((size_t)base & 0x1F) == 0)
# and __builtin_assume is a Clang builtin. GCC has no spelling of it before the
# C++23 [[assume]] attribute, so every one of the 57,822 generated function bodies
# fails to compile under GCC — 16.1 emits "'__builtin_assume' was not declared in
# this scope" per function. Both earlier ports in this workspace build their
# runtimes with clang++ for the same reason.
#
# Override deliberately if you ever want to try otherwise:
#   cmake -DCMAKE_CXX_COMPILER=g++ ...
if(NOT DEFINED CMAKE_CXX_COMPILER)
    find_program(CW_CLANGXX clang++)
    if(NOT CW_CLANGXX)
        message(FATAL_ERROR
            "clang++ not found. The recompiled image needs Clang's __builtin_assume "
            "(see the comment above this message in runtime/CMakeLists.txt).")
    endif()
    set(CMAKE_CXX_COMPILER "${CW_CLANGXX}")
endif()

# C is enabled for exactly one file — thirdparty/o1heap/o1heap.c, the guest user
# heap. Without it CMake ignores the .c source entirely and the failure arrives at
# link time as three undefined o1heap* references, which reads like a missing
# library rather than a disabled language.
project(CaseWestRuntime C CXX)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(WARNING
        "Building the recompiled image with ${CMAKE_CXX_COMPILER_ID}. "
        "ppc_context.h uses __builtin_assume, which is Clang-only; expect a "
        "compile error in every generated function.")
endif()

# Phase 1 + the GPU + phase 3's window/present/input seam. There is still no
# RENDERER — that is phase 5, and a blank window is this phase's expected result.
# See docs/runtime-plan.md.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sibling XenonRecomp checkout: supplies simde (which ppc_context.h includes) and,
# from phase 1, the XEX loader in XenonUtils.
set(XENON_ROOT "$ENV{HOME}/GithubRepo/XenonRecomp" CACHE PATH "XenonRecomp source root")
set(PPC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ppc")

if(NOT EXISTS "${PPC_DIR}/ppc_func_mapping.cpp")
    message(FATAL_ERROR
        "No generated code in ${PPC_DIR}. It is gitignored (156 MB) — regenerate it:\n"
        "  mkdir -p ppc && cd config && \\\n"
        "    ${XENON_ROOT}/build/XenonRecomp/XenonRecomp CaseWest.toml \\\n"
        "    ${XENON_ROOT}/XenonUtils/ppc_context.h")
endif()

# RelWithDebInfo by default. Fable 2 measured a plain Debug build running recompiled
# VMX decode at roughly one movie frame per minute, and the addr2line workflow both
# earlier ports rely on needs the symbols.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

file(GLOB PPC_SOURCES CONFIGURE_DEPENDS "${PPC_DIR}/*.cpp")
list(LENGTH PPC_SOURCES PPC_TU_COUNT)
message(STATUS "Recompiled image: ${PPC_TU_COUNT} translation units")

# The image as a static library. Kept separate from the executable so that a
# runtime-only edit does not risk a rebuild of 227 TUs, and so the link step is
# visibly a *link* of the image rather than a compile of it.
add_library(ppc_image STATIC ${PPC_SOURCES})

target_include_directories(ppc_image PUBLIC
    ${PPC_DIR}                       # ppc_recomp_shared.h, ppc_context.h, ppc_config.h
    ${XENON_ROOT}/thirdparty/simde   # ppc_context.h includes <x86/avx.h> etc.
)

# SSE4.1 is not optional: ppc_context.h maps the guest's VMX unit onto simde, and
# several lowerings (the packs, min/max_epu32 used by the saturating vector
# arithmetic) are SSE4.1 intrinsics. Without it simde silently falls back to scalar
# emulation, which is correct but very slow.
#
# -Wno-unused-* silences the recompiler's own temporaries (`ea`, unused `temp`), not
# our code. They are emitted per-function by design and there are tens of thousands.
target_compile_options(ppc_image PRIVATE
    -msse4.1 -mavx
    -Wno-unused-variable -Wno-unused-but-set-variable)

# cpu/timebase.h is force-included over the RECOMPILED sources ONLY. `mftb` is
# lowered to __rdtsc(), which on this host runs ~70x faster than the console's
# 49.875 MHz timebase; the header shadows it with a scaled counter. The runtime's
# own sources must keep the real __rdtsc, which is why this is a source property on
# ${PPC_SOURCES} and not a target-wide option. Gotcha #1.
set_source_files_properties(${PPC_SOURCES} PROPERTIES COMPILE_OPTIONS
    "-include;${CMAKE_CURRENT_SOURCE_DIR}/cpu/timebase.h")

# CW_DEBUG_TUS — build named recompiled TUs at -O0 -fno-inline so a fault inside one
# resolves to an exact line, and every line in those files carries its original PPC
# mnemonic as a comment. Asura's Wrath established that host backtraces through
# recompiled frames at -O2 are not merely imprecise but actively misleading (its
# addr2line output named a function that was never called). Semicolon-separated
# basenames:  cmake .. -DCW_DEBUG_TUS="ppc_recomp.148.cpp"
set(CW_DEBUG_TUS "" CACHE STRING "Recompiled TU basenames to build at -O0")
foreach(tu IN LISTS CW_DEBUG_TUS)
    if(EXISTS "${PPC_DIR}/${tu}")
        message(STATUS "CW_DEBUG_TUS: ${tu} at -O0 -fno-inline")
        set_source_files_properties("${PPC_DIR}/${tu}" PROPERTIES COMPILE_OPTIONS
            "-include;${CMAKE_CURRENT_SOURCE_DIR}/cpu/timebase.h;-O0;-fno-inline")
    else()
        message(WARNING "CW_DEBUG_TUS: no such TU ${PPC_DIR}/${tu}")
    endif()
endforeach()

# SDL2 — the window, the present and the pad (phase 3). REQUIRED by default rather
# than optional-with-a-fallback: the runtime can run headless (CW_NO_WINDOW=1, and
# every gate in this project is a log diff that needs no pixels), but a build that
# silently loses its window would look exactly like a run whose input stopped
# working, and that is a session wasted. Configure with -DCW_WINDOW=OFF to say
# "headless on purpose" out loud.
option(CW_WINDOW "Build the SDL window / present / input seam (phase 3)" ON)
if(CW_WINDOW)
    find_package(SDL2 QUIET)
    if(NOT SDL2_FOUND)
        message(FATAL_ERROR
            "SDL2 not found, and it is what phase 3's window, present seam and pad "
            "are built on.\n"
            "  Fedora:  sudo dnf install SDL2-devel   (sdl2-compat-devel also works)\n"
            "  Debian:  sudo apt install libsdl2-dev\n"
            "Or build without it deliberately:  cmake -S runtime -B runtime/build "
            "-DCW_WINDOW=OFF")
    endif()
endif()

add_executable(cw_runtime
    main.cpp
    cpu/timebase.cpp
    cpu/guest_thread.cpp
    cpu/crash_report.cpp
    # cpu/guest_probe.cpp and cpu/debug_tunables.cpp are Case Zero's per-title reverse
    # engineering (89 hardcoded guest addresses between them and the D3D pair below).
    # They live in runtime/port-pending/ until Case West's own addresses are derived —
    # read that directory's README, especially the part about sub_82475718, which exists
    # in BOTH images and would have linked silently to an unrelated function.
    cpu/debug_tunables_stub.cpp
    kernel/klog.cpp
    kernel/memory.cpp
    kernel/heap.cpp
    kernel/kobject.cpp
    kernel/imports.cpp
    kernel/file_imports.cpp
    kernel/vfs.cpp
    kernel/audio.cpp
    audio/xma_decoder.cpp            # phase A/V; ffmpeg's XMA2, one per XMA context
    audio/audio_out.cpp              # phase A/V; stubs itself out without CW_HAVE_SDL
    kernel/content.cpp
    kernel/xex_imports.cpp
    gpu/vd.cpp
    gpu/pm4.cpp
    gpu/vk_renderer.cpp              # phase 5; inert unless CW_VKDRAW=1
    # gpu/d3d_hooks.cpp and gpu/d3d_draw.cpp: same reason, also in port-pending/. The
    # D3D arm was always inert without CW_D3D_DRAW=1, so the stub's "not enabled" is the
    # behaviour the disabled arm already had. PM4 is the boot engine and needs none of
    # this — it transplants with no per-title addresses at all.
    gpu/d3d_draw_stub.cpp
    host/window.cpp                  # phase 3; compiles to stubs without CW_HAVE_SDL
    kernel/import_stubs.cpp          # generated by tools/gen_import_stubs.py
    thirdparty/o1heap/o1heap.c
)

if(CW_WINDOW)
    target_compile_definitions(cw_runtime PRIVATE CW_HAVE_SDL=1)
    target_link_libraries(cw_runtime PRIVATE SDL2::SDL2)
endif()

# Vulkan — phase 5's renderer. REQUIRED for the same reason SDL2 is: the renderer is
# off at runtime unless CW_VKDRAW=1, so a machine that cannot render still builds and
# runs every gate this project owns, but a build that silently lost the renderer would
# be indistinguishable from a run where the renderer declined to start.
find_package(Vulkan QUIET)
if(NOT Vulkan_FOUND)
    message(FATAL_ERROR
        "Vulkan not found, and phase 5's renderer is built on it.\n"
        "  Fedora:  sudo dnf install vulkan-loader-devel vulkan-headers\n"
        "  Debian:  sudo apt install libvulkan-dev")
endif()
target_link_libraries(cw_runtime PRIVATE Vulkan::Vulkan)

# ffmpeg (libavcodec) — the XMA2 decoder for guest audio, phase A/V. REQUIRED, for
# the third time and the same reason as SDL2 and Vulkan: the game's own mixer reads
# PCM that only a decoder can write, so a build that quietly lost the codec produces
# a silent game that is indistinguishable from the defect this replaced. Refusing at
# CONFIGURE time is how "no sound" stays a build message instead of a mystery.
# Headers live under /usr/include/ffmpeg on Fedora; pkg-config supplies the paths.
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavutil)
target_link_libraries(cw_runtime PRIVATE PkgConfig::FFMPEG)

target_include_directories(cw_runtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/o1heap
    ${XENON_ROOT}/XenonUtils         # image.h (loader), xbox.h (guest structs), xex.h
)

target_link_libraries(cw_runtime PRIVATE ppc_image)

# The XEX loader. Prebuilt XenonUtils plus the two thirdparty libs it links against.
#
# This is the same loader `tools/xex_image_dump` uses, and it is deliberate that the
# runtime shares it: this XEX is LZX-compressed under the all-zero DEVKIT key, and
# `decrypt_xex.py` cannot read it at all (gotchas 15/16). One loader in the project
# means one thing to keep patched.
set(XENON_BUILD "${XENON_ROOT}/build" CACHE PATH "XenonRecomp build tree")
foreach(lib
        "${XENON_BUILD}/XenonUtils/libXenonUtils.a"
        "${XENON_BUILD}/thirdparty/fmt/libfmt.a"
        "${XENON_BUILD}/thirdparty/xxHash/cmake_unofficial/libxxhash.a")
    if(NOT EXISTS "${lib}")
        message(FATAL_ERROR
            "Missing ${lib}. Build the sibling XenonRecomp checkout first:\n"
            "  cmake -S ${XENON_ROOT} -B ${XENON_BUILD} && cmake --build ${XENON_BUILD}")
    endif()
    target_link_libraries(cw_runtime PRIVATE "${lib}")
endforeach()

# Whole-archive: the phase 0.2 gate — that EVERY generated symbol resolves — is
# still live, and is now `cw_runtime --smoke`. A normal static-library link pulls in
# only the objects it needs, so an unreferenced TU with an undefined symbol would
# link cleanly and the gate would pass while proving nothing. --whole-archive forces
# every object in. Keep this even once the runtime references most of the image on
# its own: "most" is not "every".
target_link_options(cw_runtime PRIVATE
    "-Wl,--whole-archive" "$<TARGET_FILE:ppc_image>" "-Wl,--no-whole-archive")
