use crate::models::order::*;
use crate::models::trade::TradeResponse;

pub fn order_submitted(order_id: u64, req: &OrderRequest) {
    tracing::info!(
        event = "OrderSubmitted",
        order_id,
        trader_id = %req.trader_id,
        side = ?req.side,
        order_type = ?req.order_type,
        price = ?req.price,
        quantity = req.quantity,
        time_in_force = ?req.time_in_force,
        stp_mode = ?req.stp_mode,
    );
}

pub fn order_accepted(order_id: u64, trades_count: usize, remaining_qty: i64) {
    tracing::info!(
        event = "OrderAccepted",
        order_id,
        trades_count,
        remaining_qty,
    );
}

pub fn order_rejected(order_id: u64, reason: &str, source: &str) {
    tracing::warn!(
        event = "OrderRejected",
        order_id,
        reason,
        source,
    );
}

pub fn order_modified(resp: &ModifyResponse) {
    tracing::info!(
        event = "OrderModified",
        order_id = resp.order_id,
        old_price = resp.old_price,
        new_price = resp.new_price,
        old_quantity = resp.old_quantity,
        new_quantity = resp.new_quantity,
    );
}

pub fn order_cancelled(order_id: u64) {
    tracing::info!(
        event = "OrderCancelled",
        order_id,
    );
}

pub fn trade_executed(trade: &TradeResponse) {
    tracing::info!(
        event = "TradeExecuted",
        trade_id = trade.trade_id,
        buy_order_id = trade.buy_order_id,
        sell_order_id = trade.sell_order_id,
        price = trade.price,
        quantity = trade.quantity,
    );
}
