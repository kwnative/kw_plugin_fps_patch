#ifndef FPS_STALL_GUARD_H
#define FPS_STALL_GUARD_H
/* Presentation-rate control only: never advances a phase or admits a command.
 * All supported rates divide 900, retaining exact frame-clock phase on changes. */
#include <stdint.h>
#include <string.h>
#define SG_TIMEBASE 900u
#define SG_MIN_FPS 30u
#define SG_EPOCHS 256u
#define SG_RECOVER_MS 5000u
#define SG_TRIAL_MS 2000u
#define SG_RETRY_MAX_MS 60000u
struct sg_policy {
    uint32_t active, pending, previous, overloads, changes;
    uint32_t requested, stable_ms, retry_ms, trial_base, trial_ms, trial_bad;
    uint32_t recoveries, failures;
    unsigned initialized;
};
static inline void sg_reset(struct sg_policy *p, uint32_t requested)
{
    memset(p, 0, sizeof(*p));
    p->active = p->requested = requested;
    p->retry_ms = SG_RECOVER_MS;
}
static inline void sg_gap(struct sg_policy *p)
{
    p->pending = p->overloads = p->stable_ms = 0;
    /* A pause is not evidence for accepting or failing an upward trial. */
    p->trial_ms = p->trial_bad = 0;
}
/* Commit only after the production phase-1 hook has changed the frame clock. */
static inline void sg_commit(struct sg_policy *p)
{
    uint32_t rate = p->pending, old = p->active;
    if (rate < SG_MIN_FPS || rate > p->requested || (rate + 15u != old && rate != old + 15u))
        return;
    p->active = rate;
    p->pending = p->overloads = p->stable_ms = 0;
    if (rate > old) {
        ++p->recoveries;
        p->trial_base = old;
        p->trial_ms = p->trial_bad = 0;
    } else {
        ++p->changes;
        if (p->trial_base) {
            ++p->failures;
            p->retry_ms = p->retry_ms < SG_RETRY_MAX_MS / 2u ? p->retry_ms * 2u : SG_RETRY_MAX_MS;
        } else
            p->retry_ms = SG_RECOVER_MS;
        p->trial_base = p->trial_ms = p->trial_bad = 0;
    }
}
static inline void sg_cycle(struct sg_policy *p, uint32_t now, uint32_t budget)
{
    if (!p->initialized) {
        p->previous = now;
        p->initialized = 1;
        return;
    }
    uint32_t elapsed = now - p->previous;
    p->previous = now;
    /* Pause/loading/network gaps are not timing debt and never cause a burst. */
    if (!budget || elapsed >= 500u || budget >= 400u) {
        sg_gap(p);
        return;
    }
    if (p->pending)
        return;
    unsigned overloaded = (uint64_t)elapsed * 100u > (uint64_t)budget * 115u + 200u;
    if (p->trial_base) {
        p->trial_ms += elapsed;
        if (overloaded)
            ++p->trial_bad;
        /* Two bad cycles in a trial trigger rollback, even if not consecutive. */
        if (p->trial_bad >= 2u) {
            p->pending = p->trial_base;
            return;
        }
        if (p->trial_ms >= SG_TRIAL_MS) {
            p->trial_base = p->trial_ms = p->trial_bad = 0;
            p->retry_ms = SG_RECOVER_MS;
            p->stable_ms = 0;
        }
        return;
    }
    if (overloaded) {
        ++p->overloads;
        p->stable_ms = 0;
    } else {
        p->overloads = 0;
        p->stable_ms += elapsed;
    }
    if (p->overloads >= 3u && p->active > SG_MIN_FPS) {
        p->pending = p->active - 15u;
        p->overloads = 0;
        return;
    }
    if (!overloaded && p->active < p->requested && p->stable_ms >= p->retry_ms) {
        p->pending = p->active + 15u;
        p->stable_ms = 0;
    }
    /* No need to accumulate unbounded stable time at the configured ceiling. */
    if (p->active == p->requested)
        p->stable_ms = 0;
}
struct sg_epoch {
    uint64_t frame, ticks;
    uint32_t rate;
};
struct sg_clock {
    uint32_t raw, count, history_misses;
    uint64_t frame;
    struct sg_epoch epoch[SG_EPOCHS];
};
static inline void sg_clock_reset(struct sg_clock *c, uint32_t rate)
{
    memset(c, 0, sizeof(*c));
    c->count = 1;
    c->epoch[0].rate = rate;
}
static inline uint64_t sg_clock_at(const struct sg_clock *c, uint64_t frame)
{
    if (frame < c->epoch[0].frame)
        return c->epoch[0].ticks;
    unsigned i = c->count;
    while (i > 1u && c->epoch[i - 1u].frame > frame)
        --i;
    const struct sg_epoch *e = &c->epoch[i - 1u];
    return e->ticks + (frame - e->frame) * (SG_TIMEBASE / e->rate);
}
static inline void sg_clock_sample(struct sg_clock *c, uint32_t raw)
{
    c->frame += (uint32_t)(raw - c->raw);
    c->raw = raw;
}
static inline int sg_clock_change(struct sg_clock *c, uint32_t rate)
{
    if (!c->count || rate < SG_MIN_FPS || rate > 90u || rate % 15u || SG_TIMEBASE % rate)
        return 0;
    uint64_t ticks = sg_clock_at(c, c->frame);
    if (c->epoch[c->count - 1u].frame == c->frame) {
        c->epoch[c->count - 1u].rate = rate;
        return 1;
    }
    /* Bounded history supports unlimited transitions without allocation.
     * Prefix ticks remain absolute; evicting an epoch never resets live time. */
    if (c->count == SG_EPOCHS) {
        memmove(c->epoch, c->epoch + 1, (SG_EPOCHS - 1u) * sizeof(c->epoch[0]));
        --c->count;
    }
    c->epoch[c->count++] = (struct sg_epoch){c->frame, ticks, rate};
    return 1;
}
static inline uint64_t sg_clock_span(struct sg_clock *c, uint32_t frames)
{
    uint64_t begin = c->frame >= frames ? c->frame - frames : 0;
    /* A stale historical audio timestamp gets no catch-up burst. The native
     * caller then refreshes its saved frame. Ordinary spans stay exact. */
    if (begin < c->epoch[0].frame) {
        ++c->history_misses;
        return 0;
    }
    return sg_clock_at(c, c->frame) - sg_clock_at(c, begin);
}
static inline uint32_t sg_clock_retail(const struct sg_clock *c)
{
    return (uint32_t)((sg_clock_at(c, c->frame) + 29u) / 30u);
}
#endif
