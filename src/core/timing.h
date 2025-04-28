#pragma once

#include <time.h>

typedef struct timespec DeltaTimer;

static inline void gem_dt_record(DeltaTimer* dt) { clock_gettime(CLOCK_MONOTONIC_RAW, dt); }

static inline double gem_dt_record_get_secs(DeltaTimer* dt)
{
    struct timespec prev = *dt;
    gem_dt_record(dt);
    return (double)(dt->tv_sec - prev.tv_sec) + (double)(dt->tv_nsec - prev.tv_nsec) / 1000000000.0;
}

static inline double gem_dt_record_get_ms(DeltaTimer* dt)
{
    struct timespec prev = *dt;
    gem_dt_record(dt);
    return (double)(dt->tv_sec - prev.tv_sec) * 1000.0 +
           (double)(dt->tv_nsec - prev.tv_nsec) / 1000000.0;
}

static inline double gem_dt_record_get_ns(DeltaTimer* dt)
{
    struct timespec prev = *dt;
    gem_dt_record(dt);
    return (double)(dt->tv_sec - prev.tv_sec) * 1000000000.0 + (double)(dt->tv_nsec - prev.tv_nsec);
}
