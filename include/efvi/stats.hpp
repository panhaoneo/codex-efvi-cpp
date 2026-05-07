#pragma once

#include <atomic>
#include <cstdint>

namespace efvi {

class Stats {
public:
  struct Snapshot {
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_drops;
    uint64_t tx_drops;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t latency_ns;
  };

  Snapshot snapshot() const {
    return Snapshot{rx_packets_.load(std::memory_order_relaxed),
                    tx_packets_.load(std::memory_order_relaxed),
                    rx_drops_.load(std::memory_order_relaxed),
                    tx_drops_.load(std::memory_order_relaxed),
                    rx_errors_.load(std::memory_order_relaxed),
                    tx_errors_.load(std::memory_order_relaxed),
                    latency_ns_.load(std::memory_order_relaxed)};
  }

  void reset();
  void on_rx();
  void on_tx();
  void on_rx_drop();
  void on_tx_drop();
  void on_rx_error();
  void on_tx_error();

private:
  std::atomic<uint64_t> rx_packets_{0};
  std::atomic<uint64_t> tx_packets_{0};
  std::atomic<uint64_t> rx_drops_{0};
  std::atomic<uint64_t> tx_drops_{0};
  std::atomic<uint64_t> rx_errors_{0};
  std::atomic<uint64_t> tx_errors_{0};
  std::atomic<uint64_t> latency_ns_{0};
};

}  // namespace efvi
