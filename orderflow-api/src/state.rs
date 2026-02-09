use std::sync::atomic::AtomicU64;
use std::sync::Arc;
use std::time::Instant;

use tokio::sync::broadcast;

use crate::config::Config;
use crate::engine::orderbook::Engine;
use crate::services::order_service::OrderService;
use crate::services::rate_limiter::RateLimiterService;
use crate::services::risk_service::RiskService;

#[derive(Clone)]
pub struct AppState {
    pub order_service: Arc<OrderService>,
    pub engine: Arc<Engine>,
    pub start_time: Instant,
    pub ws_broadcast: broadcast::Sender<String>,
    pub ws_connections: Arc<AtomicU64>,
}

impl AppState {
    pub fn new(config: &Config) -> Self {
        let engine = Arc::new(Engine::new());
        let risk = Arc::new(RiskService::new(config.risk.clone()));
        let rate_limiter = Arc::new(RateLimiterService::new(config.risk.max_orders_per_second));

        let (ws_broadcast, _) = broadcast::channel(1024);

        let order_service = Arc::new(OrderService::new(
            Arc::clone(&engine),
            risk,
            rate_limiter,
            ws_broadcast.clone(),
        ));

        Self {
            order_service,
            engine,
            start_time: Instant::now(),
            ws_broadcast,
            ws_connections: Arc::new(AtomicU64::new(0)),
        }
    }
}
