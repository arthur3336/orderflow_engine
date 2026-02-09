use std::os::raw::c_char;

// Mirrors ob_side_t
pub const OB_SIDE_BUY: u32 = 0;
pub const OB_SIDE_SELL: u32 = 1;

// Mirrors ob_order_type_t
pub const OB_ORDER_TYPE_LIMIT: u32 = 0;
pub const OB_ORDER_TYPE_MARKET: u32 = 1;

// Mirrors ob_time_in_force_t
pub const OB_TIF_GTC: u32 = 0;
pub const OB_TIF_IOC: u32 = 1;
pub const OB_TIF_FOK: u32 = 2;

// Mirrors ob_stp_mode_t
pub const OB_STP_ALLOW: u32 = 0;
pub const OB_STP_CANCEL_NEWEST: u32 = 1;
pub const OB_STP_CANCEL_OLDEST: u32 = 2;
pub const OB_STP_CANCEL_BOTH: u32 = 3;
pub const OB_STP_DECREMENT_AND_CANCEL: u32 = 4;

#[repr(C)]
pub struct ObOrderT {
    pub trader_id: *const c_char,
    pub id: u64,
    pub price: i64,
    pub quantity: i64,
    pub side: u32,
    pub order_type: u32,
    pub time_in_force: u32,
    pub stp_mode: u32,
    pub has_price: bool,
}

#[repr(C)]
pub struct ObTradeT {
    pub trade_id: u64,
    pub buy_order_id: u64,
    pub sell_order_id: u64,
    pub price: i64,
    pub quantity: i64,
    pub timestamp_ns: i64,
}

#[repr(C)]
pub struct ObStpResultT {
    pub self_trade: bool,
    pub cancelled_orders: *mut u64,
    pub cancelled_orders_len: usize,
    pub action: *mut c_char,
}

#[repr(C)]
pub struct ObOrderResultT {
    pub accepted: bool,
    pub reject_reason: *mut c_char,
    pub trades: *mut ObTradeT,
    pub trades_len: usize,
    pub remaining_quantity: i64,
    pub stp_result: ObStpResultT,
}

#[repr(C)]
pub struct ObModifyResultT {
    pub accepted: bool,
    pub reject_reason: *mut c_char,
    pub old_price: i64,
    pub new_price: i64,
    pub old_quantity: i64,
    pub new_quantity: i64,
}

#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct ObPriceDataT {
    pub timestamp_ns: i64,
    pub bid_price: i64,
    pub ask_price: i64,
    pub mid_price: i64,
    pub spread: i64,
    pub last_trade_price: i64,
    pub last_trade_qty: i64,
}
