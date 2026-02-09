use std::sync::atomic::{AtomicU64, Ordering};
use tokio::sync::RwLock;

use crate::ffi::safe_wrapper::OwnedOrderBook;
use crate::ffi::types;
use crate::models::error::ApiError;
use crate::models::market::MarketSnapshot;
use crate::models::order::*;
use crate::models::trade::TradeResponse;

pub struct Engine {
    book: RwLock<OwnedOrderBook>,
    next_order_id: AtomicU64,
    total_orders: AtomicU64,
    total_trades: AtomicU64,
}

impl Engine {
    pub fn new() -> Self {
        Self {
            book: RwLock::new(OwnedOrderBook::new()),
            next_order_id: AtomicU64::new(1),
            total_orders: AtomicU64::new(0),
            total_trades: AtomicU64::new(0),
        }
    }

    pub fn next_order_id(&self) -> u64 {
        self.next_order_id.fetch_add(1, Ordering::Relaxed)
    }

    pub fn total_orders(&self) -> u64 {
        self.total_orders.load(Ordering::Relaxed)
    }

    pub fn total_trades(&self) -> u64 {
        self.total_trades.load(Ordering::Relaxed)
    }

    pub async fn add_order(&self, req: OrderRequest) -> Result<OrderResponse, ApiError> {
        validate_order_request(&req)?;

        let order_id = self.next_order_id();

        let price_cents = match req.order_type {
            OrderType::Market => None,
            OrderType::Limit => {
                let p = req.price.ok_or_else(|| {
                    ApiError::Validation("Limit orders require a price".into())
                })?;
                Some(dollars_to_cents(p)?)
            }
        };

        let side = match req.side {
            Side::Buy => types::OB_SIDE_BUY,
            Side::Sell => types::OB_SIDE_SELL,
        };
        let order_type = match req.order_type {
            OrderType::Limit => types::OB_ORDER_TYPE_LIMIT,
            OrderType::Market => types::OB_ORDER_TYPE_MARKET,
        };
        let tif = match req.time_in_force {
            TimeInForce::Gtc => types::OB_TIF_GTC,
            TimeInForce::Ioc => types::OB_TIF_IOC,
            TimeInForce::Fok => types::OB_TIF_FOK,
        };
        let stp = match req.stp_mode {
            StpMode::Allow => types::OB_STP_ALLOW,
            StpMode::CancelNewest => types::OB_STP_CANCEL_NEWEST,
            StpMode::CancelOldest => types::OB_STP_CANCEL_OLDEST,
            StpMode::CancelBoth => types::OB_STP_CANCEL_BOTH,
            StpMode::DecrementAndCancel => types::OB_STP_DECREMENT_AND_CANCEL,
        };

        let result = {
            let mut book = self.book.write().await;
            book.add_order(
                &req.trader_id,
                order_id,
                price_cents,
                req.quantity,
                side,
                order_type,
                tif,
                stp,
            )
        };

        self.total_orders.fetch_add(1, Ordering::Relaxed);
        self.total_trades
            .fetch_add(result.trades.len() as u64, Ordering::Relaxed);

        if !result.accepted {
            let reason = result
                .reject_reason
                .unwrap_or_else(|| "Unknown rejection".into());
            return Err(ApiError::EngineRejection(reason));
        }

        let trades: Vec<TradeResponse> = result
            .trades
            .iter()
            .map(|t| TradeResponse {
                trade_id: t.trade_id,
                buy_order_id: t.buy_order_id,
                sell_order_id: t.sell_order_id,
                price: cents_to_dollars(t.price),
                quantity: t.quantity,
            })
            .collect();

        Ok(OrderResponse {
            order_id,
            accepted: true,
            reject_reason: None,
            trades,
            remaining_quantity: result.remaining_quantity,
        })
    }

