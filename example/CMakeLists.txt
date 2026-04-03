cmake_minimum_required(VERSION 3.20.0)
project(xhook_test VERSION 1.0)

set(CMAKE_CXX_STANDARD 23)

file(GLOB SRCFILES "src/*.cpp")
foreach(SRCFILE IN LISTS SRCFILES)
    get_filename_component(FILE_NAME ${SRCFILE} NAME_WE)
    add_executable(${FILE_NAME} ${SRCFILE})
    target_link_libraries(${FILE_NAME} xhook::xhook)
endforeach()