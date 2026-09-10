#include "tray_demo/thread/job_queue.hpp"

#include <queue>

#if TRAY_DEMO_HAS_PTHREAD
#include <pthread.h>
#endif

namespace tray_demo {

#if TRAY_DEMO_HAS_PTHREAD

struct JobQueue::Impl {
  pthread_mutex_t mutex;
  pthread_cond_t cond;
  pthread_t worker;
  std::queue<IJob*> jobs;
  bool stop;

  Impl() : stop(false) {
    pthread_mutex_init(&mutex, 0);
    pthread_cond_init(&cond, 0);
  }

  ~Impl() {
    pthread_cond_destroy(&cond);
    pthread_mutex_destroy(&mutex);
  }
};

void* JobQueue::WorkerMain(void* arg) {
  JobQueue::Impl* impl = static_cast<JobQueue::Impl*>(arg);
  for (;;) {
    IJob* job = 0;
    pthread_mutex_lock(&impl->mutex);
    while (!impl->stop && impl->jobs.empty()) {
      pthread_cond_wait(&impl->cond, &impl->mutex);
    }
    if (impl->stop && impl->jobs.empty()) {
      pthread_mutex_unlock(&impl->mutex);
      break;
    }
    job = impl->jobs.front();
    impl->jobs.pop();
    pthread_mutex_unlock(&impl->mutex);

    if (job) {
      job->Run();
      delete job;
    }
  }
  return 0;
}

JobQueue::JobQueue() : impl_(new Impl), running_(false) {}

JobQueue::~JobQueue() {
  Stop();
  delete impl_;
  impl_ = 0;
}

bool JobQueue::Start() {
  if (running_) {
    return true;
  }
  impl_->stop = false;
  const int rc = pthread_create(&impl_->worker, 0, &JobQueue::WorkerMain, impl_);
  if (rc != 0) {
    return false;
  }
  running_ = true;
  return true;
}

void JobQueue::Stop() {
  if (!running_) {
    return;
  }
  pthread_mutex_lock(&impl_->mutex);
  impl_->stop = true;
  pthread_cond_broadcast(&impl_->cond);
  pthread_mutex_unlock(&impl_->mutex);
  pthread_join(impl_->worker, 0);

  pthread_mutex_lock(&impl_->mutex);
  while (!impl_->jobs.empty()) {
    delete impl_->jobs.front();
    impl_->jobs.pop();
  }
  pthread_mutex_unlock(&impl_->mutex);
  running_ = false;
}

bool JobQueue::Enqueue(IJob* job) {
  if (!job || !running_) {
    delete job;
    return false;
  }
  pthread_mutex_lock(&impl_->mutex);
  impl_->jobs.push(job);
  pthread_cond_signal(&impl_->cond);
  pthread_mutex_unlock(&impl_->mutex);
  return true;
}

#else  // !TRAY_DEMO_HAS_PTHREAD

struct JobQueue::Impl {};

JobQueue::JobQueue() : impl_(new Impl), running_(false) {}

JobQueue::~JobQueue() {
  Stop();
  delete impl_;
  impl_ = 0;
}

bool JobQueue::Start() {
  running_ = true;
  return true;
}

void JobQueue::Stop() { running_ = false; }

bool JobQueue::Enqueue(IJob* job) {
  // 无 pthread 时同步执行，便于先跑通框架
  if (!job) {
    return false;
  }
  if (!running_) {
    delete job;
    return false;
  }
  job->Run();
  delete job;
  return true;
}

#endif

}  // namespace tray_demo
