// ============================================================================
// LilyGo_H752_Trains.h — every waveform the LilyGo T5 S3 H752 drives.
//
// One file per board — but this board has never had a scanner-tuning
// session, so all it carries is the hand-built 4-level waveform pairs
// (adapted from the H716's calibrated set, same panel family). There are
// no 16-grey tables and no direct trains: 16-grey runs on the formula
// library and grey-to-grey transitions stay on the two-step path until a
// match-card session tunes it. A tuned blob in NVS (see
// EPD_Painter_tuned.h and examples/other/tuneup) overrides these at
// begin(), which is the intended route to real tables for this board.
// ============================================================================

#ifndef EPD_PAINTER_LILYGO_H752_TRAINS_H
#define EPD_PAINTER_LILYGO_H752_TRAINS_H

#include <stdint.h>

#if defined(EPD_PAINTER_PRESET_LILYGO_T5_S3_H752) || defined(EPD_PAINTER_PRESET_AUTO)

// ---- 4-level waveform pairs (hand-built, never scanner-verified) -----------
inline const EPD_Painter::Waveforms EPD_WF_H752 = {
    .fast_lighter   = { { 1, 3, 2, 3, 2, 2, 3 },
                        { 3, 2, 3, 2, 2, 3, 2 },
                        { 2, 2, 2, 2, 2, 2, 2 } },
    .fast_darker    = { { 3, 1, 3, 2, 1, 1, 3 },
                        { 1, 3, 1, 3, 1, 1, 3 },
                        { 1, 1, 1, 1, 1, 1, 1 } },
    .normal_lighter = { { 1, 1, 1, 1, 2, 3, 3, 2, 2, 2, 2, 2, 2 },
                        { 2, 1, 2, 2, 1, 2, 2, 2, 0, 0, 2, 2, 3 },
                        { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 } },
    .normal_darker  = { { 1, 2, 1, 3, 1, 3, 1, 2, 2, 1, 2, 1, 1 },
                        { 1, 1, 1, 2, 2, 3, 1, 1, 3, 2, 1, 1, 3 },
                        { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 } },
    .high_lighter   = { { 1, 3, 1, 1, 1, 2, 1, 2, 2, 2, 2, 2, 2 },
                        { 1, 1, 2, 2, 1, 2, 2, 2, 2, 1, 2, 2, 2 },
                        { 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 } },
    .high_darker    = { { 1, 3, 1, 1, 1, 2, 2, 2, 1, 2, 2, 1, 1 },
                        { 1, 1, 1, 1, 2, 2, 1, 1, 2, 1, 2, 1, 1 },
                        { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 } },
};

#endif // H752
#endif // EPD_PAINTER_LILYGO_H752_TRAINS_H
