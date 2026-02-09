use std::sync::Arc;
use std::time::Instant;

use tokio::sync::broadcast;

use crate::engine::orderbook::Engine;
use crate::middleware::metrics as m;
use crate::models::error::ApiError;
use crate::models::order::*;

use super::audit_service as audit;
use super::rate_limiter::RateLimiterService;
use super::risk_service::RiskService;

pub struct OrderService {
    pub engine: Arc<Engine>,
    risk: Arc<RiskService>,
    rate_limiter: Arc<RateLimiterService>,
    ws_broadcast: broadcast::Sender<String>,
}

impl OrderService {
    pub fn new(
        engine: Arc<Engine>,
        risk: Arc<RiskService>,
        rate_limiter: Arc<RateLimiterService>,
        ws_broadcast: broadcast::Sender<String>,
    ) -> Self {
        Self {
            engine,
            risk,
            rate_limiter,
            ws_broadcast,
        }
    }

    fn broadcast(&self, msg: &str) {
        // Ignore send errors (no active receivers is fine)
        let _ = self.ws_broadcast.send(msg.to_string());
    }

    pub async fn submit_order(&self, req: OrderRequest) -> Result<OrderResponse, ApiError> {
        let start = Instant::now();
        let side_str = format!("{:?}", req.side);
        let type_str = format!("{:?}", req.order_type);

        m::record_order_submitted(&side_str, &type_str);

        // 1. Rate limit check
        if let Err(e) = self.rate_limiter.check_rate_limit(&req.trader_id) {
            audit::order_rejected(0, &e.to_string(), "rate_limit");
            m::record_order_rejected("rate_limit");
            return Err(e);
        }

        // 2. Get current snapshot for risk checks (read lock, fast)
        let snapshot = self.engine.get_snapshot().await;

        // 3. Risk checks (size, price band, position limit)
        if let Err(e) = self.risk.check_order(
            &req.trader_id,
            req.quantity,
            req.side,
            req.order_type,
            req.price,
            &snapshot,
        ) {
            audit::order_rejected(0, &e.to_string(), "risk");
            m::record_order_rejected("risk");
            return Err(e);
        }

        let trader_id = req.trader_id.clone();
        let side = req.side;

        // 4. Audit: order submitted
        audit::order_submitted(0, &req);

        // 5. Submit to engine (validates, generates ID, calls FFI)
        let engine_start = Instant::now();
        let response = match self.engine.add_order(req).await {
            Ok(resp) => resp,
            Err(e) => {
                audit::order_rejected(0, &e.to_string(), "engine");
                m::record_order_rejected("engine");
                m::record_order_latency(start);
                return Err(e);
            }
        };
        m::record_engine_latency(engine_start);

        // 6. Audit: order accepted + trades
        audit::order_accepted(
            response.order_id,
            response.trades.len(),
            response.remaining_quantity,
        );
        for trade in &response.trades {
            audit::trade_executed(trade);
        }
        m::record_order_accepted(&side_str, &type_str);
        m::record_trades(response.trades.len() as u64);

        // 7. Register this order for counterparty position tracking
        self.risk
            .register_order(response.order_id, &trader_id, side);

        // 8. Update positions for both sides of each trade
        let trades: Vec<(u64, u64, i64)> = response
            .trades
            .iter()
            .map(|t| (t.buy_order_id, t.sell_order_id, t.quantity))
            .collect();
        self.risk
            .update_positions_from_trades(&trader_id, side, &trades);

        // 9. Unregister fully filled orders (remaining_quantity == 0)
        if response.remaining_quantity == 0 {
            self.risk.unregister_order(response.order_id);
        }

        // 10. Broadcast trades to WebSocket clients
        for trade in &response.trades {
            let msg = serde_json::json!({
                "type": "trade",
                "data": {
                    "tradeId": trade.trade_id,
                    "buyOrderId": trade.buy_order_id,
                    "sellOrderId": trade.sell_order_id,
                    "price": trade.price,
                    "quantity": trade.quantity
                }
            });
            self.broadcast(&msg.to_string());
        }

        m::record_order_latency(start);
        Ok(response)
    }

    pub async fn modify_order(
        &self,
        order_id: u64,
        req: ModifyRequest,
    ) -> Result<ModifyResponse, ApiError> {
        let response = self.engine.modify_order(order_id, req).await?;
        audit::order_modified(&response);

        let msg = serde_json::json!({
            "type": "orderModified",
            "data": {
                "orderId": response.order_id,
                "oldPrice": response.old_price,
                "newPrice": response.new_price,
                "oldQuantity": response.old_quantity,
                "newQuantity": response.new_quantity
            }
        });
        self.broadcast(&msg.to_string());

        Ok(response)
    }

