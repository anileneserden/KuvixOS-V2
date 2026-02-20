#ifndef MOUSE_PS2_H
#define MOUSE_PS2_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern int mouse_x;
extern int mouse_y;

// (Opsiyonel) telemetri/debug
extern volatile int32_t g_mouse_last_dx;
extern volatile int32_t g_mouse_last_dy;
extern volatile int32_t g_mouse_last_wheel;
extern volatile uint32_t g_mouse_irq_count;

void ps2_mouse_init(void);
void mouse_handler(void);

// IRQ/poll byte assembly
void ps2_mouse_handle_byte(uint8_t data);

// Event queue pop:
// dx,dy = hareket
// wheel = teker delta (genelde -1/+1, yoksa 0)
// buttons = bit0 L, bit1 R, bit2 M
int ps2_mouse_pop(int* dx, int* dy, int* wheel, uint8_t* buttons);

void ps2_mouse_poll(void);
void ps2_mouse_update(void);

#ifdef __cplusplus
}
#endif

#endif