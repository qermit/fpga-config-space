/* Self-Describing Wishbone Bus (SDWB) specification header file */

#ifndef _SDWB_H
#define _SDWB_H

/* SDWB magic numbers */

/* 'SDWBhead' (big endian). Used in SDWB Header */
#define SDWB_HEAD_MAGIC 0x5344574248656164LL

/* 'WB' (big endian, 16 bits). Used in SDWB device descriptors */
#define SDWB_WBD_MAGIC 0x5742 

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
	uint8_t bstream_release[20];
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
 *                (FIXME rename  to hdl_?)
 *
 * @hdl_class   : Class of HDL block
 * @hdl_version : Version of HDL block
 * @hdl_date    : Date of HDL block (release date)
 *
 * vendor_name  : Vendor name in ASCII (for debugging)
 * device_name  : Device name in ASCII (for debugging)
 */
struct sdwb_wbd {
	uint16_t wbd_magic;
	uint16_t wbd_version;

	uint64_t vendor;
	uint32_t device;

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
