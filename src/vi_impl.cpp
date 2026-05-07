#include "efvi/vi.hpp"

#include <cstring>
#include <deque>
#include <map>
#include <vector>

namespace efvi {

namespace {

void maybe_log(const LogCallback& cb, LogLevel level, const std::string& msg) {
  if (cb) {
    cb(level, msg);
  }
}

void validate_config_or_throw(const ViConfig& cfg) {
  if (cfg.ifname.empty()) throw ViException(-1, "ifname must not be empty");
  if (!is_power_of_two(cfg.rxq_size) || cfg.rxq_size < 64 || cfg.rxq_size > 4096) throw ViException(-4, "rxq_size must be power-of-two in [64, 4096]");
  if (!is_power_of_two(cfg.txq_size) || cfg.txq_size < 64 || cfg.txq_size > 4096) throw ViException(-5, "txq_size must be power-of-two in [64, 4096]");
  const std::size_t refill = cfg.perf.rx_refill_batch;
  if (!(refill == 8 || refill == 16 || refill == 32 || refill == 64)) throw ViException(-6, "rx_refill_batch must be one of 8/16/32/64");
  if (cfg.perf.ctpio_threshold > 1500) throw ViException(-7, "ctpio_threshold must be <= 1500");
}

}  // namespace

struct Vi::Impl {
  ViConfig cfg;
  NicInfo info;
  Stats stats;
  FilterCookie next_cookie;
  std::map<FilterCookie, FilterSpec> filters;
  std::deque<std::vector<char> > rx_queue;

  explicit Impl(const ViConfig& c) : cfg(c), next_cookie(1) {
    info.model = "mock-nic";
    info.driver_version = "mock";
    info.mac_address = "00:00:00:00:00:00";
    info.mtu = 1500;
    info.port_speed_mbps = 10000;
    info.arch = (cfg.ifname.find("efct") != std::string::npos) ? NicArch::EFCT : NicArch::EF10;
  }
};

Vi::Vi(const ViConfig& cfg) : impl_(new Impl(cfg)) {
  validate_config_or_throw(cfg);
  maybe_log(impl_->cfg.logger, LogLevel::INFO, "Vi initialized in mock mode");
}
Vi::~Vi() {}
Vi::Vi(Vi&&) noexcept = default;
Vi& Vi::operator=(Vi&&) noexcept = default;

void Vi::send(const void* buf, std::size_t len) {
  if (buf == 0 || len == 0) {
    impl_->stats.on_tx_error();
    throw ViException(-2, "invalid send buffer");
  }
  if (impl_->rx_queue.size() >= impl_->cfg.rxq_size) {
    impl_->stats.on_tx_drop();
    throw ViException(-8, "mock rx queue full");
  }
  const char* cbuf = static_cast<const char*>(buf);
  impl_->rx_queue.push_back(std::vector<char>(cbuf, cbuf + len));
  impl_->stats.on_tx();
}

void Vi::send_batch(const void* const* bufs, const std::size_t* lens, int count) {
  if (count < 0) throw ViException(-3, "count must be >=0");
  if (impl_->info.arch != NicArch::EF10) throw ViException(-9, "send_batch only supported for EF10");
  for (int i = 0; i < count; ++i) send(bufs[i], lens[i]);
}

int Vi::poll(RxBatch& batch) {
  batch.clear();
  const std::size_t max_take = impl_->cfg.perf.rx_refill_batch;
  std::size_t taken = 0;
  while (taken < max_take && !impl_->rx_queue.empty()) {
    std::vector<char>& p = impl_->rx_queue.front();
    batch.owned_payloads.push_back(p);
    std::vector<char>& owned = batch.owned_payloads.back();
    batch.packets.push_back(PacketRef(owned.empty() ? 0 : static_cast<const void*>(&owned[0]), owned.size(), 0));
    impl_->stats.on_rx();
    ++taken;
    impl_->rx_queue.pop_front();
  }
  return static_cast<int>(batch.packets.size());
}

FilterCookie Vi::add_filter(const FilterSpec& spec) {
  const FilterCookie cookie = impl_->next_cookie++;
  impl_->filters[cookie] = spec;
  maybe_log(impl_->cfg.logger, LogLevel::INFO, "filter added");
  return cookie;
}

void Vi::remove_filter(FilterCookie cookie) {
  if (impl_->filters.erase(cookie) == 0) {
    impl_->stats.on_rx_error();
    maybe_log(impl_->cfg.logger, LogLevel::WARN, "filter cookie not found");
    return;
  }
  maybe_log(impl_->cfg.logger, LogLevel::INFO, "filter removed");
}

NicArch Vi::arch() const { return impl_->info.arch; }
const NicInfo& Vi::nic_info() const { return impl_->info; }
Stats::Snapshot Vi::stats_snapshot() const { return impl_->stats.snapshot(); }
void Vi::reset_stats() { impl_->stats.reset(); }

}  // namespace efvi
