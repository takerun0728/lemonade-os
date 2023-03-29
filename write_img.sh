mkdir -p mnt
mount disk.img mnt
mkdir -p mnt/EFI/BOOT
cp loader/DEBUG_CLANG38/X64/Loader.efi mnt/EFI/BOOT/BOOTX64.EFI
cp kernel/kernel.elf mnt/
umount mnt
