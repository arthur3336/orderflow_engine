use std::sync::atomic::Ordering;

use axum::extract::ws::{Message, WebSocket, WebSocketUpgrade};
use axum::extract::State;
use axum::response::IntoResponse;
use tokio::sync::broadcast;

use crate::state::AppState;

const MAX_WS_CONNECTIONS: u64 = 100;

pub async fn ws_upgrade(
    State(state): State<AppState>,
    ws: WebSocketUpgrade,
) -> impl IntoResponse {
    let current = state.ws_connections.load(Ordering::Relaxed);
    if current >= MAX_WS_CONNECTIONS {
        return (
            axum::http::StatusCode::SERVICE_UNAVAILABLE,
            "Too many WebSocket connections",
        )
            .into_response();
    }

    ws.on_upgrade(move |socket| handle_ws(socket, state))
        .into_response()
}

async fn handle_ws(mut socket: WebSocket, state: AppState) {
    state.ws_connections.fetch_add(1, Ordering::Relaxed);
    tracing::info!(
        event = "WsConnected",
        active = state.ws_connections.load(Ordering::Relaxed)
    );

    let mut rx = state.ws_broadcast.subscribe();

    // Forward broadcast messages to the WebSocket client
    loop {
        tokio::select! {
            // Receive from broadcast channel
            msg = rx.recv() => {
                match msg {
                    Ok(text) => {
                        if socket.send(Message::Text(text.into())).await.is_err() {
                            break;
                        }
                    }
                    Err(broadcast::error::RecvError::Lagged(n)) => {
                        tracing::warn!(event = "WsLagged", skipped = n);
                        // Send a lag notification
                        let lag_msg = serde_json::json!({
                            "type": "error",
                            "data": { "message": format!("Missed {} messages", n) }
                        });
                        let _ = socket.send(Message::Text(lag_msg.to_string().into())).await;
                    }
                    Err(broadcast::error::RecvError::Closed) => break,
                }
            }
            // Receive from client (for ping/pong or close)
            client_msg = socket.recv() => {
                match client_msg {
                    Some(Ok(Message::Close(_))) | None => break,
                    Some(Ok(Message::Ping(data))) => {
                        if socket.send(Message::Pong(data)).await.is_err() {
                            break;
                        }
                    }
                    _ => {} // Ignore text/binary from client for now
                }
            }
        }
    }

    state.ws_connections.fetch_sub(1, Ordering::Relaxed);
    tracing::info!(
        event = "WsDisconnected",
        active = state.ws_connections.load(Ordering::Relaxed)
    );
}
