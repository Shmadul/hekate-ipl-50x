/*
* Copyright (c) 2018 naehrwert
*
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public License,
* version 2, as published by the Free Software Foundation.
*
* This program is distributed in the hope it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <string.h>
#include "hos.h"
#include "sdmmc.h"
#include "nx_emmc.h"
#include "t210.h"
#include "se.h"
#include "se_t210.h"
#include "pmc.h"
#include "cluster.h"
#include "heap.h"
#include "tsec.h"
#include "pkg2.h"
#include "nx_emmc.h"
#include "util.h"
#include "pkg1.h"
#include "pkg2.h"
#include "ff.h"
#include "gfx.h"

#define NUM_KEYBLOB_KEYS 5
static const u8 keyblob_keyseeds[NUM_KEYBLOB_KEYS][0x10] = {
	{ 0xDF, 0x20, 0x6F, 0x59, 0x44, 0x54, 0xEF, 0xDC, 0x70, 0x74, 0x48, 0x3B, 0x0D, 0xED, 0x9F, 0xD3 }, //1.0.0
	{ 0x0C, 0x25, 0x61, 0x5D, 0x68, 0x4C, 0xEB, 0x42, 0x1C, 0x23, 0x79, 0xEA, 0x82, 0x25, 0x12, 0xAC }, //3.0.0
	{ 0x33, 0x76, 0x85, 0xEE, 0x88, 0x4A, 0xAE, 0x0A, 0xC2, 0x8A, 0xFD, 0x7D, 0x63, 0xC0, 0x43, 0x3B }, //3.0.1
	{ 0x2D, 0x1F, 0x48, 0x80, 0xED, 0xEC, 0xED, 0x3E, 0x3C, 0xF2, 0x48, 0xB5, 0x65, 0x7D, 0xF7, 0xBE }, //4.0.0
	{ 0xBB, 0x5A, 0x01, 0xF9, 0x88, 0xAF, 0xF5, 0xFC, 0x6C, 0xFF, 0x07, 0x9E, 0x13, 0x3C, 0x39, 0x80 }  //5.0.0
};

static const u8 cmac_keyseed[0x10] =
	{ 0x59, 0xC7, 0xFB, 0x6F, 0xBE, 0x9B, 0xBE, 0x87, 0x65, 0x6B, 0x15, 0xC0, 0x53, 0x73, 0x36, 0xA5 };

static const u8 mkey_keyseed_retail[0x10] =
	{ 0xD8, 0xA2, 0x41, 0x0A, 0xC6, 0xC5, 0x90, 0x01, 0xC6, 0x1D, 0x6A, 0x26, 0x7C, 0x51, 0x3F, 0x3C };

static const u8 ckey_keyseed[0x10] =
	{ 0x4F, 0x02, 0x5F, 0x0E, 0xB6, 0x6D, 0x11, 0x0E, 0xDC, 0x32, 0x7D, 0x41, 0x86, 0xC2, 0xF4, 0x78 };

static const u8 key8_keyseed[] =
	{ 0xFB, 0x8B, 0x6A, 0x9C, 0x79, 0x00, 0xC8, 0x49, 0xEF, 0xD2, 0x4D, 0x85, 0x4D, 0x30, 0xA0, 0xC7 };

static const u8 mkey_keyseed_retail_new[0x10] =
	{ 0x2D, 0xC1, 0xF4, 0x8D, 0xF3, 0x5B, 0x69, 0x33, 0x42, 0x10, 0xAC, 0x65, 0xDA, 0x90, 0x46, 0x66 };

static const u8 ckey_keyseed_new[0x10] =
	{ 0x0C, 0x91, 0x09, 0xDB, 0x93, 0x93, 0x07, 0x81, 0x07, 0x3C, 0xC4, 0x16, 0x22, 0x7C, 0x6C, 0x28 };

static void _se_lock()
{
	for (u32 i = 0; i < 16; i++)
		se_key_acc_ctrl(i, 0x15);

	for (u32 i = 0; i < 2; i++)
		se_rsa_acc_ctrl(i, 1);

	SE(0x4) = 0; //Make this reg secure only.
	SE(SE_KEY_TABLE_ACCESS_LOCK_OFFSET) = 0; //Make all key access regs secure only.
	SE(SE_RSA_KEYTABLE_ACCESS_LOCK_OFFSET) = 0; //Make all rsa access regs secure only.
	SE(SE_SECURITY_0) &= 0xFFFFFFFB; //Make access lock regs secure only.
}

//Key derivation for < 4.0.0
static int _keygen_1(u8 *keyblob, u32 kb, void *tsec_fw)
{
	u8 *tmp = (u8 *)malloc(0x10);
	u8 *tmp2 = (u8 *)malloc(0x10);

	se_key_acc_ctrl(13, 0x15);
	se_key_acc_ctrl(14, 0x15);

	//Get TSEC key.
	if (tsec_query(tmp, 1, tsec_fw) < 0)
		return 0;
	se_aes_key_set(13, tmp, 0x10);

	//Derive keyblob keys from TSEC+SBK.
	memcpy(tmp2, keyblob_keyseeds[0], 0x10);
	se_aes_crypt_block_ecb(13, 0, tmp2, tmp2);
	se_aes_unwrap_key(15, 14, tmp2);

	memcpy(tmp, keyblob_keyseeds[kb], 0x10);
	se_aes_crypt_block_ecb(13, 0, tmp, tmp);
	se_aes_unwrap_key(13, 14, tmp);
	
	se_aes_key_clear(14);

	//TODO: verify keyblob CMAC.
	//se_aes_unwrap_key(11, 13, cmac_keyseed);
	//se_aes_cmac(tmp, 0x10, 11, keyblob + 0x10, 0xA0);
	//if (!memcmp(keyblob, tmp, 0x10))
	//	return 0;
	memcpy(tmp, cmac_keyseed, 0x10);
	se_aes_crypt_block_ecb(13, 0, tmp, tmp);
	memcpy(tmp, cmac_keyseed, 0x10);
	se_aes_unwrap_key(11, 13, tmp);

	//Decrypt keyblob and set keyslots.
	se_aes_crypt_ctr(13, keyblob + 0x20, 0x90, keyblob + 0x20, 0x90, keyblob + 0x10);
	se_aes_key_set(11, keyblob + 0x20 + 0x80, 0x10);
	se_aes_key_set(12, keyblob + 0x20, 0x10);
	se_aes_key_set(13, keyblob + 0x20, 0x10);

	//TODO: for some reason SE likes to hang if we don't execute an operation here.
	memcpy(tmp, mkey_keyseed_retail, 0x10);
	se_aes_crypt_block_ecb(12, 0, tmp, tmp);

	//Generate retail master key.
	memcpy(tmp, ckey_keyseed_new, 0x10);
	se_aes_unwrap_key(10, 15, tmp);

	memcpy(tmp, ckey_keyseed, 0x10);
	se_aes_unwrap_key(15, 15, tmp);

	memcpy(tmp, mkey_keyseed_retail_new, 0x10);
	se_aes_unwrap_key(14, 12, tmp);

	memcpy(tmp, mkey_keyseed_retail, 0x10);
	se_aes_unwrap_key(12, 12, tmp);

	memcpy(tmp, key8_keyseed, 0x10);
	se_key_acc_ctrl(8, 0x15);
	se_aes_unwrap_key(8, 12, tmp);

	free(tmp);
	free(tmp2);
}

typedef struct _launch_ctxt_t
{
	void *keyblob;

	void *pkg1;
	const pkg1_id_t *pkg1_id;

	void *warmboot;
	u32 warmboot_size;
	void *secmon;
	u32 secmon_size;

	void *pkg2;
	u32 pkg2_size;

	void *kernel;
	u32 kernel_size;
	link_t kip1_list;
} launch_ctxt_t;

typedef struct _merge_kip_t
{
	void *kip1;
	link_t link;
} merge_kip_t;

static int _read_emmc_pkg1(launch_ctxt_t *ctxt, gfx_con_t * con)
{
	int res = 0;
	sdmmc_storage_t storage;
	sdmmc_t sdmmc;

	sdmmc_storage_init_mmc(&storage, &sdmmc, SDMMC_4, SDMMC_BUS_WIDTH_8, 4);

	//Read package1.
	ctxt->pkg1 = (u8 *)malloc(0x40000);
	sdmmc_storage_set_mmc_partition(&storage, 1);
	sdmmc_storage_read(&storage, 0x100000 / NX_EMMC_BLOCKSIZE, 0x40000 / NX_EMMC_BLOCKSIZE, ctxt->pkg1);
	ctxt->pkg1_id = pkg1_identify(ctxt->pkg1);
	if (!ctxt->pkg1_id)
	{
		gfx_debug(con, error, "Could not identify pkg1 version (= '%s').\n", (char *)ctxt->pkg1 + 0x10);

		goto out;
	}

	gfx_debug(con, message, "Identified pkg1('%s'), and keyblob(%d)\n", (char *)(ctxt->pkg1 + 0x10), ctxt->pkg1_id->kb);

	//Read the correct keyblob.
	ctxt->keyblob = (u8 *)malloc(NX_EMMC_BLOCKSIZE);
	sdmmc_storage_read(&storage, 0x180000 / NX_EMMC_BLOCKSIZE + ctxt->pkg1_id->kb, 1, ctxt->keyblob);

	res = 1;

out:;
	sdmmc_storage_end(&storage);
	return res;
}

static int _read_emmc_pkg2(launch_ctxt_t *ctxt, gfx_con_t * con)
{
	int res = 0;
	sdmmc_storage_t storage;
	sdmmc_t sdmmc;

	sdmmc_storage_init_mmc(&storage, &sdmmc, SDMMC_4, SDMMC_BUS_WIDTH_8, 4);
	sdmmc_storage_set_mmc_partition(&storage, 0);

	//Parse eMMC GPT.
	LIST_INIT(gpt);
	nx_emmc_gpt_parse(&gpt, &storage);

	gfx_debug(con, message, "Parsed GPT\n");

	//Find package2 partition.
	emmc_part_t *pkg2_part = nx_emmc_part_find(&gpt, "BCPKG2-1-Normal-Main");
	if (!pkg2_part)
		goto out;

	//Read in package2 header and get package2 real size.
	//TODO: implement memalign for DMA buffers.
	u8 *tmp = (u8 *)malloc(NX_EMMC_BLOCKSIZE);
	nx_emmc_part_read(&storage, pkg2_part, 0x4000 / NX_EMMC_BLOCKSIZE, 1, tmp);
	u32 *hdr = (u32 *)(tmp + 0x100);
	u32 pkg2_size = hdr[0] ^ hdr[2] ^ hdr[3];
	free(tmp);
	gfx_debug(con, message, "The size of pkg2 is %08X\n", pkg2_size);

	//Read in package2.
	u32 pkg2_size_aligned = ALIGN(pkg2_size, NX_EMMC_BLOCKSIZE);
	gfx_debug(con, message, "The size of pkg2 aligned is %08X\n", pkg2_size_aligned);

	ctxt->pkg2 = malloc(pkg2_size_aligned);
	ctxt->pkg2_size = pkg2_size;
	nx_emmc_part_read(&storage, pkg2_part, 0x4000 / NX_EMMC_BLOCKSIZE, 
		pkg2_size_aligned / NX_EMMC_BLOCKSIZE, ctxt->pkg2);

	res = 1;

out:;
	nx_emmc_gpt_free(&gpt);
	sdmmc_storage_end(&storage);
	return res;
}

static int _config_kip1(launch_ctxt_t *ctxt, const char *value, gfx_con_t * con)
{
	FIL fp;
	if (f_open(&fp, value, FA_READ) != FR_OK)
		return 0;
	merge_kip_t *mkip1 = (merge_kip_t *)malloc(sizeof(merge_kip_t));
	mkip1->kip1 = malloc(f_size(&fp));
	f_read(&fp, mkip1->kip1, f_size(&fp), NULL);

	gfx_debug(con, ok, "Loaded %s.\n", value);

	f_close(&fp);
	list_append(&ctxt->kip1_list, &mkip1->link);
	return 1;
}

int hos_launch(gfx_con_t * con, bool hen) 
{
	launch_ctxt_t ctxt;
	memset(&ctxt, 0, sizeof(launch_ctxt_t));
	list_init(&ctxt.kip1_list);

	if (hen == true) {
		_config_kip1(&ctxt, "loader.kip1", con);
		_config_kip1(&ctxt, "sm.kip1", con);
	}

	//Read package1 and the correct keyblob.
	if (!_read_emmc_pkg1(&ctxt, con))
		return 0;

	gfx_debug(con, ok, "Loaded pkg1 and keyblob.\n");

	//Generate keys.
	_keygen_1(ctxt.keyblob, ctxt.pkg1_id->kb, (u8 *)ctxt.pkg1 + ctxt.pkg1_id->tsec_off);

	gfx_debug(con, ok, "Generated keys.\n");

	//Decrypt and unpack package1 if we require parts of it.
	if (!ctxt.warmboot || !ctxt.secmon)
	{
		pkg1_decrypt(ctxt.pkg1_id, ctxt.pkg1);
		pkg1_unpack((void *)0x8000D000, (void *)ctxt.pkg1_id->secmon_base, ctxt.pkg1_id, ctxt.pkg1);
	}

	gfx_debug(con, ok, "Decrypted and unpacked pkg1.\n");

	//Replace 'warmboot.bin' if requested.
	if (ctxt.warmboot)
		memcpy((void *)0x8000D000, ctxt.warmboot, ctxt.warmboot_size);

	//Set warmboot address in PMC.
	PMC(APBDEV_PMC_SCRATCH1) = 0x8000D000;

	//Replace 'SecureMonitor' if requested.
	if (ctxt.secmon)
		memcpy((void *)ctxt.pkg1_id->secmon_base, ctxt.secmon, ctxt.secmon_size);
	else
	{
		//Else we patch it to allow for an unsigned package2.
		patch_t *secmon_patchset = ctxt.pkg1_id->secmon_patchset;
		for (u32 i = 0; secmon_patchset[i].off != 0xFFFFFFFF; i++)
			*(vu32 *)(ctxt.pkg1_id->secmon_base + secmon_patchset[i].off) = secmon_patchset[i].val;
	}

	gfx_debug(con, ok, "Loaded warmboot.bin and secmon.\n");

	//Read package2.
	if (!_read_emmc_pkg2(&ctxt, con))
		return 0;

	gfx_debug(con, ok, "Loaded pkg2.\n");

	//Decrypt package2 and parse KIP1 blobs in INI1 section.
	pkg2_hdr_t *pkg2_hdr = pkg2_decrypt(ctxt.pkg2);
	LIST_INIT(kip1_info);
	pkg2_parse_kips(&kip1_info, pkg2_hdr);

	gfx_debug(con, ok, "Decrypted and parsed out KIP1 blobs.\n");

	//Use the kernel included in package2 in case we didn't load one already.
	if (!ctxt.kernel)
	{
		ctxt.kernel = pkg2_hdr->data;
		ctxt.kernel_size = pkg2_hdr->sec_size[PKG2_SEC_KERNEL];
	}

	if (hen == true) {
		//Merge extra KIP1s into loaded ones.
		LIST_FOREACH_ENTRY(merge_kip_t, mki, &ctxt.kip1_list, link)
			pkg2_merge_kip(&kip1_info, (pkg2_kip1_t *)mki->kip1);
	}

	//Rebuild and encrypt package2.
	pkg2_build_encrypt((void *)0xA9800000, ctxt.kernel, ctxt.kernel_size, &kip1_info);
	se_aes_key_clear(8);
	se_aes_key_clear(11);
	se_key_acc_ctrl(12, 0xFF);
	se_key_acc_ctrl(15, 0xFF);

	gfx_debug(con, ok, "Rebuilt and encrypted pkg2.\n");

	//Clear 'BootConfig'.
	memset((void *)0x4003D000, 0, 0x3000);

	//Lock SE before starting 'SecureMonitor'.
	_se_lock();

	vu32 *mb_in = (vu32 *)0x40002EF8;
	vu32 *mb_out = (vu32 *)0x40002EFC;

	*mb_in = 0;
	*mb_out = 0;

	//Wait for secmon to get ready.
	cluster_boot_cpu0(ctxt.pkg1_id->secmon_base);
	while (!*mb_out)
		sleep(1);

	//Signal 'BootConfig'.
	*mb_in = 1;
	sleep(100);

	//Signal package2 available.
	*mb_in = 2;
	sleep(100);
	*mb_in = 3;
	sleep(100);
	*mb_in = 4;
	sleep(100);

	//Halt ourselves in waitevent state.
	while (1)
		FLOW_CTLR(0x4) = 0x50000000;

	return 0;
}
