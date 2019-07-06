file(GLOB_RECURSE libsrc *.h *.cpp *.hpp *.c *.cc)

set(target_name videopp)

list(FILTER libsrc EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/detail/wgl")
list(FILTER libsrc EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/detail/egl")
list(FILTER libsrc EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/detail/glx")

find_package(OpenGL REQUIRED)

if(WIN32)
    file(GLOB wgl_src ${CMAKE_CURRENT_SOURCE_DIR}/detail/wgl/*)
else()
    if(OpenGL_EGL_FOUND)
        message(STATUS "Found EGL")
        file(GLOB egl_src ${CMAKE_CURRENT_SOURCE_DIR}/detail/egl/*)
    endif()

    #if(OpenGL_GLX_FOUND)
        message(STATUS "Found GLX")
        file(GLOB glx_src ${CMAKE_CURRENT_SOURCE_DIR}/detail/glx/*)
    #endif()
endif()

list(APPEND libsrc ${wgl_src})
list(APPEND libsrc ${egl_src})
list(APPEND libsrc ${glx_src})

add_library(${target_name} ${libsrc})
target_link_libraries(${target_name} PUBLIC ospp fontpp glm litehtml)

target_link_libraries(${target_name} PUBLIC ${OPENGL_LIBRARIES})
target_include_directories(${target_name} PUBLIC ${OPENGL_INCLUDE_DIR})

if(OpenGL_EGL_FOUND)
    target_link_libraries(${target_name} PUBLIC ${OPENGL_egl_LIBRARY})
    target_include_directories(${target_name} PUBLIC ${OPENGL_EGL_INCLUDE_DIRS})
    target_compile_definitions(${target_name} PRIVATE EGL_CONTEXT)
endif()

if(OpenGL_GLX_FOUND)
    target_link_libraries(${target_name} PUBLIC ${OPENGL_glx_LIBRARY})
    target_include_directories(${target_name} PUBLIC ${OPENGL_GLX_INCLUDE_DIRS})
    target_compile_definitions(${target_name} PRIVATE GLX_CONTEXT)
endif()

if(WIN32)
    target_compile_definitions(${target_name} PRIVATE WGL_CONTEXT)
else()
    target_link_libraries(${target_name} PRIVATE X11)
endif()


target_include_directories(${target_name}
    PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/..
)
set_target_properties(${target_name} PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
	WINDOWS_EXPORT_ALL_SYMBOLS ON
)

include(target_warning_support)
set_warning_level(${target_name} high)

include(target_code_style_support)
set_code_style(${target_name} lower_case check_headers "${extra_flags}")

if(MSVC)
	#target_compile_options(${target_name} PRIVATE /wd4505)
else()
	target_compile_options(${target_name} PRIVATE -Wno-unused-function)
endif()
