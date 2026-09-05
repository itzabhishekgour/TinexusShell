add_executable(test-lru-cache test_lru_cache.cpp)
target_link_libraries(test-lru-cache PRIVATE txui vterm tinexus_common)

add_executable(test-txui-clipboard test_txui_clipboard.cpp)
target_link_libraries(test-txui-clipboard PRIVATE txui tinexus_common)

