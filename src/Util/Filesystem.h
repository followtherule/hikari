#pragma once

#include <vector>
#include <string>

namespace hkr {

std::vector<char> ReadFile(const std::string& fileName);

std::string GetFilePath(const std::string& fileName);

std::string GetFileExtension(const std::string& fileName);

}  // namespace hkr
