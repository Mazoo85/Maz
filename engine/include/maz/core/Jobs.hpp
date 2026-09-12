#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <thread>
#include <type_traits>
#include <vector>

namespace maz::core {

// A fixed-size worker thread pool with a task queue — the engine's parallelism foundation. submit()
// runs a callable on a worker and hands back a std::future for its result; parallelFor()/
// parallelRanges() split an index range across the workers and block until the whole range is done,
// which is the common shape for data-parallel work (fractal/image gen, particle and transform
// updates, culling, batched pathfinding). The pool is created once and reused; the destructor
// drains outstanding tasks and joins. Not re-entrant: don't call parallelFor from inside a task
// (the caller thread blocks on the results, so nesting can starve the pool). Single-owner, single-
// submitter model — intended to be driven from the game thread.
class JobSystem {
public:
    // threadCount 0 => hardware_concurrency (at least 1).
    explicit JobSystem(unsigned threadCount = 0) {
        unsigned n = threadCount;
        if (n == 0) {
            n = std::thread::hardware_concurrency();
            if (n == 0) {
                n = 1;
            }
        }
        m_workers.reserve(n);
        for (unsigned i = 0; i < n; ++i) {
            m_workers.emplace_back([this] { workerLoop(); });
        }
    }

    ~JobSystem() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_cv.notify_all();
        for (std::thread& w : m_workers) {
            if (w.joinable()) {
                w.join();
            }
        }
    }

    JobSystem(const JobSystem&) = delete;
    JobSystem& operator=(const JobSystem&) = delete;

    unsigned workerCount() const { return static_cast<unsigned>(m_workers.size()); }

    // Enqueue a task and get a future for its return value.
    template <typename F>
    auto submit(F&& f) -> std::future<std::invoke_result_t<F>> {
        using R = std::invoke_result_t<F>;
        auto task = std::make_shared<std::packaged_task<R()>>(std::forward<F>(f));
        std::future<R> fut = task->get_future();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_tasks.emplace([task] { (*task)(); });
        }
        m_cv.notify_one();
        return fut;
    }

    // Run fn(i) for every i in [begin, end), spread across the workers, blocking until all complete.
    // `grain` is the minimum indices per chunk (batches small items to amortize scheduling cost).
    void parallelFor(size_t begin, size_t end, const std::function<void(size_t)>& fn,
                     size_t grain = 1) {
        if (end <= begin) {
            return;
        }
        parallelRanges(
            begin, end,
            [&fn](size_t a, size_t b) {
                for (size_t i = a; i < b; ++i) {
                    fn(i);
                }
            },
            0, grain);
    }

    // Run fn(chunkBegin, chunkEnd) over contiguous chunks tiling [begin, end) exactly once, blocking
    // until done. Lower call overhead than parallelFor when the body can loop a range itself.
    void parallelRanges(size_t begin, size_t end, const std::function<void(size_t, size_t)>& fn,
                        size_t chunks = 0, size_t grain = 1) {
        if (end <= begin) {
            return;
        }
        const size_t total = end - begin;
        if (grain == 0) {
            grain = 1;
        }
        // Aim for a few chunks per worker so load balances, but never smaller than `grain`.
        size_t target = chunks;
        if (target == 0) {
            target = static_cast<size_t>(workerCount()) * 4;
            if (target == 0) {
                target = 1;
            }
        }
        size_t chunkSize = (total + target - 1) / target;
        if (chunkSize < grain) {
            chunkSize = grain;
        }

        std::vector<std::future<void>> futures;
        futures.reserve(total / chunkSize + 1);
        for (size_t a = begin; a < end; a += chunkSize) {
            const size_t b = a + chunkSize < end ? a + chunkSize : end;
            futures.push_back(submit([&fn, a, b] { fn(a, b); }));
        }
        for (std::future<void>& f : futures) {
            f.get(); // propagate exceptions + wait
        }
    }

private:
    void workerLoop() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cv.wait(lock, [this] { return m_stop || !m_tasks.empty(); });
                if (m_stop && m_tasks.empty()) {
                    return;
                }
                task = std::move(m_tasks.front());
                m_tasks.pop();
            }
            task();
        }
    }

    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_stop = false;
};

} // namespace maz::core
