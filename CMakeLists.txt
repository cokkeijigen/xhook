cmake_minimum_required(VERSION 3.20.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    
    set(TOP_PROJECT_SOURCE_DIR TRUE)

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")

	    add_compile_options(
		    "-finput-charset=utf-8"
		    "-Wno-braced-scalar-init"
		    "-Wno-microsoft-cast"
            "-Wno-extra-tokens"
            "-Wno-extra-qualification"
            "-Wno-defaulted-function-deleted"
            "-Wno-parentheses"
            "-fasm-blocks"
	    )
	    add_link_options("-Wl")

    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")

	    string(REPLACE "/O2" "/O1" 
            CMAKE_CXX_FLAGS_RELEASE
            ${CMAKE_CXX_FLAGS_RELEASE}
        )
	    add_compile_options(
            "/source-charset:utf-8"
            "$<$<CONFIG:Release>:/GF>"
            "$<$<CONFIG:Release>:/Oy>"
            "$<$<CONFIG:Release>:/GL>"
            "$<$<CONFIG:Release>:/GS->"
        )
    endif()

endif()

add_subdirectory("xhook")

if(TOP_PROJECT_SOURCE_DIR)
    add_subdirectory("example")
endif()