    pub async fn cancel_order(&self, order_id: u64) -> Result<CancelResponse, ApiError> {
        let cancelled = {
            let mut book = self.book.write().await;
            book.cancel_order(order_id)
        };

        if !cancelled {
            return Err(ApiError::NotFound(order_id));
        }

        Ok(CancelResponse {
            order_id,
            cancelled: true,
        })
    }

    pub async fn modify_order(
        &self,
        order_id: u64,
        req: ModifyRequest,
    ) -> Result<ModifyResponse, ApiError> {
        if req.new_quantity <= 0 {
            return Err(ApiError::Validation("Quantity must be positive".into()));
        }
        if req.new_price < 0.0 {
            return Err(ApiError::Validation("Price cannot be negative".into()));
        }

        let new_price_cents = dollars_to_cents(req.new_price)?;

        let result = {
            let mut book = self.book.write().await;
            book.modify_order(order_id, new_price_cents, req.new_quantity)
        };

        if !result.accepted {
            let reason = result
                .reject_reason
                .unwrap_or_else(|| "Unknown rejection".into());
            // Distinguish "not found" from other rejections
            if reason.contains("not found") {
                return Err(ApiError::NotFound(order_id));
            }
            return Err(ApiError::EngineRejection(reason));
        }

        Ok(ModifyResponse {
            order_id,
            accepted: true,
            reject_reason: None,
            old_price: cents_to_dollars(result.old_price),
            new_price: cents_to_dollars(result.new_price),
            old_quantity: result.old_quantity,
            new_quantity: result.new_quantity,
        })
    }

    pub async fn get_snapshot(&self) -> MarketSnapshot {
        let snap = {
            let book = self.book.read().await;
            book.get_snapshot()
        };

        let has_bid = snap.bid_price > 0;
        let has_ask = snap.ask_price > 0;
        let has_both = has_bid && has_ask;

        MarketSnapshot {
            best_bid: if has_bid { Some(cents_to_dollars(snap.bid_price)) } else { None },
            best_ask: if has_ask { Some(cents_to_dollars(snap.ask_price)) } else { None },
            spread: if has_both { Some(cents_to_dollars(snap.spread)) } else { None },
            mid_price: if has_both { Some(cents_to_dollars(snap.mid_price)) } else { None },
            last_trade_price: cents_to_optional_dollars(snap.last_trade_price),
            last_trade_qty: if snap.last_trade_qty == 0 {
                None
            } else {
                Some(snap.last_trade_qty)
            },
        }
    }
}

// ======================================================================
// Price conversion helpers
// ======================================================================

fn dollars_to_cents(dollars: f64) -> Result<i64, ApiError> {
    if dollars < 0.0 {
        return Err(ApiError::Validation("Price cannot be negative".into()));
    }
    let cents = (dollars * 100.0).round() as i64;
    // Verify no sub-cent precision was lost
    let roundtrip = cents as f64 / 100.0;
    if (roundtrip - dollars).abs() > 0.001 {
        return Err(ApiError::Validation(
            "Price precision limited to 2 decimal places (cents)".into(),
        ));
    }
    Ok(cents)
}

fn cents_to_dollars(cents: i64) -> f64 {
    cents as f64 / 100.0
}

fn cents_to_optional_dollars(cents: i64) -> Option<f64> {
    if cents == 0 {
        None
    } else {
        Some(cents_to_dollars(cents))
    }
}

