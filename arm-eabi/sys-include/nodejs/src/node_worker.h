#ifndef SRC_NODE_WORKER_H_
#define SRC_NODE_WORKER_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include <unordered_map>
#include "node_messaging.h"
#include "uv.h"

namespace node {
namespace worker {

class WorkerThreadData;

enum ResourceLimits {
  kMaxYoungGenerationSizeMb,
  kMaxOldGenerationSizeMb,
  kCodeRangeSizeMb,
  kStackSizeMb,
  kTotalResourceLimitCount
};

// A worker thread, as represented in its parent thread.
class Worker : public AsyncWrap {
 public:
  Worker(Environment* env,
         v8::Local<v8::Object> wrap,
         const std::string& url,
         std::shared_ptr<PerIsolateOptions> per_isolate_opts,
         std::vector<std::string>&& exec_argv,
         std::shared_ptr<KVStore> env_vars);
  ~Worker() override;

  // Run the worker. This is only called from the worker thread.
  void Run();

  // Forcibly exit the thread with a specified exit code. This may be called
  // from any thread. `error_code` and `error_message` can be used to create
  // a custom `'error'` event before emitting `'exit'`.
  void Exit(int code,
            const char* error_code = nullptr,
            const char* error_message = nullptr);

  // Wait for the worker thread to stop (in a blocking manner).
  void JoinThread();

  template <typename Fn>
  inline bool RequestInterrupt(Fn&& cb);

  void MemoryInfo(MemoryTracker* tracker) const override;
  SET_MEMORY_INFO_NAME(Worker)
  SET_SELF_SIZE(Worker)

  bool is_stopped() const;
  std::shared_ptr<ArrayBufferAllocator> array_buffer_allocator();

  static void New(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void CloneParentEnvVars(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  static void SetEnvVars(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void StartThread(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void StopThread(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Ref(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void Unref(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void GetResourceLimits(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  v8::Local<v8::Float64Array> GetResourceLimits(v8::Isolate* isolate) const;
  static void TakeHeapSnapshot(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void LoopIdleTime(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void LoopStartTime(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  bool CreateEnvMessagePort(Environment* env);
  static size_t NearHeapLimit(void* data, size_t current_heap_limit,
                              size_t initial_heap_limit);

  std::shared_ptr<PerIsolateOptions> per_isolate_opts_;
  std::vector<std::string> exec_argv_;
  std::vector<std::string> argv_;

  MultiIsolatePlatform* platform_;
  std::shared_ptr<ArrayBufferAllocator> array_buffer_allocator_;
  v8::Isolate* isolate_ = nullptr;
  uv_thread_t tid_;

  std::unique_ptr<InspectorParentHandle> inspector_parent_handle_;

  // This mutex protects access to all variables listed below it.
  mutable Mutex mutex_;

  bool thread_joined_ = true;
  const char* custom_error_ = nullptr;
  std::string custom_error_str_;
  int exit_code_ = 0;
  ThreadId thread_id_;
  uintptr_t stack_base_ = 0;

  // Custom resource constraints:
  double resource_limits_[kTotalResourceLimitCount];
  void UpdateResourceConstraints(v8::ResourceConstraints* constraints);

  // Full size of the thread's stack.
  size_t stack_size_ = 4 * 1024 * 1024;
  // Stack buffer size that is not available to the JS engine.
  static constexpr size_t kStackBufferSize = 192 * 1024;

  std::unique_ptr<MessagePortData> child_port_data_;
  std::shared_ptr<KVStore> env_vars_;

  // This is always kept alive because the JS object associated with the Worker
  // instance refers to it via its [kPort] property.
  MessagePort* parent_port_ = nullptr;

  // A raw flag that is used by creator and worker threads to
  // sync up on pre-mature termination of worker  - while in the
  // warmup phase.  Once the worker is fully warmed up, use the
  // async handle of the worker's Environment for the same purpose.
  bool stopped_ = true;

  bool has_ref_ = true;
  uint64_t environment_flags_ = EnvironmentFlags::kNoFlags;

  // The real Environment of the worker object. It has a lesser
  // lifespan than the worker object itself - comes to life
  // when the worker thread creates a new Environment, and gets
  // destroyed alongwith the worker thread.
  Environment* env_ = nullptr;

  friend class WorkerThreadData;
};

template <typename Fn>
bool Worker::RequestInterrupt(Fn&& cb) {
  Mutex::ScopedLock lock(mutex_);
  if (env_ == nullptr) return false;
  env_->RequestInterrupt(std::move(cb));
  return true;
}

}  // namespace worker
}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS


#endif  // SRC_NODE_WORKER_H_
