use std::os::raw::c_void;
use super::types::*;

extern "C" {
    // Lifecycle
    pub fn ob_orderbook_create() -> *mut c_void;
    pub fn ob_orderbook_destroy(book: *mut c_void);

    // Order operations
    pub fn ob_orderbook_add_order(
        book: *mut c_void,
        order: *const ObOrderT,
    ) -> *mut ObOrderResultT;

    pub fn ob_orderbook_cancel_order(book: *mut c_void, id: u64) -> bool;

    pub fn ob_orderbook_modify_order(
        book: *mut c_void,
        id: u64,
        new_price: i64,
        new_quantity: i64,
    ) -> *mut ObModifyResultT;

    // Market data queries
    pub fn ob_orderbook_get_snapshot(book: *const c_void) -> ObPriceDataT;
    pub fn ob_orderbook_get_best_bid(book: *const c_void) -> i64;
    pub fn ob_orderbook_get_best_ask(book: *const c_void) -> i64;
    pub fn ob_orderbook_get_spread(book: *const c_void) -> i64;
    pub fn ob_orderbook_get_mid_price(book: *const c_void) -> i64;
    pub fn ob_orderbook_get_last_trade_price(book: *const c_void) -> i64;
    pub fn ob_orderbook_get_last_trade_qty(book: *const c_void) -> i64;

    // Memory cleanup
    pub fn ob_free_order_result(result: *mut ObOrderResultT);
    pub fn ob_free_modify_result(result: *mut ObModifyResultT);
}
