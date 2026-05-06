# Minimal CMake build for aubio (vendored copy of upstream 0.4.9 src/ tree).
# Builds aubio as a static library without external dependencies.
#
# Source files are taken directly from upstream's src/ layout (aubio.h, onset/,
# pitch/, tempo/, notes/, spectral/, temporal/, utils/, ...). Audio I/O and
# synthesis modules are excluded — QLC+ feeds audio in directly.

cmake_minimum_required(VERSION 3.16)

# Collect all .c source files from aubio's src tree
file(GLOB_RECURSE AUBIO_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

# Exclude I/O sources (audio capture/decoding is handled by QLC+)
list(FILTER AUBIO_SOURCES EXCLUDE REGEX ".*/io/.*")
# Exclude synthesis sources (sampler/wavetable depend on parameter and resampler)
list(FILTER AUBIO_SOURCES EXCLUDE REGEX ".*/synth/.*")
# Exclude FFT/DCT backends that need external libs we don't link against
list(FILTER AUBIO_SOURCES EXCLUDE REGEX ".*/dct_fftw\\.c$")
list(FILTER AUBIO_SOURCES EXCLUDE REGEX ".*/dct_ipp\\.c$")
# Exclude Windows-only DLL entry point
list(FILTER AUBIO_SOURCES EXCLUDE REGEX ".*/windll\\.c$")

# Pick the DCT backend matching the FFT backend defined below
if(APPLE)
    list(FILTER AUBIO_SOURCES EXCLUDE REGEX ".*/dct_ooura\\.c$")
else()
    list(FILTER AUBIO_SOURCES EXCLUDE REGEX ".*/dct_accelerate\\.c$")
endif()

add_library(qlcplusaubio STATIC ${AUBIO_SOURCES})
set_property(TARGET qlcplusaubio PROPERTY POSITION_INDEPENDENT_CODE ON)

# Public include dir: parent dir so consumers write `#include <aubio/aubio.h>`
# (matches upstream's installed include layout: <prefix>/include/aubio/aubio.h).
# Private include dir: this dir, so internal sources find "fvec.h", "spectral/fft.h", etc.
target_include_directories(qlcplusaubio
    PUBLIC  "${CMAKE_CURRENT_SOURCE_DIR}/.."
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
)

# Feature defines. We rely on aubio's built-in Ooura FFT (or Accelerate on Apple),
# so no external FFT/audio dependency is needed.
target_compile_definitions(qlcplusaubio PRIVATE
    HAVE_STDLIB_H=1
    HAVE_STDIO_H=1
    HAVE_MATH_H=1
    HAVE_STRING_H=1
    HAVE_LIMITS_H=1
    HAVE_STDARG_H=1
    HAVE_ERRNO_H=1
    HAVE_C99_VARARGS_MACROS=1
    AUBIO_LOG_DOMAIN="qlcplus"
)

if(APPLE)
    target_compile_definitions(qlcplusaubio PRIVATE HAVE_ACCELERATE=1)
    target_link_libraries(qlcplusaubio PRIVATE "-framework Accelerate")
endif()

if(WIN32)
    target_compile_definitions(qlcplusaubio PRIVATE HAVE_WIN_HACKS=1)
endif()

# Vendored third-party code: silence all warnings to keep parent -Werror builds clean.
if(MSVC)
    target_compile_options(qlcplusaubio PRIVATE /w)
else()
    target_compile_options(qlcplusaubio PRIVATE -w)
endif()
