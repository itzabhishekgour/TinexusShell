add_executable(tinexus-pkg
    main.cpp
    package_manifest.cpp
    crypto_verifier.cpp
    dependency_solver.cpp
    package_database.cpp
    transaction_manager.cpp
    package_manager.cpp
)

target_include_directories(tinexus-pkg PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-pkg PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-pkg RUNTIME DESTINATION bin)
