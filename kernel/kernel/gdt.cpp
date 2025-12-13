#include <kernel/gdt.h>

extern "C" void setGdt(void *gdtr_ptr);

void DefaultGlobalDescriptorTable::set_gdt_entry(uint32_t index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
    GDTEntry *entry = &_gdt[index];
    entry->limit_low = limit & 0xFFFF;
    entry->base_low = base & 0xFFFF;
    entry->base_mid = (base >> 16) & 0xFF;
    entry->access = access;
    entry->granularity = ((gran & 0xF0) | ((limit >> 16) & 0x0F));
    entry->base_high = (base >> 24) & 0xFF;
}

void DefaultGlobalDescriptorTable::load()
{
    setGdt(&_gdt_descriptor);
}

void DefaultGlobalDescriptorTable::init()
{
    set_gdt_entry(0, 0, 0, 0, 0);
    set_gdt_entry(1, 0, 0x000FFFFF, 0x9A, 0xCF); // Code segment
    set_gdt_entry(2, 0, 0x000FFFFF, 0x92, 0xCF); // Data segment
    set_gdt_entry(3, 0, 0x000FFFFF, 0xfa, 0xCF);
    set_gdt_entry(4, 0, 0x000FFFFF, 0xf2, 0xCF);
    _gdt_descriptor = {sizeof(_gdt) - 1, (uint32_t)_gdt};
    
    load();
}
