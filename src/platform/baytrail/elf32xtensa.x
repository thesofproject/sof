/* This linker script generated from xt-genldscripts.tpp for LSP lsp_BYT_Android_Platform_ddr_stack */
/* Linker Script for default link */

OUTPUT_ARCH(xtensa)

MEMORY
{
  ddrconf0_seg :                      	org = 0xC0000000, len = 0x1000
  ddr0_seg :                          	org = 0xC0001000, len = 0x1FF000
  iram0_0_seg :                       	org = 0xFF2C0000, len = 0x2E0
  iram0_1_seg :                       	org = 0xFF2C02E0, len = 0x120
  iram0_2_seg :                       	org = 0xFF2C0400, len = 0x178
  iram0_3_seg :                       	org = 0xFF2C0578, len = 0x4
  iram0_4_seg :                       	org = 0xFF2C057C, len = 0x1C
  iram0_5_seg :                       	org = 0xFF2C0598, len = 0x4
  iram0_6_seg :                       	org = 0xFF2C059C, len = 0x1C
  iram0_7_seg :                       	org = 0xFF2C05B8, len = 0x4
  iram0_8_seg :                       	org = 0xFF2C05BC, len = 0x1C
  iram0_9_seg :                       	org = 0xFF2C05D8, len = 0x4
  iram0_10_seg :                      	org = 0xFF2C05DC, len = 0x1C
  iram0_11_seg :                      	org = 0xFF2C05F8, len = 0x4
  iram0_12_seg :                      	org = 0xFF2C05FC, len = 0x1C
  iram0_13_seg :                      	org = 0xFF2C0618, len = 0x4
  iram0_14_seg :                      	org = 0xFF2C061C, len = 0x1C
  iram0_15_seg :                      	org = 0xFF2C0638, len = 0x4
  iram0_16_seg :                      	org = 0xFF2C063C, len = 0x1C
  iram0_17_seg :                      	org = 0xFF2C0658, len = 0x4
  iram0_18_seg :                      	org = 0xFF2C065C, len = 0x1C
  iram0_19_seg :                      	org = 0xFF2C0678, len = 0x4
  iram0_20_seg :                      	org = 0xFF2C067C, len = 0x1C
  iram0_21_seg :                      	org = 0xFF2C0698, len = 0x13968
  dram0_0_seg :                       	org = 0xFF300000, len = 0x8
  dram0_1_seg :                       	org = 0xFF300008, len = 0x26FF8
}

PHDRS
{
  ddrconf0_phdr PT_LOAD;
  ddrconf0_bss_phdr PT_LOAD;
  ddr0_phdr PT_LOAD;
  ddr0_bss_phdr PT_LOAD;
  iram0_0_phdr PT_LOAD;
  iram0_1_phdr PT_LOAD;
  iram0_2_phdr PT_LOAD;
  iram0_3_phdr PT_LOAD;
  iram0_4_phdr PT_LOAD;
  iram0_5_phdr PT_LOAD;
  iram0_6_phdr PT_LOAD;
  iram0_7_phdr PT_LOAD;
  iram0_8_phdr PT_LOAD;
  iram0_9_phdr PT_LOAD;
  iram0_10_phdr PT_LOAD;
  iram0_11_phdr PT_LOAD;
  iram0_12_phdr PT_LOAD;
  iram0_13_phdr PT_LOAD;
  iram0_14_phdr PT_LOAD;
  iram0_15_phdr PT_LOAD;
  iram0_16_phdr PT_LOAD;
  iram0_17_phdr PT_LOAD;
  iram0_18_phdr PT_LOAD;
  iram0_19_phdr PT_LOAD;
  iram0_20_phdr PT_LOAD;
  iram0_21_phdr PT_LOAD;
  dram0_0_phdr PT_LOAD;
  dram0_1_phdr PT_LOAD;
  dram0_1_bss_phdr PT_LOAD;
  dram0_2_phdr PT_LOAD;
}


