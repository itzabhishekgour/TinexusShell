#ifndef TINEXUS_SEARCH_WIRE_PROTOCOL_HPP
#define TINEXUS_SEARCH_WIRE_PROTOCOL_HPP

#include "searchd/provider.hpp"
#include <cstdint>
#include <vector>
#include <cstring>

namespace tinexus::searchd {

#pragma pack(push, 1)
struct SearchResultWireHeader {
    uint32_t version{1};
    uint32_t result_count{0};
    uint64_t session_latency_us{0};
};
#pragma pack(pop)

class WireSerializer {
public:
    static std::vector<uint8_t> serialize_batch(const std::vector<SearchResult>& results, uint64_t latency_us) {
        std::vector<uint8_t> payload;
        SearchResultWireHeader header{1, static_cast<uint32_t>(results.size()), latency_us};

        size_t header_size = sizeof(header);
        payload.resize(header_size);
        std::memcpy(payload.data(), &header, header_size);

        for (const auto& res : results) {
            uint32_t title_len = static_cast<uint32_t>(res.title.size());
            uint32_t subtitle_len = static_cast<uint32_t>(res.subtitle.size());
            uint32_t action_len = static_cast<uint32_t>(res.action.size());
            uint32_t icon_len = static_cast<uint32_t>(res.icon.size());
            float score = res.score;

            auto append_bytes = [&payload](const void* data, size_t size) {
                const uint8_t* p = static_cast<const uint8_t*>(data);
                payload.insert(payload.end(), p, p + size);
            };

            append_bytes(&score, sizeof(score));
            append_bytes(&title_len, sizeof(title_len));
            append_bytes(res.title.data(), title_len);
            append_bytes(&subtitle_len, sizeof(subtitle_len));
            append_bytes(res.subtitle.data(), subtitle_len);
            append_bytes(&action_len, sizeof(action_len));
            append_bytes(res.action.data(), action_len);
            append_bytes(&icon_len, sizeof(icon_len));
            append_bytes(res.icon.data(), icon_len);
        }

        return payload;
    }
};

} // namespace tinexus::searchd

#endif // TINEXUS_SEARCH_WIRE_PROTOCOL_HPP
