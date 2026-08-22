include_guard(GLOBAL)

include(FetchContent)

# These revisions are immutable. Developers and packagers can build without
# network access by setting FETCHCONTENT_SOURCE_DIR_FREETYPE and
# FETCHCONTENT_SOURCE_DIR_RMLUI to verified source trees at these revisions.
set(INGNOMIA_FREETYPE_REVISION
    "42608f77f20749dd6ddc9e0536788eaad70ea4b5"
    CACHE INTERNAL "Pinned FreeType 2.13.3 revision")
set(INGNOMIA_RMLUI_REVISION
    "2230d1a6e8e0848ed87a5761e2a5160b2a175ba4"
    CACHE INTERNAL "Pinned RmlUi 6.2 revision")

# Keep both dependencies static and keep FreeType's optional system-library
# discovery out of the reference build. This prevents host packages from
# silently changing the produced UI binary.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static third-party libraries" FORCE)
set(FT_DISABLE_ZLIB ON CACHE BOOL "Use FreeType's internal zlib" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "Disable FreeType bzip2 support" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "Disable FreeType PNG bitmap support" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "Disable FreeType HarfBuzz integration" FORCE)
set(FT_DISABLE_BROTLI ON CACHE BOOL "Disable FreeType WOFF2 support" FORCE)
if(DEFINED SKIP_INSTALL_ALL)
    set(_INGNOMIA_PREVIOUS_SKIP_INSTALL_ALL "${SKIP_INSTALL_ALL}")
    set(_INGNOMIA_HAD_SKIP_INSTALL_ALL TRUE)
else()
    set(_INGNOMIA_HAD_SKIP_INSTALL_ALL FALSE)
endif()
set(SKIP_INSTALL_ALL ON CACHE BOOL "Do not install embedded FreeType targets" FORCE)

FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG        ${INGNOMIA_FREETYPE_REVISION}
    GIT_SHALLOW    FALSE
    GIT_PROGRESS   FALSE
)
FetchContent_MakeAvailable(freetype)

if(_INGNOMIA_HAD_SKIP_INSTALL_ALL)
    set(SKIP_INSTALL_ALL "${_INGNOMIA_PREVIOUS_SKIP_INSTALL_ALL}" CACHE BOOL "" FORCE)
else()
    unset(SKIP_INSTALL_ALL CACHE)
    unset(SKIP_INSTALL_ALL)
endif()
unset(_INGNOMIA_PREVIOUS_SKIP_INSTALL_ALL)
unset(_INGNOMIA_HAD_SKIP_INSTALL_ALL)

# FreeType's in-tree target records this namespace only as an export name.
# RmlUi consumes the conventional package target during the same configure.
if(NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype ALIAS freetype-interface)
endif()

set(RMLUI_SAMPLES OFF CACHE BOOL "Build RmlUi samples" FORCE)
set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "Build RmlUi Lua bindings" FORCE)
set(RMLUI_SVG_PLUGIN OFF CACHE BOOL "Build RmlUi SVG plugin" FORCE)
set(RMLUI_LOTTIE_PLUGIN OFF CACHE BOOL "Build RmlUi Lottie plugin" FORCE)
set(RMLUI_HARFBUZZ_SAMPLE OFF CACHE BOOL "Build RmlUi HarfBuzz sample" FORCE)
set(RMLUI_TRACY_PROFILING OFF CACHE BOOL "Build RmlUi Tracy support" FORCE)
set(RMLUI_FONT_ENGINE freetype CACHE STRING "RmlUi font engine" FORCE)

FetchContent_Declare(
    rmlui
    GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
    GIT_TAG        ${INGNOMIA_RMLUI_REVISION}
    GIT_SHALLOW    FALSE
    GIT_PROGRESS   FALSE
)
FetchContent_MakeAvailable(rmlui)

if(NOT TARGET RmlUi::Core OR NOT TARGET RmlUi::Debugger)
    message(FATAL_ERROR "Pinned RmlUi source did not provide its expected CMake targets")
endif()
