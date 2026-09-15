#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
"""
Teensy 4.1 SOF Audio Hardware Diagnostic Tool
Queries real-time hardware registers, DMA TCD status, and SOF pipeline states via USB vendor request 0xFC.
"""

import sys
import struct
import usb.core
import usb.util

TEENSY_VID = 0x16C0
TEENSY_PID = 0x04D2

# Port mappings
PORTS = {
    'a': (3, (12, 4, 1, 3)),
    'b': (3, (12, 4, 1, 2)),
}

DIAG_FMT_44 = "<44I"
DIAG_FMT_32 = "<32I"
DIAG_FMT_24 = "<24I"
DIAG_FMT_LEGACY = "<20I"

def query_diag(target):
    target = target.lower()
    target_port = PORTS.get(target)
    
    devices = list(usb.core.find(find_all=True, idVendor=TEENSY_VID, idProduct=TEENSY_PID))
    match_dev = None
    for d in devices:
        port = (d.bus, tuple(d.port_numbers) if d.port_numbers else None)
        if target_port and port == target_port:
            match_dev = d
            break
        elif not target_port:
            match_dev = d
            break

    if not match_dev:
        print(f"Error: Teensy '{target}' not found!")
        return None

    try:
        # Vendor IN request 0xFC, recipient device (0xC0)
        data = match_dev.ctrl_transfer(0xC0, 0xFC, 0, 0, 256, timeout=1000)
    except Exception as e:
        print(f"Error reading diag from Teensy '{target}': {e}")
        return None

    if len(data) >= struct.calcsize(DIAG_FMT_44):
        unpacked = struct.unpack(DIAG_FMT_44, bytes(data[:struct.calcsize(DIAG_FMT_44)]))
        fields = [
            "tcsr", "rcsr", "tcr2", "rcr2", "tcr3", "rcr3", "tfr0", "rfr0",
            "rx_pkt_cnt", "tx_pkt_cnt", "pipe1_status", "pipe2_status",
            "dai5_state", "dai6_state",
            "dma_tx_csr", "dma_rx_csr", "dma_tx_saddr", "dma_rx_daddr",
            "dma_tx_citer", "dma_rx_citer",
            "c1_state", "c2_state", "c9_state", "c10_state",
            "tcr4", "rcr4", "tcr5", "rcr5",
            "pin8_mux", "pin8_daisy", "pin7_mux", "pin8_pad", "pin7_pad",
            "gpio1_psr", "gpio2_psr", "interface_type",
            "g_dbg0", "g_dbg1", "g_dbg2", "g_dbg3",
            "g_dbg4", "g_dbg5", "g_dbg6", "g_dbg7"
        ]
    elif len(data) >= struct.calcsize(DIAG_FMT_32):
        unpacked = struct.unpack(DIAG_FMT_32, bytes(data[:struct.calcsize(DIAG_FMT_32)]))
        fields = [
            "tcsr", "rcsr", "tcr2", "rcr2", "tcr3", "rcr3", "tfr0", "rfr0",
            "rx_pkt_cnt", "tx_pkt_cnt", "pipe1_status", "pipe2_status",
            "dai5_state", "dai6_state",
            "dma_tx_csr", "dma_rx_csr", "dma_tx_saddr", "dma_rx_daddr",
            "dma_tx_citer", "dma_rx_citer",
            "c1_state", "c2_state", "c9_state", "c10_state",
            "tcr4", "rcr4", "tcr5", "rcr5",
            "pin8_mux", "pin8_daisy", "pin7_mux", "pin8_pad"
        ]
    elif len(data) >= struct.calcsize(DIAG_FMT_24):
        unpacked = struct.unpack(DIAG_FMT_24, bytes(data[:struct.calcsize(DIAG_FMT_24)]))
        fields = [
            "tcsr", "rcsr", "tcr2", "rcr2", "tcr3", "rcr3", "tfr0", "rfr0",
            "rx_pkt_cnt", "tx_pkt_cnt", "pipe1_status", "pipe2_status",
            "dai5_state", "dai6_state",
            "dma_tx_csr", "dma_rx_csr", "dma_tx_saddr", "dma_rx_daddr",
            "dma_tx_citer", "dma_rx_citer",
            "c1_state", "c2_state", "c9_state", "c10_state"
        ]
    elif len(data) >= struct.calcsize(DIAG_FMT_LEGACY):
        unpacked = struct.unpack(DIAG_FMT_LEGACY, bytes(data[:struct.calcsize(DIAG_FMT_LEGACY)]))
        fields = [
            "tcsr", "rcsr", "tcr2", "rcr2", "tcr3", "rcr3", "tfr0", "rfr0",
            "rx_pkt_cnt", "tx_pkt_cnt", "pipe1_status", "pipe2_status",
            "dai5_state", "dai6_state",
            "dma_tx_csr", "dma_rx_csr", "dma_tx_saddr", "dma_rx_daddr",
            "dma_tx_citer", "dma_rx_citer"
        ]
    else:
        print(f"Error: Short response ({len(data)} bytes)")
        return None

    res = dict(zip(fields, unpacked))
    return res

