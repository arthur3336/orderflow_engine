#include <orderbook/Orderbook.hpp>
#include <iostream>
#include <vector>

using namespace std; 

namespace orderbook {
    OrderBook::OrderBook() {
        // empty for now
    }

    void OrderBook::print() const {
        Price spread = 0; 
        if (!asks.empty() && !bids.empty()) {
            spread = asks.begin()->first - bids.begin()->first;
        } 
        cout << string(10, '=') << " ORDER BOOK " << string(10, '=') << endl; 
        cout << "ASKS :" << endl; 
        
        for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
            Price price = it->first;
            const PriceLevel& level = it->second;
            cout << "  $" << priceToString(price) << " | " << level.totalQuantity << " shares" << endl;
        }

        cout << string(10, '-') << " SPREAD: " << priceToString(spread) << " " << string(10, '-') << endl;

        cout << "BIDS :" << endl;

        for (auto it = bids.begin(); it != bids.end(); ++it) {
            Price price = it-> first;
            const PriceLevel& level = it->second;
            cout << "  $" << priceToString(price) << " | " << level.totalQuantity << " shares" << endl;
        }

        cout << string(32, '=') << endl; 

    }

    // Match against all orders at that price level, FIFO
    void OrderBook::fillOrderAtPriceLevel(Order& order, PriceLevel& level, std::vector<Trade>& trades) {
        bool isBuy = (order.side == Side::BUY);
        while (order.quantity > 0 && !level.orders.empty()) {
            Order& front = level.orders.front(); 
            auto fillQty = min(order.quantity, front. quantity); 

            Trade trade = {
                isBuy? order.id : front.id, 
                isBuy? front.id : order.id, 
                front.price.value(), 
                fillQty, 
                std::chrono::steady_clock::now()
            }; 

            trades.push_back(trade);
            lastTradePrice = front.price.value();
            lastTradeQty = fillQty;
            order.quantity -= fillQty;
            front.quantity -= fillQty;
            level.totalQuantity -= fillQty;

            if (front.quantity == 0) {
                orderIndex.erase(front.id);
                level.orders.pop_front();
            }
        }
    }


    std::vector<Trade> OrderBook::fillLimitOrder(Order& order) {
        std::vector<Trade> trades; 

        auto matchBook = [&](auto& book, auto priceCheckFn) {
            while (order.quantity > 0 && !book.empty()) {
                auto it = book.begin();
                if (priceCheckFn(it->first, order.price.value())) break;  // Use the lambda!

                PriceLevel& level = it->second;
                fillOrderAtPriceLevel(order, level, trades);
                if (level.orders.empty()) book.erase(it);
            }
        };

        if (order.side == Side::BUY) {
            matchBook(asks, [](Price best, Price limit) { return best > limit; });
        } else {
            matchBook(bids, [](Price best, Price limit) { return best < limit; });
        }

        return trades; 
    }

    std::vector<Trade> OrderBook::fillMarketOrder(Order& order) {
        std::vector<Trade> trades; 

        auto matchBook = [&](auto& book) {
            while (order.quantity > 0 && !book.empty()) {
                auto it = book.begin();
                PriceLevel& level = it->second;
                fillOrderAtPriceLevel(order, level, trades);
                if (level.orders.empty()) book.erase(it);
            }
        };

        if (order.side == Side::BUY) {
            matchBook(asks);
        } else {
            matchBook(bids);
        }

        return trades; 
    }

    OrderResult OrderBook::addOrder(Order order) {
        OrderResult result;
        result.remainingQuantity = order.quantity;

        if (order.quantity <= 0) {
            result.accepted = false;
            result.rejectReason = "Invalid quantity: must be positive";
            return result;
        }

        if (order.orderType == OrderType::MARKET) {
            if (order.side == Side::BUY && asks.empty()) {
                result.accepted = false;
                result.rejectReason = "No liquidity: ask side empty";
                return result;
            }
            if (order.side == Side::SELL && bids.empty()) {
                result.accepted = false;
                result.rejectReason = "No liquidity: bid side empty";
                return result;
            }
            
            // Match market order
            result.accepted = true;
            result.trades = fillMarketOrder(order);
            result.remainingQuantity = order.quantity;

            return result;
        }

        // Step 3: Handle LIMIT orders
        // Validate price exists
        if (!order.price.has_value()) {
            result.accepted = false;
            result.rejectReason = "Limit order requires price";
            return result;
        }

        // Validate price is positive
        if (order.price.value() <= 0) {
            result.accepted = false;
            result.rejectReason = "Invalid price: must be positive";
            return result;
        }

        // Match limit order
        result.accepted = true;
        result.trades = fillLimitOrder(order);
        result.remainingQuantity = order.quantity;

        // Add to book if not fully filled
        if (order.quantity > 0) {
            auto addToBook = [&](auto& book) {
                PriceLevel& level = book[order.price.value()];
                level.orders.push_back(order); 
                auto position = std::prev(level.orders.end()); 
                orderIndex[order.id] = OrderLocation{order.side, order.price.value(), position};
                level.totalQuantity += order.quantity; 
            };

            if (order.side == Side::BUY) {
                addToBook(bids); 
            } else {
                addToBook(asks);
            }
        }
        
        return result; 
    } 

    bool OrderBook::cancelOrder(OrderId id) {
        auto it = orderIndex.find(id); 
        if (it == orderIndex.end()) return false; 
        auto& loc = it->second; 

        auto removeFromBook = [&](auto& book) { 
            PriceLevel& level = book[loc.price]; 
            level.totalQuantity -= loc.position->quantity; 
            level.orders.erase(loc.position);
            if (level.orders.empty()) book.erase(loc.price); 
        };

        if (loc.side == Side::BUY) {
            removeFromBook(bids);
        } else {
            removeFromBook(asks); 
        }
        orderIndex.erase(id); 
        return true; 
    }


}