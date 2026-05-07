#include "efvi/vi.hpp"

#include <cassert>
#include <string>
#include <vector>

int main() {
  std::vector<std::string> logs;
  efvi::ViConfig cfg = efvi::default_config();
  cfg.logger = [&](efvi::LogLevel, const std::string& msg) { logs.push_back(msg); };

  efvi::Vi vi(cfg);
  assert(!logs.empty());

  efvi::FilterSpec spec;
  spec.kind = efvi::FilterKind::UDP_V4;
  vi.add_filter(spec);
  vi.remove_filter(1);
  assert(logs.size() >= 3);

  return 0;
}
