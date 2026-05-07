#include "efvi/vi.hpp"

#include <cassert>

int main() {
  efvi::ViConfig cfg = efvi::default_config();
  cfg.ifname = "eth0";
  efvi::Vi vi(cfg);
  assert(vi.nic_info().mtu == 1500);
  vi.send("x", 1);
  assert(vi.stats_snapshot().tx_packets == 1);
  return 0;
}
