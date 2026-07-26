#ifndef TINEXUS_COMMON_SMALL_OBJECT_POOL_HPP
#define TINEXUS_COMMON_SMALL_OBJECT_POOL_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include <utility>
#include <memory>
#include <new>

namespace tinexus {

template <typename T, size_t BlockCapacity = 1024>
class SmallObjectPool {
private:
    union Node {
        alignas(alignof(T)) uint8_t storage[sizeof(T)];
        Node* next;
    };

public:
    SmallObjectPool() : m_free_list(nullptr) {
        allocate_block();
    }

    ~SmallObjectPool() = default;

    SmallObjectPool(const SmallObjectPool&) = delete;
    SmallObjectPool& operator=(const SmallObjectPool&) = delete;

    template <typename... Args>
    T* allocate(Args&&... args) {
        if (!m_free_list) {
            allocate_block();
        }

        Node* node = m_free_list;
        m_free_list = m_free_list->next;

        T* ptr = reinterpret_cast<T*>(node->storage);
        return ::new (static_cast<void*>(ptr)) T(std::forward<Args>(args)...);
    }

    void deallocate(T* ptr) noexcept {
        if (!ptr) return;

        ptr->~T();
        Node* node = reinterpret_cast<Node*>(ptr);
        node->next = m_free_list;
        m_free_list = node;
    }

private:
    void allocate_block() {
        auto block = std::make_unique<Node[]>(BlockCapacity);
        for (size_t i = 0; i < BlockCapacity - 1; ++i) {
            block[i].next = &block[i + 1];
        }
        block[BlockCapacity - 1].next = nullptr;
        m_free_list = &block[0];
        m_blocks.push_back(std::move(block));
    }

    Node* m_free_list;
    std::vector<std::unique_ptr<Node[]>> m_blocks;
};

} // namespace tinexus

#endif // TINEXUS_COMMON_SMALL_OBJECT_POOL_HPP
