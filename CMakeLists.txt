cmake_minimum_required(VERSION 3.16)
project(TrueHeight CXX)
include(CTest)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(TrueHeight WIN32
    src/main.cpp
    res/trueheight.rc
)
target_include_directories(TrueHeight PRIVATE ${CMAKE_SOURCE_DIR}/openvr/headers)

if(MSVC)
    target_compile_options(TrueHeight PRIVATE /W4 /permissive- /EHsc)
    set_property(TARGET TrueHeight PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    target_link_libraries(TrueHeight PRIVATE
        ${CMAKE_SOURCE_DIR}/openvr/lib/win64/openvr_api.lib
        user32 winmm comctl32 shell32)
else()
    # MinGW: link directly against the DLL
    target_link_libraries(TrueHeight PRIVATE
        ${CMAKE_SOURCE_DIR}/openvr/bin/win64/openvr_api.dll
        user32 winmm comctl32 shell32)
    target_link_options(TrueHeight PRIVATE -static-libgcc -static-libstdc++ -static)
endif()

if(BUILD_TESTING)
    add_executable(TrueHeightControlTests tests/control_tests.cpp)
    target_include_directories(TrueHeightControlTests PRIVATE ${CMAKE_SOURCE_DIR}/src)
    if(MSVC)
        target_compile_options(TrueHeightControlTests PRIVATE /W4 /permissive- /EHsc)
        set_property(TARGET TrueHeightControlTests PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    add_test(NAME control-math COMMAND TrueHeightControlTests)
endif()

add_custom_command(TARGET TrueHeight POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/openvr/bin/win64/openvr_api.dll
        $<TARGET_FILE_DIR:TrueHeight>)
