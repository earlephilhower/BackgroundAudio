/*
    libmad - MPEG audio decoder library
    Copyright (C) 2000-2004 Underbit Technologies, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    $Id: D.dat,v 1.9 2004/01/23 09:41:32 rob Exp $
*/

/*
    These are the coefficients for the subband synthesis window. This is a
    reordered version of Table B.3 from ISO/IEC 11172-3.

    Every value is parameterized so that shift optimizations can be made at
    compile-time. For example, every value can be right-shifted 12 bits to
    minimize multiply instruction times without any loss of accuracy.
*/

{
    PRESHIFT(0x00000000) /*  0.000000000 */,	/*  0 */
    -PRESHIFT(0x0001d000) /* -0.000442505 */,
    PRESHIFT(0x000d5000) /*  0.003250122 */,
    -PRESHIFT(0x001cb000) /* -0.007003784 */,
    PRESHIFT(0x007f5000) /*  0.031082153 */,
    -PRESHIFT(0x01421000) /* -0.078628540 */,
    PRESHIFT(0x019ae000) /*  0.100311279 */,
    -PRESHIFT(0x09271000) /* -0.572036743 */,
    PRESHIFT(0x1251e000) /*  1.144989014 */,
    PRESHIFT(0x09271000) /*  0.572036743 */,
    PRESHIFT(0x019ae000) /*  0.100311279 */,
    PRESHIFT(0x01421000) /*  0.078628540 */,
    PRESHIFT(0x007f5000) /*  0.031082153 */,
    PRESHIFT(0x001cb000) /*  0.007003784 */,
    PRESHIFT(0x000d5000) /*  0.003250122 */,
    PRESHIFT(0x0001d000) /*  0.000442505 */,

    PRESHIFT(0x00000000) /*  0.000000000 */,
    -PRESHIFT(0x0001d000) /* -0.000442505 */,
    PRESHIFT(0x000d5000) /*  0.003250122 */,
    -PRESHIFT(0x001cb000) /* -0.007003784 */,
    PRESHIFT(0x007f5000) /*  0.031082153 */,
    -PRESHIFT(0x01421000) /* -0.078628540 */,
    PRESHIFT(0x019ae000) /*  0.100311279 */,
    -PRESHIFT(0x09271000) /* -0.572036743 */,
    PRESHIFT(0x1251e000) /*  1.144989014 */,
    PRESHIFT(0x09271000) /*  0.572036743 */,
    PRESHIFT(0x019ae000) /*  0.100311279 */,
    PRESHIFT(0x01421000) /*  0.078628540 */,
    PRESHIFT(0x007f5000) /*  0.031082153 */,
    PRESHIFT(0x001cb000) /*  0.007003784 */,
    PRESHIFT(0x000d5000) /*  0.003250122 */,
    PRESHIFT(0x0001d000) /*  0.000442505 */
},

{
    -PRESHIFT(0x00001000) /* -0.000015259 */,	/*  1 */
        -PRESHIFT(0x0001f000) /* -0.000473022 */,
        PRESHIFT(0x000da000) /*  0.003326416 */,
        -PRESHIFT(0x00207000) /* -0.007919312 */,
        PRESHIFT(0x007d0000) /*  0.030517578 */,
        -PRESHIFT(0x0158d000) /* -0.084182739 */,
        PRESHIFT(0x01747000) /*  0.090927124 */,
        -PRESHIFT(0x099a8000) /* -0.600219727 */,
        PRESHIFT(0x124f0000) /*  1.144287109 */,
        PRESHIFT(0x08b38000) /*  0.543823242 */,
        PRESHIFT(0x01bde000) /*  0.108856201 */,
        PRESHIFT(0x012b4000) /*  0.073059082 */,
        PRESHIFT(0x0080f000) /*  0.031478882 */,
        PRESHIFT(0x00191000) /*  0.006118774 */,
        PRESHIFT(0x000d0000) /*  0.003173828 */,
        PRESHIFT(0x0001a000) /*  0.000396729 */,

        -PRESHIFT(0x00001000) /* -0.000015259 */,
        -PRESHIFT(0x0001f000) /* -0.000473022 */,
        PRESHIFT(0x000da000) /*  0.003326416 */,
        -PRESHIFT(0x00207000) /* -0.007919312 */,
        PRESHIFT(0x007d0000) /*  0.030517578 */,
        -PRESHIFT(0x0158d000) /* -0.084182739 */,
        PRESHIFT(0x01747000) /*  0.090927124 */,
        -PRESHIFT(0x099a8000) /* -0.600219727 */,
        PRESHIFT(0x124f0000) /*  1.144287109 */,
        PRESHIFT(0x08b38000) /*  0.543823242 */,
        PRESHIFT(0x01bde000) /*  0.108856201 */,
        PRESHIFT(0x012b4000) /*  0.073059082 */,
        PRESHIFT(0x0080f000) /*  0.031478882 */,
        PRESHIFT(0x00191000) /*  0.006118774 */,
        PRESHIFT(0x000d0000) /*  0.003173828 */,
        PRESHIFT(0x0001a000) /*  0.000396729 */
    },

