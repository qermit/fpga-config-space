/*
 * Self-Describing Wishbone Bus (SDWB) specification header file
 *
 * Author: Manohar Vanga <manohar.vanga@cern.ch>
 * Copyright (C) 2011 CERN
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef _SDWB_H
#define _SDWB_H

/*
 * SDWB magic numbers. They are all big-endian. We use be**_to_cpu in
 * kernel space, and network order in user space. Please note that
 * we define the internal values as constants, but help users by
 * also defining the host-order values (without leading underscores)
 */
#define __SDWB_HEAD_MAGIC	0x5344574248454144LL	/* "SDWBHEAD" */
#define __SDWB_WBD_MAGIC	    0x5742		/* "WB" */

#define SDWB_HEAD_MAGIC	ntohll(__SDWB_HEAD_MAGIC)
#define SDWB_WBD_MAGIC	ntohs(__SDWB_WBD_MAGIC)

/* The following comes from arch/um/drivers/cow.h -- factorazing anyone? */
#if !defined(ntohll) && defined(__KERNEL__)
#  include <asm/byteorder.h>
#  define ntohll(x)  be64_to_cpu(x)
#  define htonll(x)  cpu_to_be64(x)
#elif !defined(ntohll) && !defined(__KERNEL__)
#  include <endian.h>
#  include <netinet/in.h>
#  if defined(__BYTE_ORDER)
#    if __BYTE_ORDER == __BIG_ENDIAN
#      define ntohll(x) (x)
#      define htonll(x) (x)
#    else
#      define ntohll(x)  __bswap_64(x)
#      define htonll(x)  __bswap_64(x)
#    endif
#  else
#     error "Could not determine byte order: __BYTE_ORDER undefined"
#  endif
#endif /* __KERNEL__ */


/*
 * SDWB - Header structure
 *
 * @magic        : Magic value used to verify integrity (SDWB_HEAD_MAGIC)
 * @wbid_address : Address of Wishbone ID block
 * @wbd_address  : Address of Wishbone device block
 */
struct sdwb_head {
	uint64_t magic;
	uint64_t wbid_address;
	uint64_t wbd_address;
	uint64_t reserved;
};

/*
 * SDWB - Wishbone ID (WBID) Descriptor
 *
 * @bstream_type    : The unique bitstream type ID
 * @bstream_version : The version of the bitstream
 * @bstream_date    : Date of synthesis of bitstream (Format: 0xYYYYMMDD)
 * @bstream_release : SVN revision number or SHA-1 Git hash (FIXME add 
 *                    function for filling this for SVN and Git?)
 */
struct sdwb_wbid {
	uint64_t bstream_type;
	uint32_t bstream_version;
	uint32_t bstream_date;
	uint8_t bstream_release[16];
};

/*
 * SDWB - Wishbone Device (WBD) Descriptor
 *
 * @wbd_magic   : Magic value used to check validity of header (SDWB_WBD_MAGIC)
 *                Also used to check end of an array of such device descriptors.
 *                (for example in the Wishbone device descriptor block)
 *
 * @wbd_version : Device descriptor version
 *                (top byte = major, lower byte = minor)
 *
 * @vendor      : The vendor ID of the wishbone device
 * @device      : The device ID of the wishbone device
 *
 * @hdl_base    : The base address of the HDL block (Wishbone address)
 * @hdl_size    : Size of the HDL block in Wishbone memory
 *
 * @wbd_flags   : Flags pertaining to the wishbone device
 *
 * @hdl_class   : Class of HDL block
 * @hdl_version : Version of HDL block
 * @hdl_date    : Date of HDL block (release date)
 *
 * vendor_name  : Vendor name in ASCII (for debugging)
 * device_name  : Device name in ASCII (for debugging)
 */
struct sdwb_wbd {
	uint64_t vendor;
	uint32_t device;

	uint16_t wbd_magic;
	uint16_t wbd_version;

	uint64_t hdl_base;
	uint64_t hdl_size;

	uint32_t wbd_flags;
	uint32_t hdl_class;
	uint32_t hdl_version;
	uint32_t hdl_date;

	char vendor_name[16];
	char device_name[16];
};

#endif /* _SDWB_H */
