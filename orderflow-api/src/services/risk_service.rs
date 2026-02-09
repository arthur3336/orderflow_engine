use dashmap::DashMap;

use crate::config::RiskConfig;
use crate::models::error::ApiError;
use crate::models::market::MarketSnapshot;
use crate::models::order::{OrderType, Side};

pub struct OrderRegistration {
    pub trader_id: String,
    pub side: Side,
}

pub struct RiskService {
    config: RiskConfig,
    positions: DashMap<String, i64>,
    /// Maps order_id → (trader_id, side) so we can update counterparty positions after trades
    order_registry: DashMap<u64, OrderRegistration>,
}

impl RiskService {
    pub fn new(config: RiskConfig) -> Self {
        Self {
            config,
            positions: DashMap::new(),
            order_registry: DashMap::new(),
        }
    }

    pub fn check_order(
        &self,
        trader_id: &str,
        quantity: i64,
        side: Side,
        order_type: OrderType,
        price: Option<f64>,
        snapshot: &MarketSnapshot,
    ) -> Result<(), ApiError> {
        self.check_order_size(quantity)?;
        if order_type == OrderType::Limit {
            if let Some(p) = price {
                self.check_price_band(p, snapshot)?;
            }
        }
        self.check_position_limit(trader_id, quantity, side)?;
        Ok(())
    }

    fn check_order_size(&self, quantity: i64) -> Result<(), ApiError> {
        if quantity < self.config.min_order_size {
            return Err(ApiError::RiskRejection(format!(
                "Order size {} below minimum {}",
                quantity, self.config.min_order_size
            )));
        }
        if quantity > self.config.max_order_size {
            return Err(ApiError::RiskRejection(format!(
                "Order size {} exceeds maximum {}",
                quantity, self.config.max_order_size
            )));
        }
        Ok(())
    }

    fn check_price_band(&self, price: f64, snapshot: &MarketSnapshot) -> Result<(), ApiError> {
        // Use mid price as reference, fall back to last trade price
        let reference = snapshot
            .mid_price
            .or(snapshot.last_trade_price);

        // If no reference price exists (empty book, no trades), skip band check
        let reference = match reference {
            Some(r) if r > 0.0 => r,
            _ => return Ok(()),
        };

        let band = self.config.price_band_percent / 100.0;
        let lower = reference * (1.0 - band);
        let upper = reference * (1.0 + band);

        if price < lower || price > upper {
            return Err(ApiError::RiskRejection(format!(
                "Price {:.2} outside {:.1}% band [{:.2}, {:.2}] around reference {:.2}",
                price, self.config.price_band_percent, lower, upper, reference
            )));
        }
        Ok(())
    }

    fn check_position_limit(
        &self,
        trader_id: &str,
        quantity: i64,
        side: Side,
    ) -> Result<(), ApiError> {
        let current = self.positions.get(trader_id).map(|v| *v).unwrap_or(0);
        let delta = match side {
            Side::Buy => quantity,
            Side::Sell => -quantity,
        };
        let projected = current + delta;

        if projected.abs() > self.config.max_position_per_trader {
            return Err(ApiError::RiskRejection(format!(
                "Position limit exceeded: current {}, projected {} (limit ±{})",
                current, projected, self.config.max_position_per_trader
            )));
        }
        Ok(())
    }

    /// Register an order so we can look up the trader for counterparty position updates.
    pub fn register_order(&self, order_id: u64, trader_id: &str, side: Side) {
        self.order_registry.insert(
            order_id,
            OrderRegistration {
                trader_id: trader_id.to_string(),
                side,
            },
        );
    }

    /// Unregister an order (on cancel or full fill).
    pub fn unregister_order(&self, order_id: u64) {
        self.order_registry.remove(&order_id);
    }

    /// Update positions for both sides of each trade.
    /// `trades` contains (buy_order_id, sell_order_id, quantity).
    pub fn update_positions_from_trades(
        &self,
        submitting_trader: &str,
        submitting_side: Side,
        trades: &[(u64, u64, i64)],
    ) {
        for &(buy_order_id, sell_order_id, qty) in trades {
            // Update buyer position (+qty)
            let buyer = if submitting_side == Side::Buy {
                submitting_trader.to_string()
            } else {
                self.order_registry
                    .get(&buy_order_id)
                    .map(|r| r.trader_id.clone())
                    .unwrap_or_default()
            };
            if !buyer.is_empty() {
                self.apply_delta(&buyer, qty);
            }

            // Update seller position (-qty)
            let seller = if submitting_side == Side::Sell {
                submitting_trader.to_string()
            } else {
                self.order_registry
                    .get(&sell_order_id)
                    .map(|r| r.trader_id.clone())
                    .unwrap_or_default()
            };
            if !seller.is_empty() {
                self.apply_delta(&seller, -qty);
            }
        }
    }

    fn apply_delta(&self, trader_id: &str, delta: i64) {
        self.positions
            .entry(trader_id.to_string())
            .and_modify(|pos| *pos += delta)
            .or_insert(delta);
    }

