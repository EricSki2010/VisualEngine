#pragma once

#include <string>
#include <vector>

void setMemoryPath(const std::string& dir);
bool saveToMemory(const std::string& name, const std::string& formatPath,
                  const std::vector<std::string>& data);
std::vector<std::string> loadFromMemory(const std::string& name, const std::string& formatPath);
