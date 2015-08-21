/*
* extern void lpe_setup_tlb(u32 vpn, u32 ppn);
*
* Maps the selected virtual 512MB segment to the specified physical
* 512MB segment. (Lower 29 bits of both parameters are ignored.)
* VPN in current segment case is not handled
*
* Entry:
* a2 VPN/TLB entry to set
* a3 PPN to target
* Exit:
* None
*/
.text
.global lpe_setup_tlb
.type lpe_setup_tlb,@function
.align 4

lpe_setup_tlb:

entry sp, 64

#if XCHAL_HAVE_XLT_CACHEATTR

	movi a5, 0xE0000000 // tlb mask, upper 3 bits
	movi a6, 0f // PC where ITLB is set
	and a4, a3, a5 // upper 3 bits of PPN area
	and a7, a2, a5 // upper 3 bits of VPN area
	and a6, a6, a5 // upper 3 bits of local PC area

	//beq a7, a6, 1f // branch if current PC's region
	
	// Note that in the WITLB section, we don't do any load/stores.
	// May not be an issue, but it's important in the DTLB case.
	ritlb1 a5, a7 // get current PPN+AM of segment for I
	rdtlb1 a6, a7 // get current PPN+AM of segment for D
	extui a5, a5, 0, 4 // keep only AM for I
	extui a6, a6, 0, 4 // keep only AM for D
	add a2, a4, a5 // combine new PPN with orig AM for I
	add a3, a4, a6 // combine new PPN with orig AM for D
	
0: 	witlb a2, a7 // write new tlb mapping for I
	wdtlb a3, a7 // write new tlb mapping for D
	isync
	//j 2f

//1f:
	// Handle if VPN is current segment

//2f:

#endif
	retw
	
	
