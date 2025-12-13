#pragma once

#include <stdint.h>

struct GDTEntry
{
	uint16_t limit_low;	 // Bits 0–15 of segment limit
	uint16_t base_low;	 // Bits 0–15 of base address
	uint8_t base_mid;	 // Bits 16–23 of base address
	uint8_t access;		 // Access flags
	uint8_t granularity; // Granularity + high 4 bits of limit
	uint8_t base_high;	 // Bits 24–31 of base address
} __attribute__((packed));

struct GDTDescriptor
{
	uint16_t limit;
	uint32_t base;
} __attribute__((packed));

constexpr uint32_t DEFAULT_GDT_SIZE = 5;

class DefaultGlobalDescriptorTable
{
public:
	void init();

private:
	void set_gdt_entry(uint32_t index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);
	void load();

private:
	GDTEntry _gdt[DEFAULT_GDT_SIZE];
	GDTDescriptor _gdt_descriptor;
};