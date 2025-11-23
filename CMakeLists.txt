cmake_minimum_required(VERSION 3.15)
project(Gema LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)

# VST SDK path (passed from GitHub Actions)
if(NOT VST_SDK_PATH)
    message(FATAL_ERROR "VST_SDK_PATH must be set")
endif()

add_definitions(-DWIN32 -D_WINDOWS)

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    add_definitions(-D_WIN64)
endif()

include_directories(
    "${VST_SDK_PATH}/public.sdk/source/vst2.x"
    "${VST_SDK_PATH}"
)

add_library(Gema SHARED
    Gema.cpp
)

set_target_properties(Gema PROPERTIES
    PREFIX ""
    SUFFIX ".dll"
    OUTPUT_NAME "Gema"
)

target_compile_options(Gema PRIVATE /MT)  # static runtime (works on Win7+ without VC redist)