#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * Lightweight movement tracker for the running watch face.
 *
 * Distance algorithm (per design): GPS fixes are bucketed into fixed time
 * windows (TRACK_WINDOW_MS). At the end of each window the fixes in it are
 * averaged into a single "track point", which knocks down per-fix GPS jitter.
 * Distance is the running sum of the haversine distance between consecutive
 * averaged points, so we only ever need to remember the previous point (O(1)
 * memory) rather than the whole history.
 *
 * Pace is derived from just the most recent handful of averaged points.
 *
 * All functions are intended to be called from a single thread (the render
 * loop), so no locking is done here.
 */

#define TRACK_WINDOW_MS 10000   /* averaging window length: 10 s */

/* Feed the latest GPS snapshot in. Call once per render tick (~1 Hz).
 * now_ms should be k_uptime_get(). Fixes with fix_valid == false are ignored. */
void track_update(bool fix_valid, float lat, float lon, int64_t now_ms);

/* Total distance travelled since boot, in meters. */
float track_distance_m(void);

/* Recent pace in minutes per statute mile. Returns true and writes *out only
 * when there has been enough recent movement to compute a meaningful pace
 * (i.e. we're not standing still); returns false otherwise. */
bool track_pace_min_per_mile(float *out);
