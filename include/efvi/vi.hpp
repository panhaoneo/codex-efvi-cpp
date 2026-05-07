#pragma once

#include <memory>

#include "efvi/config.hpp"
#include "efvi/exception.hpp"
#include "efvi/nic_info.hpp"
#include "efvi/packet.hpp"
#include "efvi/stats.hpp"

namespace efvi {

class Vi {
public:
  explicit Vi(const ViConfig& cfg);
  ~Vi();
  Vi(const Vi&) = delete;
  Vi& operator=(const Vi&) = delete;
  Vi(Vi&&) noexcept;
  Vi& operator=(Vi&&) noexcept;

  void send(const void* buf, std::size_t len);
  void send_batch(const void* const* bufs, const std::size_t* lens, int count);
  int poll(RxBatch& batch);

  FilterCookie add_filter(const FilterSpec& spec);
  void remove_filter(FilterCookie cookie);

  NicArch arch() const;
  const NicInfo& nic_info() const;
  Stats::Snapshot stats_snapshot() const;
  void reset_stats();

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace efvi
