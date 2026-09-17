if(ENABLE_LEGACY_TXUI)
    add_executable(test-lru-cache test_lru_cache.cpp)
    target_link_libraries(test-lru-cache PRIVATE txui vterm tinexus_common)

    add_executable(test-txui-clipboard test_txui_clipboard.cpp)
    target_link_libraries(test-txui-clipboard PRIVATE txui tinexus_common)
endif()

add_executable(test-hardware-utils test_hardware_utils.cpp)
target_link_libraries(test-hardware-utils PRIVATE tinexus_common)