{
    -PRESHIFT(0x00001000) /* -0.000015259 */,	/*  2 */
        -PRESHIFT(0x00023000) /* -0.000534058 */,
        PRESHIFT(0x000de000) /*  0.003387451 */,
        -PRESHIFT(0x00245000) /* -0.008865356 */,
        PRESHIFT(0x007a0000) /*  0.029785156 */,
        -PRESHIFT(0x016f7000) /* -0.089706421 */,
        PRESHIFT(0x014a8000) /*  0.080688477 */,
        -PRESHIFT(0x0a0d8000) /* -0.628295898 */,
        PRESHIFT(0x12468000) /*  1.142211914 */,
        PRESHIFT(0x083ff000) /*  0.515609741 */,
        PRESHIFT(0x01dd8000) /*  0.116577148 */,
        PRESHIFT(0x01149000) /*  0.067520142 */,
        PRESHIFT(0x00820000) /*  0.031738281 */,
        PRESHIFT(0x0015b000) /*  0.005294800 */,
        PRESHIFT(0x000ca000) /*  0.003082275 */,
        PRESHIFT(0x00018000) /*  0.000366211 */,

        -PRESHIFT(0x00001000) /* -0.000015259 */,
        -PRESHIFT(0x00023000) /* -0.000534058 */,
        PRESHIFT(0x000de000) /*  0.003387451 */,
        -PRESHIFT(0x00245000) /* -0.008865356 */,
        PRESHIFT(0x007a0000) /*  0.029785156 */,
        -PRESHIFT(0x016f7000) /* -0.089706421 */,
        PRESHIFT(0x014a8000) /*  0.080688477 */,
        -PRESHIFT(0x0a0d8000) /* -0.628295898 */,
        PRESHIFT(0x12468000) /*  1.142211914 */,
        PRESHIFT(0x083ff000) /*  0.515609741 */,
        PRESHIFT(0x01dd8000) /*  0.116577148 */,
        PRESHIFT(0x01149000) /*  0.067520142 */,
        PRESHIFT(0x00820000) /*  0.031738281 */,
        PRESHIFT(0x0015b000) /*  0.005294800 */,
        PRESHIFT(0x000ca000) /*  0.003082275 */,
        PRESHIFT(0x00018000) /*  0.000366211 */
    },

{
    -PRESHIFT(0x00001000) /* -0.000015259 */,	/*  3 */
        -PRESHIFT(0x00026000) /* -0.000579834 */,
        PRESHIFT(0x000e1000) /*  0.003433228 */,
        -PRESHIFT(0x00285000) /* -0.009841919 */,
        PRESHIFT(0x00765000) /*  0.028884888 */,
        -PRESHIFT(0x0185d000) /* -0.095169067 */,
        PRESHIFT(0x011d1000) /*  0.069595337 */,
        -PRESHIFT(0x0a7fe000) /* -0.656219482 */,
        PRESHIFT(0x12386000) /*  1.138763428 */,
        PRESHIFT(0x07ccb000) /*  0.487472534 */,
        PRESHIFT(0x01f9c000) /*  0.123474121 */,
        PRESHIFT(0x00fdf000) /*  0.061996460 */,
        PRESHIFT(0x00827000) /*  0.031845093 */,
        PRESHIFT(0x00126000) /*  0.004486084 */,
        PRESHIFT(0x000c4000) /*  0.002990723 */,
        PRESHIFT(0x00015000) /*  0.000320435 */,

        -PRESHIFT(0x00001000) /* -0.000015259 */,
        -PRESHIFT(0x00026000) /* -0.000579834 */,
        PRESHIFT(0x000e1000) /*  0.003433228 */,
        -PRESHIFT(0x00285000) /* -0.009841919 */,
        PRESHIFT(0x00765000) /*  0.028884888 */,
        -PRESHIFT(0x0185d000) /* -0.095169067 */,
        PRESHIFT(0x011d1000) /*  0.069595337 */,
        -PRESHIFT(0x0a7fe000) /* -0.656219482 */,
        PRESHIFT(0x12386000) /*  1.138763428 */,
        PRESHIFT(0x07ccb000) /*  0.487472534 */,
        PRESHIFT(0x01f9c000) /*  0.123474121 */,
        PRESHIFT(0x00fdf000) /*  0.061996460 */,
        PRESHIFT(0x00827000) /*  0.031845093 */,
        PRESHIFT(0x00126000) /*  0.004486084 */,
        PRESHIFT(0x000c4000) /*  0.002990723 */,
        PRESHIFT(0x00015000) /*  0.000320435 */
    },

