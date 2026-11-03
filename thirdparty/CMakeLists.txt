# Vendored third-party dependencies for the QLC+ audio analysis pipeline.
#
# Each library is gated by an option set at the top level. Targets are static
# so the QLC+ binary stays self-contained; we never depend on a system-installed
# build of these libraries at runtime.

if(audio_aubio)
    include(${CMAKE_CURRENT_SOURCE_DIR}/aubio.cmake)
endif()
