#include "efvi/vi_set.hpp"

#include "efvi/exception.hpp"

namespace efvi {

ViSet::ViSet(const ViSetConfig& cfg) {
  if (cfg.queue_count <= 0) {
    throw ViException(-10, "queue_count must be positive");
  }
  if (!cfg.queue_configs.empty() && static_cast<int>(cfg.queue_configs.size()) != cfg.queue_count) {
    throw ViException(-11, "queue_configs size mismatch");
  }

  for (int i = 0; i < cfg.queue_count; ++i) {
    ViConfig vi_cfg = cfg.queue_configs.empty() ? default_config() : cfg.queue_configs[i];
    vis_.push_back(std::unique_ptr<Vi>(new Vi(vi_cfg)));
  }
}

ViSet::~ViSet() {}

Vi& ViSet::vi(int queue_idx) {
  if (queue_idx < 0 || queue_idx >= static_cast<int>(vis_.size())) {
    throw ViException(-12, "queue index out of range");
  }
  return *vis_[queue_idx];
}

int ViSet::queue_count() const { return static_cast<int>(vis_.size()); }

Stats::Snapshot ViSet::aggregate_stats() const {
  Stats::Snapshot out = {0, 0, 0, 0, 0, 0, 0};
  for (std::size_t i = 0; i < vis_.size(); ++i) {
    const Stats::Snapshot s = vis_[i]->stats_snapshot();
    out.rx_packets += s.rx_packets;
    out.tx_packets += s.tx_packets;
    out.rx_drops += s.rx_drops;
    out.tx_drops += s.tx_drops;
    out.rx_errors += s.rx_errors;
    out.tx_errors += s.tx_errors;
    out.latency_ns += s.latency_ns;
  }
  return out;
}

void ViSet::reset_all_stats() {
  for (std::size_t i = 0; i < vis_.size(); ++i) {
    vis_[i]->reset_stats();
  }
}

}  // namespace efvi
