/* Linker Script for broadwell - TODO: this should be run through the GCC
   pre-processor to align heap and stack variables */

OUTPUT_ARCH(xtensa)

MEMORY
{
  iram_0_seg :	org = 0x00000000, len = 0x300	/* .ResetVector.text */
  iram_1_seg :	org = 0x00000300, len = 0x0	/* .ResetVector.literal */
  iram_2_seg :	org = 0x00000400, len = 0x180	/* .WindowVectors.text */
  
  iram_3_seg :	org = 0x00000678, len = 0x4	/* .Level2InterruptVector.literal */
  iram_4_seg :	org = 0x00000640, len = 0x38	/* .Level2InterruptVector.text */
  iram_5_seg :	org = 0x000006b4, len = 0x4	/* .Level3InterruptVector.literal */
  iram_6_seg :	org = 0x0000067c, len = 0x38	/* .Level3InterruptVector.text */
  iram_7_seg :	org = 0x000006f0, len = 0x4	/* .Level4InterruptVector.literal */
  iram_8_seg :	org = 0x000006b8, len = 0x38	/* .Level4InterruptVector.text */
  iram_9_seg :	org = 0x0000072c, len = 0x4	/* .Level5InterruptVector.literal */
  iram_10_seg :	org = 0x000006f4, len = 0x38	/* .Level5InterruptVector.text */
  
  iram_11_seg :	org = 0x00000a78, len = 0x4	/* .DebugExceptionVector.literal */
  iram_12_seg :	org = 0x00000a80, len = 0x38	/* .DebugExceptionVector.text */
  iram_13_seg :	org = 0x00000ab8, len = 0x4	/* .NMIExceptionVector.literal */
  iram_14_seg :	org = 0x00000ac0, len = 0x38	/* .NMIExceptionVector.text */
  
  iram_15_seg :	org = 0x00000580, len = 0x4	/* .KernelExceptionVector.literal */
  iram_16_seg :	org = 0x00000584, len = 0x38	/* .KernelExceptionVector.text */
  iram_17_seg :	org = 0x000005bc, len = 0x4	/* .UserExceptionVector.literal */
  iram_18_seg :	org = 0x000005c0, len = 0x38	/* .UserExceptionVector.text */
  
  iram_19_seg :	org = 0x0000063c, len = 0x3c	/* .DoubleExceptionVector.literal */
  iram_20_seg :	org = 0x000005fc, len = 0x40	/* .DoubleExceptionVector.text */
  
  iram_21_seg :	org = 0x0000af8, len = 0x4F508 /* 0 - 320kB IRAM */
  dram_seg0 :	org = 0x0400000, len = 0xa0000 /* 0 - 640kB DRAM */
}

PHDRS
{
  iram_0_phdr PT_LOAD;
  iram_1_phdr PT_LOAD;
  iram_2_phdr PT_LOAD;
  iram_3_phdr PT_LOAD;
  iram_4_phdr PT_LOAD;
  iram_5_phdr PT_LOAD;
  iram_6_phdr PT_LOAD;
  iram_7_phdr PT_LOAD;
  iram_8_phdr PT_LOAD;
  iram_9_phdr PT_LOAD;
  iram_10_phdr PT_LOAD;
  iram_11_phdr PT_LOAD;
  iram_12_phdr PT_LOAD;
  iram_13_phdr PT_LOAD;
  iram_14_phdr PT_LOAD;
  iram_15_phdr PT_LOAD;
  iram_16_phdr PT_LOAD;
  iram_17_phdr PT_LOAD;
  iram_18_phdr PT_LOAD;
  iram_19_phdr PT_LOAD;
  iram_20_phdr PT_LOAD;
  iram_21_phdr PT_LOAD;
  dram_phdr PT_LOAD;
  dram_rodata_phdr PT_LOAD;
  dram_bss_phdr PT_LOAD;
}


/*  Default entry point:  */
ENTRY(_ResetVector)
_iram_store_table = 0;

/* ABI0 does not use Window base */
PROVIDE(_memmap_vecbase_reset = 0x00000000);

