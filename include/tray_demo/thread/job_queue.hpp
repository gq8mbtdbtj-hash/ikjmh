#pragma once

/**
 * @file job_queue.hpp
 * @brief 基于 pthread 的后台任务队列。
 */

#include <string>

namespace tray_demo {

/**
 * @class IJob
 * @brief 单个后台任务。
 *
 * @customize 拉取配置、校验更新包等实现 `Run()`，再 `jobs().Enqueue(new MyJob)`。
 * 注意：不要在 `Run()` 里直接碰 HWND；结果封送回 UI 线程。
 */
class IJob {
public:
  virtual ~IJob() {}
  virtual void Run() = 0;
};

/**
 * @class JobQueue
 * @brief 单 worker pthread + 任务队列（Windows 需 pthreads4w）。
 *
 * @internal 未链接 pthread 时退化为同步执行，便于无库冒烟。
 */
class JobQueue {
public:
  JobQueue();
  ~JobQueue();

  bool Start();   ///< 启动 worker
  void Stop();    ///< 停止并 join

  /**
   * @brief 投递任务
   * @param job 所有权转移；`Run` 后由队列 `delete`
   */
  bool Enqueue(IJob* job);

  bool is_running() const { return running_; }

private:
  JobQueue(const JobQueue&);
  JobQueue& operator=(const JobQueue&);

  struct Impl;
  static void* WorkerMain(void* arg);

  Impl* impl_;
  bool running_;
};

}  // namespace tray_demo
