add_executable(ddp_partial_test
    ddp_partial_test.cpp ddp_partial_test.h
)
target_include_directories(ddp_partial_test PRIVATE
    ../../interfaces
    ..
)
target_link_libraries(ddp_partial_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Network
    Qt${QT_MAJOR_VERSION}::Test
    ddp
)

add_executable(ddp_mutex_bench
    mutex_bench.cpp
)
target_include_directories(ddp_mutex_bench PRIVATE ../../interfaces ..)
target_link_libraries(ddp_mutex_bench PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Network
    ddp
)

# Microbench: isolate packet-build cost (no socket).
add_executable(ddp_packetizer_bench packetizer_bench.cpp)
target_link_libraries(ddp_packetizer_bench PRIVATE Qt${QT_MAJOR_VERSION}::Core)

# Microbench: full send loop incl. writeDatagram on loopback.
add_executable(ddp_wire_bench wire_bench.cpp)
target_link_libraries(ddp_wire_bench PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Network
)
