#include "efvi/stats.hpp"

namespace efvi {

void Stats::reset() {
  rx_packets_.store(0, std::memory_order_relaxed);
  tx_packets_.store(0, std::memory_order_relaxed);
  rx_drops_.store(0, std::memory_order_relaxed);
  tx_drops_.store(0, std::memory_order_relaxed);
  rx_errors_.store(0, std::memory_order_relaxed);
  tx_errors_.store(0, std::memory_order_relaxed);
  latency_ns_.store(0, std::memory_order_relaxed);
}

void Stats::on_rx() { rx_packets_.fetch_add(1, std::memory_order_relaxed); }
void Stats::on_tx() { tx_packets_.fetch_add(1, std::memory_order_relaxed); }
void Stats::on_rx_drop() { rx_drops_.fetch_add(1, std::memory_order_relaxed); }
void Stats::on_tx_drop() { tx_drops_.fetch_add(1, std::memory_order_relaxed); }
void Stats::on_rx_error() { rx_errors_.fetch_add(1, std::memory_order_relaxed); }
void Stats::on_tx_error() { tx_errors_.fetch_add(1, std::memory_order_relaxed); }

}  // namespace efvi
