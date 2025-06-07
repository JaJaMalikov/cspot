#pragma once
#include <functional>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
namespace bell {
class Task {
 public:
  Task(const char* name = "task", uint32_t stack = 4096, UBaseType_t prio = 5, int core = tskNO_AFFINITY)
      : name_(name), stack_(stack), prio_(prio), core_(core), handle_(nullptr) {}
  virtual ~Task() { if (handle_) vTaskDelete(handle_); }
  void startTask() {
    xTaskCreatePinnedToCore(&Task::taskFunc, name_, stack_ / sizeof(StackType_t), this, prio_, &handle_, core_);
  }
 protected:
  virtual void runTask() = 0;
 private:
  static void taskFunc(void* arg) { static_cast<Task*>(arg)->runTask(); }
  const char* name_; uint32_t stack_; UBaseType_t prio_; int core_; TaskHandle_t handle_;
};
}
