#ifndef TINEXUS_SDK_TRANSPORT_HPP
#define TINEXUS_SDK_TRANSPORT_HPP

#include <vector>
#include <cstdint>
#include <functional>
#include "tinexus/result.hpp"

namespace tinexus {

using EventCallback = std::function<void(const std::vector<uint8_t>&)>;

class ITransport {
public:
    virtual ~ITransport() = default;

    virtual Result<bool> connect(const std::string& endpoint) = 0;
    virtual void close() = 0;

    virtual Result<std::vector<uint8_t>> request(uint16_t msg_type, const std::vector<uint8_t>& payload) = 0;
    virtual Result<bool> subscribe(uint16_t topic_id, EventCallback callback) = 0;
};

} // namespace tinexus

#endif // TINEXUS_SDK_TRANSPORT_HPP
