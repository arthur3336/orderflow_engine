use axum::http::StatusCode;
use axum::response::{IntoResponse, Response};
use axum::Json;
use serde::Serialize;

#[derive(Debug, thiserror::Error)]
pub enum ApiError {
    #[error("Validation error: {0}")]
    Validation(String),

    #[error("Order not found: {0}")]
    NotFound(u64),

    #[error("Engine rejected: {0}")]
    EngineRejection(String),

    #[error("Risk rejection: {0}")]
    RiskRejection(String),

    #[error("Rate limited: {0}")]
    RateLimited(String),

    #[error("Internal error: {0}")]
    Internal(String),
}

#[derive(Serialize)]
struct ErrorBody {
    error: String,
    code: u16,
}

impl IntoResponse for ApiError {
    fn into_response(self) -> Response {
        let (status, message) = match &self {
            ApiError::Validation(msg) => (StatusCode::BAD_REQUEST, msg.clone()),
            ApiError::NotFound(id) => (StatusCode::NOT_FOUND, format!("Order {} not found", id)),
            ApiError::EngineRejection(msg) => (StatusCode::CONFLICT, msg.clone()),
            ApiError::RiskRejection(msg) => (StatusCode::UNPROCESSABLE_ENTITY, msg.clone()),
            ApiError::RateLimited(msg) => (StatusCode::TOO_MANY_REQUESTS, msg.clone()),
            ApiError::Internal(msg) => (StatusCode::INTERNAL_SERVER_ERROR, msg.clone()),
        };

        let body = ErrorBody {
            error: message,
            code: status.as_u16(),
        };

        (status, Json(body)).into_response()
    }
}
