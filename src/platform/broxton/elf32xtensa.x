/* Linker Script for broxton - TODO: this should be run through the GCC
   pre-processor to align heap and stack variables */

OUTPUT_ARCH(xtensa)

MEMORY
{
  rom_0_seg :		org = 0xbefe0000, len = 0x300	/* .ResetVector.text */
  rom_1_seg :		org = 0xbefe0300, len = 0x0	/* .ResetVector.literal */
  rom_2_seg :		org = 0xbefe0800, len = 0x178	/* .WindowVectors.text - CoE maps this at 0xbe000400 */
  rom_3_seg :		org = 0xbefe0978, len = 0x8	/* .Level2InterruptVector.literal */
  rom_4_seg :		org = 0xbefe0980, len = 0x38	/* .Level2InterruptVector.text */
  rom_5_seg :		org = 0xbefe09b8, len = 0x8	/* .Level3InterruptVector.literal */
  rom_6_seg :		org = 0xbefe09c0, len = 0x38	/* .Level3InterruptVector.text */
  rom_7_seg :		org = 0xbefe09f8, len = 0x8	/* .Level4InterruptVector.literal */
  rom_8_seg :		org = 0xbefe0a00, len = 0x38	/* .Level4InterruptVector.text */
  rom_9_seg :		org = 0xbefe0a38, len = 0x8	/* .Level5InterruptVector.literal */
  rom_10_seg :		org = 0xbefe0a40, len = 0x38	/* .Level5InterruptVector.text */
  rom_11_seg :		org = 0xbefe0a78, len = 0x8	/* .DebugExceptionVector.literal */
  rom_12_seg :		org = 0xbefe0a80, len = 0x38	/* .DebugExceptionVector.text */
  rom_13_seg :		org = 0xbefe0ab8, len = 0x8	/* .NMIExceptionVector.literal */
  rom_14_seg :		org = 0xbefe0ac0, len = 0x38	/* .NMIExceptionVector.text */
  rom_15_seg :		org = 0xbefe0bf8, len = 0x8	/* .KernelExceptionVector.literal */
  rom_16_seg :		org = 0xbefe0b00, len = 0x38	/* .KernelExceptionVector.text */
  rom_17_seg :		org = 0xbefe0b38, len = 0x8	/* .UserExceptionVector.literal */
  rom_18_seg :		org = 0xbefe0b40, len = 0x38	/* .UserExceptionVector.text */
  rom_19_seg :		org = 0xbefe0b78, len = 0x48	/* .DoubleExceptionVector.literal */
  rom_20_seg :		org = 0xbefe0bc0, len = 0x40	/* .DoubleExceptionVector.text */
  rom_21_seg :		org = 0xbefe0300, len = 0x100	/* .memoryExceptionVector.literal */
  rom_22_seg :		org = 0xbefe0400, len = 0x180	/* .MemoryExceptionVector.text */
  l2_hp_sram_seg0 :	org = 0xbe000000, len = 0x20000 /* 0 - 512kB High Performance SRAM */
  l2_hp_sram_seg1 :	org = 0xbe200000, len = 0x60000 /* 0 - 512kB High Performance SRAM */
  l2_lp_sram_seg :	org = 0xbe800000, len = 0x20000 /* 128kB Low Power SRAM */
}

PHDRS
{
  rom_0_phdr PT_LOAD;
  rom_1_phdr PT_LOAD;
  rom_2_phdr PT_LOAD;
  rom_3_phdr PT_LOAD;
  rom_4_phdr PT_LOAD;
  rom_5_phdr PT_LOAD;
  rom_6_phdr PT_LOAD;
  rom_7_phdr PT_LOAD;
  rom_8_phdr PT_LOAD;
  rom_9_phdr PT_LOAD;
  rom_10_phdr PT_LOAD;
  rom_11_phdr PT_LOAD;
  rom_12_phdr PT_LOAD;
  rom_13_phdr PT_LOAD;
  rom_14_phdr PT_LOAD;
  rom_15_phdr PT_LOAD;
  rom_16_phdr PT_LOAD;
  rom_17_phdr PT_LOAD;
  rom_18_phdr PT_LOAD;
  rom_19_phdr PT_LOAD;
  rom_20_phdr PT_LOAD;
  rom_21_phdr PT_LOAD;
  rom_22_phdr PT_LOAD;
  l2_hp_sram_phdr PT_LOAD;
  l2_hp_sram_rodata_phdr PT_LOAD;
  l2_hp_sram_bss_phdr PT_LOAD;
}