    pub fn get_position(&self, trader_id: &str) -> i64 {
        self.positions.get(trader_id).map(|v| *v).unwrap_or(0)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn default_config() -> RiskConfig {
        RiskConfig {
            min_order_size: 1,
            max_order_size: 10_000,
            price_band_percent: 10.0,
            max_position_per_trader: 1_000,
            max_orders_per_second: 100,
        }
    }

    fn empty_snapshot() -> MarketSnapshot {
        MarketSnapshot {
            best_bid: None,
            best_ask: None,
            spread: None,
            mid_price: None,
            last_trade_price: None,
            last_trade_qty: None,
        }
    }

    fn snapshot_with_mid(mid: f64) -> MarketSnapshot {
        MarketSnapshot {
            best_bid: Some(mid - 0.50),
            best_ask: Some(mid + 0.50),
            spread: Some(1.0),
            mid_price: Some(mid),
            last_trade_price: Some(mid),
            last_trade_qty: Some(10),
        }
    }

    #[test]
    fn test_order_size_valid() {
        let svc = RiskService::new(default_config());
        assert!(svc.check_order_size(100).is_ok());
        assert!(svc.check_order_size(1).is_ok());
        assert!(svc.check_order_size(10_000).is_ok());
    }

    #[test]
    fn test_order_size_too_small() {
        let svc = RiskService::new(default_config());
        assert!(svc.check_order_size(0).is_err());
        assert!(svc.check_order_size(-1).is_err());
    }

    #[test]
    fn test_order_size_too_large() {
        let svc = RiskService::new(default_config());
        assert!(svc.check_order_size(10_001).is_err());
        assert!(svc.check_order_size(100_000).is_err());
    }

    #[test]
    fn test_price_band_valid() {
        let svc = RiskService::new(default_config());
        let snap = snapshot_with_mid(100.0);
        // 10% band: [90.0, 110.0]
        assert!(svc.check_price_band(100.0, &snap).is_ok());
        assert!(svc.check_price_band(90.0, &snap).is_ok());
        assert!(svc.check_price_band(110.0, &snap).is_ok());
    }

    #[test]
    fn test_price_band_breach() {
        let svc = RiskService::new(default_config());
        let snap = snapshot_with_mid(100.0);
        assert!(svc.check_price_band(89.99, &snap).is_err());
        assert!(svc.check_price_band(110.01, &snap).is_err());
    }

    #[test]
    fn test_price_band_skipped_on_empty_book() {
        let svc = RiskService::new(default_config());
        let snap = empty_snapshot();
        // No reference price, so band check is skipped
        assert!(svc.check_price_band(999.0, &snap).is_ok());
    }

    #[test]
    fn test_position_limit_valid() {
        let svc = RiskService::new(default_config());
        assert!(svc
            .check_position_limit("alice", 1000, Side::Buy)
            .is_ok());
        assert!(svc
            .check_position_limit("alice", 1000, Side::Sell)
            .is_ok());
    }

    #[test]
    fn test_position_limit_exceeded() {
        let svc = RiskService::new(default_config());
        assert!(svc
            .check_position_limit("alice", 1001, Side::Buy)
            .is_err());
    }

    #[test]
    fn test_position_tracking() {
        let svc = RiskService::new(default_config());

        // Register resting sell from alice (order 1)
        svc.register_order(1, "alice", Side::Sell);

        // Bob buys, trade fills: buy_order_id=2, sell_order_id=1, qty=500
        svc.update_positions_from_trades("bob", Side::Buy, &[(2, 1, 500)]);
        assert_eq!(svc.get_position("bob"), 500);
        assert_eq!(svc.get_position("alice"), -500);

        // Another trade: bob sells 200 back
        svc.register_order(2, "bob", Side::Buy); // bob's resting buy
        svc.update_positions_from_trades("alice", Side::Sell, &[(2, 3, 200)]);
        // alice sold 200 more: -500 + (-200) = -700
        // bob: counterparty on buy side: 500 + 200 = 700
        assert_eq!(svc.get_position("alice"), -700);
        assert_eq!(svc.get_position("bob"), 700);

        // Now check limit with existing position for bob
        // Current: 700, buying 301 would make 1001 > 1000
        assert!(svc
            .check_position_limit("bob", 301, Side::Buy)
            .is_err());
        // Current: 700, buying 300 would make 1000 <= 1000
        assert!(svc
            .check_position_limit("bob", 300, Side::Buy)
            .is_ok());
    }

    #[test]
    fn test_full_check_passes() {
        let svc = RiskService::new(default_config());
        let snap = snapshot_with_mid(100.0);
        assert!(svc
            .check_order("alice", 100, Side::Buy, OrderType::Limit, Some(100.0), &snap)
            .is_ok());
    }

    #[test]
    fn test_full_check_market_order_skips_price_band() {
        let svc = RiskService::new(default_config());
        let snap = snapshot_with_mid(100.0);
        // Market order with no price — price band check is skipped
        assert!(svc
            .check_order("alice", 100, Side::Buy, OrderType::Market, None, &snap)
            .is_ok());
    }
}
