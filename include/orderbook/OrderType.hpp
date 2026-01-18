#pragma once
#include <cstdint>

namespace orderbook {
    
    enum class OrderType : uint8_t {
        LIMIT,   // Default: order has a price, rests on book if not filled
        MARKET   // No price check: takes best available price, never rests
    };

    enum class TimeInForce : uint8_t {
        GTC,  // Good-Till-Cancel (default)
        IOC,  // Immediate-or-Cancel
        FOK   // Fill-or-Kill
    };

} 
