#include <stdio.h>
#include <cstdint>
#include <kernel/memory/memorymanager.h>
#include <kernel/kernel.h>
#include <stdlib.h>

Kernel Kernel::i;

void Kernel::init(uintptr_t p_multiboot_info)
{
	gdtInitializer.init();
	terminal.init();
	memoryManager.init(p_multiboot_info);
	interruptManager.init();
	// abort(); 
	printf("welcome to SharifOSs \n");
	
	// for (int i = 0; i < 100; i++)
	// {
	// 	printf("hello world %d\n", i);
	// }

}
extern "C" void kernel_main(uint32_t magic, uint32_t mbi_addr)
{

	krn.init(mbi_addr);
}
