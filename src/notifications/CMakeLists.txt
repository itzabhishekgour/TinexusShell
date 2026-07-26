add_executable(tinexus-notifications main.cpp)
target_link_libraries(tinexus-notifications PRIVATE tinexus_common)
install(TARGETS tinexus-notifications RUNTIME DESTINATION bin)
