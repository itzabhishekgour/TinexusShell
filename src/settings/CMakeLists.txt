add_executable(tinexus-settings main.cpp)
target_link_libraries(tinexus-settings PRIVATE tinexus_common)
install(TARGETS tinexus-settings RUNTIME DESTINATION bin)
