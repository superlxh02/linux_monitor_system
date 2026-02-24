#pragma once

#include "fastlog/fastlog.hpp"
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr char kWorkerLoggerName[] = "worker_file_logger";
}

namespace monitor {
// 读取文件类
class ReadFile {
public:
  ReadFile(const std::string &name) : _file_stream(name) {}

  ~ReadFile() {
    if (_file_stream.is_open())
      _file_stream.close();
  }
  // 读取一行并将其分割成单词存储在args中
  bool read_line(std::vector<std::string> *args) {
    std::string line;
    std::getline(_file_stream, line);
    if (_file_stream.eof() || line.empty()) {
      auto* log = fastlog::file::get_logger(kWorkerLoggerName);
      if (log) log->debug("ReadFile: end of file or empty line");
      // fastlog::console.debug("ReadFile: end of file or empty line");
      return false;
    }
    std::istringstream line_stream(line);
    while (!line_stream.eof()) {
      std::string word;
      line_stream >> word;
      args->push_back(word);
    }
    return true;
  }

private:
  std::ifstream _file_stream;
};

} // namespace monitor