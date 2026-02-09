use serde::Deserialize;
use std::path::Path;

#[derive(Debug, Clone, Deserialize)]
pub struct Config {
    #[serde(default)]
    pub server: ServerConfig,
    #[serde(default)]
    pub risk: RiskConfig,
}

#[derive(Debug, Clone, Deserialize)]
pub struct ServerConfig {
    #[serde(default = "default_host")]
    pub host: String,
    #[serde(default = "default_port")]
    pub port: u16,
}

#[derive(Debug, Clone, Deserialize)]
pub struct RiskConfig {
    #[serde(default = "default_min_order_size")]
    pub min_order_size: i64,
    #[serde(default = "default_max_order_size")]
    pub max_order_size: i64,
    #[serde(default = "default_price_band_percent")]
    pub price_band_percent: f64,
    #[serde(default = "default_max_position_per_trader")]
    pub max_position_per_trader: i64,
    #[serde(default = "default_max_orders_per_second")]
    pub max_orders_per_second: u32,
}

fn default_host() -> String {
    "0.0.0.0".into()
}
fn default_port() -> u16 {
    8080
}
fn default_min_order_size() -> i64 {
    1
}
fn default_max_order_size() -> i64 {
    100_000
}
fn default_price_band_percent() -> f64 {
    10.0
}
fn default_max_position_per_trader() -> i64 {
    1_000_000
}
fn default_max_orders_per_second() -> u32 {
    100
}

impl Default for ServerConfig {
    fn default() -> Self {
        Self {
            host: default_host(),
            port: default_port(),
        }
    }
}

impl Default for RiskConfig {
    fn default() -> Self {
        Self {
            min_order_size: default_min_order_size(),
            max_order_size: default_max_order_size(),
            price_band_percent: default_price_band_percent(),
            max_position_per_trader: default_max_position_per_trader(),
            max_orders_per_second: default_max_orders_per_second(),
        }
    }
}

impl Default for Config {
    fn default() -> Self {
        Self {
            server: ServerConfig::default(),
            risk: RiskConfig::default(),
        }
    }
}

impl Config {
    pub fn load() -> Self {
        let path = Path::new("config.toml");
        if path.exists() {
            match std::fs::read_to_string(path) {
                Ok(contents) => match toml::from_str(&contents) {
                    Ok(config) => {
                        tracing::info!("Loaded config from config.toml");
                        return config;
                    }
                    Err(e) => {
                        tracing::warn!("Failed to parse config.toml: {}, using defaults", e);
                    }
                },
                Err(e) => {
                    tracing::warn!("Failed to read config.toml: {}, using defaults", e);
                }
            }
        } else {
            tracing::info!("No config.toml found, using defaults");
        }
        Self::default()
    }
}
