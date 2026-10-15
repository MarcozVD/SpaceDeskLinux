/* SDL scancode (USB HID usage) -> Windows virtual key + PC/AT set-1 scancode. */
#ifndef SDV_KEYMAP_H
#define SDV_KEYMAP_H

#include <stdint.h>

#include <SDL.h>

struct sdv_key {
    uint16_t vk;
    uint16_t scan;
    uint8_t extended;
};

/* Returns 0 and fills *out when the key is known, -1 otherwise. */
int sdv_keymap_lookup(SDL_Scancode sc, struct sdv_key *out);

#endif
