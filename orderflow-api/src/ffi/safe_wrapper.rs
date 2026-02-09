use std::ffi::{CStr, CString};
use std::os::raw::c_void;
use std::slice;

use super::bindings;
use super::types::*;

// ======================================================================
// Safe Rust types (owned, no raw pointers)
// ======================================================================

#[derive(Debug, Clone)]
pub struct Trade {
    pub trade_id: u64,
    pub buy_order_id: u64,
    pub sell_order_id: u64,
    pub price: i64,
    pub quantity: i64,
    pub timestamp_ns: i64,
}

#[derive(Debug, Clone)]
pub struct StpResult {
    pub self_trade: bool,
    pub cancelled_orders: Vec<u64>,
    pub action: Option<String>,
}

#[derive(Debug, Clone)]
pub struct OrderResult {
    pub accepted: bool,
    pub reject_reason: Option<String>,
    pub trades: Vec<Trade>,
    pub remaining_quantity: i64,
    pub stp_result: StpResult,
}

#[derive(Debug, Clone)]
pub struct ModifyResult {
    pub accepted: bool,
    pub reject_reason: Option<String>,
    pub old_price: i64,
    pub new_price: i64,
    pub old_quantity: i64,
    pub new_quantity: i64,
}

#[derive(Debug, Clone, Copy)]
pub struct PriceData {
    pub timestamp_ns: i64,
    pub bid_price: i64,
    pub ask_price: i64,
    pub mid_price: i64,
    pub spread: i64,
    pub last_trade_price: i64,
    pub last_trade_qty: i64,
}

// ======================================================================
// Conversion helpers
// ======================================================================

unsafe fn ptr_to_option_string(ptr: *const std::os::raw::c_char) -> Option<String> {
    if ptr.is_null() {
        None
    } else {
        Some(CStr::from_ptr(ptr).to_string_lossy().into_owned())
    }
}

fn convert_order_result(raw: *mut ObOrderResultT) -> OrderResult {
    unsafe {
        let r = &*raw;

        let trades: Vec<Trade> = if r.trades.is_null() || r.trades_len == 0 {
            Vec::new()
        } else {
            slice::from_raw_parts(r.trades, r.trades_len)
                .iter()
                .map(|t| Trade {
                    trade_id: t.trade_id,
                    buy_order_id: t.buy_order_id,
                    sell_order_id: t.sell_order_id,
                    price: t.price,
                    quantity: t.quantity,
                    timestamp_ns: t.timestamp_ns,
                })
                .collect()
        };

        let cancelled: Vec<u64> = if r.stp_result.cancelled_orders.is_null()
            || r.stp_result.cancelled_orders_len == 0
        {
            Vec::new()
        } else {
            slice::from_raw_parts(
                r.stp_result.cancelled_orders,
                r.stp_result.cancelled_orders_len,
            )
            .to_vec()
        };

        OrderResult {
            accepted: r.accepted,
            reject_reason: ptr_to_option_string(r.reject_reason),
            trades,
            remaining_quantity: r.remaining_quantity,
            stp_result: StpResult {
                self_trade: r.stp_result.self_trade,
                cancelled_orders: cancelled,
                action: ptr_to_option_string(r.stp_result.action),
            },
        }
    }
}

fn convert_modify_result(raw: *mut ObModifyResultT) -> ModifyResult {
    unsafe {
        let r = &*raw;
        ModifyResult {
            accepted: r.accepted,
            reject_reason: ptr_to_option_string(r.reject_reason),
            old_price: r.old_price,
            new_price: r.new_price,
            old_quantity: r.old_quantity,
            new_quantity: r.new_quantity,
        }
    }
}

// ======================================================================
// OwnedOrderBook — safe RAII wrapper
// ======================================================================

pub struct OwnedOrderBook {
    ptr: *mut c_void,
}

// SAFETY: The C++ OrderBook is single-threaded. We enforce exclusive access
// through RwLock at the engine layer. The pointer can safely move between threads.
// Sync is safe because all access goes through RwLock — the raw pointer is never
// exposed, and &self methods on the C++ side are const/read-only.
unsafe impl Send for OwnedOrderBook {}
unsafe impl Sync for OwnedOrderBook {}

impl OwnedOrderBook {
    pub fn new() -> Self {
        let ptr = unsafe { bindings::ob_orderbook_create() };
        assert!(!ptr.is_null(), "Failed to create OrderBook");
        Self { ptr }
    }

    pub fn add_order(
        &mut self,
        trader_id: &str,
        id: u64,
        price: Option<i64>,
        quantity: i64,
        side: u32,
        order_type: u32,
        time_in_force: u32,
        stp_mode: u32,
    ) -> OrderResult {
        let c_trader_id = CString::new(trader_id).unwrap_or_default();

        let c_order = ObOrderT {
            trader_id: c_trader_id.as_ptr(),
            id,
            price: price.unwrap_or(0),
            quantity,
            side,
            order_type,
            time_in_force,
            stp_mode,
            has_price: price.is_some(),
        };

        let raw = unsafe { bindings::ob_orderbook_add_order(self.ptr, &c_order) };
        assert!(!raw.is_null(), "ob_orderbook_add_order returned NULL");

        let result = convert_order_result(raw);
        unsafe { bindings::ob_free_order_result(raw) };
        result
    }

