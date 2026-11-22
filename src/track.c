#include "track.h"
#include <math.h>

#define TRACK_PI          3.14159265358979323846f
#define EARTH_RADIUS_M    6371000.0f
#define METERS_PER_MILE   1609.344f

/* How many recent averaged points feed the pace estimate. "Past 3 points." */
#define PACE_POINTS       3
/* Below this displacement over the pace window we treat the user as stopped
 * and report no pace, rather than dividing a few meters of GPS noise into a
 * wildly optimistic (or infinite) number. */
#define PACE_MIN_DIST_M   3.0f

typedef struct {
    float lat;
    float lon;
    int64_t t_ms;
} track_point_t;

/* --- current averaging window ------------------------------------------- */
static bool    win_open;          /* a window is in progress */
static int64_t win_start_ms;      /* uptime the window opened */
static float   win_sum_lat;
static float   win_sum_lon;
static int     win_count;

/* --- distance accumulator ----------------------------------------------- */
static bool         have_prev;    /* prev_point holds a real previous point */
static track_point_t prev_point;  /* last finalized point (for distance) */
static float        total_dist_m;

/* --- recent points for pace (oldest .. newest) -------------------------- */
static track_point_t recent[PACE_POINTS];
static int           recent_count;

static float deg2rad(float d) {
    return d * (TRACK_PI / 180.0f);
}

/* Great-circle distance between two lat/lon points, in meters. */
static float haversine_m(float lat1, float lon1, float lat2, float lon2) {
    float dlat = deg2rad(lat2 - lat1);
    float dlon = deg2rad(lon2 - lon1);
    float a = sinf(dlat * 0.5f) * sinf(dlat * 0.5f) +
              cosf(deg2rad(lat1)) * cosf(deg2rad(lat2)) *
              sinf(dlon * 0.5f) * sinf(dlon * 0.5f);
    float c = 2.0f * atan2f(sqrtf(a), sqrtf(1.0f - a));
    return EARTH_RADIUS_M * c;
}

/* Append a freshly finalized averaged point to the recent-points queue,
 * shifting out the oldest once it is full. */
static void push_recent(const track_point_t *pt) {
    if (recent_count < PACE_POINTS) {
        recent[recent_count++] = *pt;
    } else {
        for (int i = 1; i < PACE_POINTS; i++) {
            recent[i - 1] = recent[i];
        }
        recent[PACE_POINTS - 1] = *pt;
    }
}

/* Close out the current averaging window: average its fixes into one point,
 * add the segment distance, and record it for the pace estimate. */
static void finalize_window(int64_t now_ms) {
    if (win_count <= 0) {
        return;
    }

    track_point_t pt;
    pt.lat = win_sum_lat / win_count;
    pt.lon = win_sum_lon / win_count;
    pt.t_ms = now_ms;

    if (have_prev) {
        total_dist_m += haversine_m(prev_point.lat, prev_point.lon,
                                    pt.lat, pt.lon);
    }
    prev_point = pt;
    have_prev = true;

    push_recent(&pt);

    /* reset the window */
    win_open = false;
    win_count = 0;
    win_sum_lat = 0.0f;
    win_sum_lon = 0.0f;
}

void track_update(bool fix_valid, float lat, float lon, int64_t now_ms) {
    /* Close any window whose 10 s have elapsed before folding in new data, so
     * the finalized point is timestamped at the window boundary. */
    if (win_open && (now_ms - win_start_ms) >= TRACK_WINDOW_MS) {
        finalize_window(now_ms);
    }

    if (!fix_valid) {
        return;
    }

    if (!win_open) {
        win_open = true;
        win_start_ms = now_ms;
        win_count = 0;
        win_sum_lat = 0.0f;
        win_sum_lon = 0.0f;
    }

    win_sum_lat += lat;
    win_sum_lon += lon;
    win_count++;
}

float track_distance_m(void) {
    return total_dist_m;
}

bool track_pace_min_per_mile(float *out) {
    if (recent_count < 2) {
        return false;  /* need at least two points to have moved between */
    }

    float dist_m = 0.0f;
    for (int i = 1; i < recent_count; i++) {
        dist_m += haversine_m(recent[i - 1].lat, recent[i - 1].lon,
                              recent[i].lat, recent[i].lon);
    }

    int64_t span_ms = recent[recent_count - 1].t_ms - recent[0].t_ms;
    if (dist_m < PACE_MIN_DIST_M || span_ms <= 0) {
        return false;
    }

    float miles = dist_m / METERS_PER_MILE;
    float minutes = (float)span_ms / 60000.0f;
    *out = minutes / miles;
    return true;
}
