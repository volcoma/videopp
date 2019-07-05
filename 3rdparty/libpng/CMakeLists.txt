
if(BUILD_SHARED_LIBS)
    set(PNG_SHARED ON CACHE BOOL "Build shared lib" FORCE)
    set(PNG_STATIC OFF CACHE BOOL "Build static lib" FORCE)
else()
    set(PNG_SHARED OFF CACHE BOOL "Build shared lib" FORCE)
    set(PNG_STATIC ON CACHE BOOL "Build static lib" FORCE)
endif()

set(PNG_TESTS OFF CACHE BOOL "Build libpng tests" FORCE)

add_subdirectory( libpng )