    pub fn cancel_order(&mut self, id: u64) -> bool {
        unsafe { bindings::ob_orderbook_cancel_order(self.ptr, id) }
    }

    pub fn modify_order(&mut self, id: u64, new_price: i64, new_quantity: i64) -> ModifyResult {
        let raw =
            unsafe { bindings::ob_orderbook_modify_order(self.ptr, id, new_price, new_quantity) };
        assert!(!raw.is_null(), "ob_orderbook_modify_order returned NULL");

        let result = convert_modify_result(raw);
        unsafe { bindings::ob_free_modify_result(raw) };
        result
    }

    pub fn get_snapshot(&self) -> PriceData {
        let raw = unsafe { bindings::ob_orderbook_get_snapshot(self.ptr as *const _) };
        PriceData {
            timestamp_ns: raw.timestamp_ns,
            bid_price: raw.bid_price,
            ask_price: raw.ask_price,
            mid_price: raw.mid_price,
            spread: raw.spread,
            last_trade_price: raw.last_trade_price,
            last_trade_qty: raw.last_trade_qty,
        }
    }

    pub fn get_best_bid(&self) -> i64 {
        unsafe { bindings::ob_orderbook_get_best_bid(self.ptr as *const _) }
    }

    pub fn get_best_ask(&self) -> i64 {
        unsafe { bindings::ob_orderbook_get_best_ask(self.ptr as *const _) }
    }

    pub fn get_spread(&self) -> i64 {
        unsafe { bindings::ob_orderbook_get_spread(self.ptr as *const _) }
    }

    pub fn get_mid_price(&self) -> i64 {
        unsafe { bindings::ob_orderbook_get_mid_price(self.ptr as *const _) }
    }

    pub fn get_last_trade_price(&self) -> i64 {
        unsafe { bindings::ob_orderbook_get_last_trade_price(self.ptr as *const _) }
    }

    pub fn get_last_trade_qty(&self) -> i64 {
        unsafe { bindings::ob_orderbook_get_last_trade_qty(self.ptr as *const _) }
    }
}

impl Drop for OwnedOrderBook {
    fn drop(&mut self) {
        unsafe {
            bindings::ob_orderbook_destroy(self.ptr);
        }
    }
}

