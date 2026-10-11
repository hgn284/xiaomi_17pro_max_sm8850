// SPDX-License-Identifier: GPL-2.0-or-later
/* Derived from Linux UFS core (Samsung 2011-2013, Linux Foundation 2013-2016).
 * OS44 instruction-verified reconstruction; no private owner fabricated. */
#include <linux/slab.h>
#include <linux/nls.h>
#include <ufs/ufshcd.h>
#include <ufs/ufs_quirks.h>
#include "os44_descriptor_owner.h"
#include "os44_device_identity.h"
#define QUERY_REQ_RETRIES 3
struct uc_string_id { u8 len; u8 type; wchar_t uc[]; } __packed;
static inline char ufshcd_remove_non_printable(u8 ch)
{
	return (ch >= 0x20 && ch <= 0x7e) ? ch : ' ';
}
int os44_ufshcd_read_string_desc(struct ufs_hba *hba, u8 desc_index,
			    u8 **buf, bool ascii)
{
	struct uc_string_id *uc_str;
	u8 *str;
	int ret;

	if (!buf)
		return -EINVAL;

	uc_str = kzalloc(QUERY_DESC_MAX_SIZE, GFP_KERNEL);
	if (!uc_str)
		return -ENOMEM;

	ret = os44_mi_ufshcd_read_desc_param(hba, QUERY_DESC_IDN_STRING, desc_index, 0,
				     (u8 *)uc_str, QUERY_DESC_MAX_SIZE);
	if (ret < 0) {
		dev_err(hba->dev, "Reading String Desc failed after %d retries. err = %d\n",
			QUERY_REQ_RETRIES, ret);
		str = NULL;
		goto out;
	}

	if (uc_str->len <= QUERY_DESC_HDR_SIZE) {
		dev_dbg(hba->dev, "String Desc is of zero length\n");
		str = NULL;
		ret = 0;
		goto out;
	}

	if (ascii) {
		ssize_t ascii_len;
		int i;
		/* remove header and divide by 2 to move from UTF16 to UTF8 */
		ascii_len = (uc_str->len - QUERY_DESC_HDR_SIZE) / 2 + 1;
		str = kzalloc(ascii_len, GFP_KERNEL);
		if (!str) {
			ret = -ENOMEM;
			goto out;
		}

		/*
		 * the descriptor contains string in UTF16 format
		 * we need to convert to utf-8 so it can be displayed
		 */
		ret = utf16s_to_utf8s(uc_str->uc,
				      uc_str->len - QUERY_DESC_HDR_SIZE,
				      UTF16_BIG_ENDIAN, str, ascii_len - 1);

		/* replace non-printable or non-ASCII characters with spaces */
		for (i = 0; i < ret; i++)
			str[i] = ufshcd_remove_non_printable(str[i]);

		str[ret++] = '\0';

	} else {
		str = kmemdup(uc_str->uc, uc_str->len, GFP_KERNEL);
		if (!str) {
			ret = -ENOMEM;
			goto out;
		}
		ret = uc_str->len;
	}
out:
	*buf = str;
	kfree(uc_str);
	return ret;
}
void os44_ufshcd_fixup_dev_quirks(struct ufs_hba *hba,
			     const struct ufs_dev_quirk *fixups)
{
	const struct ufs_dev_quirk *f;
	struct ufs_dev_info *dev_info = &hba->dev_info;

	if (!fixups)
		return;

	for (f = fixups; f->quirk; f++) {
		if ((f->wmanufacturerid == dev_info->wmanufacturerid ||
		     f->wmanufacturerid == UFS_ANY_VENDOR) &&
		     ((dev_info->model &&
		       STR_PRFX_EQUAL(f->model, dev_info->model)) ||
		      !strcmp(f->model, UFS_ANY_MODEL)))
			hba->dev_quirks |= f->quirk;
	}
}

/* Stock OS44 table: seven entries + sentinel; unlike the newer public table,
 * Samsung lacks PA_HIBER8TIME and there is no THGJFJT1E45BATP row. */
const struct ufs_dev_quirk os44_ufs_fixups[] = {
 { UFS_VENDOR_MICRON, UFS_ANY_MODEL, UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM },
 { UFS_VENDOR_SAMSUNG, UFS_ANY_MODEL, UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM |
   UFS_DEVICE_QUIRK_HOST_PA_TACTIVATE | UFS_DEVICE_QUIRK_RECOVERY_FROM_DL_NAC_ERRORS },
 { UFS_VENDOR_SKHYNIX, UFS_ANY_MODEL, UFS_DEVICE_QUIRK_HOST_PA_SAVECONFIGTIME },
 { UFS_VENDOR_SKHYNIX, "hB8aL1", UFS_DEVICE_QUIRK_HOST_VS_DEBUGSAVECONFIGTIME },
 { UFS_VENDOR_TOSHIBA, UFS_ANY_MODEL, UFS_DEVICE_QUIRK_DELAY_BEFORE_LPM },
 { UFS_VENDOR_TOSHIBA, "THGLF2G9C8KBADG", UFS_DEVICE_QUIRK_PA_TACTIVATE },
 { UFS_VENDOR_TOSHIBA, "THGLF2G9D8KBADG", UFS_DEVICE_QUIRK_PA_TACTIVATE },
 { }
};
static_assert(sizeof(os44_ufs_fixups) == 192);
