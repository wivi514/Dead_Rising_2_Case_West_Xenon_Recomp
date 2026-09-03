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

# Windows: the CRT deprecates getenv, strerror, fopen and friends in favour of its own
# _s variants. This runtime uses the standard ones throughout and deliberately keeps
# doing so — they are what the POSIX build uses and a divergence here would be a
# portability difference invented for a warning. Silencing it is not ignoring a defect;
# it is declining an invitation to use a Microsoft-only API.
if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_WARNINGS)

    # NOMINMAX is not a style preference, it is a correctness requirement. windows.h
    # defines `min` and `max` as function-like MACROS, so every `std::max(a, b)` in
    # this runtime expands to `std::((a)>(b)?(a):(b))` and the compiler reports
    # `expected unqualified-id` — pointing at our line, naming nothing, and saying
    # nothing about macros. Three of the nine broken files were only this.
    #
    # WIN32_LEAN_AND_MEAN drops the RPC, OLE, and socket headers nothing here uses,
    # which is both faster and a smaller surface for exactly this kind of collision.
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# host/win_compat.h is FORCE-INCLUDED into every runtime source on Windows rather than
# added to nine files by hand. Two reasons, and the second is the real one:
#
#   * it has to come before anything that pulls windows.h in transitively, and
#     "transitively" is not a property any single #include line can guarantee;
#   * a header that must be included first is a rule someone eventually forgets, and
#     the failure is a compile error in a file nobody touched.
#
# It compiles to nothing on every other platform, so the flag is Windows-only and the
# POSIX build is bit-identical with or without the file existing.
# Phase 1 + the GPU + phase 3's window/present/input seam. There is still no
# RENDERER — that is phase 5, and a blank window is this phase's expected result.
# See docs/runtime-plan.md.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sibling XenonRecomp checkout: supplies simde (which ppc_context.h includes) and,
# from phase 1, the XEX loader in XenonUtils.
set(XENON_ROOT "$ENV{HOME}/GithubRepo/XenonRecomp" CACHE PATH "XenonRecomp source root")
# CW_PPC_DIR overrides where the generated code comes from — CI points it at a
# STUB tree from tools/gen_stub_ppc.py, because no runner may hold the game
# (release-plan E.1). Unset, the sibling ppc/ is used as always.
set(CW_PPC_DIR "" CACHE PATH "Generated ppc tree (default: sibling ppc/)")
if(CW_PPC_DIR)
    set(PPC_DIR "${CW_PPC_DIR}")
