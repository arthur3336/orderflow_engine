#pragma once

#include <orderbook/Types.hpp>
#include <orderbook/OrderType.hpp>
#include <list>
#include <optional>
#include <orderbook/SelfTradePrevent.hpp>

namespace orderbook {
    struct Order {
        std::string traderId; 
        OrderId id;
        std::optional<Price> price;  // None for market orders, Some(x) for limits
        Quantity quantity;
        Side side;
        OrderType orderType = OrderType::LIMIT;
        TimeInForce timeInForce = TimeInForce::GTC;
        Timestamp timestamp;
        STPMode stpMode = STPMode::ALLOW; 
        

        Order() = default;

    private:
        Order(OrderId id, std::optional<Price> price, Quantity quantity, Side side,
              OrderType type, TimeInForce tif, std::string traderID, STPMode stp)
            : traderId(std::move(traderID)), id(id), price(price), quantity(quantity), side(side),
              orderType(type), timeInForce(tif), timestamp(now()), stpMode(stp) {}

    public:
        static Order Limit(OrderId id, Price price, Quantity quantity, Side side,
            std::string traderID, STPMode stp, TimeInForce tif = TimeInForce::GTC) {
                return Order(id, price, quantity, side, OrderType::LIMIT, tif, std::move(traderID), stp);
            }

        static Order Market(OrderId id, Quantity quantity, Side side,
            std::string traderID, STPMode stp, TimeInForce tif = TimeInForce::IOC) {
                return Order(id, std::nullopt, quantity, side, OrderType::MARKET, tif, std::move(traderID), stp);
            }
    };

    struct OrderLocation {
        Side side;
        Price price;
        std::list<Order>::iterator position;
    };
}