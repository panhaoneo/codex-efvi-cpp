#include "efvi/vi_set.hpp"

#include <cassert>

int main() {
  efvi::ViSetConfig cfg;
  cfg.queue_count = 2;
  cfg.queue_configs.push_back(efvi::default_config());
  cfg.queue_configs.push_back(efvi::default_config());

  efvi::ViSet set(cfg);
  assert(set.queue_count() == 2);

  set.vi(0).send("a", 1);
  set.vi(1).send("b", 1);

  efvi::Stats::Snapshot s = set.aggregate_stats();
  assert(s.tx_packets == 2);

  set.reset_all_stats();
  s = set.aggregate_stats();
  assert(s.tx_packets == 0);

  return 0;
}