// ======================================================================
// Tests
// ======================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_create_destroy() {
        let _book = OwnedOrderBook::new();
        // Drop cleans up
    }

    #[test]
    fn test_empty_book_snapshot() {
        let book = OwnedOrderBook::new();
        let snap = book.get_snapshot();
        assert_eq!(snap.bid_price, 0);
        assert_eq!(snap.ask_price, 0);
        assert_eq!(snap.spread, 0);
        assert_eq!(snap.mid_price, 0);
    }

    #[test]
    fn test_add_limit_order() {
        let mut book = OwnedOrderBook::new();
        let result = book.add_order("traderA", 1, Some(10050), 100, OB_SIDE_BUY, OB_ORDER_TYPE_LIMIT, OB_TIF_GTC, OB_STP_ALLOW);
        assert!(result.accepted);
        assert_eq!(result.trades.len(), 0);
        assert_eq!(result.remaining_quantity, 100);
        assert_eq!(book.get_best_bid(), 10050);
    }

    #[test]
    fn test_matching_trade() {
        let mut book = OwnedOrderBook::new();

        // Resting sell
        let r1 = book.add_order("seller", 1, Some(10050), 50, OB_SIDE_SELL, OB_ORDER_TYPE_LIMIT, OB_TIF_GTC, OB_STP_ALLOW);
        assert!(r1.accepted);
        assert_eq!(r1.trades.len(), 0);

        // Crossing buy
        let r2 = book.add_order("buyer", 2, Some(10050), 30, OB_SIDE_BUY, OB_ORDER_TYPE_LIMIT, OB_TIF_GTC, OB_STP_ALLOW);
        assert!(r2.accepted);
        assert_eq!(r2.trades.len(), 1);
        assert_eq!(r2.trades[0].quantity, 30);
        assert_eq!(r2.trades[0].price, 10050);
        assert_eq!(r2.trades[0].buy_order_id, 2);
        assert_eq!(r2.trades[0].sell_order_id, 1);
        assert!(r2.trades[0].trade_id > 0);
        assert_eq!(r2.remaining_quantity, 0);
    }

    #[test]
    fn test_market_order() {
        let mut book = OwnedOrderBook::new();

        book.add_order("seller", 1, Some(10000), 100, OB_SIDE_SELL, OB_ORDER_TYPE_LIMIT, OB_TIF_GTC, OB_STP_ALLOW);

        let r = book.add_order("buyer", 2, None, 40, OB_SIDE_BUY, OB_ORDER_TYPE_MARKET, OB_TIF_IOC, OB_STP_ALLOW);
        assert!(r.accepted);
        assert_eq!(r.trades.len(), 1);
        assert_eq!(r.trades[0].quantity, 40);
        assert_eq!(r.remaining_quantity, 0);
    }

    #[test]
    fn test_cancel_order() {
        let mut book = OwnedOrderBook::new();
        book.add_order("traderA", 1, Some(10000), 100, OB_SIDE_BUY, OB_ORDER_TYPE_LIMIT, OB_TIF_GTC, OB_STP_ALLOW);

        assert!(book.cancel_order(1));
        assert_eq!(book.get_best_bid(), 0);
        assert!(!book.cancel_order(999));
    }

    #[test]
    fn test_modify_order() {
        let mut book = OwnedOrderBook::new();

        // Add sell for spread
        book.add_order("seller", 10, Some(10500), 50, OB_SIDE_SELL, OB_ORDER_TYPE_LIMIT, OB_TIF_GTC, OB_STP_ALLOW);
        // Add buy to modify
        book.add_order("buyer", 1, Some(10000), 100, OB_SIDE_BUY, OB_ORDER_TYPE_LIMIT, OB_TIF_GTC, OB_STP_ALLOW);

        // Quantity change
        let m1 = book.modify_order(1, 10000, 60);
        assert!(m1.accepted);
        assert_eq!(m1.old_quantity, 100);
        assert_eq!(m1.new_quantity, 60);

        // Price change
        let m2 = book.modify_order(1, 10200, 60);
        assert!(m2.accepted);
        assert_eq!(m2.old_price, 10000);
        assert_eq!(m2.new_price, 10200);
        assert_eq!(book.get_best_bid(), 10200);

        // Cross-spread rejection
        let m3 = book.modify_order(1, 10500, 60);
        assert!(!m3.accepted);
        assert!(m3.reject_reason.is_some());

        // Not found
        let m4 = book.modify_order(999, 10000, 50);
        assert!(!m4.accepted);
    }

    #[test]
    fn test_fok_rejection() {
        let mut book = OwnedOrderBook::new();
        book.add_order("seller", 1, Some(10000), 50, OB_SIDE_SELL, OB_ORDER_TYPE_LIMIT, OB_TIF_GTC, OB_STP_ALLOW);

        let r = book.add_order("buyer", 2, Some(10000), 100, OB_SIDE_BUY, OB_ORDER_TYPE_LIMIT, OB_TIF_FOK, OB_STP_ALLOW);
        assert!(!r.accepted);
        assert!(r.reject_reason.is_some());
    }

    #[test]
    fn test_stp_cancel_newest() {
        let mut book = OwnedOrderBook::new();
        book.add_order("traderA", 1, Some(10000), 50, OB_SIDE_SELL, OB_ORDER_TYPE_LIMIT, OB_TIF_GTC, OB_STP_CANCEL_NEWEST);

        let r = book.add_order("traderA", 2, Some(10000), 30, OB_SIDE_BUY, OB_ORDER_TYPE_LIMIT, OB_TIF_GTC, OB_STP_CANCEL_NEWEST);
        assert!(r.accepted);
        assert_eq!(r.trades.len(), 0);
        assert_eq!(r.remaining_quantity, 0); // killed by STP
        assert_eq!(book.get_best_ask(), 10000); // resting sell survives
    }

    #[test]
    fn test_duplicate_order_id() {
        let mut book = OwnedOrderBook::new();
        let r1 = book.add_order("traderA", 1, Some(10000), 100, OB_SIDE_BUY, OB_ORDER_TYPE_LIMIT, OB_TIF_GTC, OB_STP_ALLOW);
        assert!(r1.accepted);

        let r2 = book.add_order("traderA", 1, Some(10000), 100, OB_SIDE_BUY, OB_ORDER_TYPE_LIMIT, OB_TIF_GTC, OB_STP_ALLOW);
        assert!(!r2.accepted);
    }

    #[test]
    fn test_snapshot_after_trades() {
        let mut book = OwnedOrderBook::new();
        book.add_order("seller", 1, Some(10100), 100, OB_SIDE_SELL, OB_ORDER_TYPE_LIMIT, OB_TIF_GTC, OB_STP_ALLOW);
        book.add_order("buyer", 2, Some(9900), 200, OB_SIDE_BUY, OB_ORDER_TYPE_LIMIT, OB_TIF_GTC, OB_STP_ALLOW);

        let snap = book.get_snapshot();
        assert_eq!(snap.bid_price, 9900);
        assert_eq!(snap.ask_price, 10100);
        assert_eq!(snap.spread, 200);
        assert_eq!(snap.mid_price, 10000);

        // Cross the spread
        let r = book.add_order("crosser", 3, Some(10100), 50, OB_SIDE_BUY, OB_ORDER_TYPE_LIMIT, OB_TIF_GTC, OB_STP_ALLOW);
        assert_eq!(r.trades.len(), 1);
        assert_eq!(book.get_last_trade_price(), 10100);
        assert_eq!(book.get_last_trade_qty(), 50);
    }
}
