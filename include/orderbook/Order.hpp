#pragma once

#include <orderbook/Types.hpp>
#include <list>

namespace orderbook {
    struct Order {
        OrderId id; 
        Price price; 
        Quantity quantity; 
        Side side;
        Timestamp timestamp; 
    };

    struct OrderLocation {
        Side side;
        Price price;
        std::list<Order>::iterator position;
    };
}