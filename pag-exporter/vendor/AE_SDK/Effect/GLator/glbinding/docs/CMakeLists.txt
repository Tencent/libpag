
# Target 'docs'
add_custom_target(docs)
set_target_properties(docs PROPERTIES EXCLUDE_FROM_DEFAULT_BUILD 1)

# Documentation
add_subdirectory(doxygen)
