#!/usr/bin/env python3
"""
Simple price visualization for orderbook data.
Reads price_history.csv and generates charts.
"""

import pandas as pd
import matplotlib.pyplot as plt

# Read CSV
df = pd.read_csv('price_history.csv')

# Convert prices from cents to dollars
for col in ['bid', 'ask', 'mid', 'spread', 'last_price']:
    df[col] = df[col] / 100.0

# Convert timestamp from nanoseconds to milliseconds
df['time_ms'] = df['timestamp_ns'] / 1_000_000

# Create figure with subplots
fig, axes = plt.subplots(3, 1, figsize=(10, 8), sharex=True)

# Plot 1: Bid/Ask/Mid prices
ax1 = axes[0]
ax1.plot(df['time_ms'], df['bid'], label='Bid', color='green', marker='o')
ax1.plot(df['time_ms'], df['ask'], label='Ask', color='red', marker='o')
ax1.plot(df['time_ms'], df['mid'], label='Mid', color='blue', linestyle='--', marker='o')
ax1.fill_between(df['time_ms'], df['bid'], df['ask'], alpha=0.2, color='gray')
ax1.set_ylabel('Price ($)')
ax1.set_title('Order Book Prices Over Time')
ax1.legend()
ax1.grid(True, alpha=0.3)

# Plot 2: Spread
ax2 = axes[1]
ax2.plot(df['time_ms'], df['spread'], color='purple', marker='o')
ax2.fill_between(df['time_ms'], 0, df['spread'], alpha=0.3, color='purple')
ax2.set_ylabel('Spread ($)')
ax2.set_title('Bid-Ask Spread')
ax2.grid(True, alpha=0.3)

# Plot 3: Last trade price and quantity
ax3 = axes[2]
# Only show where there was a trade (last_price > 0)
trade_mask = df['last_price'] > 0
ax3.scatter(df.loc[trade_mask, 'time_ms'], df.loc[trade_mask, 'last_price'],
            s=df.loc[trade_mask, 'last_qty'], alpha=0.6, color='orange', label='Trade (size=qty)')
ax3.set_xlabel('Time (ms)')
ax3.set_ylabel('Trade Price ($)')
ax3.set_title('Trade Executions (bubble size = quantity)')
ax3.legend()
ax3.grid(True, alpha=0.3)

plt.tight_layout()
plt.savefig('price_chart.png', dpi=150)
print("Saved chart to price_chart.png")
plt.show()
