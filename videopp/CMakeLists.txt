file(GLOB_RECURSE libsrc *.h *.cpp *.hpp *.c *.cc)

set(target_name videopp)

list(FILTER libsrc EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/detail/wgl/")
list(FILTER libsrc EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/detail/egl/")
list(FILTER libsrc EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/detail/glx/")
list(FILTER libsrc EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/detail/png/")
list(FILTER libsrc EXCLUDE REGEX "${CMAKE_CURRENT_SOURCE_DIR}/detail/stb/")

set(PNG_LOADER "STB" CACHE STRING "Backend type chosen by the user at CMake configure time")
set_property(CACHE PNG_LOADER PROPERTY STRINGS LIBPNG STB)

message( STATUS "--------------------------------" )
if("${PNG_LOADER}" STREQUAL "LIBPNG")
    message( STATUS "LIBPNG IMAGE loader selected." )

    find_package(PNG 1.6 REQUIRED)

    file(GLOB LIBPNG_libsrc "${CMAKE_CURRENT_SOURCE_DIR}/detail/png/*.h"
                             "${CMAKE_CURRENT_SOURCE_DIR}/detail/png/*.cpp"
                             "${CMAKE_CURRENT_SOURCE_DIR}/detail/png/*.hpp"
                             "${CMAKE_CURRENT_SOURCE_DIR}/detail/png/*.c"
                             "${CMAKE_CURRENT_SOURCE_DIR}/detail/png/*.cc")
    list(APPEND libsrc ${LIBPNG_libsrc})

elseif("${PNG_LOADER}" STREQUAL "STB")
    message( STATUS "STB IMAGE loader selected." )
    file(GLOB STB_libsrc "${CMAKE_CURRENT_SOURCE_DIR}/detail/stb/*.h"
                          "${CMAKE_CURRENT_SOURCE_DIR}/detail/stb/*.cpp"
                          "${CMAKE_CURRENT_SOURCE_DIR}/detail/stb/*.hpp"
                          "${CMAKE_CURRENT_SOURCE_DIR}/detail/stb/*.c"
                          "${CMAKE_CURRENT_SOURCE_DIR}/detail/stb/*.cc")
    list(APPEND libsrc ${STB_libsrc})

else()
    message( FATAL_ERROR "No IMAGE backend type was selected." )
endif()
message( STATUS "--------------------------------" )

set(OpenGL_GL_PREFERENCE LEGACY)
find_package(OpenGL REQUIRED)

if(WIN32)
    file(GLOB WGL_libsrc ${CMAKE_CURRENT_SOURCE_DIR}/detail/wgl/*)
else()
    if(OpenGL_EGL_FOUND)
        message(STATUS "Found EGL")
        file(GLOB EGL_libsrc ${CMAKE_CURRENT_SOURCE_DIR}/detail/egl/*)
    endif()

    if(OpenGL_GLX_FOUND)
        message(STATUS "Found GLX")
        file(GLOB GLX_libsrc ${CMAKE_CURRENT_SOURCE_DIR}/detail/glx/*)
    endif()

    if(OPENGL_gl_LIBRARY)
        message(STATUS "Found Legacy GLX")
        file(GLOB GLX_libsrc ${CMAKE_CURRENT_SOURCE_DIR}/detail/glx/*)
    endif()
endif()

list(APPEND libsrc ${WGL_libsrc})
#list(APPEND libsrc ${EGL_libsrc})
list(APPEND libsrc ${GLX_libsrc})

add_library(${target_name} ${libsrc})

set_target_properties(${target_name} PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
    POSITION_INDEPENDENT_CODE ON
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

target_link_libraries(${target_name} PUBLIC gli)
target_link_libraries(${target_name} PUBLIC glm)
target_link_libraries(${target_name} PUBLIC fontpp)
target_link_libraries(${target_name} PUBLIC ospp)

if(WIN32)
    target_link_libraries(${target_name} PRIVATE OpenGL::GL)
    target_compile_definitions(${target_name} PRIVATE WGL_CONTEXT)
else()
    if(OpenGL_EGL_FOUND)
        #target_link_libraries(${target_name} PRIVATE OpenGL::EGL)
        #target_compile_definitions(${target_name} PRIVATE EGL_CONTEXT)
    endif()

    if(OpenGL_GLX_FOUND)
        #target_link_libraries(${target_name} PRIVATE OpenGL::GLX)
        target_compile_definitions(${target_name} PRIVATE GLX_CONTEXT)
    endif()

    if(OPENGL_gl_LIBRARY)
        target_compile_definitions(${target_name} PRIVATE GLX_CONTEXT)
    endif()

    find_package(X11 REQUIRED)

    target_link_libraries(${target_name} PRIVATE X11::X11)
endif()

if("${PNG_LOADER}" STREQUAL "LIBPNG")
    target_link_libraries(${target_name} PRIVATE PNG::PNG)
endif()

target_include_directories(${target_name} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)


if(MSVC)
	target_compile_definitions(${target_name} PRIVATE _CRT_SECURE_NO_WARNINGS)
	target_compile_definitions(${target_name} PRIVATE _SCL_SECURE_NO_WARNINGS)
else()
	set_target_properties(${target_name} PROPERTIES COMPILE_FLAGS "-Wno-unused-function -Wno-unused-variable")
endif()
