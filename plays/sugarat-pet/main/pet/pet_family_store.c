/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
#include "pet_family_store.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void path(const pet_family_store_t *s, unsigned slot, char out[224])
{ snprintf(out, 224, "%s/family-%c.dat", s->directory, slot ? 'b' : 'a'); }
/* Result: absent (0), complete (1), recognized future header (2), or unreadable (-1). */
static int read_slot(const pet_family_store_t *s, unsigned slot, uint8_t *bytes)
{
    char name[224]; path(s, slot, name);
    FILE *file = fopen(name, "rb");
    if (!file) return errno == ENOENT ? 0 : -1;
    size_t count = fread(bytes, 1, PET_FAMILY_BYTES, file);
    int extra = fgetc(file); bool bad = ferror(file) != 0;
    if (fclose(file) != 0) bad = true;
    if (!bad && count >= 8 && !memcmp(bytes, "SGF", 3) &&
        (bytes[3] != '2' || bytes[4] != 2 || bytes[5] || bytes[6] || bytes[7])) return 2;
    return !bad && count == PET_FAMILY_BYTES && extra == EOF ? 1 : -1;
}
bool pet_family_store_open(pet_family_store_t *s, const char *directory, pet_family_t *family)
{
    memset(s, 0, sizeof(*s));
    if (!directory || !directory[0] || strlen(directory) >= sizeof(s->directory)) {
        s->notice = "Family storage unavailable"; return false;
    }
    strcpy(s->directory, directory);
    uint8_t *bytes = malloc(PET_FAMILY_BYTES);
    pet_family_t *candidate = malloc(sizeof(*candidate));
    if (!bytes || !candidate) { free(bytes); free(candidate); s->notice = "Family memory unavailable"; return false; }
    bool unknown = false, bad = false;
    for (unsigned slot = 0; slot < 2; slot++) {
        int read = read_slot(s, slot, bytes);
        if (!read) continue;
        if (read == 2) { unknown = true; continue; }
        if (read < 0) { bad = true; continue; }
        uint32_t sequence = 0;
        pet_family_result_t result = pet_family_decode(candidate, &sequence, bytes, PET_FAMILY_BYTES);
        if (result == PET_FAMILY_VERSION) { unknown = true; continue; }
        if (result != PET_FAMILY_OK) { bad = true; continue; }
        if (!s->loaded || sequence > s->sequence) {
            *family = *candidate; s->sequence = sequence; s->next_slot = slot ^ 1u; s->loaded = true;
        }
    }
    free(bytes); free(candidate);
    s->writable = !unknown && (!bad || s->loaded) && s->sequence != UINT32_MAX;
    s->notice = unknown ? "Newer family: update firmware" : !s->writable ? "Family unreadable: writes stopped" : bad ? "Recovered previous family" : NULL;
    return s->writable;
}
bool pet_family_store_save(pet_family_store_t *s, const pet_family_t *family)
{
    if (!s->writable) return false;
    if (s->backend_save) return s->backend_save(s->backend_context, s, family);
    uint8_t *bytes = malloc(PET_FAMILY_BYTES), *check = malloc(PET_FAMILY_BYTES);
    bool ok = bytes && check && s->sequence < UINT32_MAX && pet_family_encode(family, s->sequence + 1u, bytes);
    char name[224]; path(s, s->next_slot, name);
    FILE *file = ok ? fopen(name, "wb") : NULL;
    if (!file) ok = false;
    if (file) {
        ok = fwrite(bytes, 1, PET_FAMILY_BYTES, file) == PET_FAMILY_BYTES && fflush(file) == 0;
        if (ok) ok = fsync(fileno(file)) == 0;
        if (fclose(file) != 0) ok = false;
    }
    if (ok) ok = read_slot(s, s->next_slot, check) == 1 && memcmp(bytes, check, PET_FAMILY_BYTES) == 0;
    free(bytes); free(check);
    if (!ok) { s->writable = false; s->notice = "Save unconfirmed: restart to recover"; return false; }
    s->sequence++; s->next_slot ^= 1u; s->loaded = true; s->notice = NULL;
    if (s->sequence == UINT32_MAX) { s->writable = false; s->notice = "Family save limit reached"; }
    return true;
}
