#pragma once

#include <orderbook/Types.hpp>
#include <orderbook/OrderType.hpp>
#include <list>
#include <optional>

namespace orderbook {
    struct Order {
        OrderId id;
        std::optional<Price> price;  // None for market orders, Some(x) for limits
        Quantity quantity;
        Side side;
        OrderType orderType = OrderType::LIMIT;
        TimeInForce timeInForce = TimeInForce::GTC;
        Timestamp timestamp;

        Order() = default;

    private:
        Order(OrderId id, std::optional<Price> price, Quantity quantity, Side side,
              OrderType type, TimeInForce tif)
            : id(id), price(price), quantity(quantity), side(side),
              orderType(type), timeInForce(tif), timestamp(now()) {}

    public:
        // LIMIT order - requires price
        static Order Limit(OrderId id, Price price, Quantity quantity,
            Side side, TimeInForce tif = TimeInForce::GTC) {
                return Order(id, price, quantity, side, OrderType::LIMIT, tif);
            }

        // MARKET order - no price needed
        static Order Market(OrderId id, Quantity quantity, Side side,
            TimeInForce tif = TimeInForce::GTC) {
                return Order(id, std::nullopt, quantity, side, OrderType::MARKET, tif);
            }
    };

    struct OrderLocation {
        Side side;
        Price price;
        std::list<Order>::iterator position;
    };
}