/* Various memory-map dependent cache attribute settings: */
_memmap_cacheattr_wb_base = 0x44024000;
_memmap_cacheattr_wt_base = 0x11021000;
_memmap_cacheattr_bp_base = 0x22022000;
_memmap_cacheattr_unused_mask = 0x00F00FFF;
_memmap_cacheattr_wb_trapnull = 0x4422422F;
_memmap_cacheattr_wba_trapnull = 0x4422422F;
_memmap_cacheattr_wbna_trapnull = 0x25222222;
_memmap_cacheattr_wt_trapnull = 0x1122122F;
_memmap_cacheattr_bp_trapnull = 0x2222222F;
_memmap_cacheattr_wb_strict = 0x44F24FFF;
_memmap_cacheattr_wt_strict = 0x11F21FFF;
_memmap_cacheattr_bp_strict = 0x22F22FFF;
_memmap_cacheattr_wb_allvalid = 0x44224222;
_memmap_cacheattr_wt_allvalid = 0x11221222;
_memmap_cacheattr_bp_allvalid = 0x22222222;
PROVIDE(_memmap_cacheattr_reset = _memmap_cacheattr_unused_mask);

SECTIONS
{
  .ResetVector.text : ALIGN(4)
  {
    _ResetVector_text_start = ABSOLUTE(.);
    KEEP (*(.ResetVector.text))
    _ResetVector_text_end = ABSOLUTE(.);
  } >iram_0_seg :iram_0_phdr

  .ResetVector.literal : ALIGN(4)
  {
    _ResetVector_literal_start = ABSOLUTE(.);
    *(.ResetVector.literal)
    _ResetVector_literal_end = ABSOLUTE(.);
  } >iram_1_seg :iram_1_phdr

  .WindowVectors.text : ALIGN(4)
  {
    _WindowVectors_text_start = ABSOLUTE(.);
    KEEP (*(.WindowVectors.text))
    _WindowVectors_text_end = ABSOLUTE(.);
  } >iram_2_seg :iram_2_phdr

  .Level2InterruptVector.literal : ALIGN(4)
  {
    _Level2InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level2InterruptVector.literal)
    _Level2InterruptVector_literal_end = ABSOLUTE(.);
  } >iram_3_seg :iram_3_phdr

  .Level2InterruptVector.text : ALIGN(4)
  {
    _Level2InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level2InterruptVector.text))
    _Level2InterruptVector_text_end = ABSOLUTE(.);
  } >iram_4_seg :iram_4_phdr

  .Level3InterruptVector.literal : ALIGN(4)
  {
    _Level3InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level3InterruptVector.literal)
    _Level3InterruptVector_literal_end = ABSOLUTE(.);
  } >iram_5_seg :iram_5_phdr

  .Level3InterruptVector.text : ALIGN(4)
  {
    _Level3InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level3InterruptVector.text))
    _Level3InterruptVector_text_end = ABSOLUTE(.);
  } >iram_6_seg :iram_6_phdr

  .Level4InterruptVector.literal : ALIGN(4)
  {
    _Level4InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level4InterruptVector.literal)
    _Level4InterruptVector_literal_end = ABSOLUTE(.);
  } >iram_7_seg :iram_7_phdr

  .Level4InterruptVector.text : ALIGN(4)
  {
    _Level4InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level4InterruptVector.text))
    _Level4InterruptVector_text_end = ABSOLUTE(.);
  } >iram_8_seg :iram_8_phdr

  .Level5InterruptVector.literal : ALIGN(4)
  {
    _Level5InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level5InterruptVector.literal)
    _Level5InterruptVector_literal_end = ABSOLUTE(.);
  } >iram_9_seg :iram_9_phdr

  .Level5InterruptVector.text : ALIGN(4)
  {
    _Level5InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level5InterruptVector.text))
    _Level5InterruptVector_text_end = ABSOLUTE(.);
  } >iram_10_seg :iram_10_phdr

  .DebugExceptionVector.literal : ALIGN(4)
  {
    _DebugExceptionVector_literal_start = ABSOLUTE(.);
    *(.DebugExceptionVector.literal)
    _DebugExceptionVector_literal_end = ABSOLUTE(.);
  } >iram_11_seg :iram_11_phdr

  .DebugExceptionVector.text : ALIGN(4)
  {
    _DebugExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.DebugExceptionVector.text))
    _DebugExceptionVector_text_end = ABSOLUTE(.);
  } >iram_12_seg :iram_12_phdr

  .NMIExceptionVector.literal : ALIGN(4)
  {
    _NMIExceptionVector_literal_start = ABSOLUTE(.);
    *(.NMIExceptionVector.literal)
    _NMIExceptionVector_literal_end = ABSOLUTE(.);
  } >iram_13_seg :iram_13_phdr

  .NMIExceptionVector.text : ALIGN(4)
  {
    _NMIExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.NMIExceptionVector.text))
    _NMIExceptionVector_text_end = ABSOLUTE(.);
  } >iram_14_seg :iram_14_phdr

  .KernelExceptionVector.literal : ALIGN(4)
  {
    _KernelExceptionVector_literal_start = ABSOLUTE(.);
    *(.KernelExceptionVector.literal)
    _KernelExceptionVector_literal_end = ABSOLUTE(.);
  } >iram_15_seg :iram_15_phdr

  .KernelExceptionVector.text : ALIGN(4)
  {
    _KernelExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.KernelExceptionVector.text))
    _KernelExceptionVector_text_end = ABSOLUTE(.);
  } >iram_16_seg :iram_16_phdr

  .UserExceptionVector.literal : ALIGN(4)
  {
    _UserExceptionVector_literal_start = ABSOLUTE(.);
    *(.UserExceptionVector.literal)
    _UserExceptionVector_literal_end = ABSOLUTE(.);
  } >iram_17_seg :iram_17_phdr

  .UserExceptionVector.text : ALIGN(4)
  {
    _UserExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.UserExceptionVector.text))
    _UserExceptionVector_text_end = ABSOLUTE(.);
  } >iram_18_seg :iram_18_phdr

  .DoubleExceptionVector.literal : ALIGN(4)
  {
    _DoubleExceptionVector_literal_start = ABSOLUTE(.);
    *(.DoubleExceptionVector.literal)
    _DoubleExceptionVector_literal_end = ABSOLUTE(.);
  } >iram_19_seg :iram_19_phdr

  .DoubleExceptionVector.text : ALIGN(4)
  {
    _DoubleExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.DoubleExceptionVector.text))
    _DoubleExceptionVector_text_end = ABSOLUTE(.);
  } >iram_20_seg :iram_20_phdr

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
  } >iram_21_seg :iram_21_phdr

  .reset.rodata : ALIGN(4)
  {
    _reset_rodata_start = ABSOLUTE(.);
    *(.reset.rodata)
    _reset_rodata_end = ABSOLUTE(.);
  } >dram_seg0 :dram_phdr

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
  } >dram_seg0 :dram_rodata_phdr

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
  } >dram_seg0 :dram_rodata_phdr

  .lit4 : ALIGN(4)
  {
    _lit4_start = ABSOLUTE(.);
    *(*.lit4)
    *(.lit4.*)
    *(.gnu.linkonce.lit4.*)
    _lit4_end = ABSOLUTE(.);
  } >dram_seg0 :dram_rodata_phdr


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
  } >dram_seg0 :dram_bss_phdr

/* TODO: !! HSW and BDW have different amount of IRAM and DRAM !!
   Only setup for BDW atm !! */

/* TODO: need to clean up what we are using/not using here */
/* TODO: need to run the pre-processor on this to align with C headers */

  _end = 0x0049f000;
  PROVIDE(end = 0x0049f000);
  _stack_sentry = 0x0049f000;
 __stack = 0x004a0000;
  _heap_sentry = 0x004a0000;

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
  _system_heap = 0x00404000;


  /* module heap */
  _module_heap = 0x00405000;

  /* buffer heap */
  _buffer_heap = 0x0040c000;
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

