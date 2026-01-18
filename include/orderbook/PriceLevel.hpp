#pragma once

#include <list>
#include <orderbook/Order.hpp>

namespace orderbook {
    struct PriceLevel {
        Quantity totalQuantity;
        std::list<Order> orders;
    };
}