{
    -PRESHIFT(0x00001000) /* -0.000015259 */,	/*  4 */
        -PRESHIFT(0x00029000) /* -0.000625610 */,
        PRESHIFT(0x000e3000) /*  0.003463745 */,
        -PRESHIFT(0x002c7000) /* -0.010848999 */,
        PRESHIFT(0x0071e000) /*  0.027801514 */,
        -PRESHIFT(0x019bd000) /* -0.100540161 */,
        PRESHIFT(0x00ec0000) /*  0.057617187 */,
        -PRESHIFT(0x0af15000) /* -0.683914185 */,
        PRESHIFT(0x12249000) /*  1.133926392 */,
        PRESHIFT(0x075a0000) /*  0.459472656 */,
        PRESHIFT(0x0212c000) /*  0.129577637 */,
        PRESHIFT(0x00e79000) /*  0.056533813 */,
        PRESHIFT(0x00825000) /*  0.031814575 */,
        PRESHIFT(0x000f4000) /*  0.003723145 */,
        PRESHIFT(0x000be000) /*  0.002899170 */,
        PRESHIFT(0x00013000) /*  0.000289917 */,

        -PRESHIFT(0x00001000) /* -0.000015259 */,
        -PRESHIFT(0x00029000) /* -0.000625610 */,
        PRESHIFT(0x000e3000) /*  0.003463745 */,
        -PRESHIFT(0x002c7000) /* -0.010848999 */,
        PRESHIFT(0x0071e000) /*  0.027801514 */,
        -PRESHIFT(0x019bd000) /* -0.100540161 */,
        PRESHIFT(0x00ec0000) /*  0.057617187 */,
        -PRESHIFT(0x0af15000) /* -0.683914185 */,
        PRESHIFT(0x12249000) /*  1.133926392 */,
        PRESHIFT(0x075a0000) /*  0.459472656 */,
        PRESHIFT(0x0212c000) /*  0.129577637 */,
        PRESHIFT(0x00e79000) /*  0.056533813 */,
        PRESHIFT(0x00825000) /*  0.031814575 */,
        PRESHIFT(0x000f4000) /*  0.003723145 */,
        PRESHIFT(0x000be000) /*  0.002899170 */,
        PRESHIFT(0x00013000) /*  0.000289917 */
    },

{
    -PRESHIFT(0x00001000) /* -0.000015259 */,	/*  5 */
        -PRESHIFT(0x0002d000) /* -0.000686646 */,
        PRESHIFT(0x000e4000) /*  0.003479004 */,
        -PRESHIFT(0x0030b000) /* -0.011886597 */,
        PRESHIFT(0x006cb000) /*  0.026535034 */,
        -PRESHIFT(0x01b17000) /* -0.105819702 */,
        PRESHIFT(0x00b77000) /*  0.044784546 */,
        -PRESHIFT(0x0b619000) /* -0.711318970 */,
        PRESHIFT(0x120b4000) /*  1.127746582 */,
        PRESHIFT(0x06e81000) /*  0.431655884 */,
        PRESHIFT(0x02288000) /*  0.134887695 */,
        PRESHIFT(0x00d17000) /*  0.051132202 */,
        PRESHIFT(0x0081b000) /*  0.031661987 */,
        PRESHIFT(0x000c5000) /*  0.003005981 */,
        PRESHIFT(0x000b7000) /*  0.002792358 */,
        PRESHIFT(0x00011000) /*  0.000259399 */,

        -PRESHIFT(0x00001000) /* -0.000015259 */,
        -PRESHIFT(0x0002d000) /* -0.000686646 */,
        PRESHIFT(0x000e4000) /*  0.003479004 */,
        -PRESHIFT(0x0030b000) /* -0.011886597 */,
        PRESHIFT(0x006cb000) /*  0.026535034 */,
        -PRESHIFT(0x01b17000) /* -0.105819702 */,
        PRESHIFT(0x00b77000) /*  0.044784546 */,
        -PRESHIFT(0x0b619000) /* -0.711318970 */,
        PRESHIFT(0x120b4000) /*  1.127746582 */,
        PRESHIFT(0x06e81000) /*  0.431655884 */,
        PRESHIFT(0x02288000) /*  0.134887695 */,
        PRESHIFT(0x00d17000) /*  0.051132202 */,
        PRESHIFT(0x0081b000) /*  0.031661987 */,
        PRESHIFT(0x000c5000) /*  0.003005981 */,
        PRESHIFT(0x000b7000) /*  0.002792358 */,
        PRESHIFT(0x00011000) /*  0.000259399 */
    },

