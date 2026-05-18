set(module_name "vdjtelemetryclient_test")

add_executable(${module_name} WIN32
    ${module_name}.cpp ${module_name}.h
    ../../vdjbridge.cpp ../../vdjbridge.h
    ../../vdjdeckmodel.cpp ../../vdjdeckmodel.h
    ../../vdjtelemetryclient.cpp ../../vdjtelemetryclient.h
    ../../vdjbonjour.cpp ../../vdjbonjour.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../../plugins/interfaces
    ../..
)

target_link_libraries(${module_name} PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Network
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
