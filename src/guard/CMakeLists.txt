cmake_minimum_required(VERSION 3.28)

project(tinexus_guard VERSION ${TINEXUS_VERSION})

find_package(OpenSSL REQUIRED)

add_library(tinexus-guard STATIC
    crypto_validator.cpp
)

target_include_directories(tinexus-guard PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(tinexus-guard PRIVATE
    tinexus_warnings
    tinexus_sanitizers
    OpenSSL::Crypto
)

# Also create the keygen tool
add_executable(txapp-signer signer.cpp)
target_link_libraries(txapp-signer PRIVATE tinexus-guard tinexus_warnings OpenSSL::Crypto)

install(TARGETS tinexus-guard txapp-signer
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
