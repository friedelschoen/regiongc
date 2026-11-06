cmake_minimum_required(VERSION 3.20)

project(regiongc LANGUAGES C)

include(CTest)

option(REGIONGC_WERROR "Treat regiongc warnings as errors" ON)
option(REGIONGC_TEST_ON_BUILD "Run tests as part of the default build" ON)
option(REGIONGC_BUILD_EXAMPLES "Build regiongc examples" ON)
option(REGIONGC_BUILD_DOCS "Build regiongc docs" ON)

add_library(regiongc
    src/rgc-context.c
    src/rgc-error.c
    src/rgc-layer.c
    src/rgc-layer-gc.c
    src/rgc-layer-region.c
    src/rgc-object.c
    src/rgc-ref.c
    src/rgc-segment.c
    src/rgc-vector.c     
)

target_include_directories(regiongc
    PUBLIC include 
)

target_compile_options(regiongc PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

set_property(TARGET regiongc PROPERTY C_STANDARD 11)

if(REGIONGC_WERROR)
    target_compile_options(regiongc PRIVATE -Werror)
endif()

add_library(regiongc::regiongc ALIAS regiongc)

if(BUILD_TESTING)
    add_subdirectory(tests)

    if(REGIONGC_TEST_ON_BUILD)
        add_custom_target(check ALL
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
            DEPENDS regiongc_tests
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            USES_TERMINAL
        )
    endif()
endif()

#if (REGIONGC_BUILD_OPTIONS)
#    add_subdirectory(options)
#endif()

if(REGIONGC_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

if(REGIONGC_BUILD_DOCS)
    find_package(Doxygen QUIET)

    if(Doxygen_FOUND)
        add_subdirectory(docs)
    else()
        message(STATUS "Doxygen not found; documentation will not be built")
    endif()
endif()

