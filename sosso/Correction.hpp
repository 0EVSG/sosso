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

#ifndef SOSSO_CORRECTION_HPP
#define SOSSO_CORRECTION_HPP

#include <cstdint>

namespace sosso {

/*!
 * \brief Drift Correction
 *
 * Calculates drift correction for a channel, relative to another channel if
 * required. Usually the playback channel is corrected relative to the recording
 * channel, if in use.
 * It keeps track of the correction parameter (in frames), and also the
 * threshhold values which determine the amount of correction. Above these
 * threshholds, either single frame correction is applied for smaller drift,
 * or rigorous correction in case of large discrepance. The idea is that single
 * frame corrections typically go unnoticed, but it may not be sufficient to
 * correct something more grave like packet loss on a USB audio interface.
 */
class Correction {
public:
  //! Default constructor, threshhold values are set separately.
  Correction() = default;

  /*!
   * \brief Set thresholds for small drift correction.
   * \param drift_max Balance threshold for small corrections, in frames.
   */
  void set_drift_limit(unsigned drift_max) { _drift_max = drift_max; }

  /*!
   * \brief Set thresholds for rigorous large discrepance correction.
   * \param loss_max Hard limit for balance discrepance, in frames.
   */
  void set_loss_limit(unsigned loss_max) { _loss_max = loss_max; }

  //! Get current correction parameter.
  std::int64_t correction() const { return _correction; }

  /*!
   * \brief Calculate a new correction parameter.
   * \param balance Balance of the corrected channel, compared to FrameClock.
   * \param target Balance of a master channel which acts as reference.
   * \return Current correction parameter.
   */
  std::int64_t correct(std::int64_t balance, std::int64_t target = 0) {
    // Judge balance relative to target balance.
    std::int64_t offset = target - balance;
    // Exponentially weighted moving average, for small drift correction.
    _average_offset = (_average_offset + offset) / 2;
    if (offset - _correction < -_loss_max || offset - _correction > _loss_max) {
      // Large discrepance, rigorous correction.
      _correction = offset;
    } else {
      // Correct by a few frames if average offset exceeds drift threshold.
      _correction += (_average_offset - _correction) / (_drift_max + 1);
    }
    return _correction;
  }

  //! Clear the current correction parameter, but not the thresholds.
  void clear() { _correction = 0; }

private:
  std::int64_t _loss_max = 128;     // Threshold for rigorous correction.
  std::int64_t _drift_max = 64;     // Threshold for small drift correction.
  std::int64_t _correction = 0;     // Correction parameter.
  std::int64_t _average_offset = 0; // Moving average of balance offset.
};

} // namespace sosso

#endif // SOSSO_CORRECTION_HPP
