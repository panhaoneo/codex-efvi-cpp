#pragma once

#include <string>

namespace efvi {

enum class NicArch { EF10, EFCT };

struct NicInfo {
  std::string model;
  std::string driver_version;
  std::string mac_address;
  unsigned mtu;
  unsigned port_speed_mbps;
  NicArch arch;
};

}  // namespace efvi
