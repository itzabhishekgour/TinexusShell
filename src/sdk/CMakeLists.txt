add_library(tinexus_sdk INTERFACE)
target_include_directories(tinexus_sdk INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include/tinexus-sdk>)
install(TARGETS tinexus_sdk EXPORT TinexusSDKTargets)
install(DIRECTORY include/sdk/ DESTINATION include/tinexus-sdk)
