#pragma once

#include <orderbook/Types.hpp>
#include <orderbook/Order.hpp>
#include <map>
#include <orderbook/PriceLevel.hpp>
#include <functional>
#include <unordered_map>
#include <orderbook/PriceHistory.hpp>

namespace orderbook {
    class OrderBook {
        public:
            OrderBook();
            void print() const;
            std::vector<Trade> addOrder(Order order);
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
            // Bids sorted highest price first
            std::map<Price, PriceLevel, std::greater<Price>> bids;

            // Asks sprted lowest first
            std::map<Price, PriceLevel> asks;

            std::unordered_map<OrderId, OrderLocation> orderIndex;

            Price lastTradePrice = 0;
            Quantity lastTradeQty = 0;

            std::vector<Trade> matchOrder(Order& order);
    };
}