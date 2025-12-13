#pragma once

#include <kernel/tty.h>
#include <kernel/memory/MemoryManager.h>
#include <kernel/gdt.h>
#include <kernel/interrupts/InterruptManager.h>

#define krn Kernel::i


class Kernel
{
public:
    Kernel() = default;
    Kernel(const Kernel &) = delete;
    Kernel &operator=(const Kernel &) = delete;

public:
    void init(uintptr_t p_multiboot_info);

public:
    static Kernel i;

public:
    DefaultGlobalDescriptorTable defaultGDT;
    Terminal terminal;
    MemoryManager memoryManager;
    InterruptManager interruptManager;
};
