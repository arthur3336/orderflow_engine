#pragma once

#include <orderbook/Types.hpp>
#include <cstdint>
#include <vector>

namespace orderbook {
    enum class STPMode : uint8_t {
        ALLOW,              // No self-trade prevention (default)
        CANCEL_NEWEST,      // Reject incoming order if would self-trade
        CANCEL_OLDEST,      // Cancel resting order if would self-trade
        CANCEL_BOTH,        // Cancel both orders
        DECREMENT_AND_CANCEL // Skip self-trades, fill rest
    };

    struct STPResult {
        bool selfTrade = false;
        std::vector<OrderId> cancelledOrders;
        std::string action;
    };


}