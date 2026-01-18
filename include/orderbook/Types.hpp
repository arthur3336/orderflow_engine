#pragma once
// ^^^ This is a header guard - prevents the file from being included twice
// Alternative older style: #ifndef TYPES_HPP / #define TYPES_HPP / #endif

#include <cstdint>   // For uint64_t, int64_t - fixed-size integer types
#include <chrono>    // For time handling
#include <string>    // For priceToString helper

namespace orderbook {

    // =============================================================================
    // LESSON: enum class (scoped enumeration)
    // =============================================================================
    // 'enum class' is better than plain 'enum' because:
    // 1. Values are scoped: must write Side::BUY, not just BUY
    // 2. No implicit conversion to int (prevents bugs)
    // 3. Can specify underlying type (uint8_t = 1 byte)

    enum class Side : uint8_t {
        BUY,   // Buyer wants to purchase (goes in bids)
        SELL   // Seller wants to sell (goes in asks)
    };

    // =============================================================================
    // LESSON: Type aliases with 'using'
    // =============================================================================
    // Type aliases make code more readable and easier to change later.
    // If we ever need to change OrderId from uint64_t to something else,
    // we only change it here.

    using OrderId = uint64_t;      // Unique identifier for each order
    using Quantity = int64_t;      // Number of shares/units (signed for safe arithmetic)
    using Price = int64_t;         // Price in cents (2 decimal places, signed for safe arithmetic)

    // =============================================================================
    // Fixed-point price handling
    // =============================================================================
    // We store prices as integers to avoid floating-point precision issues.
    // PRICE_SCALE = 100 means 2 decimal places: $100.50 is stored as 10050

    constexpr Price PRICE_SCALE = 100;

    // Convert internal price (cents) to display string
    // Example: 10050 → "100.50"
    inline std::string priceToString(Price price) {
        bool negative = price < 0;
        if (negative) price = -price;

        int64_t dollars = price / PRICE_SCALE;
        int64_t cents = price % PRICE_SCALE;

        std::string result = (negative ? "-" : "") +
                            std::to_string(dollars) + "." +
                            (cents < 10 ? "0" : "") + std::to_string(cents);
        return result;
    }

    // For timestamps, we use the chrono library
    // steady_clock is monotonic (always goes forward, never adjusted)
    using Timestamp = std::chrono::steady_clock::time_point;

    // Helper function to get current time
    inline Timestamp now() {
        return std::chrono::steady_clock::now();
    }

    // Forward declaration needed for OrderLocation
    struct Order;  // tells compiler "Order exists, details later"

    struct Trade {
        OrderId buyOrderId; 
        OrderId sellOrderId; 
        Price price; 
        Quantity quantity;
        Timestamp time; 
    }; 
}
