file(GLOB_RECURSE libsrc *.h *.cpp *.hpp *.c *.cc)

set(target_name videopp)

list(FILTER libsrc EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/detail/context_")
list(FILTER libsrc EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/glad/glad_")

if(WIN32)
    list(APPEND libsrc ${CMAKE_CURRENT_SOURCE_DIR}/detail/context_wgl.h)
    list(APPEND libsrc ${CMAKE_CURRENT_SOURCE_DIR}/detail/context_wgl.cpp)
    list(APPEND libsrc ${CMAKE_CURRENT_SOURCE_DIR}/glad/glad_wgl.h)
    list(APPEND libsrc ${CMAKE_CURRENT_SOURCE_DIR}/glad/glad_wgl.c)

else()
    list(APPEND libsrc ${CMAKE_CURRENT_SOURCE_DIR}/detail/context_glx.h)
    list(APPEND libsrc ${CMAKE_CURRENT_SOURCE_DIR}/detail/context_glx.cpp)
    list(APPEND libsrc ${CMAKE_CURRENT_SOURCE_DIR}/glad/glad_glx.h)
    list(APPEND libsrc ${CMAKE_CURRENT_SOURCE_DIR}/glad/glad_glx.c)

	list(APPEND libsrc ${CMAKE_CURRENT_SOURCE_DIR}/detail/context_egl.h)
	list(APPEND libsrc ${CMAKE_CURRENT_SOURCE_DIR}/detail/context_egl.cpp)
	list(APPEND libsrc ${CMAKE_CURRENT_SOURCE_DIR}/glad/glad_egl.h)
	list(APPEND libsrc ${CMAKE_CURRENT_SOURCE_DIR}/glad/glad_egl.c)
endif()


add_library(${target_name} ${libsrc})
target_link_libraries(${target_name} PUBLIC ospp fontpp glm)

find_package(OpenGL REQUIRED)
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
	#target_compile_options(${target_name} PRIVATE -Wno-bad-function-cast)
endif()
