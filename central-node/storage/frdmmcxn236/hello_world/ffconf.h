/*---------------------------------------------------------------------------/
/  Configurations of FatFs Module - Smart Cargo Black Box (central node)
/---------------------------------------------------------------------------*/

#define FFCONF_DEF	80386	/* Revision ID - MUST match ff.c's expected value */

/*---------------------------------------------------------------------------/
/ Function Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_READONLY	0
#define FF_FS_MINIMIZE	0
#define FF_USE_FIND		0
#define FF_USE_MKFS		0
#define FF_USE_FASTSEEK	0
#define FF_USE_EXPAND	0
#define FF_USE_CHMOD	0
#define FF_USE_LABEL	0
#define FF_USE_FORWARD	0
#define FF_USE_STRFUNC	0
#define FF_PRINT_LLI	1
#define FF_PRINT_FLOAT	1
#define FF_STRF_ENCODE	3

/*---------------------------------------------------------------------------/
/ Locale and Namespace Configurations
/---------------------------------------------------------------------------*/

#define FF_CODE_PAGE	437
#define FF_USE_LFN		0
#define FF_MAX_LFN		255
#define FF_LFN_UNICODE	0
#define FF_LFN_BUF		255
#define FF_SFN_BUF		12
#define FF_FS_RPATH		0

/*---------------------------------------------------------------------------/
/ Drive/Volume Configurations
/---------------------------------------------------------------------------*/

#define FF_VOLUMES		1
#define FF_STR_VOLUME_ID	0
#define FF_VOLUME_STRS		"SD"
#define FF_MULTI_PARTITION	0
#define FF_MIN_SS		512
#define FF_MAX_SS		512
#define FF_LBA64		0
#define FF_MIN_GPT		0x10000000
#define FF_USE_TRIM		0

/*---------------------------------------------------------------------------/
/ System Configurations
/---------------------------------------------------------------------------*/

#define FF_FS_TINY		0
#define FF_FS_EXFAT		0

/*
 * FF_FS_NORTC 1 = fixed timestamp from FF_NORTC_* (RTC driver not needed).
 * Set to 0 once Person A's RTC driver fix is in; diskio.c then uses
 * get_fattime() with the real RTC. Also re-enable fsl_rtc.c in CMakeLists.txt.
 */
#define FF_FS_NORTC		1
#define FF_NORTC_MON	1
#define FF_NORTC_MDAY	1
#define FF_NORTC_YEAR	2025

#define FF_FS_NOFSINFO	0
#define FF_FS_LOCK		0

/*
 * Bare-metal for now (no FreeRTOS tasks touching FATFS concurrently).
 * Revisit once Person A's FreeRTOS task structure splits CAN-receive and
 * SD-logging into separate tasks: set to 1 and implement
 * ff_mutex_create/delete/take/give (samples in ffsystem.c).
 */
#define FF_FS_REENTRANT	0
#define FF_FS_TIMEOUT	1000

/*--- End of configuration options ---*/