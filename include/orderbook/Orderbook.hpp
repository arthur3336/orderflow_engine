#pragma once

#include <orderbook/Types.hpp>
#include <orderbook/Order.hpp>
#include <map>
#include <orderbook/PriceLevel.hpp>
#include <functional>
#include <unordered_map>
#include <orderbook/PriceHistory.hpp>
#include <string>
#include <vector>
#include <orderbook/SelfTradePrevent.hpp>

namespace orderbook {

    // =========================================================================
    // API Layer Validation Requirements
    // =========================================================================
    // The following validations should be performed at the API layer BEFORE
    // calling OrderBook methods. These are business rules, not safety checks.
    //
    // 1. Order size limits:
    //    - quantity >= MIN_ORDER_SIZE (1)
    //    - quantity <= MAX_ORDER_SIZE (100,000)
    //
    // 2. Price band check (for limit orders):
    //    - price within ±PRICE_BAND_PERCENT (10%) of last trade price
    //
    // 3. Trader ID validation:
    //    - traderId not empty (if STP enabled)
    //    - traderId is authenticated/authorized
    //
    // 4. Rate limiting:
    //    - Max orders per second per trader
    //
    // The OrderBook performs only safety-critical validation (duplicate ID,
    // positive quantity, valid price, etc.) to maximize throughput.
    // =========================================================================

    // Validation constants (for API layer use)
    constexpr Quantity API_MIN_ORDER_SIZE = 1;
    constexpr Quantity API_MAX_ORDER_SIZE = 100000;
    constexpr Price API_PRICE_BAND_PERCENT = 10;

    // Result of adding an order
    struct OrderResult {
        bool accepted = false;              // Was order accepted?
        std::string rejectReason;           // If rejected, why?
        std::vector<Trade> trades;          // Trades generated
        Quantity remainingQuantity = 0;     // Unfilled quantity
        STPResult stpResult; 

    };

    class OrderBook {
        public:
            OrderBook();
            void print() const;
            OrderResult addOrderToBook(Order order);  // Changed return type
            bool cancelOrder(OrderId id);

            // Price getters
            Price getBestBid() const { return bids.empty() ? 0 : bids.begin()->first; }
            Price getBestAsk() const { return asks.empty() ? 0 : asks.begin()->first; }
            Price getSpread() const { return getBestAsk() - getBestBid(); }
            Price getMidPrice() const {
                if (bids.empty() || asks.empty()) return 0;
                return (getBestBid() + getBestAsk()) / 2;
            }
            Price getLastTradePrice() const { return lastTradePrice; }
            Quantity getLastTradeQty() const { return lastTradeQty; }

            // Get current market snapshot
            PriceData getSnapshot() const {
                return {
                    now(),
                    getBestBid(),
                    getBestAsk(),
                    getMidPrice(),
                    getSpread(),
                    lastTradePrice,
                    lastTradeQty
                };
            }

        private:
            std::map<Price, PriceLevel, std::greater<Price>> bids;
            std::map<Price, PriceLevel> asks;
            std::unordered_map<OrderId, OrderLocation> orderIndex;
            Price lastTradePrice = 0;
            Quantity lastTradeQty = 0;

            std::vector<Trade> fillLimitOrder(Order& order);
            std::vector<Trade> fillMarketOrder(Order& order);

            void fillOrderAtPriceLevel(Order& order, PriceLevel& level, std::vector<Trade>& trades);
            Quantity getAvailableLiquidity(Side side, std::optional<Price> limitPrice);
            bool validateOrder(const Order& order, OrderResult& result);
            OrderResult handleMarketOrder(Order order);
            OrderResult handleLimitOrder(Order order); 

            // Selft-trade prevention
            STPResult checkSelfTrade(const Order& incomingOrder, const Order& restingOrder);
            void handleSelfTrade(Order& incomingOrder, Order& restingOrder, PriceLevel& level, STPResult& stpResult);
    };
}