/*  Default entry point:  */
ENTRY(_ResetVector)
_rom_store_table = 0;
PROVIDE(_memmap_vecbase_reset = 0xff2c0400);
/* Various memory-map dependent cache attribute settings: */
_memmap_cacheattr_wb_base = 0x44024000;
_memmap_cacheattr_wt_base = 0x11021000;
_memmap_cacheattr_bp_base = 0x22022000;
_memmap_cacheattr_unused_mask = 0x00F00FFF;
_memmap_cacheattr_wb_trapnull = 0x4422422F;
_memmap_cacheattr_wba_trapnull = 0x4422422F;
_memmap_cacheattr_wbna_trapnull = 0x5522522F;
_memmap_cacheattr_wt_trapnull = 0x1122122F;
_memmap_cacheattr_bp_trapnull = 0x2222222F;
_memmap_cacheattr_wb_strict = 0x44F24FFF;
_memmap_cacheattr_wt_strict = 0x11F21FFF;
_memmap_cacheattr_bp_strict = 0x22F22FFF;
_memmap_cacheattr_wb_allvalid = 0x44224222;
_memmap_cacheattr_wt_allvalid = 0x11221222;
_memmap_cacheattr_bp_allvalid = 0x22222222;
PROVIDE(_memmap_cacheattr_reset = _memmap_cacheattr_wb_trapnull);

