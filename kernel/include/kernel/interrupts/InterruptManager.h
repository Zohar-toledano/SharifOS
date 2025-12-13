#pragma once
#include <stdint.h>
#include <stdlib.h>

typedef struct
{
	uint16_t offset_1;		 // offset bits 0..15
	uint16_t selector;		 // a code segment selector in DefaultGlobalDescriptorTable or LDT
	uint8_t zero;			 // unused, set to 0
	uint8_t type_attributes; // gate type, dpl, and p fields
	uint16_t offset_2;		 // offset bits 16..31
} __attribute__((packed)) idt_entry_t;

typedef struct
{
	uint16_t size;
	uint32_t offset;
} __attribute__((packed)) idt_descriptor_t;

#define DEFINE_ISR(name)                                 \
	extern "C" void name();                              \
	extern "C" __attribute__((naked)) void name##_stub() \
	{                                                    \
		asm volatile(                                    \
			"call " #name "\n\t"                         \
			"iret\n\t");                                 \
	}                                                    \
	void name()

class InterruptManager
{

public:
	void init();

private:
	void fill_idt();
	void set_idt_entry(uint8_t idx, uint32_t offset, uint16_t selector, uint8_t gate_type, uint8_t dpl, bool present);
	void load_idt();

private:
	idt_entry_t *_idt;
};
