use axum::extract::State;
use axum::Json;

use crate::models::market::MarketSnapshot;
use crate::state::AppState;

pub async fn get_market_snapshot(
    State(state): State<AppState>,
) -> Json<MarketSnapshot> {
    Json(state.engine.get_snapshot().await)
}
