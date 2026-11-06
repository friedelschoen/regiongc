find_package(PkgConfig REQUIRED)
pkg_check_modules(CHECK REQUIRED IMPORTED_TARGET check)

add_custom_target(regiongc_tests)

function(add_regiongc_test name)
    add_executable(test-${name}
        ${ARGN}
    )

    target_include_directories(test-${name} PRIVATE ../include ../src)
    target_link_libraries(test-${name} PRIVATE
        regiongc
        PkgConfig::CHECK
    )

    add_test(
        NAME ${name}
        COMMAND test-${name}
    )

    add_dependencies(regiongc_tests test-${name})
endfunction()

add_regiongc_test(segment test-segment.c allocator.c)
add_regiongc_test(layer test-layer.c allocator.c)
