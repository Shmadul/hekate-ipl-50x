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
#include "pkg2.h"
#include "heap.h"
#include "se.h"

static u32 _pkg2_calc_kip1_size(pkg2_kip1_t *kip1)
{
	u32 size = sizeof(pkg2_kip1_t);
	for (u32 j = 0; j < KIP1_NUM_SECTIONS; j++)
		size += kip1->sections[j].size_comp;
	return size;
}

void pkg2_parse_kips(link_t *info, pkg2_hdr_t *pkg2)
{
	u8 *ptr = pkg2->data + pkg2->sec_size[PKG2_SEC_KERNEL];
	pkg2_ini1_t *ini1 = (pkg2_ini1_t *)ptr;
	ptr += sizeof(pkg2_ini1_t);

	for (u32 i = 0; i < ini1->num_procs; i++)
	{
		pkg2_kip1_t *kip1 = (pkg2_kip1_t *)ptr;
		pkg2_kip1_info_t *ki = (pkg2_kip1_info_t *)malloc(sizeof(pkg2_kip1_info_t));
		ki->kip1 = kip1;
		ki->size = _pkg2_calc_kip1_size(kip1);
		list_append(info, &ki->link);
		ptr += ki->size;
	}
}

int pkg2_has_kip(link_t *info, u64 tid)
{
	LIST_FOREACH_ENTRY(pkg2_kip1_info_t, ki, info, link)
		if(ki->kip1->tid == tid)
			return 1;
	return 0;
}

void pkg2_replace_kip(link_t *info, u64 tid, pkg2_kip1_t *kip1)
{
	LIST_FOREACH_ENTRY(pkg2_kip1_info_t, ki, info, link)
		if (ki->kip1->tid == tid)
		{
			ki->kip1 = kip1;
			ki->size = _pkg2_calc_kip1_size(kip1);
			return;
		}
}

void pkg2_add_kip(link_t *info, pkg2_kip1_t *kip1)
{
	pkg2_kip1_info_t *ki = (pkg2_kip1_info_t *)malloc(sizeof(pkg2_kip1_info_t));
	ki->kip1 = kip1;
	ki->size = _pkg2_calc_kip1_size(kip1);
	list_append(info, &ki->link);
}

void pkg2_merge_kip(link_t *info, pkg2_kip1_t *kip1)
{
	if (pkg2_has_kip(info, kip1->tid))
		pkg2_replace_kip(info, kip1->tid, kip1);
	else
		pkg2_add_kip(info, kip1);
}

pkg2_hdr_t *pkg2_decrypt(void *data)
{
	u8 *pdata = (u8 *)data;
	
	//Skip signature.
	pdata += 0x100;

	pkg2_hdr_t *hdr = (pkg2_hdr_t *)pdata;

	//Skip header.
	pdata += sizeof(pkg2_hdr_t);

	//Decrypt header.
	se_aes_crypt_ctr(8, hdr, sizeof(pkg2_hdr_t), hdr, sizeof(pkg2_hdr_t), hdr);

	if (hdr->magic != PKG2_MAGIC)
		return NULL;

	for (u32 i = 0; i < 4; i++)
	{
		if (!hdr->sec_size[i])
			continue;

		se_aes_crypt_ctr(8, pdata, hdr->sec_size[i], pdata, hdr->sec_size[i], &hdr->sec_ctr[i * 0x10]);

		pdata += hdr->sec_size[i];
	}

	return hdr;
}

void pkg2_build_encrypt(void *dst, void *kernel, u32 kernel_size, link_t *kips_info)
{
	u8 *pdst = (u8 *)dst;

	//Signature.
	memset(pdst, 0, 0x100);
	pdst += 0x100;

	//Header.
	pkg2_hdr_t *hdr = (pkg2_hdr_t *)pdst;
	memset(hdr, 0, sizeof(pkg2_hdr_t));
	pdst += sizeof(pkg2_hdr_t);
	hdr->magic = PKG2_MAGIC;
	hdr->base = 0x10000000;

	//Kernel.
	memcpy(pdst, kernel, kernel_size);
	hdr->sec_size[PKG2_SEC_KERNEL] = kernel_size;
	hdr->sec_off[PKG2_SEC_KERNEL] = 0x10000000;
	se_aes_crypt_ctr(8, pdst, kernel_size, pdst, kernel_size, &hdr->sec_ctr[PKG2_SEC_KERNEL * 0x10]);
	pdst += kernel_size;

	//INI1.
	u32 ini1_size = sizeof(pkg2_ini1_t);
	pkg2_ini1_t *ini1 = (pkg2_ini1_t *)pdst;
	memset(ini1, 0, sizeof(pkg2_ini1_t));
	ini1->magic = INI1_MAGIC;
	pdst += sizeof(pkg2_ini1_t);
	LIST_FOREACH_ENTRY(pkg2_kip1_info_t, ki, kips_info, link)
	{
		memcpy(pdst, ki->kip1, ki->size);
		pdst += ki->size;
		ini1_size += ki->size;
		ini1->num_procs++;
	}
	ini1->size = ini1_size;
	hdr->sec_size[PKG2_SEC_INI1] = ini1_size;
	hdr->sec_off[PKG2_SEC_INI1] = 0x14080000;
	se_aes_crypt_ctr(8, ini1, ini1_size, ini1, ini1_size, &hdr->sec_ctr[PKG2_SEC_INI1 * 0x10]);

	//Encrypt header.
	*(u32 *)hdr->ctr = 0x100 + sizeof(pkg2_hdr_t) + kernel_size + ini1_size;
	se_aes_crypt_ctr(8, hdr, sizeof(pkg2_hdr_t), hdr, sizeof(pkg2_hdr_t), hdr);
	memset(hdr->ctr, 0 , 0x10);
	*(u32 *)hdr->ctr = 0x100 + sizeof(pkg2_hdr_t) + kernel_size + ini1_size;
}
