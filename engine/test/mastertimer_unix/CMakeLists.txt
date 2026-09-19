set(module_name "mastertimer_unix_test")

# Diagnostic source-inclusion test: compiles mastertimer-unix.cpp inline with a
# virtual clock/sleep, so it does NOT link qlcplusengine. Unix only.
add_executable(${module_name}
    ${module_name}.cpp ${module_name}.h
    ../../src/timingdiagnostics.cpp
)
target_include_directories(${module_name} PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(${module_name} PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Test
)