def print_diag(target, res):
    print(f"=== Teensy 4.1 '{target.upper()}' Diag Report ===")
    if res.get('interface_type') == 2:
        print("  Active Hardware Interface: S/PDIF Transceiver (Board A Pin 14 ALT3 -> Board B Pin 15 ALT3)")
        print(f"  SPDIF SCR:  0x{res['tcsr']:08X} (TXFIFO_CTRL={(res['tcsr']>>10)&3}, USRC_SEL={(res['tcsr']>>2)&3})")
        print(f"  SPDIF SRPC: 0x{res['rcsr']:08X} (CLKSRC_SEL={(res['rcsr']>>7)&0xF}, GAIN_SEL={(res['rcsr']>>3)&7})")
        print(f"  SPDIF STC:  0x{res['tcr2']:08X} (TXCLK_SOURCE={(res['tcr2']>>8)&7}, SYSCLK_DF={res['tcr2']&0xFF})")
        print(f"  SPDIF SRFM: 0x{res['rcr2']:08X} (FREQMEAS={res['rcr2']&0xFFFFFF})")
        print(f"  SPDIF SIS:  0x{res['tcr3']:08X}")
        print(f"  SPDIF SIE:  0x{res['rcr3']:08X}")
    else:
        print("  Active Hardware Interface: SAI1 I2S (Pins 7, 8, 20, 21, 23)")
        print(f"  I2S1 TCSR: 0x{res['tcsr']:08X} (TE={bool(res['tcsr'] & (1<<31))}, BCE={bool(res['tcsr'] & (1<<28))}, FEF={bool(res['tcsr'] & (1<<18))})")
        print(f"  I2S1 RCSR: 0x{res['rcsr']:08X} (RE={bool(res['rcsr'] & (1<<31))}, BCE={bool(res['rcsr'] & (1<<28))}, FEF={bool(res['rcsr'] & (1<<18))})")
        print(f"  I2S1 TCR2: 0x{res['tcr2']:08X} (SYNC={(res['tcr2']>>30)&3}, BCD={(res['tcr2']>>24)&1}, MSEL={(res['tcr2']>>26)&3}, DIV={(res['tcr2']&0xFF)})")
        print(f"  I2S1 RCR2: 0x{res['rcr2']:08X} (SYNC={(res['rcr2']>>30)&3}, BCD={(res['rcr2']>>24)&1}, MSEL={(res['rcr2']>>26)&3}, DIV={(res['rcr2']&0xFF)})")
        print(f"  I2S1 TCR3: 0x{res['tcr3']:08X} (TCE={(res['tcr3']>>16)&0xF})")
        print(f"  I2S1 RCR3: 0x{res['rcr3']:08X} (RCE={(res['rcr3']>>16)&0xF})")
    if 'tcr4' in res and res.get('interface_type') != 2:
        print(f"  I2S1 TCR4: 0x{res['tcr4']:08X} (FSE={(res['tcr4']>>3)&1}, FSP={(res['tcr4']>>1)&1}, FSD={res['tcr4']&1}, SYWD={(res['tcr4']>>8)&0x1F}, FRSZ={(res['tcr4']>>16)&0xF})")
        print(f"  I2S1 RCR4: 0x{res['rcr4']:08X} (FSE={(res['rcr4']>>3)&1}, FSP={(res['rcr4']>>1)&1}, FSD={res['rcr4']&1}, SYWD={(res['rcr4']>>8)&0x1F}, FRSZ={(res['rcr4']>>16)&0xF})")
        print(f"  I2S1 TCR5: 0x{res['tcr5']:08X} (WNW={(res['tcr5']>>24)&0x1F}, W0W={(res['tcr5']>>16)&0x1F}, FBT={(res['tcr5']>>8)&0x1F})")
        print(f"  I2S1 RCR5: 0x{res['rcr5']:08X} (WNW={(res['rcr5']>>24)&0x1F}, W0W={(res['rcr5']>>16)&0x1F}, FBT={(res['rcr5']>>8)&0x1F})")
        p8_mux = res.get('pin8_mux', res.get('dbg0', 0))
        p8_daisy = res.get('pin8_daisy', res.get('dbg1', 0))
        p7_mux = res.get('pin7_mux', res.get('dbg2', 0))
        p8_pad = res.get('pin8_pad', res.get('dbg3', 0))
        p7_pad = res.get('pin7_pad', 0)
        print(f"  Pinmux: Pin8_MUX(B1_00)=0x{p8_mux:08X}, Pin8_DAISY=0x{p8_daisy:08X}, Pin7_MUX(B1_01)=0x{p7_mux:08X}, Pin8_PAD=0x{p8_pad:08X}, Pin7_PAD=0x{p7_pad:08X}")
    if 'gpio1_psr' in res:
        g1 = res['gpio1_psr']
        g2 = res['gpio2_psr']
        # Pin 23: GPIO1_25 (MCLK), Pin 20: GPIO1_26 (SYNC), Pin 21: GPIO1_27 (BCLK)
        # Pin 8: GPIO2_16 (RX), Pin 7: GPIO2_17 (TX)
        print(f"  GPIO: GPIO1_PSR=0x{g1:08X} (P23_MCLK={(g1>>25)&1}, P20_SYNC={(g1>>26)&1}, P21_BCLK={(g1>>27)&1}), GPIO2_PSR=0x{g2:08X} (P8_RX={(g2>>16)&1}, P7_TX={(g2>>17)&1})")
    if 'g_dbg0' in res:
        print(f"  SOF DAI Dbg: PB[calls={res['g_dbg0']}, copy=0x{res['g_dbg1']:08X}, p0=0x{res['g_dbg2']:08X}, p1=0x{res['g_dbg3']:08X}]")
        print(f"               CAP[calls={res['g_dbg4']}, copy=0x{res['g_dbg5']:08X}, p0=0x{res['g_dbg6']:08X}, p1=0x{res['g_dbg7']:08X}]")
    print(f"  I2S1 TFR0: 0x{res['tfr0']:08X} (WFP={(res['tfr0']>>16)&0x3F}, RFP={res['tfr0']&0x3F})")
    print(f"  I2S1 RFR0: 0x{res['rfr0']:08X} (WFP={(res['rfr0']>>16)&0x3F}, RFP={res['rfr0']&0x3F})")
    print(f"  USB Packets: Rx(Host PB)={res['rx_pkt_cnt']}, Tx(Host CAP)={res['tx_pkt_cnt']}")
    print(f"  Pipelines: P1(PB) state={res['pipe1_status']}, P2(CAP) state={res['pipe2_status']}")
    comps_str = f"DAI5={res.get('dai5_state')}, DAI6={res.get('dai6_state')}"
    if 'c1_state' in res:
        comps_str += f", C1(USB_PB)={res['c1_state']}, C2(VOL_PB)={res['c2_state']}, C9(VOL_CAP)={res['c9_state']}, C10(USB_CAP)={res['c10_state']}"
    print(f"  Components: {comps_str}")
    print(f"  eDMA TX: CSR=0x{res['dma_tx_csr']:04X}, SADDR=0x{res['dma_tx_saddr']:08X}, CITER={res['dma_tx_citer']}")
    print(f"  eDMA RX: CSR=0x{res['dma_rx_csr']:04X}, DADDR=0x{res['dma_rx_daddr']:08X}, CITER={res['dma_rx_citer']}")
    print()

if __name__ == '__main__':
    target = sys.argv[1] if len(sys.argv) > 1 else 'all'
    if target in ('all', 'both'):
        for t in ('a', 'b'):
            r = query_diag(t)
            if r:
                print_diag(t, r)
    else:
        r = query_diag(target)
        if r:
            print_diag(target, r)
