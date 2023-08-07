/*
 * Copyright (c) 2023 Florian Walpen <dev@submerge.ch>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef SOSSO_TESTRUN_HPP
#define SOSSO_TESTRUN_HPP

#include "sosso/Buffer.hpp"
#include "sosso/Channel.hpp"
#include "sosso/Correction.hpp"
#include "sosso/DoubleBuffer.hpp"
#include "sosso/FrameClock.hpp"
#include "sosso/Logging.hpp"
#include "sosso/ReadChannel.hpp"
#include "sosso/WriteChannel.hpp"
#include <vector>

namespace sosso {

class TestRun {
public:
  ~TestRun() { close(); }

  WriteChannel &out() { return _out; }

  ReadChannel &in() { return _in; }

  void close() {
    _out.close();
    _in.close();
  }

  bool read_write(unsigned period, unsigned repetitions,
                  bool memory_map = true) {
    if (!_in.recording()) {
      Log::warn(SOSSO_LOC, "In device not in recording mode.");
      return false;
    }
    if (!_out.playback()) {
      Log::warn(SOSSO_LOC, "Out device not in playback mode.");
      return false;
    }
    if (memory_map && _in.can_memory_map() && !_in.memory_map()) {
      Log::warn(SOSSO_LOC, "In device not memory mapped.");
      return false;
    }
    if (memory_map && _out.can_memory_map() && !_out.memory_map()) {
      Log::warn(SOSSO_LOC, "Out device not memory mapped.");
      return false;
    }
    // Compute period time, sync time and frame progress.
    Log::info(SOSSO_LOC, "Period of %u is %lld ns.", period,
              _clock.frames_to_time(period));
    // Create buffer data and prepare channels.
    std::vector<char> in_buffer_data(period * _in.frame_size(), '\0');
    Buffer in_buffer(in_buffer_data.data(), in_buffer_data.size());
    std::int64_t in_frames = period;
    _in.set_buffer(std::move(in_buffer), in_frames);
    in_buffer = Buffer(in_buffer_data.data(), in_buffer_data.size());
    in_frames += period;
    _in.set_buffer(std::move(in_buffer), in_frames);
    std::vector<char> out_buffer_data(period * _out.frame_size(), '\0');
    Buffer out_buffer(out_buffer_data.data(), out_buffer_data.size());
    std::int64_t out_frames = period;
    _out.set_buffer(std::move(out_buffer), out_frames);
    out_frames += period;
    out_buffer = Buffer(out_buffer_data.data(), out_buffer_data.size());
    _out.set_buffer(std::move(out_buffer), out_frames);
    // Step is 16 frames at 48kHz and lower, 32 at 96kHz, 64 at 192kHz.
    if (_out.stepping() != _out.stepping() ||
        _in.sample_rate() != _out.sample_rate()) {
      Log::warn(SOSSO_LOC, "Recording sample rate %u vs playback %u.",
                _in.sample_rate(), _out.sample_rate());
      return false;
    }
    Log::info(SOSSO_LOC, "Step of %u is %lld ns.", _in.stepping(),
              _clock.frames_to_time(_in.stepping()));
    // Initialize correction parameters.
    _in_correction.set_drift_limit(64);
    _out_correction.set_drift_limit(64);
    // Add channels to group for synchronous start.
    int sync_group_id = 0;
    if (!_in.add_to_sync_group(sync_group_id) ||
        !_out.add_to_sync_group(sync_group_id)) {
      return false;
    }
    if (!_in.start_sync_group(sync_group_id)) {
      return false;
    }
    // Get current time.
    if (!_clock.init_clock(_in.sample_rate())) {
      return false;
    }
    // Repeated read and wait.
    unsigned finished = 0;
    while (repetitions > finished) {
      if (!process()) {
        return false;
      }
      if (_in.finished(_sync_frames)) {
        _in_correction.correct(_in.balance());
        if (_sync_frames + period != in_frames) {
          Log::info(
              SOSSO_LOC,
              "In period finished at %lld frames %lld bal %lld correct %lld.",
              _sync_frames, in_frames - period - _sync_frames, _in.balance(),
              _in_correction.correction());
        }
        // Period fully read, simulate consumption.
        in_buffer = _in.take_buffer();
        in_frames += period;
        in_buffer = Buffer(in_buffer_data.data(), in_buffer_data.size());
        _in.set_buffer(std::move(in_buffer),
                       in_frames + _in_correction.correction());
        ++finished;
      }
      if (_out.finished(_sync_frames)) {
        _out_correction.correct(_out.balance());
        if (_sync_frames + period != out_frames) {
          Log::info(
              SOSSO_LOC,
              "Out period finished at %lld frames %lld bal %lld correct %lld.",
              _sync_frames, out_frames - period - _sync_frames, _out.balance(),
              _out_correction.correction());
        }
        // Period fully read, simulate consumption.
        out_buffer = _out.take_buffer();
        out_frames += period;
        out_buffer = Buffer(out_buffer_data.data(), out_buffer_data.size());
        _out.set_buffer(std::move(out_buffer),
                        out_frames + _out_correction.correction());
        ++finished;
      }
      if (!sleep()) {
        return false;
      }
      if (_gap > 0) {
        Log::warn(SOSSO_LOC, "Gap of %lld frames, reset period.", _gap);
        in_frames += _gap;
        out_frames += _gap;
        _gap = 0;
      }
    }
    _in.memory_unmap();
    _out.memory_unmap();
    return true;
  }

private:
  bool process() {
    // Read and write as much as currently possible, at most one period.
    if (_in.wakeup_time(_sync_frames) <= _sync_frames &&
        !_in.process(_sync_frames)) {
      return false;
    }
    if (_out.wakeup_time(_sync_frames) <= _sync_frames &&
        !_out.process(_sync_frames)) {
      return false;
    }
    _in.log_state(_sync_frames);
    _out.log_state(_sync_frames);
    return true;
  }

  bool sleep() {
    // Compute time offset of next step.
    std::int64_t wakeup =
        std::min(_in.wakeup_time(_sync_frames), _out.wakeup_time(_sync_frames));
    if (wakeup > _sync_frames) {
      // Sleep until next step.
      std::int64_t sim_delay = 0;
      if (((_sync_frames / 1024) % 8) == 7) {
        sim_delay = 8 * 1024;
        Log::warn(SOSSO_LOC, "Simulate late wakeup by %lld.", sim_delay);
      }
      if (!_clock.sleep(wakeup + sim_delay)) {
        return false;
      }
      _sync_frames = wakeup;
    }
    // Check wakeup time.
    std::int64_t now = 0;
    if (!_clock.now(now)) {
      return false;
    }
    // Correct current frame time if we are late.
    std::int64_t sync_diff = now - _sync_frames;
    if (sync_diff > _in.stepping()) {
      std::int64_t rounded = sync_diff - (sync_diff % _in.stepping());
      Log::info(SOSSO_LOC, "Wakeup time is %lld late, correct by %lld",
                sync_diff, rounded);
      _sync_frames += rounded;
    }
    _gap = std::max(0L, _sync_frames - _in.period_end());
    _gap = std::max(_gap, _sync_frames - _out.period_end());
    if (_gap > 1024) {
      _in.reset_buffers(_in.end_frames() + _gap);
      _out.reset_buffers(_out.end_frames() + _gap);
    } else {
      _gap = 0;
    }
    return true;
  }

  FrameClock _clock;
  std::int64_t _sync_frames = 0;
  std::int64_t _gap = 0;
  DoubleBuffer<WriteChannel> _out;
  DoubleBuffer<ReadChannel> _in;
  Correction _out_correction;
  Correction _in_correction;
};

} // namespace sosso

#endif // SOSSO_TESTRUN_HPP
