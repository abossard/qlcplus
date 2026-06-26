set(module_name "vdjbridge_test")

add_executable(${module_name} WIN32
    ${module_name}.cpp ${module_name}.h
    ../../vdjbridge.cpp ../../vdjbridge.h
    ../../../plugins/vdjbridge/configurevdjbridge.cpp ../../../plugins/vdjbridge/configurevdjbridge.h
    ../../../plugins/vdjbridge/vdjbridgeplugin.cpp ../../../plugins/vdjbridge/vdjbridgeplugin.h
    ../../../plugins/vdjbridge/vdjtelemetryclient.cpp ../../../plugins/vdjbridge/vdjtelemetryclient.h
    ../../../plugins/vdjbridge/vdjbonjour.cpp ../../../plugins/vdjbridge/vdjbonjour.h
    ../../djfsm.cpp ../../djfsm.h
    ../../showfactory.cpp ../../showfactory.h
    ../../audiobpmtag.cpp ../../audiobpmtag.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../../engine/audio/src
    ../../../plugins/interfaces
    ../../../plugins/vdjbridge
    ../..
)

target_link_libraries(${module_name} PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Network
    Qt${QT_MAJOR_VERSION}::Test
    Qt${QT_MAJOR_VERSION}::Widgets
    qlcplusengine
)
