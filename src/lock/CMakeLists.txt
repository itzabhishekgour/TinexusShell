add_executable(tinexus-lock main.cpp)
target_link_libraries(tinexus-lock PRIVATE tinexus_common)
install(TARGETS tinexus-lock RUNTIME DESTINATION bin)