else()
    set(PPC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ppc")
endif()

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
# The forced include, spelled for the driver in use. `-include <path>` is the GNU
# driver's flag; clang-cl parses the path as a second INPUT FILE and fails with
# "cannot specify '/Fo...' when compiling multiple source files" — an error that names
# the output rather than the flag, and which stopped all 228 recompiled TUs on the
# first Windows build (gotcha 494). /FI is the clang-cl spelling. MSVC is true for
# clang-cl in CMake, which is what we want: it selects on the DRIVER, not the vendor.
if(MSVC)
    set_source_files_properties(${PPC_SOURCES} PROPERTIES COMPILE_OPTIONS
        "/FI${CMAKE_CURRENT_SOURCE_DIR}/cpu/timebase.h")
else()
    set_source_files_properties(${PPC_SOURCES} PROPERTIES COMPILE_OPTIONS
        "-include;${CMAKE_CURRENT_SOURCE_DIR}/cpu/timebase.h")
endif()

# CW_PPC_O3 — build the RECOMPILED IMAGE at -O3; the runtime's own sources keep the
# build type's -O2. Exists because the 2026-08-28 uninstrumented perf profile showed the
# guest render thread SATURATED while the pump idled 23% at the HUD scene — i.e. on this
# title the recompiled code, not the renderer, can be the frame's pacing thread, which is
# the opposite of Case Zero's regime (their phase5-notes §6dm). GCC's last -O wins, so
# this appends cleanly after the build type's -O2; CW_DEBUG_TUS still wins over both
# because source-file properties are appended after target options.
option(CW_PPC_O3 "Build the recompiled guest image at -O3" OFF)
if(CW_PPC_O3)
    target_compile_options(ppc_image PRIVATE -O3)
    message(STATUS "CW_PPC_O3: recompiled image at -O3")
endif()

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

# ---------------------------------------------------------------------------
# Release D.2 — in-process shader translation (gpu/shader_translator.cpp).
#
# XenosRecomp's translator (MIT, the sibling checkout this project already patches) is
# compiled INTO the runtime — source, not a library, because upstream builds it as an
# executable and the 2,573-line recompiler TU is the whole dependency. Its build
# normally supplies a precompiled pch; gpu/xenos_pch.h is that pch minus smol-v, zstd,
# xxhash and dxcapi (see its header comment), force-included the same way.
set(XENOS_ROOT "$ENV{HOME}/GithubRepo/XenosRecomp" CACHE PATH "XenosRecomp source root")
if(NOT EXISTS "${XENOS_ROOT}/XenosRecomp/shader_recompiler.cpp")
    message(FATAL_ERROR
        "No XenosRecomp checkout at ${XENOS_ROOT} (need XenosRecomp/shader_recompiler.cpp "
        "for in-process shader translation, release D.2).\n"
        "  git clone https://github.com/hedge-dev/XenosRecomp ${XENOS_ROOT}\n"
        "or point at it:  cmake -DXENOS_ROOT=/path/to/XenosRecomp ...")
endif()

# shader_common.h is EMBEDDED, not read at run time. The HLSL the translator emits
# begins with this file's bytes, and the runtime's cache key contract is byte identity
# with the caches tools/build_shader_spv.sh built from the very same file — an
# external copy that drifted or went missing would silently produce different SPIR-V.
# Regenerated at configure time; copy_if_different so an unchanged file does not
# recompile the translator on every configure.
file(READ "${XENOS_ROOT}/XenosRecomp/shader_common.h" CW_SHADER_COMMON_HEX HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," CW_SHADER_COMMON_BYTES "${CW_SHADER_COMMON_HEX}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/cw_shader_common_embedded.h.in"
    "// Generated at configure time from ${XENOS_ROOT}/XenosRecomp/shader_common.h.\n"
    "// DO NOT EDIT — edit the source file and re-run cmake.\n"
    "#pragma once\n"
    "inline constexpr unsigned char g_czShaderCommonH[] = {${CW_SHADER_COMMON_BYTES}};\n"
    "inline constexpr unsigned long g_czShaderCommonHSize = sizeof(g_czShaderCommonH);\n")
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_BINARY_DIR}/generated/cw_shader_common_embedded.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/cw_shader_common_embedded.h")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${XENOS_ROOT}/XenosRecomp/shader_common.h")

add_executable(cw_runtime
    main.cpp
    cpu/timebase.cpp
    cpu/guest_thread.cpp
    cpu/crash_report.cpp
    cpu/thread_budget.cpp            # one thread budget for the whole runtime (imported from Case Zero part 55)
    cpu/pc_options_cw.cpp            # the Visuals panel's guest-side half (Case Zero part 60, default path only)
    cpu/fe_probe.cpp                 # frontend cFEMeter widget probe (findings 35-39)
    cpu/gap_probe.cpp                # counts calls into the 7 functions with untranslated instructions
    # cpu/guest_probe.cpp and cpu/debug_tunables.cpp are Case Zero's per-title reverse
    # engineering (89 hardcoded guest addresses between them and the D3D pair below).
    # They live in runtime/port-pending/ until Case West's own addresses are derived —
    # read that directory's README, especially the part about sub_82475718, which exists
    # in BOTH images and would have linked silently to an unrelated function.
    cpu/debug_tunables_cw.cpp
    cpu/native_kbm.cpp               # part 92 (Case Zero): native KB/M via the title's own input layer
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
    # gpu/parallel_record.cpp — part 7's per-range secondaries + deferred replay, PARKED
    # (part 8): superseded by Case Zero's part-89 recorder ("resolve serial, record
    # parallel"), which subsumes the deferred-replay win and adds worker recording that
    # our own 2c flip proved null under the range partition. The file stays as the
    # campaign's record; CW_VK_NO_PAR_RECORD=1 is the new serial control arm.
    gpu/vk_renderer.cpp              # phase 5; inert unless CW_VKDRAW=1
    gpu/shader_translator.cpp        # release D.2; --translate-shaders and D.4's seam
    gpu/shader_prebuild.cpp          # release D.3; the first-run disc shader pass
    "${XENOS_ROOT}/XenosRecomp/shader_recompiler.cpp"   # XenosRecomp's own translator
    host/host_paths.cpp              # release A.1: every path anchored to the exe
    host/first_run.cpp               # release A.2: what is missing, where it goes
    host/stfs_extract.cpp            # release E (§2.3 step 2): the in-process unpack
    gpu/d3d_draw_stub.cpp
    host/settings.cpp                # the Visuals panel's persistent store (imported from Case Zero part 60)
    host/window.cpp                  # phase 3; compiles to stubs without CW_HAVE_SDL
    kernel/import_stubs.cpp          # generated by tools/gen_import_stubs.py
    thirdparty/o1heap/o1heap.c
)

