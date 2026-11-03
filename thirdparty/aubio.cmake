# Build aubio (https://aubio.org) as a static library inside the QLC+ tree.
#
# We bypass aubio's own CMakeLists.txt because it builds aubio as a SHARED
# library and pulls in optional audio I/O backends (libsndfile, libav,
# rubberband, vorbis, flac, Apple Audio, Accelerate, Intel IPP) that QLC+
# doesn't need — we feed aubio raw float buffers from QLC+'s capture layer.
#
# What we keep: the analysis core (vec/fft/filterbank/onset/pitch/tempo/
# spectral/temporal/notes/utils), enough to run aubio_onset_t, aubio_tempo_t,
# aubio_pitch_t, aubio_specdesc_t, aubio_mfcc_t, aubio_notes_t, aubio_pvoc_t.
#
# What we skip: io/ (audio file I/O), effects/*_rubberband.c, synth/
# (sampler/wavetable read from audio sources), and the optional FFT backends
# that need external SDKs (Accelerate, IPP, FFTW). The default Ooura FFT
# backend ships with aubio and works everywhere.

set(AUBIO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/aubio)

if(NOT EXISTS ${AUBIO_DIR}/src/aubio.h)
    message(FATAL_ERROR
        "aubio submodule missing at ${AUBIO_DIR}. "
        "Run: git submodule update --init thirdparty/aubio")
endif()

file(GLOB_RECURSE AUBIO_ALL_SOURCES CONFIGURE_DEPENDS ${AUBIO_DIR}/src/*.c)

set(AUBIO_EXCLUDE_PATTERNS
    "/io/"
    "/synth/"
    "/effects/pitchshift_rubberband\\.c$"
    "/effects/timestretch_rubberband\\.c$"
    "/effects/rubberband_utils\\.c$"
    "/spectral/dct_accelerate\\.c$"
    "/spectral/dct_fftw\\.c$"
    "/spectral/dct_ipp\\.c$"
)

set(AUBIO_SOURCES ${AUBIO_ALL_SOURCES})
foreach(_pat IN LISTS AUBIO_EXCLUDE_PATTERNS)
    list(FILTER AUBIO_SOURCES EXCLUDE REGEX "${_pat}")
endforeach()

# Generate aubio's config.h. The upstream cmake build only exports a handful
# of HAVE_* defines for system headers; we replicate that minimum set.
set(HAVE_STDLIB_H 1)
set(HAVE_STDIO_H 1)
set(HAVE_MATH_H 1)
set(HAVE_STRING_H 1)
set(HAVE_ERRNO_H 1)
set(HAVE_LIMITS_H 1)
set(HAVE_STDARG_H 1)
if(UNIX)
    set(HAVE_UNISTD_H 1)
    set(HAVE_GETOPT_H 1)
endif()
configure_file(
    ${AUBIO_DIR}/src/config.h.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/aubio_config/config.h
)

add_library(aubio_static STATIC ${AUBIO_SOURCES})
set_target_properties(aubio_static PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(aubio_static
    PUBLIC ${AUBIO_DIR}/src
    PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/aubio_config
)
target_compile_definitions(aubio_static PRIVATE HAVE_CONFIG_H)

# aubio's own sources have a few legacy warnings (unused variables, sign
# comparisons) that we don't want to police on the upstream code. Keep our
# own -Werror; just silence this one target.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(aubio_static PRIVATE -w)
endif()

add_library(qlc::aubio ALIAS aubio_static)
