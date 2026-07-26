add_executable(tinexus-settings-ui main.cpp)
target_link_libraries(tinexus-settings-ui PRIVATE tinexus_common)
install(TARGETS tinexus-settings-ui RUNTIME DESTINATION bin)
