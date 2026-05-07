#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace efvi {

class PacketRef {
public:
  PacketRef() : data_(0), len_(0), ts_ns_(0) {}
  PacketRef(const void* data, std::size_t len, uint64_t ts_ns)
      : data_(data), len_(len), ts_ns_(ts_ns) {}

  const void* data() const { return data_; }
  std::size_t len() const { return len_; }
  uint64_t timestamp_ns() const { return ts_ns_; }

private:
  const void* data_;
  std::size_t len_;
  uint64_t ts_ns_;
};

struct RxBatch {
  std::vector<PacketRef> packets;
  std::vector<std::vector<char> > owned_payloads;

  void clear() {
    packets.clear();
    owned_payloads.clear();
  }
};

}  // namespace efvi
