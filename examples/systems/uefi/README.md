# A Nori UEFI application

`hello_efi.nori` is the smallest program that can boot a real machine: a UEFI application, a PE32+
image with subsystem `EFI_APPLICATION` that firmware loads and calls.

Where a kernel (`examples/systems/kernel`) is an ELF booted by QEMU's `-kernel` through PVH or a multiboot
loader, a UEFI application is a PE32+ image firmware loads anywhere it pleases (a real `.reloc`
section says how to fix it up) and calls as `EfiMain(ImageHandle, SystemTable)` under the Microsoft
x64 calling convention.

```
noric --build hello_efi.nori BOOTX64.EFI --uefi

mkdir -p /tmp/esp/EFI/BOOT && cp BOOTX64.EFI /tmp/esp/EFI/BOOT/
cp /usr/share/edk2/x64/OVMF_VARS.4m.fd /tmp/vars.fd
qemu-system-x86_64 -machine q35 -m 512M -display none -no-reboot \
    -drive if=pflash,format=raw,readonly=on,file=/usr/share/edk2/x64/OVMF_CODE.4m.fd \
    -drive if=pflash,format=raw,file=/tmp/vars.fd \
    -drive format=raw,file=fat:rw:/tmp/esp \
    -serial stdio
```

The image prints to the firmware console, which under `-serial stdio` also lands on the terminal, and
returns `0` (`EFI_SUCCESS`) to the firmware.

## How it is put together

The `--uefi` flag selects a small runtime floor and a PE32+ writer in the native back end:

* The floor provides the `EfiMain` entry, the firmware-console printer (`ConOut->OutputString`,
  offsets taken from the UEFI 2.10 spec), the Boot-Services heap (`AllocatePool`/`FreePool`), and the
  runtime's `cfg(bare)` seams.
* The PE32+ writer emits subsystem 10 with no imports or TLS directory and a real `.reloc` section, so
  firmware can load the image at any address.
