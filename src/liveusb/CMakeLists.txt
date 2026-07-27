add_executable(tinexus-liveusb
    main.cpp
    device_detector.cpp
    iso_verifier.cpp
    raw_writer.cpp
    verification.cpp
    progress_tracker.cpp
    persistence_mgr.cpp
    liveusb_controller.cpp
)

target_include_directories(tinexus-liveusb PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(tinexus-liveusb PRIVATE
    tinexus-sdk
    tinexus_common
    tinexus_warnings
    tinexus_sanitizers
)

install(TARGETS tinexus-liveusb RUNTIME DESTINATION bin)
