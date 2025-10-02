cmake_minimum_required(VERSION 3.14)
project(raylib3d_transform_hierarchy LANGUAGES C CXX)

# Set up cache directory for built artifacts
set(CACHE_DIR "${CMAKE_BINARY_DIR}/cache")
file(MAKE_DIRECTORY ${CACHE_DIR})

if(NOT EXISTS "${CMAKE_BINARY_DIR}/cmake/CPM.cmake")
    file(DOWNLOAD
        "https://github.com/cpm-cmake/CPM.cmake/releases/latest/download/CPM.cmake"
        "${CMAKE_BINARY_DIR}/cmake/CPM.cmake"
    )
endif()
include(${CMAKE_BINARY_DIR}/cmake/CPM.cmake)

# raylib
CPMAddPackage(
    NAME raylib
    GITHUB_REPOSITORY raysan5/raylib
    GIT_TAG 5.5
    OPTIONS
        "BUILD_EXAMPLES OFF"
        "BUILD_GAMES OFF"
        "BUILD_SHARED_LIBS ON"
)

# raygui
CPMAddPackage(
    NAME raygui
    GITHUB_REPOSITORY raysan5/raygui
    GIT_TAG 4.0
    DOWNLOAD_ONLY YES
)

# flecs
CPMAddPackage(
    NAME flecs
    GITHUB_REPOSITORY SanderMertens/flecs
    GIT_TAG v4.0.5
    OPTIONS
        "FLECS_STATIC OFF"
        "FLECS_SHARED ON"
)

# mimalloc (Configured as DLL)
# CPMAddPackage(
#     NAME mimalloc
#     GITHUB_REPOSITORY microsoft/mimalloc
#     GIT_TAG v2.2.2
#     OPTIONS
#         "MI_BUILD_TESTS OFF"
#         "MI_BUILD_EXAMPLES OFF"
#         "MI_BUILD_SHARED ON"
# )

# Check and configure raylib.dll
set(RAYLIB_DLL_SRC "${raylib_BINARY_DIR}/raylib/Debug/raylib.dll")
set(RAYLIB_DLL "${CACHE_DIR}/raylib.dll")
if(NOT EXISTS ${RAYLIB_DLL} OR "${raylib_SOURCE_DIR}/src/raylib.h" IS_NEWER_THAN ${RAYLIB_DLL})
    add_custom_command(
        OUTPUT ${RAYLIB_DLL}
        COMMAND ${CMAKE_COMMAND} -E echo "Copying raylib.dll from ${RAYLIB_DLL_SRC} to ${RAYLIB_DLL}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${RAYLIB_DLL_SRC} ${RAYLIB_DLL}
        DEPENDS raylib
        COMMENT "Caching raylib.dll from ${RAYLIB_DLL_SRC}"
    )
    add_custom_target(raylib_cache DEPENDS ${RAYLIB_DLL})
endif()

# Check and configure flecs.dll
set(FLECS_DLL_SRC "${flecs_BINARY_DIR}/Debug/flecs.dll")
set(FLECS_DLL "${CACHE_DIR}/flecs.dll")
if(NOT EXISTS ${FLECS_DLL} OR "${flecs_SOURCE_DIR}/flecs.c" IS_NEWER_THAN ${FLECS_DLL})
    add_custom_command(
        OUTPUT ${FLECS_DLL}
        COMMAND ${CMAKE_COMMAND} -E echo "Copying flecs.dll from ${FLECS_DLL_SRC} to ${FLECS_DLL}"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${FLECS_DLL_SRC} ${FLECS_DLL}
        DEPENDS flecs
        COMMENT "Caching flecs.dll from ${FLECS_DLL_SRC}"
    )
    add_custom_target(flecs_cache DEPENDS ${FLECS_DLL})
endif()

