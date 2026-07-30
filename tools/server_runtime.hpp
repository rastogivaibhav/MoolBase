#pragma once

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <deque>
#include <functional>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace graphenedb::server {

struct ConnectionTask {
  int fd{-1};
  std::string peer_ip;
};

struct RuntimeMetrics {
  std::atomic<uint64_t> requests{0};
  std::atomic<uint64_t> completed{0};
  std::atomic<uint64_t> active{0};
  std::atomic<uint64_t> queue_depth{0};
  std::atomic<uint64_t> queue_rejected{0};
  std::atomic<uint64_t> rate_limited{0};
  std::atomic<uint64_t> auth_failures{0};
  std::atomic<uint64_t> request_bytes{0};
  std::atomic<uint64_t> response_bytes{0};
  std::atomic<uint64_t> duration_us_sum{0};
  std::atomic<uint64_t> duration_us_max{0};
  std::atomic<uint64_t> status_2xx{0};
  std::atomic<uint64_t> status_4xx{0};
  std::atomic<uint64_t> status_5xx{0};
  std::atomic<uint64_t> node_inserts{0};
  std::atomic<uint64_t> edge_inserts{0};
  std::atomic<uint64_t> vector_searches{0};
  std::atomic<uint64_t> lattice_searches{0};
  std::atomic<uint64_t> validation_calls{0};
  std::atomic<uint64_t> checkpoint_calls{0};
  std::atomic<uint64_t> backup_calls{0};
  std::atomic<uint64_t> errors{0};
  std::chrono::steady_clock::time_point started{std::chrono::steady_clock::now()};
};

inline void atomic_max(std::atomic<uint64_t>& target, uint64_t value) {
  uint64_t current = target.load(std::memory_order_relaxed);
  while (current < value && !target.compare_exchange_weak(current, value, std::memory_order_relaxed)) {}
}

class BoundedWorkerPool {
 public:
  using Handler = std::function<void(ConnectionTask)>;

  BoundedWorkerPool(size_t workers, size_t capacity, Handler handler, RuntimeMetrics& metrics)
      : capacity_(std::max<size_t>(1, capacity)), handler_(std::move(handler)), metrics_(metrics) {
    workers = std::max<size_t>(1, workers);
    threads_.reserve(workers);
    for (size_t i = 0; i < workers; ++i) {
      threads_.emplace_back([this] { run(); });
    }
  }

  BoundedWorkerPool(const BoundedWorkerPool&) = delete;
  BoundedWorkerPool& operator=(const BoundedWorkerPool&) = delete;

  ~BoundedWorkerPool() { shutdown(); }

  bool submit(ConnectionTask task) {
    std::lock_guard<std::mutex> lock(mu_);
    if (stopping_ || queue_.size() >= capacity_) {
      metrics_.queue_rejected.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    queue_.push_back(std::move(task));
    metrics_.queue_depth.store(queue_.size(), std::memory_order_relaxed);
    cv_.notify_one();
    return true;
  }

  void shutdown() {
    {
      std::lock_guard<std::mutex> lock(mu_);
      if (stopping_) return;
      stopping_ = true;
    }
    cv_.notify_all();
    for (auto& thread : threads_) {
      if (thread.joinable()) thread.join();
    }
    threads_.clear();
  }

  size_t worker_count() const { return threads_.size(); }
  size_t capacity() const { return capacity_; }

 private:
  void run() {
    while (true) {
      ConnectionTask task;
      {
        std::unique_lock<std::mutex> lock(mu_);
        cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
        if (stopping_ && queue_.empty()) return;
        task = std::move(queue_.front());
        queue_.pop_front();
        metrics_.queue_depth.store(queue_.size(), std::memory_order_relaxed);
      }
      handler_(std::move(task));
    }
  }

  size_t capacity_;
  Handler handler_;
  RuntimeMetrics& metrics_;
  mutable std::mutex mu_;
  std::condition_variable cv_;
  std::deque<ConnectionTask> queue_;
  std::vector<std::thread> threads_;
  bool stopping_{false};
};

class TokenBucketRateLimiter {
 public:
  TokenBucketRateLimiter(double requests_per_second, double burst)
      : rate_(requests_per_second), burst_(std::max(1.0, burst)) {}

  bool allow(const std::string& key) {
    if (rate_ <= 0.0) return true;
    const auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mu_);
    auto& bucket = buckets_[key];
    if (!bucket.initialized) {
      bucket.tokens = burst_;
      bucket.last = now;
      bucket.last_seen = now;
      bucket.initialized = true;
    }
    const double elapsed = std::chrono::duration<double>(now - bucket.last).count();
    bucket.tokens = std::min(burst_, bucket.tokens + elapsed * rate_);
    bucket.last = now;
    bucket.last_seen = now;
    const bool allowed = bucket.tokens >= 1.0;
    if (allowed) bucket.tokens -= 1.0;
    if (++calls_since_cleanup_ >= 4096) {
      cleanup_locked(now);
      calls_since_cleanup_ = 0;
    }
    return allowed;
  }

 private:
  struct Bucket {
    double tokens{0.0};
    std::chrono::steady_clock::time_point last{};
    std::chrono::steady_clock::time_point last_seen{};
    bool initialized{false};
  };

  void cleanup_locked(std::chrono::steady_clock::time_point now) {
    const auto stale = std::chrono::minutes(10);
    for (auto it = buckets_.begin(); it != buckets_.end();) {
      if (now - it->second.last_seen > stale) it = buckets_.erase(it);
      else ++it;
    }
    if (buckets_.size() > 100000) buckets_.clear();
  }

  double rate_;
  double burst_;
  std::mutex mu_;
  std::unordered_map<std::string, Bucket> buckets_;
  uint64_t calls_since_cleanup_{0};
};

inline std::string utc_timestamp() {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const auto seconds = time_point_cast<std::chrono::seconds>(now);
  const auto millis = duration_cast<milliseconds>(now - seconds).count();
  std::time_t tt = system_clock::to_time_t(now);
  std::tm tm{};
#if defined(_WIN32)
  gmtime_s(&tm, &tt);
#else
  gmtime_r(&tt, &tm);
#endif
  std::ostringstream os;
  os << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << millis << 'Z';
  return os.str();
}

}  // namespace graphenedb::server
