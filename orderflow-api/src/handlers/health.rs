use axum::extract::State;
use axum::Json;
use serde::Serialize;

use crate::state::AppState;

#[derive(Serialize)]
#[serde(rename_all = "camelCase")]
pub struct HealthResponse {
    pub status: &'static str,
    pub uptime_seconds: u64,
    pub total_orders: u64,
    pub total_trades: u64,
}

pub async fn health_check(
    State(state): State<AppState>,
) -> Json<HealthResponse> {
    Json(HealthResponse {
        status: "healthy",
        uptime_seconds: state.start_time.elapsed().as_secs(),
        total_orders: state.engine.total_orders(),
        total_trades: state.engine.total_trades(),
    })
}
