#include "efvi/vi.hpp"

#include <cassert>

int main() {
  efvi::Vi vi(efvi::default_config());

  const char a[] = "abc";
  vi.send(a, 3);
  efvi::RxBatch batch;
  int n = vi.poll(batch);
  assert(n == 1);
  assert(batch.packets.size() == 1);

  const void* bufs[2] = {"1", "2"};
  std::size_t lens[2] = {1, 1};
  vi.send_batch(bufs, lens, 2);
  n = vi.poll(batch);
  assert(n == 2);

  efvi::Stats::Snapshot s = vi.stats_snapshot();
  assert(s.tx_packets == 3);
  assert(s.rx_packets == 3);
  return 0;
}
