#include "StringUtil.h"
#include <codecvt>
#include <assert.h>

std::vector<std::string> StringUtil::Split(const std::string& text, const std::string& separator) {
  std::vector<std::string> result;
  std::string::size_type pos1, pos2;
  pos2 = text.find(separator);
  pos1 = 0;
  while (std::string::npos != pos2) {
    result.push_back(text.substr(pos1, pos2 - pos1));

    pos1 = pos2 + separator.size();
    pos2 = text.find(separator, pos1);
  }
  if (pos1 != text.length()) {
    result.push_back(text.substr(pos1));
  }
  return result;
}

std::string StringUtil::ReplaceAll(const std::string& text, const std::string& from, const std::string& to) {
  auto str = text;
  size_t start_pos = 0;
  while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
    str.replace(start_pos, from.length(), to);
    start_pos += to.length();
  }
  return str;
}

std::string StringUtil::ToLowerCase(const std::string& text) {
  std::string result;
  result.resize(text.length());
  std::transform(text.begin(), text.end(), result.begin(), ::tolower);
  return result;
}

std::string StringUtil::ToUpperCase(const std::string& text) {
  std::string result;
  result.resize(text.length());
  std::transform(text.begin(), text.end(), result.begin(), ::toupper);
  return result;
}

static bool LastIsSpace(const std::string& text) {
  if (text.length() == 0) {
    return false;
  }
  auto last = text.substr(text.length()-1, 1);
  if (last == " " || last == "\n") {
    return true;
  }
  return false;
}

std::string StringUtil::DeleteLastSpace(const std::string& text) {
  std::string result = text;
  while (LastIsSpace(result)) {
    result = result.substr(0, result.length()-1);
  }
  return result;
}

std::string StringUtil::ToString(const AEGP_SuiteHandler& suites, AEGP_MemHandle memHandle) {
  char16_t* name = nullptr;
  suites.MemorySuite1()->AEGP_LockMemHandle(memHandle, reinterpret_cast<void**>(&name));

  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
  std::string text = convert.to_bytes(name); // utf-16 to utf-8

  suites.MemorySuite1()->AEGP_UnlockMemHandle(memHandle);
  return text;
}
