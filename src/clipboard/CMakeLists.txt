add_executable(tinexus-clipboard main.cpp)
target_link_libraries(tinexus-clipboard PRIVATE tinexus_common)
install(TARGETS tinexus-clipboard RUNTIME DESTINATION bin)
