#ifndef TINEXUS_IPCD_BROKER_PUBSUB_BROKER_HPP
#define TINEXUS_IPCD_BROKER_PUBSUB_BROKER_HPP

#include <vector>
#include <unordered_map>
#include <mutex>
#include <algorithm>

namespace tinexus::ipcd::broker {

using TopicId = uint16_t; // Matches MessageType

class PubSubBroker {
public:
    static PubSubBroker& instance() {
        static PubSubBroker s_instance;
        return s_instance;
    }

    void subscribe(TopicId topic, int subscriber_fd) {
        std::lock_guard lock(m_mutex);
        auto& subscribers = m_subscriptions[topic];
        if (std::find(subscribers.begin(), subscribers.end(), subscriber_fd) == subscribers.end()) {
            subscribers.push_back(subscriber_fd);
        }
    }

    void unsubscribe(TopicId topic, int subscriber_fd) {
        std::lock_guard lock(m_mutex);
        auto it = m_subscriptions.find(topic);
        if (it != m_subscriptions.end()) {
            auto& subscribers = it->second;
            subscribers.erase(std::remove(subscribers.begin(), subscribers.end(), subscriber_fd), subscribers.end());
        }
    }

    void unsubscribe_all(int subscriber_fd) {
        std::lock_guard lock(m_mutex);
        for (auto& [topic, subscribers] : m_subscriptions) {
            subscribers.erase(std::remove(subscribers.begin(), subscribers.end(), subscriber_fd), subscribers.end());
        }
    }

    std::vector<int> get_subscribers(TopicId topic) const {
        std::lock_guard lock(m_mutex);
        auto it = m_subscriptions.find(topic);
        if (it != m_subscriptions.end()) {
            return it->second;
        }
        return {};
    }

private:
    PubSubBroker() = default;
    mutable std::mutex m_mutex;
    std::unordered_map<TopicId, std::vector<int>> m_subscriptions;
};

} // namespace tinexus::ipcd::broker

#endif // TINEXUS_IPCD_BROKER_PUBSUB_BROKER_HPP
