cmake_minimum_required(VERSION 3.18)

if(NOT EXISTS "${QLC_BUILD_DIR}/CMakeCache.txt")
    message(FATAL_ERROR "QLC_BUILD_DIR must name a configured QLC+ build")
endif()
load_cache("${QLC_BUILD_DIR}" READ_WITH_PREFIX QLC_
    CMAKE_CXX_COMPILER CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET
    Qt6_DIR qmlui FFTW3_FOUND FFTW3_INCLUDE_DIRS)
set(CMAKE_CXX_COMPILER "${QLC_CMAKE_CXX_COMPILER}")
set(CMAKE_OSX_ARCHITECTURES "${QLC_CMAKE_OSX_ARCHITECTURES}")
set(CMAKE_OSX_DEPLOYMENT_TARGET "${QLC_CMAKE_OSX_DEPLOYMENT_TARGET}")
project(hue_gallery LANGUAGES CXX)

if(NOT QLC_qmlui)
    message(FATAL_ERROR "The gallery requires a QLC+ QML build (qmlui=ON)")
endif()
set(Qt6_DIR "${QLC_Qt6_DIR}")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Multimedia Network)
get_filename_component(QLC_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
find_library(QLC_ENGINE_LIBRARY NAMES qlcplusengine5 qlcplusengine
    PATHS "${QLC_BUILD_DIR}/engine/src"
          "${QLC_BUILD_DIR}/engine/src/${CMAKE_BUILD_TYPE}" NO_DEFAULT_PATH REQUIRED)
add_library(qlc_gallery_engine UNKNOWN IMPORTED)
set_target_properties(qlc_gallery_engine PROPERTIES IMPORTED_LOCATION "${QLC_ENGINE_LIBRARY}")
add_executable(hue_gallery_native native_gallery.cpp)
target_compile_features(hue_gallery_native PRIVATE cxx_std_17)
target_compile_definitions(hue_gallery_native PRIVATE QMLUI)
target_include_directories(hue_gallery_native PRIVATE
    "${QLC_SOURCE_DIR}/engine/src"
    "${QLC_SOURCE_DIR}/engine/audio/src"
    "${QLC_SOURCE_DIR}/plugins/interfaces")
if(QLC_FFTW3_FOUND)
    target_compile_definitions(hue_gallery_native PRIVATE HAS_FFTW3)
    target_include_directories(hue_gallery_native PRIVATE ${QLC_FFTW3_INCLUDE_DIRS})
endif()
target_link_libraries(hue_gallery_native PRIVATE qlc_gallery_engine
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Multimedia Qt6::Network)
file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/native-paths-$<CONFIG>.txt"
    CONTENT "$<TARGET_FILE:hue_gallery_native>\n${QLC_ENGINE_LIBRARY}\n")
