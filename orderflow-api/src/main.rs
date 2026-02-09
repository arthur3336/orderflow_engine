mod config;
mod engine;
mod ffi;
mod handlers;
mod middleware;
mod models;
mod services;
mod state;

use axum::routing::{get, post, put};
use axum::Router;
use metrics_exporter_prometheus::PrometheusBuilder;
use tower_http::cors::CorsLayer;
use tower_http::trace::TraceLayer;
use tracing_subscriber::EnvFilter;

use config::Config;
use state::AppState;

#[tokio::main]
async fn main() {
    // Initialize tracing (JSON format when RUST_LOG_JSON=1)
    let json_output = std::env::var("RUST_LOG_JSON").unwrap_or_default() == "1";
    if json_output {
        tracing_subscriber::fmt()
            .json()
            .with_env_filter(EnvFilter::try_from_default_env().unwrap_or_else(|_| "info".into()))
            .init();
    } else {
        tracing_subscriber::fmt()
            .with_env_filter(EnvFilter::try_from_default_env().unwrap_or_else(|_| "info".into()))
            .init();
    }

    // Initialize Prometheus metrics recorder
    let prom_handle = PrometheusBuilder::new()
        .install_recorder()
        .expect("Failed to install Prometheus recorder");

    let config = Config::load();
    let bind_addr = format!("{}:{}", config.server.host, config.server.port);

    let state = AppState::new(&config);

    let app = Router::new()
        .route("/api/v1/orders", post(handlers::orders::submit_order))
        .route(
            "/api/v1/orders/:id",
            put(handlers::orders::modify_order).delete(handlers::orders::cancel_order),
        )
        .route("/api/v1/market", get(handlers::market::get_market_snapshot))
        .route("/api/v1/health", get(handlers::health::health_check))
        .route("/api/v1/ws", get(handlers::websocket::ws_upgrade))
        .route(
            "/metrics",
            get(move || {
                let handle = prom_handle.clone();
                async move { handle.render() }
            }),
        )
        .layer(TraceLayer::new_for_http())
        .layer(CorsLayer::permissive())
        .with_state(state);

    tracing::info!("OrderFlow API listening on {}", bind_addr);
    tracing::info!(
        "Risk config: size [{}, {}], band ±{:.1}%, position ±{}, rate {}/s",
        config.risk.min_order_size,
        config.risk.max_order_size,
        config.risk.price_band_percent,
        config.risk.max_position_per_trader,
        config.risk.max_orders_per_second
    );

    let listener = tokio::net::TcpListener::bind(&bind_addr).await.unwrap();
    axum::serve(listener, app)
        .with_graceful_shutdown(shutdown_signal())
        .await
        .unwrap();
}

async fn shutdown_signal() {
    tokio::signal::ctrl_c()
        .await
        .expect("Failed to install CTRL+C handler");
    tracing::info!("Shutting down...");
}