/*  Default entry point:  */
ENTRY(_ResetVector)
_rom_store_table = 0;

/* ABI0 does not use Window base */
PROVIDE(_memmap_vecbase_reset = 0xbefe0800);

/* Various memory-map dependent cache attribute settings: */
_memmap_cacheattr = 0xff42fff2;
PROVIDE(_memmap_cacheattr_reset = _memmap_cacheattr);

SECTIONS
{
  .ResetVector.text : ALIGN(4)
  {
    _ResetVector_text_start = ABSOLUTE(.);
    KEEP (*(.ResetVector.text))
    _ResetVector_text_end = ABSOLUTE(.);
  } >rom_0_seg :rom_0_phdr

  .ResetVector.literal : ALIGN(4)
  {
    _ResetVector_literal_start = ABSOLUTE(.);
    *(.ResetVector.literal)
    _ResetVector_literal_end = ABSOLUTE(.);
  } >rom_1_seg :rom_1_phdr

  .WindowVectors.text : ALIGN(4)
  {
    _WindowVectors_text_start = ABSOLUTE(.);
    KEEP (*(.WindowVectors.text))
    _WindowVectors_text_end = ABSOLUTE(.);
  } >rom_2_seg :rom_2_phdr

  .Level2InterruptVector.literal : ALIGN(4)
  {
    _Level2InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level2InterruptVector.literal)
    _Level2InterruptVector_literal_end = ABSOLUTE(.);
  } >rom_3_seg :rom_3_phdr

  .Level2InterruptVector.text : ALIGN(4)
  {
    _Level2InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level2InterruptVector.text))
    _Level2InterruptVector_text_end = ABSOLUTE(.);
  } >rom_4_seg :rom_4_phdr

  .Level3InterruptVector.literal : ALIGN(4)
  {
    _Level3InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level3InterruptVector.literal)
    _Level3InterruptVector_literal_end = ABSOLUTE(.);
  } >rom_5_seg :rom_5_phdr

  .Level3InterruptVector.text : ALIGN(4)
  {
    _Level3InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level3InterruptVector.text))
    _Level3InterruptVector_text_end = ABSOLUTE(.);
  } >rom_6_seg :rom_6_phdr

  .Level4InterruptVector.literal : ALIGN(4)
  {
    _Level4InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level4InterruptVector.literal)
    _Level4InterruptVector_literal_end = ABSOLUTE(.);
  } >rom_7_seg :rom_7_phdr

  .Level4InterruptVector.text : ALIGN(4)
  {
    _Level4InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level4InterruptVector.text))
    _Level4InterruptVector_text_end = ABSOLUTE(.);
  } >rom_8_seg :rom_8_phdr

  .Level5InterruptVector.literal : ALIGN(4)
  {
    _Level5InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level5InterruptVector.literal)
    _Level5InterruptVector_literal_end = ABSOLUTE(.);
  } >rom_9_seg :rom_9_phdr

  .Level5InterruptVector.text : ALIGN(4)
  {
    _Level5InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level5InterruptVector.text))
    _Level5InterruptVector_text_end = ABSOLUTE(.);
  } >rom_10_seg :rom_10_phdr

  .DebugExceptionVector.literal : ALIGN(4)
  {
    _DebugExceptionVector_literal_start = ABSOLUTE(.);
    *(.DebugExceptionVector.literal)
    _DebugExceptionVector_literal_end = ABSOLUTE(.);
  } >rom_11_seg :rom_11_phdr

  .DebugExceptionVector.text : ALIGN(4)
  {
    _DebugExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.DebugExceptionVector.text))
    _DebugExceptionVector_text_end = ABSOLUTE(.);
  } >rom_12_seg :rom_12_phdr

  .NMIExceptionVector.literal : ALIGN(4)
  {
    _NMIExceptionVector_literal_start = ABSOLUTE(.);
    *(.NMIExceptionVector.literal)
    _NMIExceptionVector_literal_end = ABSOLUTE(.);
  } >rom_13_seg :rom_13_phdr

  .NMIExceptionVector.text : ALIGN(4)
  {
    _NMIExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.NMIExceptionVector.text))
    _NMIExceptionVector_text_end = ABSOLUTE(.);
  } >rom_14_seg :rom_14_phdr

  .KernelExceptionVector.literal : ALIGN(4)
  {
    _KernelExceptionVector_literal_start = ABSOLUTE(.);
    *(.KernelExceptionVector.literal)
    _KernelExceptionVector_literal_end = ABSOLUTE(.);
  } >rom_15_seg :rom_15_phdr

  .KernelExceptionVector.text : ALIGN(4)
  {
    _KernelExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.KernelExceptionVector.text))
    _KernelExceptionVector_text_end = ABSOLUTE(.);
  } >rom_16_seg :rom_16_phdr

  .UserExceptionVector.literal : ALIGN(4)
  {
    _UserExceptionVector_literal_start = ABSOLUTE(.);
    *(.UserExceptionVector.literal)
    _UserExceptionVector_literal_end = ABSOLUTE(.);
  } >rom_17_seg :rom_17_phdr

  .UserExceptionVector.text : ALIGN(4)
  {
    _UserExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.UserExceptionVector.text))
    _UserExceptionVector_text_end = ABSOLUTE(.);
  } >rom_18_seg :rom_18_phdr

  .DoubleExceptionVector.literal : ALIGN(4)
  {
    _DoubleExceptionVector_literal_start = ABSOLUTE(.);
    *(.DoubleExceptionVector.literal)
    _DoubleExceptionVector_literal_end = ABSOLUTE(.);
  } >rom_19_seg :rom_19_phdr

  .DoubleExceptionVector.text : ALIGN(4)
  {
    _DoubleExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.DoubleExceptionVector.text))
    _DoubleExceptionVector_text_end = ABSOLUTE(.);
  } >rom_20_seg :rom_20_phdr

  .MemErrorVector.literal : ALIGN(4)
  {
    _MemErrorVector_literal_start = ABSOLUTE(.);
    *(.MemErrorVector.literal)
    _MemErrorVector_literal_end = ABSOLUTE(.);
  } >rom_21_seg :rom_21_phdr

  .MemErrorVector.text : ALIGN(4)
  {
    _MemErrorVector_text_start = ABSOLUTE(.);
    KEEP (*(.MemErrorVector.text))
    _MemErrorVector_text_end = ABSOLUTE(.);
  } >rom_22_seg :rom_22_phdr

  .ResetHandler.text : ALIGN(4)
  {
    _ResetHandler_text_start = ABSOLUTE(.);
    KEEP (*(._ResetHandler.text))
    _ResetHandler_text_end = ABSOLUTE(.);
  } >l2_hp_sram_seg0 :l2_hp_sram_phdr

  .text : ALIGN(4)
  {
    _stext = .;
    _text_start = ABSOLUTE(.);
    *(.entry.text)
    *(.init.literal)
    KEEP(*(.init))
    *(.literal .text .literal.* .text.* .stub .gnu.warning .gnu.linkonce.literal.* .gnu.linkonce.t.*.literal .gnu.linkonce.t.*)
    *(.fini.literal)
    KEEP(*(.fini))
    *(.gnu.version)
    _text_end = ABSOLUTE(.);
    _etext = .;
  } >l2_hp_sram_seg0 :l2_hp_sram_phdr


  .rodata : ALIGN(4)
  {
    _rodata_start = ABSOLUTE(.);
    *(.rodata)
    *(.rodata.*)
    *(.gnu.linkonce.r.*)
    *(.rodata1)
    __XT_EXCEPTION_TABLE__ = ABSOLUTE(.);
    KEEP (*(.xt_except_table))
    KEEP (*(.gcc_except_table))
    *(.gnu.linkonce.e.*)
    *(.gnu.version_r)
    KEEP (*(.eh_frame))
    /*  C++ constructor and destructor tables, properly ordered:  */
    KEEP (*crtbegin.o(.ctors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
    KEEP (*crtbegin.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
    /*  C++ exception handlers table:  */
    __XT_EXCEPTION_DESCS__ = ABSOLUTE(.);
    *(.xt_except_desc)
    *(.gnu.linkonce.h.*)
    __XT_EXCEPTION_DESCS_END__ = ABSOLUTE(.);
    *(.xt_except_desc_end)
    *(.dynamic)
    *(.gnu.version_d)
    . = ALIGN(4);		/* this table MUST be 4-byte aligned */
    _bss_table_start = ABSOLUTE(.);
    LONG(_bss_start)
    LONG(_bss_end)
    _bss_table_end = ABSOLUTE(.);
    _rodata_end = ABSOLUTE(.);
  } >l2_hp_sram_seg0 :l2_hp_sram_rodata_phdr

  .data : ALIGN(4)
  {
    _data_start = ABSOLUTE(.);
    *(.data)
    *(.data.*)
    *(.gnu.linkonce.d.*)
    KEEP(*(.gnu.linkonce.d.*personality*))
    *(.data1)
    *(.sdata)
    *(.sdata.*)
    *(.gnu.linkonce.s.*)
    *(.sdata2)
    *(.sdata2.*)
    *(.gnu.linkonce.s2.*)
    KEEP(*(.jcr))
    _data_end = ABSOLUTE(.);
  } >l2_hp_sram_seg0 :l2_hp_sram_rodata_phdr

  .lit4 : ALIGN(4)
  {
    _lit4_start = ABSOLUTE(.);
    *(*.lit4)
    *(.lit4.*)
    *(.gnu.linkonce.lit4.*)
    _lit4_end = ABSOLUTE(.);
  } >l2_hp_sram_seg0 :l2_hp_sram_rodata_phdr


  .bss (NOLOAD) : ALIGN(8)
  {
    . = ALIGN (8);
    _bss_start = ABSOLUTE(.);
    *(.dynsbss)
    *(.sbss)
    *(.sbss.*)
    *(.gnu.linkonce.sb.*)
    *(.scommon)
    *(.sbss2)
    *(.sbss2.*)
    *(.gnu.linkonce.sb2.*)
    *(.dynbss)
    *(.bss)
    *(.bss.*)
    *(.gnu.linkonce.b.*)
    *(COMMON)
    . = ALIGN (8);
    _bss_end = ABSOLUTE(.);
  } >l2_hp_sram_seg0 :l2_hp_sram_bss_phdr

/* TODO: need to clean up what we are using/not using here */
/* TODO: need to run the pre-processor on this to align with C headers */

  _end = 0xbe1ff000;
  PROVIDE(end = 0xbe1ff000);
  _stack_sentry = 0xbe1ff000;
 __stack = 0xbe200000;
  _heap_sentry = 0xbe200000;

  /* The Heap and stack are organised like this :-
   *
   * DRAM start --------
   *            RO Data
   *            Data
   *            BSS
   *            System Heap
   *            Module Heap
   *            Module Buffers
   *            Stack
   * DRAM End   ---------
   */

  /* System Heap */
  _system_heap = 0xbe100000;


  /* module heap */
  _module_heap = 0xbe101000;

  /* buffer heap */
  _buffer_heap = 0xbe108000;
  _buffer_heap_end = _stack_sentry;

  .debug  0 :  { *(.debug) }
  .line  0 :  { *(.line) }
  .debug_srcinfo  0 :  { *(.debug_srcinfo) }
  .debug_sfnames  0 :  { *(.debug_sfnames) }
  .debug_aranges  0 :  { *(.debug_aranges) }
  .debug_pubnames  0 :  { *(.debug_pubnames) }
  .debug_info  0 :  { *(.debug_info) }
  .debug_abbrev  0 :  { *(.debug_abbrev) }
  .debug_line  0 :  { *(.debug_line) }
  .debug_frame  0 :  { *(.debug_frame) }
  .debug_str  0 :  { *(.debug_str) }
  .debug_loc  0 :  { *(.debug_loc) }
  .debug_macinfo  0 :  { *(.debug_macinfo) }
  .debug_weaknames  0 :  { *(.debug_weaknames) }
  .debug_funcnames  0 :  { *(.debug_funcnames) }
  .debug_typenames  0 :  { *(.debug_typenames) }
  .debug_varnames  0 :  { *(.debug_varnames) }
  .xt.insn 0 :
  {
    KEEP (*(.xt.insn))
    KEEP (*(.gnu.linkonce.x.*))
  }
  .xt.prop 0 :
  {
    KEEP (*(.xt.prop))
    KEEP (*(.xt.prop.*))
    KEEP (*(.gnu.linkonce.prop.*))
  }
  .xt.lit 0 :
  {
    KEEP (*(.xt.lit))
    KEEP (*(.xt.lit.*))
    KEEP (*(.gnu.linkonce.p.*))
  }
  .xt.profile_range 0 :
  {
    KEEP (*(.xt.profile_range))
    KEEP (*(.gnu.linkonce.profile_range.*))
  }
  .xt.profile_ranges 0 :
  {
    KEEP (*(.xt.profile_ranges))
    KEEP (*(.gnu.linkonce.xt.profile_ranges.*))
  }
  .xt.profile_files 0 :
  {
    KEEP (*(.xt.profile_files))
    KEEP (*(.gnu.linkonce.xt.profile_files.*))
  }
}

