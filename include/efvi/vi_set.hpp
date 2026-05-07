#pragma once

#include <memory>
#include <vector>

#include "efvi/vi.hpp"

namespace efvi {

struct ViSetConfig {
  int queue_count;
  std::vector<ViConfig> queue_configs;
};

class ViSet {
public:
  explicit ViSet(const ViSetConfig& cfg);
  ~ViSet();

  Vi& vi(int queue_idx);
  int queue_count() const;

  Stats::Snapshot aggregate_stats() const;
  void reset_all_stats();

private:
  std::vector<std::unique_ptr<Vi> > vis_;
};

}  // namespace efvi
