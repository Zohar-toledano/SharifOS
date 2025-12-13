#include <kernel/interrupts/InterruptManager.h>
#include <kernel/kernel.h>
#include <stdio.h>

#define IDT_ENTRIES 256
constexpr uint32_t IDT_PAGES = 1;

DEFINE_ISR(default_interrupt_handler)
{
	printf("Unhandled interrupt!\n");
}

DEFINE_ISR(default_exception_handler)
{
	printf("Unhandled Exception!\n");
}

void InterruptManager::init()
{
	_idt = (idt_entry_t *)krn.memoryManager.pagingManager.allocate(IDT_PAGES);
	if (!_idt)
	{
		printf("Failed to allocate memory for IDT.\n");
		abort();
	}
	fill_idt();
	load_idt();
}

void InterruptManager::fill_idt()
{
	uint16_t vector = 0;
	for (; vector < 32; vector++)
	{
		set_idt_entry(vector, (uint32_t)&default_exception_handler_stub, 0x08, 0xE, 0, true);
	}
	for (; vector < IDT_ENTRIES; vector++)
	{
		set_idt_entry(vector, (uint32_t)&default_interrupt_handler_stub, 0x08, 0xE, 0, true);
	}
}

void InterruptManager::set_idt_entry(uint8_t idx, uint32_t offset, uint16_t selector, uint8_t gate_type, uint8_t dpl, bool present)
{
	idt_entry_t &entry = _idt[idx];
	entry.offset_1 = offset & 0xFFFF;														 // Lower 16 bits of the offset
	entry.selector = selector;																 // Code segment selector in DefaultGlobalDescriptorTable or LDT
	entry.zero = 0;																			 // Reserved, must be zero
	entry.type_attributes = (gate_type & 0x0F) | ((dpl & 0x03) << 5) | (present ? 0x80 : 0); // Gate type, DPL, and P fields
	entry.offset_2 = (offset >> 16) & 0xFFFF;												 // Upper 16 bits of the offset
}

void InterruptManager::load_idt()
{
	idt_descriptor_t idt_desc;
	idt_desc.offset = (uint32_t)_idt;
	idt_desc.size = sizeof(idt_entry_t) * IDT_ENTRIES - 1;
	asm volatile("lidt %0" : : "m"(idt_desc));
	asm volatile("sti" : :);
}
