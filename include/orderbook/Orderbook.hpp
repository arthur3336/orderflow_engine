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

namespace orderbook {

    // Result of adding an order
    struct OrderResult {
        bool accepted = false;              // Was order accepted?
        std::string rejectReason;           // If rejected, why?
        std::vector<Trade> trades;          // Trades generated
        Quantity remainingQuantity = 0;     // Unfilled quantity
    };

    class OrderBook {
        public:
            OrderBook();
            void print() const;
            OrderResult addOrder(Order order);  // Changed return type
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
    };
}