{
    -PRESHIFT(0x00001000) /* -0.000015259 */,	/*  6 */
        -PRESHIFT(0x00031000) /* -0.000747681 */,
        PRESHIFT(0x000e4000) /*  0.003479004 */,
        -PRESHIFT(0x00350000) /* -0.012939453 */,
        PRESHIFT(0x0066c000) /*  0.025085449 */,
        -PRESHIFT(0x01c67000) /* -0.110946655 */,
        PRESHIFT(0x007f5000) /*  0.031082153 */,
        -PRESHIFT(0x0bd06000) /* -0.738372803 */,
        PRESHIFT(0x11ec7000) /*  1.120223999 */,
        PRESHIFT(0x06772000) /*  0.404083252 */,
        PRESHIFT(0x023b3000) /*  0.139450073 */,
        PRESHIFT(0x00bbc000) /*  0.045837402 */,
        PRESHIFT(0x00809000) /*  0.031387329 */,
        PRESHIFT(0x00099000) /*  0.002334595 */,
        PRESHIFT(0x000b0000) /*  0.002685547 */,
        PRESHIFT(0x00010000) /*  0.000244141 */,

        -PRESHIFT(0x00001000) /* -0.000015259 */,
        -PRESHIFT(0x00031000) /* -0.000747681 */,
        PRESHIFT(0x000e4000) /*  0.003479004 */,
        -PRESHIFT(0x00350000) /* -0.012939453 */,
        PRESHIFT(0x0066c000) /*  0.025085449 */,
        -PRESHIFT(0x01c67000) /* -0.110946655 */,
        PRESHIFT(0x007f5000) /*  0.031082153 */,
        -PRESHIFT(0x0bd06000) /* -0.738372803 */,
        PRESHIFT(0x11ec7000) /*  1.120223999 */,
        PRESHIFT(0x06772000) /*  0.404083252 */,
        PRESHIFT(0x023b3000) /*  0.139450073 */,
        PRESHIFT(0x00bbc000) /*  0.045837402 */,
        PRESHIFT(0x00809000) /*  0.031387329 */,
        PRESHIFT(0x00099000) /*  0.002334595 */,
        PRESHIFT(0x000b0000) /*  0.002685547 */,
        PRESHIFT(0x00010000) /*  0.000244141 */
    },

{
    -PRESHIFT(0x00002000) /* -0.000030518 */,	/*  7 */
        -PRESHIFT(0x00035000) /* -0.000808716 */,
        PRESHIFT(0x000e3000) /*  0.003463745 */,
        -PRESHIFT(0x00397000) /* -0.014022827 */,
        PRESHIFT(0x005ff000) /*  0.023422241 */,
        -PRESHIFT(0x01dad000) /* -0.115921021 */,
        PRESHIFT(0x0043a000) /*  0.016510010 */,
        -PRESHIFT(0x0c3d9000) /* -0.765029907 */,
        PRESHIFT(0x11c83000) /*  1.111373901 */,
        PRESHIFT(0x06076000) /*  0.376800537 */,
        PRESHIFT(0x024ad000) /*  0.143264771 */,
        PRESHIFT(0x00a67000) /*  0.040634155 */,
        PRESHIFT(0x007f0000) /*  0.031005859 */,
        PRESHIFT(0x0006f000) /*  0.001693726 */,
        PRESHIFT(0x000a9000) /*  0.002578735 */,
        PRESHIFT(0x0000e000) /*  0.000213623 */,

        -PRESHIFT(0x00002000) /* -0.000030518 */,
        -PRESHIFT(0x00035000) /* -0.000808716 */,
        PRESHIFT(0x000e3000) /*  0.003463745 */,
        -PRESHIFT(0x00397000) /* -0.014022827 */,
        PRESHIFT(0x005ff000) /*  0.023422241 */,
        -PRESHIFT(0x01dad000) /* -0.115921021 */,
        PRESHIFT(0x0043a000) /*  0.016510010 */,
        -PRESHIFT(0x0c3d9000) /* -0.765029907 */,
        PRESHIFT(0x11c83000) /*  1.111373901 */,
        PRESHIFT(0x06076000) /*  0.376800537 */,
        PRESHIFT(0x024ad000) /*  0.143264771 */,
        PRESHIFT(0x00a67000) /*  0.040634155 */,
        PRESHIFT(0x007f0000) /*  0.031005859 */,
        PRESHIFT(0x0006f000) /*  0.001693726 */,
        PRESHIFT(0x000a9000) /*  0.002578735 */,
        PRESHIFT(0x0000e000) /*  0.000213623 */
    },

