#ifndef UTILS_FILE_H_
#define UTILS_FILE_H_

#include <fstream>

class FileIO {
 public:
  static std::string ReadTextFile(const char* filename) {
    std::ifstream inputStream(filename);
    std::string text((std::istreambuf_iterator<char>(inputStream)),
                     (std::istreambuf_iterator<char>()));
    inputStream.close();
    return text;
  }

  static int WriteTextFile(const char* filename, const char* text) {
    return WriteTextFileInternal(filename, text, "wt");
  }

  static int WriteTextFile(const char* filename, const std::string text) {
    return WriteTextFile(filename, text.c_str());
  }

  static int AddTextFile(const char* filename, const char* text) {
    return WriteTextFileInternal(filename, text, "a");
  }

  static int AddTextFile(const char* filename, const std::string text) {
    return AddTextFile(filename, text.c_str());
  }

  static int GetFileSize(const std::string fileName) {
    FILE* fp = nullptr;
    if (fopen_s(&fp, fileName.c_str(), "rb") != 0 || fp == nullptr) {
      return 0;
    }
    fseek(fp, 0, SEEK_END);
    auto size = ftell(fp);
    fclose(fp);
    return static_cast<int>(size);
  }
 private:
  static int WriteTextFileInternal(const char* filename, const char* text, const char* mode) {
    int len = 0;
    FILE* fp = nullptr;
    if (fopen_s(&fp, filename, mode) == 0 && fp != nullptr) {
      len = fprintf(fp, "%s", text);
      fclose(fp);
    }
    return len;
  }
};

#endif  // UTILS_FILE_H_
