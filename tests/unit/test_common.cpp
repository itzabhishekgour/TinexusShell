#include <iostream>
#include <cassert>
#include "common/version.hpp"
#include "common/result.hpp"
#include "common/lockfree_queue.hpp"
#include "common/arena_allocator.hpp"
#include "common/small_object_pool.hpp"
#include "common/string_interner.hpp"
#include "common/logger.hpp"

void test_version() {
    assert(tinexus::VERSION_MAJOR == 0);
    assert(tinexus::VERSION_STRING == "0.1.0");
    std::cout << "[PASS] test_version\n";
}

void test_result() {
    tinexus::Result<int, std::string> ok_res(42);
    assert(ok_res.has_value());
    assert(ok_res.value() == 42);

    tinexus::Result<int, std::string> err_res(tinexus::Unexpected(std::string("Error occurred")));
    assert(!err_res.has_value());
    assert(err_res.error() == "Error occurred");

    std::cout << "[PASS] test_result\n";
}

void test_lockfree_queue() {
    tinexus::SpscQueue<int, 16> queue;
    assert(queue.empty());

    for (int i = 0; i < 10; ++i) {
        assert(queue.push(i));
    }
    assert(queue.size() == 10);

    for (int i = 0; i < 10; ++i) {
        auto val = queue.pop();
        assert(val.has_value());
        assert(val.value() == i);
    }
    assert(queue.empty());

    std::cout << "[PASS] test_lockfree_queue\n";
}

void test_arena_allocator() {
    tinexus::SearchArena<1024> arena;
    assert(arena.used_bytes() == 0);

    int* num = arena.allocate<int>(42);
    assert(*num == 42);
    assert(arena.used_bytes() >= sizeof(int));

    arena.reset();
    assert(arena.used_bytes() == 0);

    std::cout << "[PASS] test_arena_allocator\n";
}

void test_small_object_pool() {
    struct TestNode {
        int x;
        double y;
    };
    tinexus::SmallObjectPool<TestNode, 16> pool;
    TestNode* n1 = pool.allocate(10, 3.14);
    assert(n1->x == 10);
    assert(n1->y == 3.14);

    pool.deallocate(n1);
    std::cout << "[PASS] test_small_object_pool\n";
}

void test_string_interner() {
    auto& interner = tinexus::StringInterner::instance();
    auto s1 = interner.intern("org.mozilla.firefox");
    auto s2 = interner.intern("org.mozilla.firefox");

    assert(s1.data() == s2.data()); // Pointer equality guaranteed by interning
    std::cout << "[PASS] test_string_interner\n";
}

int main() {
    tinexus::log::set_component_name("unit_tests");
    tinexus::log::info("Running unit test suite for tinexus_common...");

    test_version();
    test_result();
    test_lockfree_queue();
    test_arena_allocator();
    test_small_object_pool();
    test_string_interner();

    tinexus::log::info("All unit tests passed successfully!");
    return 0;
}