    pub async fn cancel_order(&self, order_id: u64) -> Result<CancelResponse, ApiError> {
        let response = self.engine.cancel_order(order_id).await?;
        self.risk.unregister_order(order_id);
        audit::order_cancelled(order_id);

        let msg = serde_json::json!({
            "type": "orderCancelled",
            "data": { "orderId": order_id }
        });
        self.broadcast(&msg.to_string());

        Ok(response)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::config::RiskConfig;

    fn make_service() -> OrderService {
        let engine = Arc::new(Engine::new());
        let risk = Arc::new(RiskService::new(RiskConfig {
            min_order_size: 1,
            max_order_size: 10_000,
            price_band_percent: 10.0,
            max_position_per_trader: 1_000,
            max_orders_per_second: 100,
        }));
        let rate_limiter = Arc::new(RateLimiterService::new(100));
        let (ws_tx, _) = broadcast::channel(16);
        OrderService::new(engine, risk, rate_limiter, ws_tx)
    }

    fn limit_order(trader: &str, price: f64, qty: i64, side: Side) -> OrderRequest {
        OrderRequest {
            trader_id: trader.into(),
            price: Some(price),
            quantity: qty,
            side,
            order_type: OrderType::Limit,
            time_in_force: TimeInForce::Gtc,
            stp_mode: StpMode::Allow,
        }
    }

    #[tokio::test]
    async fn test_submit_through_service() {
        let svc = make_service();
        let resp = svc
            .submit_order(limit_order("alice", 100.0, 100, Side::Buy))
            .await
            .unwrap();
        assert!(resp.accepted);
        assert_eq!(resp.remaining_quantity, 100);
    }

    #[tokio::test]
    async fn test_risk_rejection_oversized() {
        let svc = make_service();
        let result = svc
            .submit_order(limit_order("alice", 100.0, 10_001, Side::Buy))
            .await;
        assert!(result.is_err());
        match result.unwrap_err() {
            ApiError::RiskRejection(msg) => assert!(msg.contains("exceeds maximum")),
            e => panic!("Expected RiskRejection, got {:?}", e),
        }
    }

    #[tokio::test]
    async fn test_position_tracking_after_fill() {
        let svc = make_service();

        // Resting sell
        svc.submit_order(limit_order("seller", 100.0, 50, Side::Sell))
            .await
            .unwrap();

        // Crossing buy — fills 50
        let resp = svc
            .submit_order(limit_order("buyer", 100.0, 50, Side::Buy))
            .await
            .unwrap();
        assert_eq!(resp.trades.len(), 1);

        // Buyer's position should be +50
        assert_eq!(svc.risk.get_position("buyer"), 50);
        // Seller's position should be -50
        assert_eq!(svc.risk.get_position("seller"), -50);
    }

    #[tokio::test]
    async fn test_position_limit_blocks_order() {
        let svc = make_service();

        // Place resting sells within position limit (each seller ≤ 1000)
        svc.submit_order(limit_order("seller1", 100.0, 1000, Side::Sell))
            .await
            .unwrap();
        svc.submit_order(limit_order("seller2", 100.0, 1000, Side::Sell))
            .await
            .unwrap();

        // Buy 900 — fills against seller1, buyer position = 900
        svc.submit_order(limit_order("buyer", 100.0, 900, Side::Buy))
            .await
            .unwrap();
        assert_eq!(svc.risk.get_position("buyer"), 900);

        // Try to buy 200 more — projected 1100 > limit 1000
        let result = svc
            .submit_order(limit_order("buyer", 100.0, 200, Side::Buy))
            .await;
        assert!(result.is_err());
        match result.unwrap_err() {
            ApiError::RiskRejection(msg) => assert!(msg.contains("Position limit")),
            e => panic!("Expected RiskRejection, got {:?}", e),
        }
    }

    #[tokio::test]
    async fn test_price_band_rejection() {
        let svc = make_service();

        // Establish a two-sided book around $100
        svc.submit_order(limit_order("a", 99.50, 100, Side::Buy))
            .await
            .unwrap();
        svc.submit_order(limit_order("b", 100.50, 100, Side::Sell))
            .await
            .unwrap();

        // Mid = $100.0, band = ±10% → [90, 110]
        // Try to buy at $80 — outside band
        let result = svc
            .submit_order(limit_order("c", 80.0, 10, Side::Buy))
            .await;
        assert!(result.is_err());
        match result.unwrap_err() {
            ApiError::RiskRejection(msg) => assert!(msg.contains("outside")),
            e => panic!("Expected RiskRejection, got {:?}", e),
        }
    }
}
