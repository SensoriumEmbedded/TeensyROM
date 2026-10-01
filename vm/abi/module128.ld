/* The 128 KiB code window. Needs a host that publishes a code_floor at or
   below 0x10000 (zero reads as 0x18000), which vm_host_takes_code checks.
   vm/abi/module.ld links at the base every host accepts. Otherwise
   identical; tools/verify-extensions.mjs holds the two in step. */
MEMORY {
  CODE (rx) : ORIGIN = 0x00010000, LENGTH = 128K
  DATA (rw) : ORIGIN = 0x20014000, LENGTH = 192K
}
ENTRY(vm_entry)
SECTIONS {
  .text : { KEEP(*(.entry)) *(.text*) *(.rodata*) . = ALIGN(4); } > CODE
  .data : { *(.data*) . = ALIGN(32); } > DATA
  .bss (NOLOAD) : { *(.bss*) *(COMMON) . = ALIGN(32); } > DATA
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.eh_frame*) }
  ASSERT(SIZEOF(.text) <= 128K, "VM code does not fit RAM1 module window")
  ASSERT(SIZEOF(.data) + SIZEOF(.bss) < 192K, "VM has no remaining RAM1 workspace")
}
