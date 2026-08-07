#ifndef DXVUI_FPSCOUNTER_H
#define DXVUI_FPSCOUNTER_H

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <vector>

namespace DxvUI {

/**
 * @class FpsCounter
 * @brief Measures the application frame rate over a sliding window of frames.
 *
 * Call tick() once per presented frame; the counter averages the last N frame
 * durations and reports them as both FPS and milliseconds per frame. The first
 * tick() only records a baseline, so the reported values stay zero until the
 * second frame.
 *
 * The clock is a template parameter so tests can drive the counter with a fake
 * clock instead of waiting real time; the default is the steady clock, which is
 * monotonic and immune to wall-clock adjustments.
 *
 * @tparam Clock A type providing now() and a time_point, like std::chrono
 * clocks.
 */
template <class Clock = std::chrono::steady_clock>
class FpsCounter {
   public:
    /**
     * @brief Constructs a counter with a fixed-size averaging window.
     * @param windowSize How many of the most recent frames are averaged. A
     * larger window smooths out spikes but reacts slower to real changes.
     * @exceptionGuarantee Basic exception guarantee.
     */
    explicit FpsCounter(size_t windowSize = 60)
        : frameTimes_(windowSize == 0 ? 1 : windowSize, 0.0) {}

    /**
     * @brief Records the end of one frame.
     *
     * The elapsed time since the previous tick() (or since construction for the
     * first call) is pushed into the averaging window. Must be called exactly
     * once per presented frame for the reported values to be meaningful.
     * @exceptionGuarantee No-throw guarantee.
     */
    void tick() noexcept {
        const auto now = Clock::now();
        if (first_) {
            first_ = false;
        } else {
            const double ms = std::chrono::duration<double, std::milli>(now - last_).count();
            frameTimes_[writeIndex_] = ms;
            writeIndex_ = (writeIndex_ + 1) % frameTimes_.size();
            count_ = std::min(count_ + 1, frameTimes_.size());
        }
        last_ = now;
    }

    /**
     * @brief Gets the average frame rate over the window.
     * @return Frames per second, or 0 when fewer than two frames were ticked.
     * @exceptionGuarantee No-throw guarantee.
     */
    float getFps() const noexcept {
        const double avgMs = getFrameTimeMs();
        return avgMs > 0.0 ? static_cast<float>(1000.0 / avgMs) : 0.0f;
    }

    /**
     * @brief Gets the average frame duration over the window.
     * @return Milliseconds per frame, or 0 when fewer than two frames were
     * ticked.
     * @exceptionGuarantee No-throw guarantee.
     */
    float getFrameTimeMs() const noexcept {
        if (count_ == 0) return 0.0f;
        double sum = 0.0;
        for (size_t i = 0; i < count_; ++i) sum += frameTimes_[i];
        return static_cast<float>(sum / count_);
    }

    /**
     * @brief Clears all accumulated samples.
     *
     * The next tick() starts a fresh window with no baseline, so the reported
     * values are zero until a second frame is ticked.
     * @exceptionGuarantee No-throw guarantee.
     */
    void reset() noexcept {
        std::fill(frameTimes_.begin(), frameTimes_.end(), 0.0);
        writeIndex_ = 0;
        count_ = 0;
        first_ = true;
    }

   private:
    // Ring buffer of the most recent frame durations; the insertion order is
    // scrambled when the buffer wraps, but a sum is order-independent, so the
    // average stays correct.
    std::vector<double> frameTimes_;
    size_t writeIndex_ = 0;
    size_t count_ = 0;
    typename Clock::time_point last_{};
    bool first_ = true;
};

}  // namespace DxvUI

#endif  // DXVUI_FPSCOUNTER_H
