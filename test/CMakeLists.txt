cmake_minimum_required(VERSION 3.20.0)
project(xhook_test VERSION 1.0)

set(CMAKE_CXX_STANDARD 23)

add_executable(${PROJECT_NAME} 
	src/main.cpp
)
target_link_libraries(${PROJECT_NAME}
	xhook
)