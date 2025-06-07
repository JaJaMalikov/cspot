#pragma once
#include <condition_variable>
#include <mutex>
#include <chrono>

namespace bell {
class Semaphore {
 public:
  explicit Semaphore(int count = 0) : count(count) {}
  void give() {
    std::lock_guard<std::mutex> lock(mtx);
    ++count;
    cv.notify_one();
  }
  bool take(int timeout_ms) {
    std::unique_lock<std::mutex> lock(mtx);
    if (timeout_ms < 0) {
      cv.wait(lock, [&] { return count > 0; });
    } else {
      if (!cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] { return count > 0; })) {
        return false;
      }
    }
    --count;
    return true;
  }
 private:
  std::mutex mtx;
  std::condition_variable cv;
  int count;
};
}  // namespace bell
