use metrics::{counter, histogram};
use std::time::Instant;

pub fn record_order_submitted(side: &str, order_type: &str) {
    counter!("orderflow_orders_total", "side" => side.to_string(), "order_type" => order_type.to_string(), "result" => "submitted").increment(1);
}

pub fn record_order_accepted(side: &str, order_type: &str) {
    counter!("orderflow_orders_total", "side" => side.to_string(), "order_type" => order_type.to_string(), "result" => "accepted").increment(1);
}

pub fn record_order_rejected(reason: &str) {
    counter!("orderflow_risk_rejections_total", "reason" => reason.to_string()).increment(1);
}

pub fn record_trades(count: u64) {
    counter!("orderflow_trades_total").increment(count);
}

pub fn record_order_latency(start: Instant) {
    let duration = start.elapsed().as_secs_f64();
    histogram!("orderflow_order_latency_seconds").record(duration);
}

pub fn record_engine_latency(start: Instant) {
    let duration = start.elapsed().as_secs_f64();
    histogram!("orderflow_engine_latency_seconds").record(duration);
}