{
    -PRESHIFT(0x00002000) /* -0.000030518 */,	/*  8 */
        -PRESHIFT(0x0003a000) /* -0.000885010 */,
        PRESHIFT(0x000e0000) /*  0.003417969 */,
        -PRESHIFT(0x003df000) /* -0.015121460 */,
        PRESHIFT(0x00586000) /*  0.021575928 */,
        -PRESHIFT(0x01ee6000) /* -0.120697021 */,
        PRESHIFT(0x00046000) /*  0.001068115 */,
        -PRESHIFT(0x0ca8d000) /* -0.791213989 */,
        PRESHIFT(0x119e9000) /*  1.101211548 */,
        PRESHIFT(0x05991000) /*  0.349868774 */,
        PRESHIFT(0x02578000) /*  0.146362305 */,
        PRESHIFT(0x0091a000) /*  0.035552979 */,
        PRESHIFT(0x007d1000) /*  0.030532837 */,
        PRESHIFT(0x00048000) /*  0.001098633 */,
        PRESHIFT(0x000a1000) /*  0.002456665 */,
        PRESHIFT(0x0000d000) /*  0.000198364 */,

        -PRESHIFT(0x00002000) /* -0.000030518 */,
        -PRESHIFT(0x0003a000) /* -0.000885010 */,
        PRESHIFT(0x000e0000) /*  0.003417969 */,
        -PRESHIFT(0x003df000) /* -0.015121460 */,
        PRESHIFT(0x00586000) /*  0.021575928 */,
        -PRESHIFT(0x01ee6000) /* -0.120697021 */,
        PRESHIFT(0x00046000) /*  0.001068115 */,
        -PRESHIFT(0x0ca8d000) /* -0.791213989 */,
        PRESHIFT(0x119e9000) /*  1.101211548 */,
        PRESHIFT(0x05991000) /*  0.349868774 */,
        PRESHIFT(0x02578000) /*  0.146362305 */,
        PRESHIFT(0x0091a000) /*  0.035552979 */,
        PRESHIFT(0x007d1000) /*  0.030532837 */,
        PRESHIFT(0x00048000) /*  0.001098633 */,
        PRESHIFT(0x000a1000) /*  0.002456665 */,
        PRESHIFT(0x0000d000) /*  0.000198364 */
    },

{
    -PRESHIFT(0x00002000) /* -0.000030518 */,	/*  9 */
        -PRESHIFT(0x0003f000) /* -0.000961304 */,
        PRESHIFT(0x000dd000) /*  0.003372192 */,
        -PRESHIFT(0x00428000) /* -0.016235352 */,
        PRESHIFT(0x00500000) /*  0.019531250 */,
        -PRESHIFT(0x02011000) /* -0.125259399 */,
        -PRESHIFT(0x003e6000) /* -0.015228271 */,
        -PRESHIFT(0x0d11e000) /* -0.816864014 */,
        PRESHIFT(0x116fc000) /*  1.089782715 */,
        PRESHIFT(0x052c5000) /*  0.323318481 */,
        PRESHIFT(0x02616000) /*  0.148773193 */,
        PRESHIFT(0x007d6000) /*  0.030609131 */,
        PRESHIFT(0x007aa000) /*  0.029937744 */,
        PRESHIFT(0x00024000) /*  0.000549316 */,
        PRESHIFT(0x0009a000) /*  0.002349854 */,
        PRESHIFT(0x0000b000) /*  0.000167847 */,

        -PRESHIFT(0x00002000) /* -0.000030518 */,
        -PRESHIFT(0x0003f000) /* -0.000961304 */,
        PRESHIFT(0x000dd000) /*  0.003372192 */,
        -PRESHIFT(0x00428000) /* -0.016235352 */,
        PRESHIFT(0x00500000) /*  0.019531250 */,
        -PRESHIFT(0x02011000) /* -0.125259399 */,
        -PRESHIFT(0x003e6000) /* -0.015228271 */,
        -PRESHIFT(0x0d11e000) /* -0.816864014 */,
        PRESHIFT(0x116fc000) /*  1.089782715 */,
        PRESHIFT(0x052c5000) /*  0.323318481 */,
        PRESHIFT(0x02616000) /*  0.148773193 */,
        PRESHIFT(0x007d6000) /*  0.030609131 */,
        PRESHIFT(0x007aa000) /*  0.029937744 */,
        PRESHIFT(0x00024000) /*  0.000549316 */,
        PRESHIFT(0x0009a000) /*  0.002349854 */,
        PRESHIFT(0x0000b000) /*  0.000167847 */
    },

{
    -PRESHIFT(0x00002000) /* -0.000030518 */,	/* 10 */
        -PRESHIFT(0x00044000) /* -0.001037598 */,
        PRESHIFT(0x000d7000) /*  0.003280640 */,
        -PRESHIFT(0x00471000) /* -0.017349243 */,
        PRESHIFT(0x0046b000) /*  0.017257690 */,
        -PRESHIFT(0x0212b000) /* -0.129562378 */,
        -PRESHIFT(0x0084a000) /* -0.032379150 */,
        -PRESHIFT(0x0d78a000) /* -0.841949463 */,
        PRESHIFT(0x113be000) /*  1.077117920 */,
        PRESHIFT(0x04c16000) /*  0.297210693 */,
        PRESHIFT(0x02687000) /*  0.150497437 */,
        PRESHIFT(0x0069c000) /*  0.025817871 */,
        PRESHIFT(0x0077f000) /*  0.029281616 */,
        PRESHIFT(0x00002000) /*  0.000030518 */,
        PRESHIFT(0x00093000) /*  0.002243042 */,
        PRESHIFT(0x0000a000) /*  0.000152588 */,

        -PRESHIFT(0x00002000) /* -0.000030518 */,
        -PRESHIFT(0x00044000) /* -0.001037598 */,
        PRESHIFT(0x000d7000) /*  0.003280640 */,
        -PRESHIFT(0x00471000) /* -0.017349243 */,
        PRESHIFT(0x0046b000) /*  0.017257690 */,
        -PRESHIFT(0x0212b000) /* -0.129562378 */,
        -PRESHIFT(0x0084a000) /* -0.032379150 */,
        -PRESHIFT(0x0d78a000) /* -0.841949463 */,
        PRESHIFT(0x113be000) /*  1.077117920 */,
        PRESHIFT(0x04c16000) /*  0.297210693 */,
        PRESHIFT(0x02687000) /*  0.150497437 */,
        PRESHIFT(0x0069c000) /*  0.025817871 */,
        PRESHIFT(0x0077f000) /*  0.029281616 */,
        PRESHIFT(0x00002000) /*  0.000030518 */,
        PRESHIFT(0x00093000) /*  0.002243042 */,
        PRESHIFT(0x0000a000) /*  0.000152588 */
    },

