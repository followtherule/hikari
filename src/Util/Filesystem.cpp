#include "Util/Filesystem.h"
#include "Util/Assert.h"

#include <fstream>

namespace hkr {

std::vector<char> ReadFile(const std::string& filename) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  HKR_ASSERT(file.is_open());

  size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);

  file.close();

  return buffer;
}

}  // namespace hkr
