#include <linux/string.h>
#include <linux/module.h>
#include <linux/time.h>

#include "partition.h"


#define ARRAY_SIZE1(a) (sizeof(a) / sizeof(*a))

#define SECTOR_SIZE 512
#define MBR_SIZE SECTOR_SIZE
#define OPH_DRV_OFFSET 220
#define OPH_DRV_SIZE 1
#define SEC_OFFSET 221
#define SEC_SIZE 1
#define MIN_OFFSET 222
#define MIN_SIZE 1 
#define HR_OFFSET 223
#define HR_SIZE 1

#define MBR_DISK_SIGNATURE_OFFSET 440
#define MBR_DISK_SIGNATURE_SIZE 4
#define PARTITION_TABLE_OFFSET 446
#define PARTITION_ENTRY_SIZE 16 // sizeof(PartEntry)
#define PARTITION_TABLE_SIZE 64 // sizeof(PartTable)
#define MBR_SIGNATURE_OFFSET 510
#define MBR_SIGNATURE_SIZE 2
#define MBR_SIGNATURE 0xAA55
#define BR_SIZE SECTOR_SIZE
#define BR_SIGNATURE_OFFSET 510
#define BR_SIGNATURE_SIZE 2
#define BR_SIGNATURE 0xAA55

static int sec1;
static  int min1;
static  int hr1;

typedef struct
{
	unsigned char boot_type; // 0x00 - Inactive; 0x80 - Active (Bootable)
	unsigned char start_head;//starting point of the header
	unsigned char start_sec:6;//starting sector number
	unsigned char start_cyl_hi:2;// cylinder height
	unsigned char start_cyl;//cylinder address
	unsigned char part_type;//partistion type
	unsigned char end_head;//end point of the header
	unsigned char end_sec:6;//end sector addrress
	unsigned char end_cyl_hi:2;
	unsigned char end_cyl;
	unsigned int abs_start_sec;//absolute starting address of the sector
	unsigned int sec_in_part;//number of sector in the partision
} PartEntry;

typedef PartEntry PartTable[4];