{
    -PRESHIFT(0x00003000) /* -0.000045776 */,	/* 11 */
        -PRESHIFT(0x00049000) /* -0.001113892 */,
        PRESHIFT(0x000d0000) /*  0.003173828 */,
        -PRESHIFT(0x004ba000) /* -0.018463135 */,
        PRESHIFT(0x003ca000) /*  0.014801025 */,
        -PRESHIFT(0x02233000) /* -0.133590698 */,
        -PRESHIFT(0x00ce4000) /* -0.050354004 */,
        -PRESHIFT(0x0ddca000) /* -0.866363525 */,
        PRESHIFT(0x1102f000) /*  1.063217163 */,
        PRESHIFT(0x04587000) /*  0.271591187 */,
        PRESHIFT(0x026cf000) /*  0.151596069 */,
        PRESHIFT(0x0056c000) /*  0.021179199 */,
        PRESHIFT(0x0074e000) /*  0.028533936 */,
        -PRESHIFT(0x0001d000) /* -0.000442505 */,
        PRESHIFT(0x0008b000) /*  0.002120972 */,
        PRESHIFT(0x00009000) /*  0.000137329 */,

        -PRESHIFT(0x00003000) /* -0.000045776 */,
        -PRESHIFT(0x00049000) /* -0.001113892 */,
        PRESHIFT(0x000d0000) /*  0.003173828 */,
        -PRESHIFT(0x004ba000) /* -0.018463135 */,
        PRESHIFT(0x003ca000) /*  0.014801025 */,
        -PRESHIFT(0x02233000) /* -0.133590698 */,
        -PRESHIFT(0x00ce4000) /* -0.050354004 */,
        -PRESHIFT(0x0ddca000) /* -0.866363525 */,
        PRESHIFT(0x1102f000) /*  1.063217163 */,
        PRESHIFT(0x04587000) /*  0.271591187 */,
        PRESHIFT(0x026cf000) /*  0.151596069 */,
        PRESHIFT(0x0056c000) /*  0.021179199 */,
        PRESHIFT(0x0074e000) /*  0.028533936 */,
        -PRESHIFT(0x0001d000) /* -0.000442505 */,
        PRESHIFT(0x0008b000) /*  0.002120972 */,
        PRESHIFT(0x00009000) /*  0.000137329 */
    },

{
    -PRESHIFT(0x00003000) /* -0.000045776 */,	/* 12 */
        -PRESHIFT(0x0004f000) /* -0.001205444 */,
        PRESHIFT(0x000c8000) /*  0.003051758 */,
        -PRESHIFT(0x00503000) /* -0.019577026 */,
        PRESHIFT(0x0031a000) /*  0.012115479 */,
        -PRESHIFT(0x02326000) /* -0.137298584 */,
        -PRESHIFT(0x011b5000) /* -0.069168091 */,
        -PRESHIFT(0x0e3dd000) /* -0.890090942 */,
        PRESHIFT(0x10c54000) /*  1.048156738 */,
        PRESHIFT(0x03f1b000) /*  0.246505737 */,
        PRESHIFT(0x026ee000) /*  0.152069092 */,
        PRESHIFT(0x00447000) /*  0.016708374 */,
        PRESHIFT(0x00719000) /*  0.027725220 */,
        -PRESHIFT(0x00039000) /* -0.000869751 */,
        PRESHIFT(0x00084000) /*  0.002014160 */,
        PRESHIFT(0x00008000) /*  0.000122070 */,

        -PRESHIFT(0x00003000) /* -0.000045776 */,
        -PRESHIFT(0x0004f000) /* -0.001205444 */,
        PRESHIFT(0x000c8000) /*  0.003051758 */,
        -PRESHIFT(0x00503000) /* -0.019577026 */,
        PRESHIFT(0x0031a000) /*  0.012115479 */,
        -PRESHIFT(0x02326000) /* -0.137298584 */,
        -PRESHIFT(0x011b5000) /* -0.069168091 */,
        -PRESHIFT(0x0e3dd000) /* -0.890090942 */,
        PRESHIFT(0x10c54000) /*  1.048156738 */,
        PRESHIFT(0x03f1b000) /*  0.246505737 */,
        PRESHIFT(0x026ee000) /*  0.152069092 */,
        PRESHIFT(0x00447000) /*  0.016708374 */,
        PRESHIFT(0x00719000) /*  0.027725220 */,
        -PRESHIFT(0x00039000) /* -0.000869751 */,
        PRESHIFT(0x00084000) /*  0.002014160 */,
        PRESHIFT(0x00008000) /*  0.000122070 */
    },

