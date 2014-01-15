* Author: Cesar Prados <c.prados@gsi.de>
*
* Released according to the GNU GPL, version 2 or any later version
*
* VME-WB bridge for VME

This kernle module provides methods for using Etherbone over VME. It depens on 
the wishbone and vmebridge modules. The first steers the Ethernet communication and
the later  provides the needed methods for managing the vme bus (open, create 
windows, destroy windows, write and read) wraping up the Ethernet protocol in
VME transactions.

The module create 3 VME windows:

1) CS/CSR[AM=VME_CR_CSR, DW=VME_D32, size=0x80000, base=0x80000 * Slot]

This window exposes the configuration area of the Legacy vme64x-core:
[WB Base Addres]
[WB Ctrl Base Addres]
[IRQ Leve and Vector]
[WB bus 32 o 64 bits]
[Enable/Disable the WB Bus]

2) WB Bus[AM=VME_A32_USER_MBLT, DW=VME_D32, size=0x1000000, base=0x1000000 * Slot]

This widows exposes the internal WB bus in the device that the VME bus in connected to.


3) WB Ctrl[AM=VME_A24_USER_MBLT, DW=VME_D32, size=0xA0, base=0x400 * Slot]
 
This windows exposes a sort of registers that contain Etherbone config parameters, control the WB bus, 
and the MSI WB bus


This kernel module handles MSI interrupts using Etherbone interfaces.

For loading the module:

#insmod vme_wb.ko slot= vmebase= vector= lun=

<slot> this parameter means in a vme64, the position of the card in the rack, and in legacy
vme, the position of the base address switch.

<vmebase> set the vmebase for the CS/CSR

<vector> of the IRQ

<lun> index value for VME card
