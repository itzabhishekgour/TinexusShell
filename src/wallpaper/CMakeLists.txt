add_executable(tinexus-wallpaper main.cpp)
target_link_libraries(tinexus-wallpaper PRIVATE tinexus_common)
install(TARGETS tinexus-wallpaper RUNTIME DESTINATION bin)