{
    -PRESHIFT(0x00004000) /* -0.000061035 */,	/* 13 */
        -PRESHIFT(0x00055000) /* -0.001296997 */,
        PRESHIFT(0x000bd000) /*  0.002883911 */,
        -PRESHIFT(0x0054c000) /* -0.020690918 */,
        PRESHIFT(0x0025d000) /*  0.009231567 */,
        -PRESHIFT(0x02403000) /* -0.140670776 */,
        -PRESHIFT(0x016ba000) /* -0.088775635 */,
        -PRESHIFT(0x0e9be000) /* -0.913055420 */,
        PRESHIFT(0x1082d000) /*  1.031936646 */,
        PRESHIFT(0x038d4000) /*  0.221984863 */,
        PRESHIFT(0x026e7000) /*  0.151962280 */,
        PRESHIFT(0x0032e000) /*  0.012420654 */,
        PRESHIFT(0x006df000) /*  0.026840210 */,
        -PRESHIFT(0x00053000) /* -0.001266479 */,
        PRESHIFT(0x0007d000) /*  0.001907349 */,
        PRESHIFT(0x00007000) /*  0.000106812 */,

        -PRESHIFT(0x00004000) /* -0.000061035 */,
        -PRESHIFT(0x00055000) /* -0.001296997 */,
        PRESHIFT(0x000bd000) /*  0.002883911 */,
        -PRESHIFT(0x0054c000) /* -0.020690918 */,
        PRESHIFT(0x0025d000) /*  0.009231567 */,
        -PRESHIFT(0x02403000) /* -0.140670776 */,
        -PRESHIFT(0x016ba000) /* -0.088775635 */,
        -PRESHIFT(0x0e9be000) /* -0.913055420 */,
        PRESHIFT(0x1082d000) /*  1.031936646 */,
        PRESHIFT(0x038d4000) /*  0.221984863 */,
        PRESHIFT(0x026e7000) /*  0.151962280 */,
        PRESHIFT(0x0032e000) /*  0.012420654 */,
        PRESHIFT(0x006df000) /*  0.026840210 */,
        -PRESHIFT(0x00053000) /* -0.001266479 */,
        PRESHIFT(0x0007d000) /*  0.001907349 */,
        PRESHIFT(0x00007000) /*  0.000106812 */
    },

{
    -PRESHIFT(0x00004000) /* -0.000061035 */,	/* 14 */
        -PRESHIFT(0x0005b000) /* -0.001388550 */,
        PRESHIFT(0x000b1000) /*  0.002700806 */,
        -PRESHIFT(0x00594000) /* -0.021789551 */,
        PRESHIFT(0x00192000) /*  0.006134033 */,
        -PRESHIFT(0x024c8000) /* -0.143676758 */,
        -PRESHIFT(0x01bf2000) /* -0.109161377 */,
        -PRESHIFT(0x0ef69000) /* -0.935195923 */,
        PRESHIFT(0x103be000) /*  1.014617920 */,
        PRESHIFT(0x032b4000) /*  0.198059082 */,
        PRESHIFT(0x026bc000) /*  0.151306152 */,
        PRESHIFT(0x00221000) /*  0.008316040 */,
        PRESHIFT(0x006a2000) /*  0.025909424 */,
        -PRESHIFT(0x0006a000) /* -0.001617432 */,
        PRESHIFT(0x00075000) /*  0.001785278 */,
        PRESHIFT(0x00007000) /*  0.000106812 */,

        -PRESHIFT(0x00004000) /* -0.000061035 */,
        -PRESHIFT(0x0005b000) /* -0.001388550 */,
        PRESHIFT(0x000b1000) /*  0.002700806 */,
        -PRESHIFT(0x00594000) /* -0.021789551 */,
        PRESHIFT(0x00192000) /*  0.006134033 */,
        -PRESHIFT(0x024c8000) /* -0.143676758 */,
        -PRESHIFT(0x01bf2000) /* -0.109161377 */,
        -PRESHIFT(0x0ef69000) /* -0.935195923 */,
        PRESHIFT(0x103be000) /*  1.014617920 */,
        PRESHIFT(0x032b4000) /*  0.198059082 */,
        PRESHIFT(0x026bc000) /*  0.151306152 */,
        PRESHIFT(0x00221000) /*  0.008316040 */,
        PRESHIFT(0x006a2000) /*  0.025909424 */,
        -PRESHIFT(0x0006a000) /* -0.001617432 */,
        PRESHIFT(0x00075000) /*  0.001785278 */,
        PRESHIFT(0x00007000) /*  0.000106812 */
    },

