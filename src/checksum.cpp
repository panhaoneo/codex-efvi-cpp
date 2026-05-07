#include <cstddef>
#include <cstdint>

namespace efvi {

uint16_t ip_checksum(const void* data, std::size_t len) {
  const uint8_t* p = static_cast<const uint8_t*>(data);
  uint32_t sum = 0;
  for (std::size_t i = 0; i + 1 < len; i += 2) {
    sum += (static_cast<uint32_t>(p[i]) << 8) | p[i + 1];
  }
  if ((len & 1) != 0) {
    sum += static_cast<uint32_t>(p[len - 1]) << 8;
  }
  while ((sum >> 16) != 0) {
    sum = (sum & 0xFFFFu) + (sum >> 16);
  }
  return static_cast<uint16_t>(~sum);
}

}  // namespace efvi
