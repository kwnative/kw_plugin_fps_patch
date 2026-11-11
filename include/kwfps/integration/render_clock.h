#ifndef KWFPS_INTEGRATION_RENDER_CLOCK_H
#define KWFPS_INTEGRATION_RENDER_CLOCK_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define KWFPS_RENDER_CLOCK_ABI_V1 UINT32_C(1)
enum kwfps_clock_result { KWFPS_CLOCK_OK, KWFPS_CLOCK_ARGUMENT, KWFPS_CLOCK_BUSY,
    KWFPS_CLOCK_NOT_READY, KWFPS_CLOCK_STALE, KWFPS_CLOCK_WRONG_THREAD };
struct kwfps_render_snapshot;
struct kwfps_clock_scope_v1 {
    struct kwfps_render_snapshot *current;
    struct kwfps_clock_scope_v1 *previous;
    uint32_t owner;
};
typedef void (*kwfps_clock_drain_v1)(void *context);
/* Optional renderer integration. Handles belong to their creator thread;
 * enter/leave belong to one rendering thread and nest in strict LIFO order.
 * Capture requires the ordinary FPS writers to be quiescent (e.g. the engine
 * owner waits for the render job). Capturing on the FPS writer itself is safe.
 * Snapshot memory is provider-owned, and lives through actual consumer ACK.
 * The drain callback must finish outstanding render jobs while servicing any
 * creator-thread graphics RPCs. It runs before canonical rate/session writes.
 * This isolates FPS history, not native inline engine globals or whole scenes. */
struct kwfps_render_clock_api_v1 {
    uint32_t abi,size,snapshot_bytes;
    uint32_t (*create)(struct kwfps_render_snapshot **);
    uint32_t (*capture)(struct kwfps_render_snapshot *);
    uint32_t (*release)(struct kwfps_render_snapshot *);
    uint32_t (*destroy)(struct kwfps_render_snapshot *);
    uint32_t (*enter)(struct kwfps_render_snapshot *,struct kwfps_clock_scope_v1 *);
    uint32_t (*leave)(struct kwfps_clock_scope_v1 *);
    uint32_t (*set_drain)(kwfps_clock_drain_v1,void *);
};
typedef const struct kwfps_render_clock_api_v1 *(*kwfps_get_render_clock_api_v1_fn)(uint32_t abi);
#ifdef __cplusplus
}
#endif
#endif
