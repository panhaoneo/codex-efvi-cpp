#include "efvi/vi.hpp"

#include <cassert>

int main() {
  efvi::ViConfig cfg = efvi::default_config();
  cfg.ifname = "";
  bool threw = false;
  try {
    efvi::Vi vi(cfg);
  } catch (const efvi::ViException&) {
    threw = true;
  }
  assert(threw);

  cfg = efvi::default_config();
  cfg.rxq_size = 100;
  threw = false;
  try {
    efvi::Vi vi(cfg);
  } catch (const efvi::ViException&) {
    threw = true;
  }
  assert(threw);

  return 0;
}
