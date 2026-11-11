/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
/* Windows offline fixture. No game image, hook installation or renderer is used.
 * Exercise the real snapshot helpers against a small private fake client. */
#include <assert.h>
#include <stdio.h>
#include "../src/runtime/game_patches.c"

static void __attribute__((noreturn)) fixture_fail(const char *reason)
{
    fprintf(stderr, "FAIL render-clock fixture: %s\n", reason);
    ExitProcess(1);
}
#undef assert
#define assert(condition) ((condition) ? (void)0 : fixture_fail(#condition))
/* Link-time guards: this fixture must never install a patch or reach logging/
 * address-discovery helpers. PE retains some unused hook sections. */
u32 load_u32(const void *pointer)
{
    (void)pointer;
    fixture_fail("unexpected native read helper");
}
void encode_u32(u8 *target, u32 value)
{
    (void)target;
    (void)value;
    fixture_fail("unexpected patch encoding");
}
void encode_rel32(u8 *target, const u8 *next, const void *destination)
{
    (void)target;
    (void)next;
    (void)destination;
    fixture_fail("unexpected patch encoding");
}
void patch_transaction_init(PatchTransaction *transaction)
{
    (void)transaction;
    fixture_fail("unexpected patch transaction");
}
BOOL patch_transaction_write(PatchTransaction *transaction, void *destination, const void *replacement, size_t size)
{
    (void)transaction;
    (void)destination;
    (void)replacement;
    (void)size;
    fixture_fail("unexpected patch write");
}
void patch_transaction_rollback(PatchTransaction *transaction)
{
    (void)transaction;
    fixture_fail("unexpected patch rollback");
}
void log_line(const char *text)
{
    (void)text;
    fixture_fail("unexpected native logging");
}
void log_text(const char *text)
{
    (void)text;
    fixture_fail("unexpected native logging");
}
void log_u32(const char *label, u32 value)
{
    (void)label;
    (void)value;
    fixture_fail("unexpected native logging");
}
void log_hex32(const char *label, u32 value)
{
    (void)label;
    (void)value;
    fixture_fail("unexpected native logging");
}

