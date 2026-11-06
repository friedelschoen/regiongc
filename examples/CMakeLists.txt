add_custom_target(regiongc_examples)

function(add_regiongc_example name)
    add_executable(example_${name}
        ${ARGN}
    )

    target_link_libraries(example_${name} PRIVATE
        regiongc
    )

    add_dependencies(regiongc_examples
        example_${name}
    )
endfunction()

#add_regiongc_example(regiongc regiongc.c)
