#ifndef IDT_H
#define IDT_H

#include <stdint.h>

void idt_init(void);

void idt_register_irq_handler(int irq, void (*handler)(void));

#endif