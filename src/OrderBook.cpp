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

    std::vector<Trade> OrderBook::matchOrder(Order& order) {
        std::vector<Trade> trades; 
  
        if (order.side == Side::BUY) {
            while (order.quantity > 0 && !asks.empty()) {
                auto it = asks.begin();
                if (it->first > order.price) break; 
                PriceLevel& level = it->second; 

                // Match against all orders at that price level, FIFO
                while (order.quantity > 0 && !level.orders.empty()) {
                    Order& front = level.orders.front(); 
                    auto fillQty = min(order.quantity, front. quantity); 
                    Trade trade = {
                        order.id, 
                        front.id, 
                        front.price, 
                        fillQty, 
                        std::chrono::steady_clock::now()
                    }; 
                    trades.push_back(trade);
                    lastTradePrice = front.price;
                    lastTradeQty = fillQty;
                    order.quantity -= fillQty;
                    front.quantity -= fillQty;
                    level.totalQuantity -= fillQty;

                    if (front.quantity == 0) {
                        orderIndex.erase(front.id);
                        level.orders.pop_front();
                    }
                }
                if (level.orders.empty()) asks.erase(it);
            }
        } else {
            while (order.quantity > 0 && !bids.empty()) {
                auto it = bids.begin();
                if (it->first < order.price) break;
                PriceLevel& level = it->second;

                while (order.quantity > 0 && !level.orders.empty()) {
                    Order& front = level.orders.front();
                    auto fillQty = min(order.quantity, front.quantity);
                    Trade trade = {
                        front.id,    // buyer is the resting order
                        order.id,    // seller is the incoming order
                        front.price,
                        fillQty,
                        std::chrono::steady_clock::now()
                    };
                    trades.push_back(trade);
                    lastTradePrice = front.price;
                    lastTradeQty = fillQty;
                    order.quantity -= fillQty;
                    front.quantity -= fillQty;
                    level.totalQuantity -= fillQty;

                    if (front.quantity == 0) {
                        orderIndex.erase(front.id);
                        level.orders.pop_front();
                    }
                }
                if (level.orders.empty()) bids.erase(it);
            }
        }

        return trades; 
    }

/*
    bool OrderBook::addOrder(Order order) {
        if (order.side == Side::BUY) {
            PriceLevel& level = bids[order.price]; 
            level.orders.push_back(order);
            auto position = std::prev(level.orders.end());
            orderIndex[order.id] = OrderLocation{order.side, order.price, position};
            level.totalQuantity += order.quantity;
            return true; 
        } else {
            PriceLevel& level = asks[order.price];
            level.orders.push_back(order); 
            auto position = std::prev(level.orders.end());
            orderIndex[order.id] = OrderLocation{order.side, order.price, position};
            level.totalQuantity += order.quantity;
            return true; 
        }
    }
*/

    std::vector<Trade> OrderBook::addOrder(Order order) {
        // 1° Try to match the incoming order against opp book
        std::vector<Trade> trades = matchOrder(order); 

        // 2° If remaining quantity, add to book
        if (order.quantity > 0) {
            auto addToBook = [&](auto& book) {
                PriceLevel& level = book[order.price];
                level.orders.push_back(order); 
                auto position = std::prev(level.orders.end()); 
                orderIndex[order.id] = OrderLocation{order.side, order.price, position};
                level.totalQuantity += order.quantity; 
            };

            if (order.side == Side::BUY) {
                addToBook(bids); 
            } else {
                addToBook(asks);
            }
        }
        
        return trades; 
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