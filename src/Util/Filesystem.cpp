#include "Util/Filesystem.h"
#include "Util/Assert.h"

#include <fstream>

namespace hkr {

std::vector<char> ReadFile(const std::string& fileName) {
  std::ifstream file(fileName, std::ios::ate | std::ios::binary);

  HKR_ASSERT(file.is_open());

  const size_t fileSize = (size_t)file.tellg();
  std::vector<char> buffer(fileSize);

  file.seekg(0);
  file.read(buffer.data(), fileSize);

  file.close();

  return buffer;
}

std::string GetFilePath(const std::string& fileName) {
  const size_t pos = fileName.find_last_of('/');
  std::string filePath = fileName.substr(0, pos);
  return filePath;
}

std::string GetFileExtension(const std::string& fileName) {
  const size_t extensionPos = fileName.find_last_of(".");
  HKR_ASSERT(extensionPos != std::string::npos);
  std::string fileExtension = fileName.substr(extensionPos + 1);
  return fileExtension;
}

}  // namespace hkr
