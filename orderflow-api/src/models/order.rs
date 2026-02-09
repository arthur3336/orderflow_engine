use serde::{Deserialize, Serialize};

use super::trade::TradeResponse;

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct OrderRequest {
    pub trader_id: String,
    pub price: Option<f64>,
    pub quantity: i64,
    pub side: Side,
    pub order_type: OrderType,
    #[serde(default)]
    pub time_in_force: TimeInForce,
    #[serde(default)]
    pub stp_mode: StpMode,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct OrderResponse {
    pub order_id: u64,
    pub accepted: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub reject_reason: Option<String>,
    pub trades: Vec<TradeResponse>,
    pub remaining_quantity: i64,
}

#[derive(Debug, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct ModifyRequest {
    pub new_price: f64,
    pub new_quantity: i64,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct ModifyResponse {
    pub order_id: u64,
    pub accepted: bool,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub reject_reason: Option<String>,
    pub old_price: f64,
    pub new_price: f64,
    pub old_quantity: i64,
    pub new_quantity: i64,
}

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct CancelResponse {
    pub order_id: u64,
    pub cancelled: bool,
}

// --- Enums matching C++ types ---

#[derive(Debug, Clone, Copy, Deserialize, Serialize, PartialEq)]
#[serde(rename_all = "SCREAMING_SNAKE_CASE")]
pub enum Side {
    Buy,
    Sell,
}

#[derive(Debug, Clone, Copy, Deserialize, Serialize, PartialEq)]
#[serde(rename_all = "SCREAMING_SNAKE_CASE")]
pub enum OrderType {
    Limit,
    Market,
}

#[derive(Debug, Clone, Copy, Deserialize, Serialize, PartialEq)]
#[serde(rename_all = "SCREAMING_SNAKE_CASE")]
pub enum TimeInForce {
    Gtc,
    Ioc,
    Fok,
}

impl Default for TimeInForce {
    fn default() -> Self {
        Self::Gtc
    }
}

#[derive(Debug, Clone, Copy, Deserialize, Serialize, PartialEq)]
#[serde(rename_all = "SCREAMING_SNAKE_CASE")]
pub enum StpMode {
    Allow,
    CancelNewest,
    CancelOldest,
    CancelBoth,
    DecrementAndCancel,
}

impl Default for StpMode {
    fn default() -> Self {
        Self::Allow
    }
}