fn validate_order_request(req: &OrderRequest) -> Result<(), ApiError> {
    if req.trader_id.is_empty() {
        return Err(ApiError::Validation("traderId is required".into()));
    }
    if req.trader_id.len() > 64 {
        return Err(ApiError::Validation(
            "traderId must be 64 characters or less".into(),
        ));
    }
    if req.quantity <= 0 {
        return Err(ApiError::Validation("Quantity must be positive".into()));
    }
    if req.order_type == OrderType::Limit {
        match req.price {
            None => {
                return Err(ApiError::Validation(
                    "Limit orders require a price".into(),
                ))
            }
            Some(p) if p <= 0.0 => {
                return Err(ApiError::Validation(
                    "Price must be positive for limit orders".into(),
                ))
            }
            _ => {}
        }
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_dollars_to_cents() {
        assert_eq!(dollars_to_cents(100.50).unwrap(), 10050);
        assert_eq!(dollars_to_cents(0.01).unwrap(), 1);
        assert_eq!(dollars_to_cents(999.99).unwrap(), 99999);
        assert!(dollars_to_cents(-1.0).is_err());
    }

    #[test]
    fn test_cents_to_dollars() {
        assert_eq!(cents_to_dollars(10050), 100.50);
        assert_eq!(cents_to_dollars(0), 0.0);
        assert_eq!(cents_to_dollars(1), 0.01);
    }

    #[test]
    fn test_cents_to_optional_dollars() {
        assert_eq!(cents_to_optional_dollars(0), None);
        assert_eq!(cents_to_optional_dollars(10050), Some(100.50));
    }

    #[tokio::test]
    async fn test_engine_add_order() {
        let engine = Engine::new();
        let req = OrderRequest {
            trader_id: "alice".into(),
            price: Some(100.50),
            quantity: 100,
            side: Side::Buy,
            order_type: OrderType::Limit,
            time_in_force: TimeInForce::Gtc,
            stp_mode: StpMode::Allow,
        };
        let resp = engine.add_order(req).await.unwrap();
        assert!(resp.accepted);
        assert_eq!(resp.remaining_quantity, 100);
        assert!(resp.trades.is_empty());
        assert_eq!(engine.total_orders(), 1);
    }

    #[tokio::test]
    async fn test_engine_matching() {
        let engine = Engine::new();

        // Resting sell at $100.50
        let sell = OrderRequest {
            trader_id: "seller".into(),
            price: Some(100.50),
            quantity: 50,
            side: Side::Sell,
            order_type: OrderType::Limit,
            time_in_force: TimeInForce::Gtc,
            stp_mode: StpMode::Allow,
        };
        engine.add_order(sell).await.unwrap();

        // Crossing buy at $100.50
        let buy = OrderRequest {
            trader_id: "buyer".into(),
            price: Some(100.50),
            quantity: 30,
            side: Side::Buy,
            order_type: OrderType::Limit,
            time_in_force: TimeInForce::Gtc,
            stp_mode: StpMode::Allow,
        };
        let resp = engine.add_order(buy).await.unwrap();
        assert_eq!(resp.trades.len(), 1);
        assert_eq!(resp.trades[0].price, 100.50);
        assert_eq!(resp.trades[0].quantity, 30);
        assert_eq!(resp.remaining_quantity, 0);
        assert_eq!(engine.total_trades(), 1);
    }

    #[tokio::test]
    async fn test_engine_cancel() {
        let engine = Engine::new();
        let req = OrderRequest {
            trader_id: "alice".into(),
            price: Some(100.00),
            quantity: 100,
            side: Side::Buy,
            order_type: OrderType::Limit,
            time_in_force: TimeInForce::Gtc,
            stp_mode: StpMode::Allow,
        };
        let resp = engine.add_order(req).await.unwrap();
        let oid = resp.order_id;

        let cancel = engine.cancel_order(oid).await.unwrap();
        assert!(cancel.cancelled);

        // Cancel again should fail
        assert!(engine.cancel_order(oid).await.is_err());
    }

    #[tokio::test]
    async fn test_engine_modify() {
        let engine = Engine::new();

        // Need a sell to establish spread
        let sell = OrderRequest {
            trader_id: "seller".into(),
            price: Some(105.00),
            quantity: 50,
            side: Side::Sell,
            order_type: OrderType::Limit,
            time_in_force: TimeInForce::Gtc,
            stp_mode: StpMode::Allow,
        };
        engine.add_order(sell).await.unwrap();

        let buy = OrderRequest {
            trader_id: "buyer".into(),
            price: Some(100.00),
            quantity: 100,
            side: Side::Buy,
            order_type: OrderType::Limit,
            time_in_force: TimeInForce::Gtc,
            stp_mode: StpMode::Allow,
        };
        let resp = engine.add_order(buy).await.unwrap();
        let oid = resp.order_id;

        let modify = engine
            .modify_order(
                oid,
                ModifyRequest {
                    new_price: 101.00,
                    new_quantity: 60,
                },
            )
            .await
            .unwrap();
        assert!(modify.accepted);
        assert_eq!(modify.old_price, 100.00);
        assert_eq!(modify.new_price, 101.00);
        assert_eq!(modify.old_quantity, 100);
        assert_eq!(modify.new_quantity, 60);
    }

    #[tokio::test]
    async fn test_engine_snapshot() {
        let engine = Engine::new();

        let buy = OrderRequest {
            trader_id: "buyer".into(),
            price: Some(99.00),
            quantity: 100,
            side: Side::Buy,
            order_type: OrderType::Limit,
            time_in_force: TimeInForce::Gtc,
            stp_mode: StpMode::Allow,
        };
        engine.add_order(buy).await.unwrap();

        let sell = OrderRequest {
            trader_id: "seller".into(),
            price: Some(101.00),
            quantity: 100,
            side: Side::Sell,
            order_type: OrderType::Limit,
            time_in_force: TimeInForce::Gtc,
            stp_mode: StpMode::Allow,
        };
        engine.add_order(sell).await.unwrap();

        let snap = engine.get_snapshot().await;
        assert_eq!(snap.best_bid, Some(99.00));
        assert_eq!(snap.best_ask, Some(101.00));
        assert_eq!(snap.spread, Some(2.00));
        assert_eq!(snap.mid_price, Some(100.00));
    }

    #[tokio::test]
    async fn test_validation_empty_trader_id() {
        let engine = Engine::new();
        let req = OrderRequest {
            trader_id: "".into(),
            price: Some(100.00),
            quantity: 100,
            side: Side::Buy,
            order_type: OrderType::Limit,
            time_in_force: TimeInForce::Gtc,
            stp_mode: StpMode::Allow,
        };
        assert!(engine.add_order(req).await.is_err());
    }

    #[tokio::test]
    async fn test_validation_negative_quantity() {
        let engine = Engine::new();
        let req = OrderRequest {
            trader_id: "alice".into(),
            price: Some(100.00),
            quantity: -10,
            side: Side::Buy,
            order_type: OrderType::Limit,
            time_in_force: TimeInForce::Gtc,
            stp_mode: StpMode::Allow,
        };
        assert!(engine.add_order(req).await.is_err());
    }

    #[tokio::test]
    async fn test_validation_limit_without_price() {
        let engine = Engine::new();
        let req = OrderRequest {
            trader_id: "alice".into(),
            price: None,
            quantity: 100,
            side: Side::Buy,
            order_type: OrderType::Limit,
            time_in_force: TimeInForce::Gtc,
            stp_mode: StpMode::Allow,
        };
        assert!(engine.add_order(req).await.is_err());
    }

    #[tokio::test]
    async fn test_market_order_through_engine() {
        let engine = Engine::new();

        // Resting sell
        let sell = OrderRequest {
            trader_id: "seller".into(),
            price: Some(100.00),
            quantity: 100,
            side: Side::Sell,
            order_type: OrderType::Limit,
            time_in_force: TimeInForce::Gtc,
            stp_mode: StpMode::Allow,
        };
        engine.add_order(sell).await.unwrap();

        // Market buy
        let buy = OrderRequest {
            trader_id: "buyer".into(),
            price: None,
            quantity: 40,
            side: Side::Buy,
            order_type: OrderType::Market,
            time_in_force: TimeInForce::Ioc,
            stp_mode: StpMode::Allow,
        };
        let resp = engine.add_order(buy).await.unwrap();
        assert!(resp.accepted);
        assert_eq!(resp.trades.len(), 1);
        assert_eq!(resp.trades[0].price, 100.00);
        assert_eq!(resp.trades[0].quantity, 40);
    }
}
