use serde::Serialize;

#[derive(Debug, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct MarketSnapshot {
    #[serde(skip_serializing_if = "Option::is_none")]
    pub best_bid: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub best_ask: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub spread: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub mid_price: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub last_trade_price: Option<f64>,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub last_trade_qty: Option<i64>,
}
