/* Upstream-compatible writer API; every static mutation uses the host broker.
 * The pinned FPS implementation retains its transaction API. */
#include "runtime/memory_patch.h"
#include <kwf/plugin.h>
extern const struct kwf_context *kw_fps_context(void);
BOOL write_protected(void *destination, const void *source, size_t size)
{
    const struct kwf_context *c = kw_fps_context();
    if (!c || !size || size > PATCH_MAX_BYTES)
        return FALSE;
    return c->host->write(c->owner, (uintptr_t)destination, destination, source, (uint32_t)size) == KWF_OK;
}
void encode_u32(u8 *p, u32 value)
{
    unsigned i;
    for (i = 0; i < 4; ++i)
        p[i] = (u8)(value >> (8U * i));
}
void encode_rel32(u8 *operand, const u8 *next_instruction, const void *target)
{
    encode_u32(operand, (u32)((uintptr_t)target - (uintptr_t)next_instruction));
}
void patch_transaction_init(PatchTransaction *transaction)
{
    transaction->count = 0;
}
BOOL patch_transaction_write(PatchTransaction *transaction, void *destination, const void *replacement, size_t size)
{
    PatchRecord *record;
    if (!transaction || !destination || !replacement || !size || size > PATCH_MAX_BYTES || transaction->count == PATCH_TRANSACTION_CAPACITY)
        return FALSE;
    record = &transaction->records[transaction->count];
    record->destination = destination;
    record->size = size;
    memcpy(record->original, destination, size);
    if (!write_protected(destination, replacement, size))
        return FALSE;
    ++transaction->count;
    return TRUE;
}
void patch_transaction_rollback(PatchTransaction *transaction)
{
    const struct kwf_context *c = kw_fps_context();
    if (!c || !transaction)
        return;
    while (transaction->count) {
        PatchRecord *record = &transaction->records[--transaction->count];
        (void)c->host->restore(c->owner, (uintptr_t)record->destination, (uint32_t)record->size);
    }
}
