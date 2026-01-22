#include <orderbook/Orderbook.hpp>
#include <iostream>
#include <vector>

using namespace std; 

namespace orderbook {
    OrderBook::OrderBook() {}

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

    // Returns total quantity available at acceptable prices
    Quantity OrderBook::getAvailableLiquidity(Side side, std::optional<Price> limitPrice) {
        Quantity total = 0;
        
        if (side == Side::BUY) {
            for (auto& [price, level] : asks) {
                if (limitPrice.has_value() && price > limitPrice.value()) break;
                total += level.totalQuantity;
            }
        } else {
            for (auto& [price, level] : bids) {
                if (limitPrice.has_value() && price < limitPrice.value()) break;
                total += level.totalQuantity;
            }
        }
        
        return total;
    }

    STPResult OrderBook::checkSelfTrade(const Order& incomingOrder, const Order& restingOrder) {
        STPResult result; 

        // Self-trade if: same trader, non-empty id, and STP mode is not allow
        if (!incomingOrder.traderId.empty() &&
            incomingOrder.traderId == restingOrder.traderId &&
            incomingOrder.stpMode != STPMode::ALLOW) {
                result.selfTrade = true; 
            }
        return result;
    }

    bool OrderBook::validateOrder(const Order& order, OrderResult& result) {
        // Safety-critical checks only (prevent crashes/corruption)
        // Business rules (size limits, price bands) should be validated at API layer

        // 1. Duplicate order ID - prevents index corruption
        if (orderIndex.find(order.id) != orderIndex.end()) {
            result.accepted = false;
            result.rejectReason = "Duplicate order ID";
            return false;
        }

        // 2. Quantity must be positive - prevents infinite loops
        if (order.quantity <= 0) {
            result.accepted = false;
            result.rejectReason = "Invalid quantity: must be positive";
            return false;
        }

        // 3. Limit order must have price - prevents .value() crash
        if (order.orderType == OrderType::LIMIT && !order.price.has_value()) {
            result.accepted = false;
            result.rejectReason = "Limit order requires price";
            return false;
        }

        // 4. Price must be positive - prevents logic errors
        if (order.price.has_value() && order.price.value() <= 0) {
            result.accepted = false;
            result.rejectReason = "Price must be positive";
            return false;
        }

        // 5. Market orders cannot be GTC - invalid combination
        if (order.orderType == OrderType::MARKET && order.timeInForce == TimeInForce::GTC) {
            result.accepted = false;
            result.rejectReason = "Invalid: MARKET orders cannot be GTC";
            return false;
        }

        // 6. FOK liquidity check - required for FOK semantics
        if (order.timeInForce == TimeInForce::FOK) {
            Quantity available = getAvailableLiquidity(order.side, order.price);
            if (available < order.quantity) {
                result.accepted = false;
                result.rejectReason = "FOK: insufficient liquidity for full fill";
                return false;
            }
        }

        return true;
    }

    // Match against all orders at that price level, FIFO
        void OrderBook::fillOrderAtPriceLevel(Order& incomingOrder, PriceLevel& level, std::vector<Trade>& trades) {
        bool isBuy = (incomingOrder.side == Side::BUY);
        auto it = level.orders.begin();
        
        while (incomingOrder.quantity > 0 && it != level.orders.end()) {
            Order& restingOrder = *it;
            
            // Check for self-trade
            STPResult stp = checkSelfTrade(incomingOrder, restingOrder);
            if (stp.selfTrade) {
                if (incomingOrder.stpMode == STPMode::DECREMENT_AND_CANCEL) {
                    ++it;
                    continue;
                }
                handleSelfTrade(incomingOrder, restingOrder, level, stp);
                if (incomingOrder.quantity == 0) return;  // CANCEL_NEWEST or CANCEL_BOTH
                // CANCEL_OLDEST or CANCEL_BOTH removed resting, get next iterator
                it = level.orders.erase(it);
                continue;
            }
            
            // Normal matching
            auto fillQty = std::min(incomingOrder.quantity, restingOrder.quantity); 

            Trade trade = {
                isBuy ? incomingOrder.id : restingOrder.id, 
                isBuy ? restingOrder.id : incomingOrder.id, 
                restingOrder.price.value(), 
                fillQty, 
                std::chrono::steady_clock::now()
            }; 

            trades.push_back(trade);
            lastTradePrice = restingOrder.price.value();
            lastTradeQty = fillQty;
            incomingOrder.quantity -= fillQty;
            restingOrder.quantity -= fillQty;
            level.totalQuantity -= fillQty;

            if (restingOrder.quantity == 0) {
                orderIndex.erase(restingOrder.id);
                it = level.orders.erase(it);
            } else {
                ++it;
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

    OrderResult OrderBook::handleMarketOrder(Order order) {
        OrderResult result;
        result.remainingQuantity = order.quantity;

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

        result.accepted = true;
        result.trades = fillMarketOrder(order);
        result.remainingQuantity = order.quantity;

        return result;
    }

    OrderResult OrderBook::handleLimitOrder(Order order) {
        OrderResult result;
        result.remainingQuantity = order.quantity;
        result.accepted = true;
        result.trades = fillLimitOrder(order);
        result.remainingQuantity = order.quantity;

        // Add to book if not fully filled and not IOC Order
        if (order.quantity > 0 && order.timeInForce == TimeInForce::GTC) {
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

    void OrderBook::handleSelfTrade(Order& incomingOrder, Order& restingOrder, PriceLevel& level, STPResult& stpResult) {
        switch (incomingOrder.stpMode) {
            case STPMode::CANCEL_NEWEST:
                incomingOrder.quantity = 0;
                stpResult.cancelledOrders.push_back(incomingOrder.id);
                stpResult.action = "STP Conflict : Cancel Newest - incoming order rejected";
                break;
            case STPMode::CANCEL_OLDEST:
                // Don't call cancelOrder - caller will erase from list
                orderIndex.erase(restingOrder.id);
                level.totalQuantity -= restingOrder.quantity;
                stpResult.cancelledOrders.push_back(restingOrder.id);
                stpResult.action = "STP Conflict : Cancel Oldest - resting order cancelled";
                break;
            case STPMode::CANCEL_BOTH:
                incomingOrder.quantity = 0;
                // Don't call cancelOrder - caller will erase from list
                orderIndex.erase(restingOrder.id);
                level.totalQuantity -= restingOrder.quantity;
                stpResult.cancelledOrders.push_back(incomingOrder.id);
                stpResult.cancelledOrders.push_back(restingOrder.id);
                stpResult.action = "STP Conflict : Cancel Both - both orders cancelled";
                break;
            case STPMode::DECREMENT_AND_CANCEL:
            case STPMode::ALLOW:
                break;
        }
    }

    OrderResult OrderBook::addOrderToBook(Order order) {
        OrderResult result;
        result.remainingQuantity = order.quantity;

        if (!validateOrder(order, result)) return result;

        if (order.orderType == OrderType::MARKET) {
            return handleMarketOrder(order);
        } else {
            return handleLimitOrder(order);
        }
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