# host/win_compat.h, force-included into the RUNTIME's C++ sources and nothing else.
#
# Scoped to this target rather than the directory, for two reasons. The 228 generated
# TUs have no windows.h collisions and no need for it, and pulling windows.h into every
# one of them is a large build-time tax for nothing. And o1heap.c is C: a bare
# add_compile_options reaches it too, and a C++ header force-fed to a C compiler
# produces two thousand errors inside the header itself.
#
# The generator expression keeps it off C even within this target, so the one .c file
# here stays untouched.
if(WIN32)
    if(MSVC)
        target_compile_options(cw_runtime PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:/FI${CMAKE_CURRENT_SOURCE_DIR}/host/win_compat.h>")
    else()
        target_compile_options(cw_runtime PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/host/win_compat.h>")
    endif()
endif()

# D.2's two translation units get their include paths as SOURCE properties, not
# target-wide: the XenosRecomp directory carries generically-named headers (shader.h,
# constant_table.h) that nothing else in this runtime should ever resolve by accident.
# INCLUDE_DIRECTORIES on a source file APPENDS to the target's.
set(CW_XLATE_INCLUDES
    "${XENOS_ROOT}/XenosRecomp"
    "${XENOS_ROOT}/thirdparty/dxc-bin/inc"
    "${XENON_ROOT}/thirdparty/fmt/include"   # the fmt whose static lib is linked below
    "${CMAKE_CURRENT_BINARY_DIR}/generated")
set_source_files_properties("${XENOS_ROOT}/XenosRecomp/shader_recompiler.cpp"
    PROPERTIES INCLUDE_DIRECTORIES "${CW_XLATE_INCLUDES}")
set_source_files_properties(gpu/shader_translator.cpp
    PROPERTIES INCLUDE_DIRECTORIES "${CW_XLATE_INCLUDES}")
# Upstream's TU compiles against its pch and with its warning set; the force-include
# is the pch substitution (same /FI vs -include split as cpu/timebase.h above).
# shader_translator.cpp needs -fms-extensions on the GNU driver for dxcapi's
# WinAdapter (__declspec(uuid)); clang-cl has MS extensions on by default.
if(MSVC)
    set_source_files_properties("${XENOS_ROOT}/XenosRecomp/shader_recompiler.cpp"
        PROPERTIES COMPILE_OPTIONS
        "/FI${CMAKE_CURRENT_SOURCE_DIR}/gpu/xenos_pch.h;-Wno-switch;-Wno-unused-variable;-Wno-null-arithmetic")
else()
    set_source_files_properties("${XENOS_ROOT}/XenosRecomp/shader_recompiler.cpp"
        PROPERTIES COMPILE_OPTIONS
        "-include;${CMAKE_CURRENT_SOURCE_DIR}/gpu/xenos_pch.h;-Wno-switch;-Wno-unused-variable;-Wno-null-arithmetic")
    set_source_files_properties(gpu/shader_translator.cpp
        PROPERTIES COMPILE_OPTIONS "-fms-extensions")
endif()
# dlopen for the DXC library (a no-op string on Windows, where LoadLibrary is used).
target_link_libraries(cw_runtime PRIVATE ${CMAKE_DL_LIBS})

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
# CW_FFMPEG_PREFIX — build against the LGPL ffmpeg from tools/build_ffmpeg_lgpl.sh
# rather than the system one (docs/release-plan.md §4, A.4).
#
# This is a RELEASE requirement, not a preference. Fedora's ffmpeg is configured
# --enable-gpl, which conflicts with this repo's PolyForm Noncommercial licence the
# moment the two are shipped together; and its dependency closure is 120 shared
# objects (x264, x265, librsvg, cairo, pango, OpenCL, VA-API…) against the FOURTEEN
# ffmpeg functions this runtime calls. The purpose-built one is libavcodec + libavutil,
# 1.4 MB, linking libm and libc and nothing else.
#
# Developing against the system copy stays the default: it is the same 8.1.2, so the
# headers are identical and there is no second variable.
set(CW_FFMPEG_PREFIX "" CACHE PATH
    "Prefix of an ffmpeg built by tools/build_ffmpeg_lgpl.sh (release builds)")
if(CW_FFMPEG_PREFIX)
    if(NOT EXISTS "${CW_FFMPEG_PREFIX}/lib/pkgconfig/libavcodec.pc")
        message(FATAL_ERROR
            "CW_FFMPEG_PREFIX=${CW_FFMPEG_PREFIX} has no lib/pkgconfig/libavcodec.pc.\n"
            "  Build it:  tools/build_ffmpeg_lgpl.sh ${CW_FFMPEG_PREFIX}")
    endif()
    message(STATUS "ffmpeg: ${CW_FFMPEG_PREFIX} (LGPL, xma1+xma2 only)")
endif()

# Two ways to find ffmpeg, and which one runs is decided by whether we were TOLD
# where it is.
#
#   CW_FFMPEG_PREFIX set  -> look under it directly. No pkg-config.
#   not set               -> pkg-config, the system copy, the developer default.
#
# The direct path exists because Windows has no pkg-config, and adding one as a build
# dependency to satisfy a lookup we can do ourselves would be paying for a tool to
# tell us something we already passed in on the command line. It also removes the
# .pc-file path-translation problem entirely: an MSYS2-built .pc names
# `/c/cz/...`, which a Windows CMake cannot open.
if(CW_FFMPEG_PREFIX)
    find_path(CW_AVCODEC_INC libavcodec/avcodec.h
              PATHS "${CW_FFMPEG_PREFIX}/include" NO_DEFAULT_PATH REQUIRED)
    # Both spellings, because the two toolchains name the import library
    # differently and neither is wrong: libavcodec.so on Linux, avcodec.lib from
    # ffmpeg's MSVC toolchain.
    find_library(CW_AVCODEC_LIB NAMES avcodec libavcodec
                 PATHS "${CW_FFMPEG_PREFIX}/lib" "${CW_FFMPEG_PREFIX}/bin"
                 NO_DEFAULT_PATH REQUIRED)
    find_library(CW_AVUTIL_LIB NAMES avutil libavutil
                 PATHS "${CW_FFMPEG_PREFIX}/lib" "${CW_FFMPEG_PREFIX}/bin"
                 NO_DEFAULT_PATH REQUIRED)
    add_library(cw_ffmpeg INTERFACE)
    target_include_directories(cw_ffmpeg INTERFACE "${CW_AVCODEC_INC}")
    target_link_libraries(cw_ffmpeg INTERFACE "${CW_AVCODEC_LIB}" "${CW_AVUTIL_LIB}")
    add_library(CW::ffmpeg ALIAS cw_ffmpeg)
    message(STATUS "ffmpeg libs: ${CW_AVCODEC_LIB}  ${CW_AVUTIL_LIB}")
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET libavcodec libavutil)
    add_library(cw_ffmpeg INTERFACE)
    target_link_libraries(cw_ffmpeg INTERFACE PkgConfig::FFMPEG)
    add_library(CW::ffmpeg ALIAS cw_ffmpeg)
endif()
target_link_libraries(cw_runtime PRIVATE CW::ffmpeg)

target_include_directories(cw_runtime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/thirdparty/o1heap
    ${XENON_ROOT}/XenonUtils         # image.h (loader), xbox.h (guest structs), xex.h
)

target_link_libraries(cw_runtime PRIVATE ppc_image)

# Windows: VirtualAlloc2 / MapViewOfFile3 / UnmapViewOfFileEx are the placeholder
# mapping API kernel/memory.cpp uses for the guest's three aliased physical views.
# They live in kernelbase.dll and are NOT exported by kernel32.lib — the import
# library is onecore.lib. Verified by compiling and RUNNING a probe on the build
# laptop before any of this was written, because "the API exists" and "the API links"
# are different claims and only the second one builds.
if(WIN32)
    # onecore  — VirtualAlloc2 / MapViewOfFile3 / UnmapViewOfFileEx (kernel/memory.cpp).
    #            In kernelbase.dll and NOT exported by kernel32.lib.
    # dbghelp  — SymInitialize / SymFromAddr, dladdr's replacement in the crash reporter.
    target_link_libraries(cw_runtime PRIVATE onecore dbghelp)

    # The compiler-rt builtins. The recompiled image does 128-bit arithmetic (the guest's
    # multiply-high and divide lower to __int128), and clang-cl does NOT link its own
    # builtins library by default on Windows the way clang does on Linux — so __udivti3
    # comes out undefined at link time with nothing to say where it came from.
    #
    # Located from the compiler in use rather than hardcoded, because the path carries
    # the LLVM major version and would rot on the next upgrade.
    get_filename_component(CW_LLVM_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
    file(GLOB CW_CLANG_RT "${CW_LLVM_BIN}/../lib/clang/*/lib/windows/clang_rt.builtins-x86_64.lib")
    if(CW_CLANG_RT)
        list(GET CW_CLANG_RT 0 CW_CLANG_RT_LIB)
        message(STATUS "compiler-rt builtins: ${CW_CLANG_RT_LIB}")
        target_link_libraries(cw_runtime PRIVATE "${CW_CLANG_RT_LIB}")
    else()
        message(WARNING
            "clang_rt.builtins-x86_64.lib not found under ${CW_LLVM_BIN}/../lib/clang/. "
            "Expect an undefined __udivti3 at link time: the recompiled image needs "
            "128-bit division and clang-cl does not link its builtins by default.")
    endif()
endif()

# The XEX loader. Prebuilt XenonUtils plus the two thirdparty libs it links against.
#
# This is the same loader `tools/xex_image_dump` uses, and it is deliberate that the
# runtime shares it: this XEX is LZX-compressed under the all-zero DEVKIT key, and
# `decrypt_xex.py` cannot read it at all (gotchas 15/16). One loader in the project
# means one thing to keep patched.
set(XENON_BUILD "${XENON_ROOT}/build" CACHE PATH "XenonRecomp build tree")
# The three static libraries, named the way THIS platform names them. Same build
# tree, same directories, different decoration:
#   Linux    libXenonUtils.a
#   Windows  XenonUtils.lib
# CMAKE_STATIC_LIBRARY_{PREFIX,SUFFIX} are what CMake itself uses to construct those,
# so this cannot drift from what the sibling build actually emitted. Hardcoding `.a`
# was the last thing standing between the Windows tree and a successful configure.
set(P "${CMAKE_STATIC_LIBRARY_PREFIX}")
set(S "${CMAKE_STATIC_LIBRARY_SUFFIX}")
foreach(lib
        "${XENON_BUILD}/XenonUtils/${P}XenonUtils${S}"
        "${XENON_BUILD}/thirdparty/fmt/${P}fmt${S}"
        "${XENON_BUILD}/thirdparty/xxHash/cmake_unofficial/${P}xxhash${S}")
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
