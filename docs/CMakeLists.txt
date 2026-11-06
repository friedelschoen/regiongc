set(DOXYGEN_PROJECT_NAME "regiongc")
set(DOXYGEN_PROJECT_BRIEF "Region-based Allocator Library")

set(DOXYGEN_GENERATE_HTML YES)
set(DOXYGEN_GENERATE_MAN NO)
set(DOXYGEN_EXTRACT_ALL NO)
set(DOXYGEN_WARN_IF_UNDOCUMENTED YES)
set(DOXYGEN_QUIET YES)

doxygen_add_docs(regiongc_docs
    ${PROJECT_SOURCE_DIR}/include/regiongc.h
    ${CMAKE_CURRENT_SOURCE_DIR}/mainpage.md
    ALL
    COMMENT "Generate regiongc documentation"
)
