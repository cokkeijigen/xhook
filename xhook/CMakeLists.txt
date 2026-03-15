cmake_minimum_required(VERSION 3.20.0)
project(xhook VERSION 1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_path(DETOURS_INCLUDE_DIR NAMES "detours.h" PATH_SUFFIXES detours)
find_library(DETOURS_LIBRARY NAMES detours)

if (NOT DETOURS_INCLUDE_DIR OR NOT DETOURS_LIBRARY)
    message(FATAL_ERROR "Detours not found! Include: ${DETOURS_INCLUDE_DIR}, Lib: ${DETOURS_LIBRARY}")
endif()

add_library(${PROJECT_NAME} OBJECT 
    "src/xhook.cpp"
)

target_include_directories(${PROJECT_NAME} 
    PRIVATE ${DETOURS_INCLUDE_DIR} 
    PUBLIC  "src"
)

target_link_libraries(${PROJECT_NAME} 
    PRIVATE ${DETOURS_LIBRARY}
)

target_compile_features(${PROJECT_NAME}
    PUBLIC cxx_std_23
)