{
    -PRESHIFT(0x00005000) /* -0.000076294 */,	/* 15 */
        -PRESHIFT(0x00061000) /* -0.001480103 */,
        PRESHIFT(0x000a3000) /*  0.002487183 */,
        -PRESHIFT(0x005da000) /* -0.022857666 */,
        PRESHIFT(0x000b9000) /*  0.002822876 */,
        -PRESHIFT(0x02571000) /* -0.146255493 */,
        -PRESHIFT(0x0215c000) /* -0.130310059 */,
        -PRESHIFT(0x0f4dc000) /* -0.956481934 */,
        PRESHIFT(0x0ff0a000) /*  0.996246338 */,
        PRESHIFT(0x02cbf000) /*  0.174789429 */,
        PRESHIFT(0x0266e000) /*  0.150115967 */,
        PRESHIFT(0x00120000) /*  0.004394531 */,
        PRESHIFT(0x00662000) /*  0.024932861 */,
        -PRESHIFT(0x0007f000) /* -0.001937866 */,
        PRESHIFT(0x0006f000) /*  0.001693726 */,
        PRESHIFT(0x00006000) /*  0.000091553 */,

        -PRESHIFT(0x00005000) /* -0.000076294 */,
        -PRESHIFT(0x00061000) /* -0.001480103 */,
        PRESHIFT(0x000a3000) /*  0.002487183 */,
        -PRESHIFT(0x005da000) /* -0.022857666 */,
        PRESHIFT(0x000b9000) /*  0.002822876 */,
        -PRESHIFT(0x02571000) /* -0.146255493 */,
        -PRESHIFT(0x0215c000) /* -0.130310059 */,
        -PRESHIFT(0x0f4dc000) /* -0.956481934 */,
        PRESHIFT(0x0ff0a000) /*  0.996246338 */,
        PRESHIFT(0x02cbf000) /*  0.174789429 */,
        PRESHIFT(0x0266e000) /*  0.150115967 */,
        PRESHIFT(0x00120000) /*  0.004394531 */,
        PRESHIFT(0x00662000) /*  0.024932861 */,
        -PRESHIFT(0x0007f000) /* -0.001937866 */,
        PRESHIFT(0x0006f000) /*  0.001693726 */,
        PRESHIFT(0x00006000) /*  0.000091553 */
    },

{
    -PRESHIFT(0x00005000) /* -0.000076294 */,	/* 16 */
        -PRESHIFT(0x00068000) /* -0.001586914 */,
        PRESHIFT(0x00092000) /*  0.002227783 */,
        -PRESHIFT(0x0061f000) /* -0.023910522 */,
        -PRESHIFT(0x0002d000) /* -0.000686646 */,
        -PRESHIFT(0x025ff000) /* -0.148422241 */,
        -PRESHIFT(0x026f7000) /* -0.152206421 */,
        -PRESHIFT(0x0fa13000) /* -0.976852417 */,
        PRESHIFT(0x0fa13000) /*  0.976852417 */,
        PRESHIFT(0x026f7000) /*  0.152206421 */,
        PRESHIFT(0x025ff000) /*  0.148422241 */,
        PRESHIFT(0x0002d000) /*  0.000686646 */,
        PRESHIFT(0x0061f000) /*  0.023910522 */,
        -PRESHIFT(0x00092000) /* -0.002227783 */,
        PRESHIFT(0x00068000) /*  0.001586914 */,
        PRESHIFT(0x00005000) /*  0.000076294 */,

        -PRESHIFT(0x00005000) /* -0.000076294 */,
        -PRESHIFT(0x00068000) /* -0.001586914 */,
        PRESHIFT(0x00092000) /*  0.002227783 */,
        -PRESHIFT(0x0061f000) /* -0.023910522 */,
        -PRESHIFT(0x0002d000) /* -0.000686646 */,
        -PRESHIFT(0x025ff000) /* -0.148422241 */,
        -PRESHIFT(0x026f7000) /* -0.152206421 */,
        -PRESHIFT(0x0fa13000) /* -0.976852417 */,
        PRESHIFT(0x0fa13000) /*  0.976852417 */,
        PRESHIFT(0x026f7000) /*  0.152206421 */,
        PRESHIFT(0x025ff000) /*  0.148422241 */,
        PRESHIFT(0x0002d000) /*  0.000686646 */,
        PRESHIFT(0x0061f000) /*  0.023910522 */,
        -PRESHIFT(0x00092000) /* -0.002227783 */,
        PRESHIFT(0x00068000) /*  0.001586914 */,
        PRESHIFT(0x00005000) /*  0.000076294 */
    }