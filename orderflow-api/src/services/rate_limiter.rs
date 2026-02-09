use std::num::NonZeroU32;
use std::sync::Arc;

use dashmap::DashMap;
use governor::clock::DefaultClock;
use governor::state::{InMemoryState, NotKeyed};
use governor::{Quota, RateLimiter as GovRateLimiter};

use crate::models::error::ApiError;

type Limiter = GovRateLimiter<NotKeyed, InMemoryState, DefaultClock>;

pub struct RateLimiterService {
    limiters: DashMap<String, Arc<Limiter>>,
    max_per_second: NonZeroU32,
}

impl RateLimiterService {
    pub fn new(max_orders_per_second: u32) -> Self {
        let max = NonZeroU32::new(max_orders_per_second.max(1)).unwrap();
        Self {
            limiters: DashMap::new(),
            max_per_second: max,
        }
    }

    pub fn check_rate_limit(&self, trader_id: &str) -> Result<(), ApiError> {
        let limiter = self
            .limiters
            .entry(trader_id.to_string())
            .or_insert_with(|| {
                Arc::new(GovRateLimiter::direct(Quota::per_second(
                    self.max_per_second,
                )))
            })
            .clone();

        match limiter.check() {
            Ok(_) => Ok(()),
            Err(_) => Err(ApiError::RateLimited(format!(
                "Rate limit exceeded for trader '{}' (max {} orders/sec)",
                trader_id, self.max_per_second
            ))),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_rate_limiter_allows_within_limit() {
        let rl = RateLimiterService::new(10);
        // First request should always pass
        assert!(rl.check_rate_limit("alice").is_ok());
    }

    #[test]
    fn test_rate_limiter_blocks_burst() {
        // Allow only 1 per second
        let rl = RateLimiterService::new(1);
        // First should pass
        assert!(rl.check_rate_limit("alice").is_ok());
        // Second immediate request should be rate limited
        assert!(rl.check_rate_limit("alice").is_err());
    }

    #[test]
    fn test_rate_limiter_per_trader() {
        let rl = RateLimiterService::new(1);
        assert!(rl.check_rate_limit("alice").is_ok());
        // Different trader has their own bucket
        assert!(rl.check_rate_limit("bob").is_ok());
        // Alice is still rate limited
        assert!(rl.check_rate_limit("alice").is_err());
    }
}