SECTIONS
{

  .ddrconf.rodata : ALIGN(4)
  {
    _ddrconf_rodata_start = ABSOLUTE(.);
    *(.ddrconf.rodata)
    _ddrconf_rodata_end = ABSOLUTE(.);
  } >ddrconf0_seg :ddrconf0_phdr

  .ddrconf.text : ALIGN(4)
  {
    _ddrconf_text_start = ABSOLUTE(.);
    *(.ddrconf.literal .ddrconf.text)
    _ddrconf_text_end = ABSOLUTE(.);
  } >ddrconf0_seg :ddrconf0_phdr

  .ddrconf.data : ALIGN(4)
  {
    _ddrconf_data_start = ABSOLUTE(.);
    *(.ddrconf.data)
    _ddrconf_data_end = ABSOLUTE(.);
  } >ddrconf0_seg :ddrconf0_phdr

  .ddrconf.bss (NOLOAD) : ALIGN(8)
  {
    . = ALIGN (8);
    _ddrconf_bss_start = ABSOLUTE(.);
    *(.ddrconf.bss)
    . = ALIGN (8);
    _ddrconf_bss_end = ABSOLUTE(.);
  } >ddrconf0_seg :ddrconf0_bss_phdr

  .ddr.rodata : ALIGN(4)
  {
    _ddr_rodata_start = ABSOLUTE(.);
    *(.ddr.rodata)
    _ddr_rodata_end = ABSOLUTE(.);
  } >ddr0_seg :ddr0_phdr

  .ddr.text : ALIGN(4)
  {
    _ddr_text_start = ABSOLUTE(.);
    *(.ddr.literal .ddr.text)
    _ddr_text_end = ABSOLUTE(.);
  } >ddr0_seg :ddr0_phdr

  .ddr1.text : ALIGN(4)
  {
    _ddr1_text_start = ABSOLUTE(.);
    *(.ddr1.literal .ddr1.text)
    _ddr1_text_end = ABSOLUTE(.);
  } >ddr0_seg :ddr0_phdr

  .ddr.data : ALIGN(4)
  {
    _ddr_data_start = ABSOLUTE(.);
    *(.ddr.data)
    _ddr_data_end = ABSOLUTE(.);
  } >ddr0_seg :ddr0_phdr

  .libdrc.text : ALIGN(4)
  {
    _libdrc_text_start = ABSOLUTE(.);
    *(.libdrc.literal .libdrc.text)
    _libdrc_text_end = ABSOLUTE(.);
  } >ddr0_seg :ddr0_phdr

  .libdrc.data : ALIGN(4)
  {
    _libdrc_data_start = ABSOLUTE(.);
    *(.libdrc.data)
    _libdrc_data_end = ABSOLUTE(.);
  } >ddr0_seg :ddr0_phdr

  .libdrc.rodata : ALIGN(4)
  {
    _libdrc_rodata_start = ABSOLUTE(.);
    *(.libdrc.rodata)
    _libdrc_rodata_end = ABSOLUTE(.);
  } >ddr0_seg :ddr0_phdr

  .libstereo_equalizer.text : ALIGN(4)
  {
    _libstereo_equalizer_text_start = ABSOLUTE(.);
    *(.libstereo_equalizer.literal .libstereo_equalizer.text)
    _libstereo_equalizer_text_end = ABSOLUTE(.);
  } >ddr0_seg :ddr0_phdr

  .libstereo_equalizer.data : ALIGN(4)
  {
    _libstereo_equalizer_data_start = ABSOLUTE(.);
    *(.libstereo_equalizer.data)
    _libstereo_equalizer_data_end = ABSOLUTE(.);
  } >ddr0_seg :ddr0_phdr

  .libstereo_equalizer.rodata : ALIGN(4)
  {
    _libstereo_equalizer_rodata_start = ABSOLUTE(.);
    *(.libstereo_equalizer.rodata)
    _libstereo_equalizer_rodata_end = ABSOLUTE(.);
  } >ddr0_seg :ddr0_phdr

  .libmono_equalizer.text : ALIGN(4)
  {
    _libmono_equalizer_text_start = ABSOLUTE(.);
    *(.libmono_equalizer.literal .libmono_equalizer.text)
    _libmono_equalizer_text_end = ABSOLUTE(.);
  } >ddr0_seg :ddr0_phdr

  .libmono_equalizer.data : ALIGN(4)
  {
    _libmono_equalizer_data_start = ABSOLUTE(.);
    *(.libmono_equalizer.data)
    _libmono_equalizer_data_end = ABSOLUTE(.);
  } >ddr0_seg :ddr0_phdr

  .libmono_equalizer.rodata : ALIGN(4)
  {
    _libmono_equalizer_rodata_start = ABSOLUTE(.);
    *(.libmono_equalizer.rodata)
    _libmono_equalizer_rodata_end = ABSOLUTE(.);
  } >ddr0_seg :ddr0_phdr

  .libdc_removal.text : ALIGN(4)
  {
    _libdc_removal_text_start = ABSOLUTE(.);
    *(.libdc_removal.literal .libdc_removal.text)
    _libdc_removal_text_end = ABSOLUTE(.);
  } >ddr0_seg :ddr0_phdr

  .libdc_removal.data : ALIGN(4)
  {
    _libdc_removal_data_start = ABSOLUTE(.);
    *(.libdc_removal.data)
    _libdc_removal_data_end = ABSOLUTE(.);
  } >ddr0_seg :ddr0_phdr

  .libdc_removal.rodata : ALIGN(4)
  {
    _libdc_removal_rodata_start = ABSOLUTE(.);
    *(.libdc_removal.rodata)
    _libdc_removal_rodata_end = ABSOLUTE(.);
  } >ddr0_seg :ddr0_phdr

  .ddr.bss (NOLOAD) : ALIGN(8)
  {
    . = ALIGN (8);
    _ddr_bss_start = ABSOLUTE(.);
    *(.ddr.bss)
    *(.libdc_removal.bss)
    *(libstereo_equalizer.bss)
    *(.libmono_equalizer.bss)
    *(.libdrc.bss)
    . = ALIGN (8);
    _ddr_bss_end = ABSOLUTE(.);
  } >ddr0_seg :ddr0_bss_phdr

  .ResetVector.text : ALIGN(4)
  {
    _ResetVector_text_start = ABSOLUTE(.);
    KEEP (*(.ResetVector.text))
    _ResetVector_text_end = ABSOLUTE(.);
  } >iram0_0_seg :iram0_0_phdr

  .ResetVector.literal : ALIGN(4)
  {
    _ResetVector_literal_start = ABSOLUTE(.);
    *(.ResetVector.literal)
    _ResetVector_literal_end = ABSOLUTE(.);
  } >iram0_1_seg :iram0_1_phdr

  .WindowVectors.text : ALIGN(4)
  {
    _WindowVectors_text_start = ABSOLUTE(.);
    KEEP (*(.WindowVectors.text))
    _WindowVectors_text_end = ABSOLUTE(.);
  } >iram0_2_seg :iram0_2_phdr

  .Level2InterruptVector.literal : ALIGN(4)
  {
    _Level2InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level2InterruptVector.literal)
    _Level2InterruptVector_literal_end = ABSOLUTE(.);
  } >iram0_3_seg :iram0_3_phdr

  .Level2InterruptVector.text : ALIGN(4)
  {
    _Level2InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level2InterruptVector.text))
    _Level2InterruptVector_text_end = ABSOLUTE(.);
  } >iram0_4_seg :iram0_4_phdr

  .Level3InterruptVector.literal : ALIGN(4)
  {
    _Level3InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level3InterruptVector.literal)
    _Level3InterruptVector_literal_end = ABSOLUTE(.);
  } >iram0_5_seg :iram0_5_phdr

  .Level3InterruptVector.text : ALIGN(4)
  {
    _Level3InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level3InterruptVector.text))
    _Level3InterruptVector_text_end = ABSOLUTE(.);
  } >iram0_6_seg :iram0_6_phdr

  .Level4InterruptVector.literal : ALIGN(4)
  {
    _Level4InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level4InterruptVector.literal)
    _Level4InterruptVector_literal_end = ABSOLUTE(.);
  } >iram0_7_seg :iram0_7_phdr

  .Level4InterruptVector.text : ALIGN(4)
  {
    _Level4InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level4InterruptVector.text))
    _Level4InterruptVector_text_end = ABSOLUTE(.);
  } >iram0_8_seg :iram0_8_phdr

  .Level5InterruptVector.literal : ALIGN(4)
  {
    _Level5InterruptVector_literal_start = ABSOLUTE(.);
    *(.Level5InterruptVector.literal)
    _Level5InterruptVector_literal_end = ABSOLUTE(.);
  } >iram0_9_seg :iram0_9_phdr

  .Level5InterruptVector.text : ALIGN(4)
  {
    _Level5InterruptVector_text_start = ABSOLUTE(.);
    KEEP (*(.Level5InterruptVector.text))
    _Level5InterruptVector_text_end = ABSOLUTE(.);
  } >iram0_10_seg :iram0_10_phdr

  .DebugExceptionVector.literal : ALIGN(4)
  {
    _DebugExceptionVector_literal_start = ABSOLUTE(.);
    *(.DebugExceptionVector.literal)
    _DebugExceptionVector_literal_end = ABSOLUTE(.);
  } >iram0_11_seg :iram0_11_phdr

  .DebugExceptionVector.text : ALIGN(4)
  {
    _DebugExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.DebugExceptionVector.text))
    _DebugExceptionVector_text_end = ABSOLUTE(.);
  } >iram0_12_seg :iram0_12_phdr

  .NMIExceptionVector.literal : ALIGN(4)
  {
    _NMIExceptionVector_literal_start = ABSOLUTE(.);
    *(.NMIExceptionVector.literal)
    _NMIExceptionVector_literal_end = ABSOLUTE(.);
  } >iram0_13_seg :iram0_13_phdr

  .NMIExceptionVector.text : ALIGN(4)
  {
    _NMIExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.NMIExceptionVector.text))
    _NMIExceptionVector_text_end = ABSOLUTE(.);
  } >iram0_14_seg :iram0_14_phdr

  .KernelExceptionVector.literal : ALIGN(4)
  {
    _KernelExceptionVector_literal_start = ABSOLUTE(.);
    *(.KernelExceptionVector.literal)
    _KernelExceptionVector_literal_end = ABSOLUTE(.);
  } >iram0_15_seg :iram0_15_phdr

  .KernelExceptionVector.text : ALIGN(4)
  {
    _KernelExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.KernelExceptionVector.text))
    _KernelExceptionVector_text_end = ABSOLUTE(.);
  } >iram0_16_seg :iram0_16_phdr

  .UserExceptionVector.literal : ALIGN(4)
  {
    _UserExceptionVector_literal_start = ABSOLUTE(.);
    *(.UserExceptionVector.literal)
    _UserExceptionVector_literal_end = ABSOLUTE(.);
  } >iram0_17_seg :iram0_17_phdr

  .UserExceptionVector.text : ALIGN(4)
  {
    _UserExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.UserExceptionVector.text))
    _UserExceptionVector_text_end = ABSOLUTE(.);
  } >iram0_18_seg :iram0_18_phdr

  .DoubleExceptionVector.literal : ALIGN(4)
  {
    _DoubleExceptionVector_literal_start = ABSOLUTE(.);
    *(.DoubleExceptionVector.literal)
    _DoubleExceptionVector_literal_end = ABSOLUTE(.);
  } >iram0_19_seg :iram0_19_phdr

  .DoubleExceptionVector.text : ALIGN(4)
  {
    _DoubleExceptionVector_text_start = ABSOLUTE(.);
    KEEP (*(.DoubleExceptionVector.text))
    _DoubleExceptionVector_text_end = ABSOLUTE(.);
  } >iram0_20_seg :iram0_20_phdr

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
  } >iram0_21_seg :iram0_21_phdr

  .reset.rodata : ALIGN(4)
  {
    _reset_rodata_start = ABSOLUTE(.);
    *(.reset.rodata)
    _reset_rodata_end = ABSOLUTE(.);
  } >dram0_0_seg :dram0_0_phdr

  .dramfwplatcfgslot0.data : ALIGN(4)
  {
    _dramfwplatcfgslot0_data_start = ABSOLUTE(.);
    *(.dramfwplatcfgslot0.data)
    _dramfwplatcfgslot0_data_end = ABSOLUTE(.);
  } >dram0_1_seg :dram0_1_phdr

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
    LONG(_ddrconf_bss_start)
    LONG(_ddrconf_bss_end)
    LONG(_ddr_bss_start)
    LONG(_ddr_bss_end)
    LONG(_bss_start)
    LONG(_bss_end)
    _bss_table_end = ABSOLUTE(.);
    _rodata_end = ABSOLUTE(.);
  } >dram0_1_seg :dram0_1_phdr

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
  } >dram0_1_seg :dram0_1_phdr

  .lit4 : ALIGN(4)
  {
    _lit4_start = ABSOLUTE(.);
    *(*.lit4)
    *(.lit4.*)
    *(.gnu.linkonce.lit4.*)
    _lit4_end = ABSOLUTE(.);
  } >dram0_1_seg :dram0_1_phdr

  .dramfwslot0.data : ALIGN(4)
  {
    _dramfwslot0_data_start = ABSOLUTE(.);
    *(.dramfwslot0.data)
    _dramfwslot0_data_end = ABSOLUTE(.);
  } >dram0_1_seg :dram0_1_phdr

  .dramfwslot1.data : ALIGN(4)
  {
    _dramfwslot1_data_start = ABSOLUTE(.);
    *(.dramfwslot1.data)
    _dramfwslot1_data_end = ABSOLUTE(.);
  } >dram0_1_seg :dram0_1_phdr

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
  } >dram0_1_seg :dram0_1_bss_phdr
  _end = 0xff327000;
  PROVIDE(end = 0xff327000);
  _stack_sentry = 0xff327000;
 __stack = 0xff328000;
  _heap_sentry = 0xff328000;
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