struct fake_client {
    void **vtable;
    uint32_t frame;
};
static uint32_t THISCALL fake_frame(void *client)
{
    return ((struct fake_client *)client)->frame;
}
static void no_ack(void *context)
{
    ++*(unsigned *)context;
}
struct worker_context {
    const struct kwfps_render_clock_api_v1 *api;
    struct kwfps_render_snapshot *snapshot;
    HANDLE entered, proceed, done, thread;
    uint32_t wrong_capture, wrong_release, wrong_destroy, enter_result, leave_result;
};
static DWORD WINAPI worker(void *argument)
{
    struct worker_context *context = argument;
    struct kwfps_clock_scope_v1 scope = {0};
    context->wrong_capture = context->api->capture(context->snapshot);
    context->wrong_release = context->api->release(context->snapshot);
    context->wrong_destroy = context->api->destroy(context->snapshot);
    context->enter_result = context->api->enter(context->snapshot, &scope);
    assert(SetEvent(context->entered));
    assert(WaitForSingleObject(context->proceed, 5000) == WAIT_OBJECT_0);
    /* A canonical writer cannot synchronously join its own render scope. */
    assert(!render_clock_source_barrier());
    context->leave_result = context->api->leave(&scope);
    assert(SetEvent(context->done));
    return 0;
}
static void acknowledge(void *argument)
{
    struct worker_context *context = argument;
    assert(SetEvent(context->proceed));
    assert(WaitForSingleObject(context->done, 5000) == WAIT_OBJECT_0);
    assert(WaitForSingleObject(context->thread, 5000) == WAIT_OBJECT_0);
    assert(context->api->release(context->snapshot) == KWFPS_CLOCK_OK);
}
int main(void)
{
    const struct kwfps_render_clock_api_v1 *api = kwfps_get_render_clock_api_v1(KWFPS_RENDER_CLOCK_ABI_V1);
    assert(api && api->abi == 1 && api->size == sizeof(*api) && api->snapshot_bytes == sizeof(struct kwfps_render_snapshot));
    assert(!kwfps_get_render_clock_api_v1(2));
    assert(api->create(NULL) == KWFPS_CLOCK_ARGUMENT);
    struct kwfps_render_snapshot *first = NULL, *second = NULL;
    assert(api->create(&first) == KWFPS_CLOCK_OK && api->create(&first) == KWFPS_CLOCK_ARGUMENT);
    assert(api->create(&second) == KWFPS_CLOCK_OK);
    assert(api->capture(first) == KWFPS_CLOCK_NOT_READY);

    _Alignas(16) unsigned char image[256] = {0};
    void *vtable[31] = {0};
    struct fake_client client = {vtable, 0};
    vtable[0x78 / sizeof(void *)] = (void *)(uintptr_t)fake_frame;
    *(void **)image = &client;
    GameLayout game = {0};
    game.module = image;
    game.timing.game_client_pointer = 0;
    game.timing.w3d_accumulated_time_ms = 64;
    game.timing.w3d_milliseconds_per_frame = 68;
    FpsConfig config = {0};
    config.target_fps = 60;
    g_game = &game;
    g_config = &config;
    g_installed = TRUE;
    sg_reset(&g_stall, 60);
    sg_clock_reset(&g_frame_clock, 60);
    assert(render_clock_source_barrier());
    for (unsigned i = 0; i < 60; ++i)
        advance_w3d_time(1);
    assert(*(uint32_t *)(image + 64) == W3D_RETAIL_MILLISECONDS_PER_SECOND && !g_w3d_time_remainder);
    client.frame = 90;
    assert(get_retail_visual_frame(&client) == 45);

    unsigned attempts = 0;
    assert(api->set_drain(no_ack, &attempts) == KWFPS_CLOCK_OK);
    assert(api->set_drain(no_ack, &attempts) == KWFPS_CLOCK_OK);
    assert(api->set_drain(no_ack, first) == KWFPS_CLOCK_BUSY);
    config.stall_guard = TRUE;
    client.frame = 120;
    frame_clock_sample_current();
    assert(api->capture(first) == KWFPS_CLOCK_OK && api->capture(first) == KWFPS_CLOCK_BUSY);
    assert(api->set_drain(NULL, NULL) == KWFPS_CLOCK_BUSY);
    const uint32_t original_time = *(uint32_t *)(image + 64), original_step = *(uint32_t *)(image + 68);
    const uint32_t original_remainder = g_w3d_time_remainder;
    const struct sg_clock original_clock = g_frame_clock;
    struct kwfps_clock_scope_v1 outer = {0}, inner = {0};
    assert(api->enter(first, &outer) == KWFPS_CLOCK_OK);
    assert(api->enter(first, &outer) == KWFPS_CLOCK_ARGUMENT);
    assert(api->release(first) == KWFPS_CLOCK_BUSY);
    client.frame = 180;
    advance_w3d_time(2);
    assert(get_retail_visual_frame(&client) == sg_clock_retail(&original_clock));
    assert(*(uint32_t *)(image + 64) == original_time && *(uint32_t *)(image + 68) == original_step);
    assert(g_w3d_time_remainder == original_remainder && !memcmp(&g_frame_clock, &original_clock, sizeof(original_clock)));
    assert(!render_clock_source_barrier() && !attempts);
    assert(api->leave(&outer) == KWFPS_CLOCK_OK);

    assert(api->capture(second) == KWFPS_CLOCK_OK);
    assert(api->enter(first, &outer) == KWFPS_CLOCK_OK);
    assert(api->enter(second, &inner) == KWFPS_CLOCK_OK);
    assert(api->leave(&outer) == KWFPS_CLOCK_BUSY);
    assert(api->destroy(second) == KWFPS_CLOCK_BUSY);
    assert(api->leave(&inner) == KWFPS_CLOCK_OK && api->leave(&outer) == KWFPS_CLOCK_OK);
    assert(api->release(second) == KWFPS_CLOCK_OK);
    assert(!render_clock_source_barrier() && attempts == 1);
    const uint32_t session = g_render_clock_session;
    g_config = NULL;
    game_patches_reset_state();
    assert(g_render_clock_session == session && attempts == 2);
    assert(api->release(first) == KWFPS_CLOCK_OK);
    game_patches_reset_state();
    assert(g_render_clock_session == session + 1 && !g_render_clock_readers);
    g_config = &config;
    assert(api->set_drain(NULL, NULL) == KWFPS_CLOCK_OK);

    struct worker_context context = {0};
    context.api = api;
    context.snapshot = first;
    context.entered = CreateEventW(NULL, TRUE, FALSE, NULL);
    context.proceed = CreateEventW(NULL, TRUE, FALSE, NULL);
    context.done = CreateEventW(NULL, TRUE, FALSE, NULL);
    assert(context.entered && context.proceed && context.done);
    assert(api->set_drain(acknowledge, &context) == KWFPS_CLOCK_OK);
    assert(api->capture(first) == KWFPS_CLOCK_OK);
    context.thread = CreateThread(NULL, 0, worker, &context, 0, NULL);
    assert(context.thread && WaitForSingleObject(context.entered, 5000) == WAIT_OBJECT_0);
    assert(context.wrong_capture == KWFPS_CLOCK_WRONG_THREAD && context.wrong_release == KWFPS_CLOCK_WRONG_THREAD &&
           context.wrong_destroy == KWFPS_CLOCK_WRONG_THREAD && context.enter_result == KWFPS_CLOCK_OK);
    assert(api->release(first) == KWFPS_CLOCK_BUSY);
    assert(render_clock_source_barrier());
    assert(context.leave_result == KWFPS_CLOCK_OK && !g_render_clock_readers && !g_render_clock_tid);
    CloseHandle(context.thread);
    CloseHandle(context.entered);
    CloseHandle(context.proceed);
    CloseHandle(context.done);
    assert(api->release(first) == KWFPS_CLOCK_NOT_READY);
    assert(api->destroy(first) == KWFPS_CLOCK_OK && api->destroy(second) == KWFPS_CLOCK_OK);
    assert(api->set_drain(NULL, NULL) == KWFPS_CLOCK_OK);
    puts("PASS render clock: native fallback, private timing, creator/thread fences, LIFO, ACK drain and session reset");
    return 0;
}