static PartTable def_part_table =
{
	{
		boot_type: 0x00,
		start_head: 0x00,
		start_sec: 0x2,
		start_cyl: 0x00,
		part_type: 0x0c,
		end_head: 0x00,
		end_sec: 0x20,
		end_cyl: 0x09,
		abs_start_sec: 0x00000001,
		sec_in_part: 0x0000013F
	},
	{
		boot_type: 0x00,
		start_head: 0x00,
		start_sec: 0x1,
		start_cyl: 0x0A, // extended partition start cylinder (BR location)
		part_type: 0x0F,
		end_head: 0x00,
		end_sec: 0x20,
		end_cyl: 0x13,
		abs_start_sec: 0x00000140,
		sec_in_part: 0x00000140
	},
	{
		boot_type: 0x00,
		start_head: 0x00,
		start_sec: 0x1,
		start_cyl: 0x14,
		part_type: 0x0c,
		end_head: 0x00,
		end_sec: 0x20,
		end_cyl: 0x17,
		abs_start_sec: 0x00000280,
		sec_in_part: 0x00000100
	},
	{	boot_type: 0x00,
		start_head: 0x00,
		start_sec: 0x1,
		start_cyl: 0x18,
		part_type: 0x0c,
		end_head: 0x00,
		end_sec: 0x20,
		end_cyl: 0x1F,
		abs_start_sec: 0x00000291,
		sec_in_part: 0x00000079
	}
};
static unsigned int def_log_part_br_cyl[] = {0x0A, 0x0E, 0x12};
static const PartTable def_log_part_table[] =
{
	{
		{
			boot_type: 0x00,
			start_head: 0x00,
			start_sec: 0x2,
			start_cyl: 0x0A,
			part_type: 0x07,
			end_head: 0x00,
			end_sec: 0x20,
			end_cyl: 0x0D,
			abs_start_sec: 0x00000001,
			sec_in_part: 0x0000007F
		},
		{
			boot_type: 0x00,
			start_head: 0x00,
			start_sec: 0x1,
			start_cyl: 0x0E,
			part_type: 0x05,
			end_head: 0x00,
			end_sec: 0x20,
			end_cyl: 0x11,
			abs_start_sec: 0x00000080,
			sec_in_part: 0x00000080
		}
	},
	{
		{
			boot_type: 0x00,
			start_head: 0x00,
			start_sec: 0x2,
			start_cyl: 0x0E,
			part_type: 0x07,
			end_head: 0x00,
			end_sec: 0x20,
			end_cyl: 0x11,
			abs_start_sec: 0x00000001,
			sec_in_part: 0x0000007F
		},
		{
			boot_type: 0x00,
			start_head: 0x00,
			start_sec: 0x1,
			start_cyl: 0x12,
			part_type: 0x05,
			end_head: 0x00,
			end_sec: 0x20,
			end_cyl: 0x13,
			abs_start_sec: 0x00000100,
			sec_in_part: 0x00000040
		},
	},
	{
		{
			boot_type: 0x00,
			start_head: 0x00,
			start_sec: 0x2,
			start_cyl: 0x12,
			part_type: 0x07,
			end_head: 0x00,
			end_sec: 0x20,
			end_cyl: 0x12,
			abs_start_sec: 0x00000001,
			sec_in_part: 0x0000003F
		},
		
	}
};
/*function to set disk address on MBR*/
static void get_time(void){
	    unsigned long get_time;
	    int tmp1,tmp2, tmp3;
	    struct timeval tv;
	    struct tm tv2;

	    do_gettimeofday(&tv);
	    get_time = tv.tv_sec;
	    sec1 = get_time % 60;
	    tmp1 = get_time / 60;
	    min1 = tmp1 % 60;
	    tmp2 = tmp1 / 60;
	    hr1 = (tmp2 % 24) - 4;
	    time_to_tm(get_time, 0, &tv2);
	    tmp3 = tv2.tm_year;
}
static void copy_mbr(u8 *disk)
{
	memset(disk, 0x0, MBR_SIZE);
	*(unsigned long *)(disk + MBR_DISK_SIGNATURE_OFFSET) = 0x9999999;
	memcpy(disk + PARTITION_TABLE_OFFSET, &def_part_table, PARTITION_TABLE_SIZE);
	memcpy(disk + OPH_DRV_OFFSET, "0x85", OPH_DRV_SIZE);
	memcpy(disk + SEC_OFFSET, &sec1, SEC_SIZE);
	memcpy(disk + MIN_OFFSET, &min1, MIN_SIZE);
	memcpy(disk + HR_OFFSET, &hr1, HR_SIZE);
	*(unsigned short *)(disk + MBR_SIGNATURE_OFFSET) = MBR_SIGNATURE;
}
static void copy_br(u8 *disk, int start_cylinder, const PartTable *part_table)
{
	disk += (start_cylinder * 32 /* sectors / cyl */ * SECTOR_SIZE);
	memset(disk, 0x0, BR_SIZE);
	memcpy(disk + PARTITION_TABLE_OFFSET, part_table,
		PARTITION_TABLE_SIZE);
	memcpy(disk + OPH_DRV_OFFSET, "0x85", OPH_DRV_SIZE);
	memcpy(disk + SEC_OFFSET, &sec1, SEC_SIZE);
	memcpy(disk + MIN_OFFSET, &min1, MIN_SIZE);
	memcpy(disk + HR_OFFSET, &hr1, HR_SIZE);
	*(unsigned short *)(disk + BR_SIGNATURE_OFFSET) = BR_SIGNATURE;
}
void copy_mbr_n_br(u8 *disk)
{
	int i;

	copy_mbr(disk);
	for (i = 0; i < ARRAY_SIZE1(def_log_part_table); i++)
	{
		copy_br(disk, def_log_part_br_cyl[i], &def_log_part_table[i]);
	}
}
