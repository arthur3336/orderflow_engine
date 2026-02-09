use axum::extract::{Path, State};
use axum::http::StatusCode;
use axum::Json;

use crate::models::error::ApiError;
use crate::models::order::*;
use crate::state::AppState;

pub async fn submit_order(
    State(state): State<AppState>,
    Json(req): Json<OrderRequest>,
) -> Result<(StatusCode, Json<OrderResponse>), ApiError> {
    let response = state.order_service.submit_order(req).await?;
    Ok((StatusCode::CREATED, Json(response)))
}

pub async fn modify_order(
    State(state): State<AppState>,
    Path(order_id): Path<u64>,
    Json(req): Json<ModifyRequest>,
) -> Result<Json<ModifyResponse>, ApiError> {
    let response = state.order_service.modify_order(order_id, req).await?;
    Ok(Json(response))
}

pub async fn cancel_order(
    State(state): State<AppState>,
    Path(order_id): Path<u64>,
) -> Result<Json<CancelResponse>, ApiError> {
    let response = state.order_service.cancel_order(order_id).await?;
    Ok(Json(response))
}
