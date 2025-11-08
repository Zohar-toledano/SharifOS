## in qemu
```text
ctrl+alt+2 to qemu shell
ctrl+alt+1 to vm
```

## gdb
```sh
make debug
# or 
qemu-system-i386 -cdrom "dist/sharifos.iso" -m 256M -s -S
```

then in GDB

```sh
gdb /build/kernel/Sharifos.kernel
	target remote :1234
	directory /mnt/d/coding/SharifOS/kernel/
	c
	ctrl+c
```
or in oneliner
```sh
gdb ./build/kernel/sharifos.kernel -ex "target remote :1234" -ex "directory /mnt/d/coding/SharifOS/kernel/" -ex "directory /mnt/d/coding/SharifOS/libc/" -ex "c"
```