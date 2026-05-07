#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace efvi {

enum class LogLevel { DEBUG, INFO, WARN, ERROR };
typedef std::function<void(LogLevel, const std::string&)> LogCallback;

enum class FilterKind { UDP_V4, TCP_V4, MULTICAST_ALL, MULTICAST_IP, MAC_VLAN };

enum PdFlags {
  EF_PD_DEFAULT = 0,
  EF_PD_VF = 1 << 0,
  EF_PD_PHYS_MODE = 1 << 1
};

struct FilterSpec {
  FilterKind kind;
  std::string local_ip;
  uint16_t local_port;
  std::string multicast_ip;
  std::string mac;
  uint16_t vlan;

  FilterSpec() : kind(FilterKind::UDP_V4), local_port(0), vlan(0) {}
};

typedef uint64_t FilterCookie;

struct PerformanceParams {
  std::size_t rx_refill_batch;
  std::size_t ctpio_threshold;
  std::size_t rx_buffer_align;
  bool hugepage;
};

struct ViConfig {
  std::string ifname;
  std::size_t rxq_size;
  std::size_t txq_size;
  unsigned pd_flags;
  std::vector<FilterSpec> filters;
  LogCallback logger;
  PerformanceParams perf;
};

inline bool is_power_of_two(std::size_t value) {
  return value != 0 && (value & (value - 1)) == 0;
}

inline ViConfig default_config() {
  ViConfig cfg;
  cfg.ifname = "eth0";
  cfg.rxq_size = 512;
  cfg.txq_size = 512;
  cfg.pd_flags = EF_PD_DEFAULT;
  cfg.perf = PerformanceParams{32, 64, 4u * 1024u * 1024u, false};
  return cfg;
}

}  // namespace efvi
