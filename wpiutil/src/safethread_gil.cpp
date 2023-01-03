
#include <atomic>
#include <robotpy_build.h>

using OnThreadStartFn = void *(*)();
using OnThreadEndFn = void (*)(void *);

namespace wpi::impl {
void SetSafeThreadNotifiers(OnThreadStartFn OnStart, OnThreadEndFn OnEnd);
}

struct SafeThreadState {
  py::gil_scoped_acquire *acquire = nullptr;
  py::gil_scoped_release *release = nullptr;
};

std::atomic<bool> g_gilstate_managed = false;

void *on_safe_thread_start() {
  auto *st = new SafeThreadState;

  // acquires the GIL and creates pybind11's thread state for this thread
  st->acquire = new py::gil_scoped_acquire;
  // releases the GIL so the thread can start without it
  st->release = new py::gil_scoped_release;

  return st;
}

void on_safe_thread_end(void *opaque) {
  // on entry, GIL should not be acquired

  // don't cleanup if it's unsafe to do so. Several possibilities here:
  if (!opaque                       // internal error?
      || _Py_IsFinalizing()         // python is shutting down
      || !g_gilstate_managed.load() // python has shutdown
  ) {
    return;
  }

  auto *st = (SafeThreadState *)opaque;
  delete st->release; // causes GIL to be acquired
  delete st->acquire; // causes GIL to be released and thread state deleted
  delete st;
}

void setup_safethread_gil() {
  g_gilstate_managed = true;
  wpi::impl::SetSafeThreadNotifiers(on_safe_thread_start, on_safe_thread_end);
}

void cleanup_safethread_gil() { g_gilstate_managed = false; }