# Check and configure mimalloc.dll (Updated to mimalloc-debug.dll)
# set(MIMALLOC_DLL_SRC "${mimalloc_BINARY_DIR}/Debug/mimalloc-debug.dll")  # Updated to match actual output
# set(MIMALLOC_DLL "${CACHE_DIR}/mimalloc-debug.dll")  # Keep the debug suffix for consistency
# if(NOT EXISTS ${MIMALLOC_DLL} OR "${mimalloc_SOURCE_DIR}/src/alloc.c" IS_NEWER_THAN ${MIMALLOC_DLL})
#     add_custom_command(
#         OUTPUT ${MIMALLOC_DLL}
#         COMMAND ${CMAKE_COMMAND} -E echo "Copying mimalloc-debug.dll from ${MIMALLOC_DLL_SRC} to ${MIMALLOC_DLL}"
#         COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MIMALLOC_DLL_SRC} ${MIMALLOC_DLL}
#         DEPENDS mimalloc
#         COMMENT "Caching mimalloc-debug.dll from ${MIMALLOC_DLL_SRC}"
#     )
#     add_custom_target(mimalloc_cache DEPENDS ${MIMALLOC_DLL})
# endif()

# MAIN
add_executable(${PROJECT_NAME} 
  src/raylib3d_transform_hierarchy04.c
)

if(TARGET raylib_cache)
    add_dependencies(${PROJECT_NAME} raylib_cache)
endif()
if(TARGET flecs_cache)
    add_dependencies(${PROJECT_NAME} flecs_cache)
endif()
# if(TARGET mimalloc_cache)
#     add_dependencies(${PROJECT_NAME} mimalloc_cache)
# endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${raylib_SOURCE_DIR}/src
    ${raygui_SOURCE_DIR}/src
    ${flecs_SOURCE_DIR}/include
    # ${mimalloc_SOURCE_DIR}/include
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    raylib
    flecs
    # mimalloc
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 winmm)
endif()


# Copy DLLs to output directory
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${RAYLIB_DLL}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/raylib.dll"
    COMMENT "Copying raylib.dll to output directory"
)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${FLECS_DLL}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/flecs.dll"
    COMMENT "Copying flecs.dll to output directory"
)

# add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
#     COMMAND ${CMAKE_COMMAND} -E copy_if_different
#         "${MIMALLOC_DLL}"
#         "$<TARGET_FILE_DIR:${PROJECT_NAME}>/mimalloc-debug.dll"  # Updated to match cached name
#     COMMENT "Copying mimalloc-debug.dll to output directory"
# )

# set(BUILD_ALL_EXAMPLES OFF CACHE BOOL "Build all examples")
# set(BUILD_ALL_EXAMPLES OFF)
set(BUILD_ALL_EXAMPLES ON)

set(examples
  #src/test.c
  src/raylib3d_flecs_transform_hierarchy06.c
  src/raylib2d_transform_hierarchy01.c
  src/raylib2d_flecs_transform_hierarchy04.c
)
# message(STATUS "BUILD_ALL_EXAMPLES >> ${BUILD_ALL_EXAMPLES}")

foreach(example_source ${examples})
    #message(STATUS "BUILD_ALL_EXAMPLES >> ${BUILD_ALL_EXAMPLES}")
    get_filename_component(example_name ${example_source} NAME_WE)
    string(REPLACE ".c" "" example_name ${example_name})
    
    # Create individual option, default ON
    option(BUILD_${example_name} "Build ${example_name} example" ON)
    
    # Build only if BUILD_ALL_EXAMPLES is ON and individual option is ON
    # If BUILD_ALL_EXAMPLES is OFF, nothing builds regardless of individual options
    if(BUILD_ALL_EXAMPLES)
        if(BUILD_${example_name})
            add_executable(${example_name} ${example_source})
            
            target_link_libraries(${example_name} PRIVATE
                raylib
                flecs
                # luajit
                # mimalloc
            )
            
            if(WIN32)
                target_link_libraries(${example_name} PRIVATE
                    ws2_32
                    winmm
                )
            endif()
            
            message(STATUS "Added example: ${example_name}")
        endif()
    endif()
endforeach()