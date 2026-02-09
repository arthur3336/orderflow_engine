#include "orderbook_c.h"
#include <orderbook/Orderbook.hpp>
#include <cstring>
#include <cstdlib>
#include <chrono>

using namespace orderbook;

// Cast helpers
static OrderBook* as_book(ob_orderbook_t* handle) {
    return reinterpret_cast<OrderBook*>(handle);
}

static const OrderBook* as_book(const ob_orderbook_t* handle) {
    return reinterpret_cast<const OrderBook*>(handle);
}

// Convert C++ steady_clock timepoint to nanoseconds since boot
static int64_t to_nanos(Timestamp ts) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        ts.time_since_epoch()
    ).count();
}

// Convert C enum to C++ enum class
static Side to_side(ob_side_t s) {
    return s == OB_SIDE_BUY ? Side::BUY : Side::SELL;
}

static TimeInForce to_tif(ob_time_in_force_t t) {
    switch (t) {
        case OB_TIF_IOC: return TimeInForce::IOC;
        case OB_TIF_FOK: return TimeInForce::FOK;
        default:         return TimeInForce::GTC;
    }
}

static STPMode to_stp_mode(ob_stp_mode_t m) {
    switch (m) {
        case OB_STP_CANCEL_NEWEST:        return STPMode::CANCEL_NEWEST;
        case OB_STP_CANCEL_OLDEST:        return STPMode::CANCEL_OLDEST;
        case OB_STP_CANCEL_BOTH:          return STPMode::CANCEL_BOTH;
        case OB_STP_DECREMENT_AND_CANCEL: return STPMode::DECREMENT_AND_CANCEL;
        default:                          return STPMode::ALLOW;
    }
}

// Heap-allocate a copy of a string (caller must free). Returns NULL for empty strings.
static char* dup_string(const std::string& s) {
    if (s.empty()) return nullptr;
    return strdup(s.c_str());
}

// ======================================================================
// Lifecycle
// ======================================================================

ob_orderbook_t* ob_orderbook_create(void) {
    auto* book = new (std::nothrow) OrderBook();
    return reinterpret_cast<ob_orderbook_t*>(book);
}

void ob_orderbook_destroy(ob_orderbook_t* handle) {
    delete as_book(handle);
}

// ======================================================================
// Order operations
// ======================================================================

ob_order_result_t* ob_orderbook_add_order(ob_orderbook_t* handle, const ob_order_t* c_order) {
    OrderBook* book = as_book(handle);

    // Build C++ Order from C struct
    Order order;
    if (c_order->order_type == OB_ORDER_TYPE_MARKET) {
        order = Order::Market(
            c_order->id,
            c_order->quantity,
            to_side(c_order->side),
            c_order->trader_id ? c_order->trader_id : "",
            to_stp_mode(c_order->stp_mode),
            to_tif(c_order->time_in_force)
        );
    } else {
        order = Order::Limit(
            c_order->id,
            c_order->price,
            c_order->quantity,
            to_side(c_order->side),
            c_order->trader_id ? c_order->trader_id : "",
            to_stp_mode(c_order->stp_mode),
            to_tif(c_order->time_in_force)
        );
    }

    OrderResult cpp_result = book->addOrderToBook(order);

    // Allocate C result
    auto* result = static_cast<ob_order_result_t*>(malloc(sizeof(ob_order_result_t)));
    if (!result) return nullptr;

    result->accepted = cpp_result.accepted;
    result->reject_reason = dup_string(cpp_result.rejectReason);
    result->remaining_quantity = cpp_result.remainingQuantity;

    // Convert trades vector
    result->trades_len = cpp_result.trades.size();
    if (result->trades_len > 0) {
        result->trades = static_cast<ob_trade_t*>(malloc(sizeof(ob_trade_t) * result->trades_len));
        for (size_t i = 0; i < result->trades_len; i++) {
            const auto& t = cpp_result.trades[i];
            result->trades[i] = {
                t.tradeId,
                t.buyOrderId,
                t.sellOrderId,
                t.price,
                t.quantity,
                to_nanos(t.time)
            };
        }
    } else {
        result->trades = nullptr;
    }

    // Convert STP result
    const auto& stp = cpp_result.stpResult;
    result->stp_result.self_trade = stp.selfTrade;
    result->stp_result.action = dup_string(stp.action);
    result->stp_result.cancelled_orders_len = stp.cancelledOrders.size();
    if (stp.cancelledOrders.empty()) {
        result->stp_result.cancelled_orders = nullptr;
    } else {
        result->stp_result.cancelled_orders = static_cast<ob_order_id_t*>(
            malloc(sizeof(ob_order_id_t) * stp.cancelledOrders.size())
        );
        for (size_t i = 0; i < stp.cancelledOrders.size(); i++) {
            result->stp_result.cancelled_orders[i] = stp.cancelledOrders[i];
        }
    }

    return result;
}

bool ob_orderbook_cancel_order(ob_orderbook_t* handle, ob_order_id_t id) {
    return as_book(handle)->cancelOrder(id);
}

ob_modify_result_t* ob_orderbook_modify_order(ob_orderbook_t* handle,
                                               ob_order_id_t id,
                                               ob_price_t new_price,
                                               ob_quantity_t new_quantity) {
    ModifyResult cpp_result = as_book(handle)->modifyOrder(id, new_price, new_quantity);

    auto* result = static_cast<ob_modify_result_t*>(malloc(sizeof(ob_modify_result_t)));
    if (!result) return nullptr;

    result->accepted = cpp_result.accepted;
    result->reject_reason = dup_string(cpp_result.rejectReason);
    result->old_price = cpp_result.oldPrice;
    result->new_price = cpp_result.newPrice;
    result->old_quantity = cpp_result.oldQuantity;
    result->new_quantity = cpp_result.newQuantity;

    return result;
}

// ======================================================================
// Market data queries
// ======================================================================

ob_price_data_t ob_orderbook_get_snapshot(const ob_orderbook_t* handle) {
    PriceData snap = as_book(handle)->getSnapshot();
    return {
        to_nanos(snap.time),
        snap.bidPrice,
        snap.askPrice,
        snap.midPrice,
        snap.spread,
        snap.lastTradePrice,
        snap.lastTradeQty
    };
}

ob_price_t ob_orderbook_get_best_bid(const ob_orderbook_t* handle) {
    return as_book(handle)->getBestBid();
}

ob_price_t ob_orderbook_get_best_ask(const ob_orderbook_t* handle) {
    return as_book(handle)->getBestAsk();
}

ob_price_t ob_orderbook_get_spread(const ob_orderbook_t* handle) {
    return as_book(handle)->getSpread();
}

ob_price_t ob_orderbook_get_mid_price(const ob_orderbook_t* handle) {
    return as_book(handle)->getMidPrice();
}

ob_price_t ob_orderbook_get_last_trade_price(const ob_orderbook_t* handle) {
    return as_book(handle)->getLastTradePrice();
}

ob_quantity_t ob_orderbook_get_last_trade_qty(const ob_orderbook_t* handle) {
    return as_book(handle)->getLastTradeQty();
}

// ======================================================================
// Memory cleanup
// ======================================================================

void ob_free_order_result(ob_order_result_t* result) {
    if (!result) return;
    free(result->reject_reason);
    free(result->trades);
    free(result->stp_result.cancelled_orders);
    free(result->stp_result.action);
    free(result);
}

void ob_free_modify_result(ob_modify_result_t* result) {
    if (!result) return;
    free(result->reject_reason);
    free(result);
}
