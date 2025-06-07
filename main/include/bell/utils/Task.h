#pragma once
#include <thread>

namespace bell {
class Task {
 public:
  Task(const char* /*name*/ = "", size_t /*stack*/ = 0) {}
  virtual ~Task() {
    if (task.joinable()) task.join();
  }
  void startTask() { task = std::thread([this] { this->taskLoop(); }); }

 protected:
  virtual void taskLoop() = 0;

 private:
  std::thread task;
};
}  // namespace bell
