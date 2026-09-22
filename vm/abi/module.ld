MEMORY {
  CODE (rx) : ORIGIN = 0x00018000, LENGTH = 96K
  DATA (rw) : ORIGIN = 0x20014000, LENGTH = 192K
}
ENTRY(vm_entry)
SECTIONS {
  .text : { KEEP(*(.entry)) *(.text*) *(.rodata*) . = ALIGN(4); } > CODE
  .data : { *(.data*) . = ALIGN(32); } > DATA
  .bss (NOLOAD) : { *(.bss*) *(COMMON) . = ALIGN(32); } > DATA
  /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) *(.eh_frame*) }
  ASSERT(SIZEOF(.text) <= 96K, "VM code does not fit RAM1 module window")
  ASSERT(SIZEOF(.data) + SIZEOF(.bss) < 192K, "VM has no remaining RAM1 workspace")
}
