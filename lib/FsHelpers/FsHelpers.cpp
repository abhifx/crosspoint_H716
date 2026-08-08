#include "FsHelpers.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string_view>
#include <vector>

namespace FsHelpers {

namespace {
bool isHexDigit(const char c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

uint8_t hexValue(const char c) {
  if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
  if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(10 + (c - 'a'));
  return static_cast<uint8_t>(10 + (c - 'A'));
}
}  // namespace

std::string decodeUriEscapes(const std::string& path) {
  std::string decoded;
  decoded.reserve(path.size());

  for (size_t i = 0; i < path.size(); i++) {
    if (path[i] == '%' && i + 2 < path.size() && isHexDigit(path[i + 1]) && isHexDigit(path[i + 2])) {
      const uint8_t value = static_cast<uint8_t>((hexValue(path[i + 1]) << 4) | hexValue(path[i + 2]));
      decoded += static_cast<char>(value);
      i += 2;
      continue;
    }

    decoded += path[i];
  }

  return decoded;
}

std::string normalisePath(const std::string& path) {
  std::vector<std::string_view> components;
  components.reserve(8);  // Eight nested folders is more than we might expect

  size_t start = 0;
  for (size_t i = 0; i <= path.length(); ++i) {
    if (i == path.length() || path[i] == '/') {
      if (i > start) {
        std::string_view component(path.data() + start, i - start);
        if (component == "..") {
          if (!components.empty()) {
            components.pop_back();
          }
        } else if (component != ".") {
          components.push_back(component);
        }
      }
      start = i + 1;
    }
  }

  if (components.empty()) {
    return (path.length() > 0 && path[0] == '/') ? "/" : "";
  }

  std::string result;
  if (path.length() > 0 && path[0] == '/') {
    result += '/';
  }

  for (size_t i = 0; i < components.size(); ++i) {
    result += components[i];
    if (i < components.size() - 1) {
      result += '/';
    }
  }
  return result;
}

bool naturalLess(const std::string& str1, const std::string& str2) {
  const char* p1 = str1.c_str();
  const char* p2 = str2.c_str();

  while (*p1 && *p2) {
    if (isdigit(*p1) && isdigit(*p2)) {
      char* end1;
      char* end2;
      unsigned long n1 = strtoul(p1, &end1, 10);
      unsigned long n2 = strtoul(p2, &end2, 10);

      if (n1 != n2) return n1 < n2;

      p1 = end1;
      p2 = end2;
    } else {
      char c1 = tolower(static_cast<unsigned char>(*p1));
      char c2 = tolower(static_cast<unsigned char>(*p2));
      if (c1 != c2) return c1 < c2;
      p1++;
      p2++;
    }
  }
  return *p2 != '\0';
}

void sortFileList(std::vector<std::string>& files) {
  std::sort(files.begin(), files.end(), [](const std::string& a, const std::string& b) {
    bool aDir = !a.empty() && a.back() == '/';
    bool bDir = !b.empty() && b.back() == '/';
    if (aDir != bDir) return aDir;
    return naturalLess(a, b);
  });
}

bool checkFileExtension(std::string_view fileName, const char* extension) {
  const size_t extLen = strlen(extension);
  if (fileName.length() < extLen) {
    return false;
  }

  const size_t offset = fileName.length() - extLen;
  for (size_t i = 0; i < extLen; i++) {
    if (tolower(static_cast<unsigned char>(fileName[offset + i])) !=
        tolower(static_cast<unsigned char>(extension[i]))) {
      return false;
    }
  }
  return true;
}

bool hasJpgExtension(std::string_view fileName) {
  return checkFileExtension(fileName, ".jpg") || checkFileExtension(fileName, ".jpeg");
}

bool hasPngExtension(std::string_view fileName) { return checkFileExtension(fileName, ".png"); }

bool hasBmpExtension(std::string_view fileName) { return checkFileExtension(fileName, ".bmp"); }

bool hasGifExtension(std::string_view fileName) { return checkFileExtension(fileName, ".gif"); }

bool hasEpubExtension(std::string_view fileName) { return checkFileExtension(fileName, ".epub"); }

bool hasXtcExtension(std::string_view fileName) {
  return checkFileExtension(fileName, ".xtc") || checkFileExtension(fileName, ".xtch");
}

bool hasTxtExtension(std::string_view fileName) { return checkFileExtension(fileName, ".txt"); }

bool hasMarkdownExtension(std::string_view fileName) { return checkFileExtension(fileName, ".md"); }

bool hasCssExtension(std::string_view fileName) { return checkFileExtension(fileName, ".css"); }

std::string extractFolderPath(const std::string& filePath) {
  const auto lastSlash = filePath.find_last_of('/');
  if (lastSlash == std::string::npos || lastSlash == 0) {
    return "/";
  }
  return filePath.substr(0, lastSlash);
}

void sanitizePathComponentForFat32(const char* input, char* output, size_t maxLen) {
  if (maxLen == 0) {
    return;
  }

  size_t i = 0;
  for (; i < maxLen - 1 && input[i] != '\0'; i++) {
    const char c = input[i];
    if (c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' ||
        c == ' ' || (c > 0x00 && c <= 0x1f)) {
      output[i] = '-';
    } else {
      output[i] = c;
    }
  }
  output[i] = '\0';
}

}  // namespace FsHelpers
