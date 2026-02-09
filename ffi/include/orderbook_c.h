#ifndef ORDERBOOK_C_H
#define ORDERBOOK_C_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ======================================================================
   Type Aliases (mirror C++ types)
   ====================================================================== */
typedef uint64_t ob_order_id_t;
typedef uint64_t ob_trade_id_t;
typedef int64_t  ob_price_t;
typedef int64_t  ob_quantity_t;

/* ======================================================================
   Enums (mirror C++ enum class values)
   ====================================================================== */
typedef enum {
    OB_SIDE_BUY  = 0,
    OB_SIDE_SELL = 1
} ob_side_t;

typedef enum {
    OB_ORDER_TYPE_LIMIT  = 0,
    OB_ORDER_TYPE_MARKET = 1
} ob_order_type_t;

typedef enum {
    OB_TIF_GTC = 0,
    OB_TIF_IOC = 1,
    OB_TIF_FOK = 2
} ob_time_in_force_t;

typedef enum {
    OB_STP_ALLOW               = 0,
    OB_STP_CANCEL_NEWEST       = 1,
    OB_STP_CANCEL_OLDEST       = 2,
    OB_STP_CANCEL_BOTH         = 3,
    OB_STP_DECREMENT_AND_CANCEL = 4
} ob_stp_mode_t;

/* ======================================================================
   Opaque handle
   ====================================================================== */
typedef struct ob_orderbook_t ob_orderbook_t;

/* ======================================================================
   Input struct (caller owns)
   ====================================================================== */
typedef struct {
    const char*         trader_id;
    ob_order_id_t       id;
    ob_price_t          price;          /* 0 for market orders */
    ob_quantity_t       quantity;
    ob_side_t           side;
    ob_order_type_t     order_type;
    ob_time_in_force_t  time_in_force;
    ob_stp_mode_t       stp_mode;
    bool                has_price;      /* false for market orders */
} ob_order_t;

/* ======================================================================
   Output structs (library allocates, caller frees via ob_free_*)
   ====================================================================== */
typedef struct {
    ob_trade_id_t   trade_id;
    ob_order_id_t   buy_order_id;
    ob_order_id_t   sell_order_id;
    ob_price_t      price;
    ob_quantity_t   quantity;
    int64_t         timestamp_ns;
} ob_trade_t;

typedef struct {
    bool                self_trade;
    ob_order_id_t*      cancelled_orders;       /* heap-allocated array, may be NULL */
    size_t              cancelled_orders_len;
    char*               action;                 /* heap-allocated string, may be NULL */
} ob_stp_result_t;

typedef struct {
    bool                accepted;
    char*               reject_reason;          /* heap-allocated, NULL if accepted */
    ob_trade_t*         trades;                 /* heap-allocated array, may be NULL */
    size_t              trades_len;
    ob_quantity_t       remaining_quantity;
    ob_stp_result_t     stp_result;
} ob_order_result_t;

typedef struct {
    bool                accepted;
    char*               reject_reason;          /* heap-allocated, NULL if accepted */
    ob_price_t          old_price;
    ob_price_t          new_price;
    ob_quantity_t       old_quantity;
    ob_quantity_t       new_quantity;
} ob_modify_result_t;

typedef struct {
    int64_t         timestamp_ns;
    ob_price_t      bid_price;
    ob_price_t      ask_price;
    ob_price_t      mid_price;
    ob_price_t      spread;
    ob_price_t      last_trade_price;
    ob_quantity_t   last_trade_qty;
} ob_price_data_t;

/* ======================================================================
   Lifecycle
   ====================================================================== */
ob_orderbook_t*     ob_orderbook_create(void);
void                ob_orderbook_destroy(ob_orderbook_t* book);

/* ======================================================================
   Order operations
   ====================================================================== */
ob_order_result_t*  ob_orderbook_add_order(ob_orderbook_t* book, const ob_order_t* order);
bool                ob_orderbook_cancel_order(ob_orderbook_t* book, ob_order_id_t id);
ob_modify_result_t* ob_orderbook_modify_order(ob_orderbook_t* book,
                                               ob_order_id_t id,
                                               ob_price_t new_price,
                                               ob_quantity_t new_quantity);

/* ======================================================================
   Market data queries (returned by value, no heap allocation)
   ====================================================================== */
ob_price_data_t     ob_orderbook_get_snapshot(const ob_orderbook_t* book);
ob_price_t          ob_orderbook_get_best_bid(const ob_orderbook_t* book);
ob_price_t          ob_orderbook_get_best_ask(const ob_orderbook_t* book);
ob_price_t          ob_orderbook_get_spread(const ob_orderbook_t* book);
ob_price_t          ob_orderbook_get_mid_price(const ob_orderbook_t* book);
ob_price_t          ob_orderbook_get_last_trade_price(const ob_orderbook_t* book);
ob_quantity_t       ob_orderbook_get_last_trade_qty(const ob_orderbook_t* book);

/* ======================================================================
   Memory cleanup
   ====================================================================== */
void                ob_free_order_result(ob_order_result_t* result);
void                ob_free_modify_result(ob_modify_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* ORDERBOOK_C_H */
