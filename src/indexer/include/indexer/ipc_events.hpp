#ifndef TINEXUS_INDEXER_IPC_EVENTS_HPP
#define TINEXUS_INDEXER_IPC_EVENTS_HPP

#include <cstdint>

namespace tinexus::indexer {

#pragma pack(push, 1)
struct IndexUpdateEvent {
    uint32_t version{1};         // Protocol version (v1.0)
    uint64_t generation{0};      // Monotonically increasing index generation counter
    uint32_t changed_items{0};  // Number of items added/modified/deleted in this update
    uint64_t timestamp{0};       // UNIX epoch timestamp (seconds)
};
#pragma pack(pop)

} // namespace tinexus::indexer

#endif // TINEXUS_INDEXER_IPC_EVENTS_HPP
