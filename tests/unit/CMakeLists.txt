add_executable(test-lru-cache test_lru_cache.cpp)
target_link_libraries(test-lru-cache PRIVATE txui vterm tinexus_common)
