#pragma once
#include <string>
#include <map>
namespace bell {
class MDNSService {
 public:
  static void registerService(const std::string&, const std::string&, const std::string&, const std::string&, unsigned, const std::map<std::string,std::string>&) {}
};
}
