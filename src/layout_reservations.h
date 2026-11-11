/* SPDX-FileCopyrightText: 2026 N.Leiten
 * SPDX-License-Identifier: LicenseRef-KW-Game-Plugin
 */
#ifndef KW_FPS_LAYOUT_RESERVATIONS_H
#define KW_FPS_LAYOUT_RESERVATIONS_H
#include <kwf/plugin.h>
/* Called only after the strict upstream GameLayout/PE32 validation. */
typedef uint32_t(KWF_CALL *kw_fps_reserve_fn)(void *, uintptr_t, uint32_t);
static inline uint32_t kw_fps_reserve_rva(const GameLayout *g, kw_fps_reserve_fn reserve, void *context, uint32_t rva, uint32_t size)
{
    if (!rva || rva > g->pe_size_of_image || size > g->pe_size_of_image - rva)
        return KWF_ERROR;
    return reserve(context, (uintptr_t)g->module + rva, size);
}
static inline uint32_t kw_fps_reserve_scroll_slots(const GameLayout *g, kw_fps_reserve_fn reserve, void *context)
{
    const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)g->module;
    const IMAGE_NT_HEADERS32 *nt = (const IMAGE_NT_HEADERS32 *)(g->module + dos->e_lfanew);
    const IMAGE_SECTION_HEADER *sections = IMAGE_FIRST_SECTION(nt);
    uint32_t expected = (uint32_t)(uintptr_t)g->module + g->camera_input.scroll_by_function;
    unsigned found = 0, i;
    /* The view may not exist yet. Reserve all immutable in-image vtable slots
     * containing the independently resolved method. The upstream installer
     * later validates its live derived-class slot before selecting one. */
    for (i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        const IMAGE_SECTION_HEADER *s = &sections[i];
        uint32_t at;
        if (!(s->Characteristics & IMAGE_SCN_MEM_READ) || (s->Characteristics & (IMAGE_SCN_MEM_EXECUTE | IMAGE_SCN_MEM_WRITE)))
            continue;
        if (s->VirtualAddress > g->pe_size_of_image || s->Misc.VirtualSize > g->pe_size_of_image - s->VirtualAddress)
            return KWF_ERROR;
        for (at = (s->VirtualAddress + 3U) & ~3U; at + 4U <= s->VirtualAddress + s->Misc.VirtualSize; at += 4U) {
            if (load_u32(g->module + at) == expected) {
                if (++found > 32 || kw_fps_reserve_rva(g, reserve, context, at, 4) != KWF_OK)
                    return KWF_CONFLICT;
            }
        }
    }
    return found ? KWF_OK : KWF_UNSUPPORTED;
}
static inline uint32_t kw_fps_reserve_layout(const GameLayout *g, kw_fps_reserve_fn reserve, void *context)
{
    const struct {
        uint32_t rva, size;
    } ranges[] = {{g->bootstrap.runtime_config_call.instruction, 5},
                  {g->bootstrap.start_session_tail.instruction, 5},
                  {g->visual.camera_step_operand, 4},
                  {g->visual.laser_step_operand, 4},
                  {g->visual.model_step_operand, 4},
                  {g->keyboard.input_frame_increment, 6},
                  {g->keyboard.hardware_poll.instruction, 5},
                  {g->keyboard.check_repeat.instruction, 5},
                  {g->audio.update_vtable_entry, 4},
                  {g->camera_input.zoom_in_behavior_function, 5},
                  {g->camera_input.zoom_out_behavior_function, 5},
                  {g->camera_input.shake_decay_operand, 4},
                  {g->visual.tracer_reset_frame_call, 5},
                  {g->visual.tracer_update_frame_call, 6},
                  {g->visual.cloud_frame_call, 5},
                  {g->visual.anim2d_timestamp_frame_call, 5},
                  {g->visual.anim2d_update_frame_call, 5},
                  {g->visual.particle_simulation.instruction, 5},
                  {g->visual.gpu_particle_fps_operand, 4},
                  {g->visual.radius_cursor_fps_operand, 4},
                  {g->scheduler.phase_batch_block, 22},
                  {g->scheduler.phase_interpolation_function, 5},
                  {g->scheduler.client_slice_flush.instruction - 34U, 39},
                  {g->w3d_clock.special_advance_block, 17},
                  {g->w3d_clock.normal_advance_block, 20},
                  {g->pacing.display_limiter_branch, 2},
                  {g->pacing.limiter_interval_block, 25}};
    size_t i;
    for (i = 0; i < ARRAY_COUNT(ranges); ++i)
        if (kw_fps_reserve_rva(g, reserve, context, ranges[i].rva, ranges[i].size) != KWF_OK)
            return KWF_CONFLICT;
    return kw_fps_reserve_scroll_slots(g, reserve, context);
}
static inline uint32_t kw_fps_reserve_stall_layout(const GameLayout *g, kw_fps_reserve_fn reserve, void *context)
{
    return kw_fps_reserve_rva(g, reserve, context, g->audio.frame_delta_block, 43u);
}
#endif
