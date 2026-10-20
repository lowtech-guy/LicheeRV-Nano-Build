// SPDX-License-Identifier: GPL-2.0
/*
 * Runtime log level for the AIC8800 host drivers.
 *
 * /sys/kernel/config/aic8800/level
 *
 * 0 off
 * 1 errors
 * 2 errors and info (fdrv default)
 * 3 plus trace
 * 4 plus debug (bsp default)
 * 5 every AICWFDBG flag
 *
 * A write sets both aic8800_bsp and, when it is loaded, aic8800_fdrv.
 * The value is not kept across a reboot. A mask written through the
 * fdrv module parameter is reported as that raw number when it is not
 * one of the steps above.
 */

#include <linux/configfs.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "aic_bsp_export.h"

extern int aicwf_dbg_level_bsp;
extern void aicwf_dbg_level_set(int mask);
extern int aicwf_dbg_level_get(void);

/* Bits match LOGERROR..LOGFW in the fdrv debug header. */
static const int aic8800_level_mask[] = {
	0,
	0x0001,
	0x0003,
	0x0007,
	0x000f,
	0x07ff,
};

static bool level_written;
static bool configfs_live;

static int mask_to_level(int mask)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(aic8800_level_mask); i++) {
		if (mask == aic8800_level_mask[i])
			return i;
	}
	return -1;
}

bool aic8800_log_level(int *mask)
{
	if (!level_written || !mask)
		return false;
	*mask = READ_ONCE(aicwf_dbg_level_bsp);
	return true;
}
EXPORT_SYMBOL_GPL(aic8800_log_level);

static int current_mask(void)
{
	int (*get)(void) = symbol_get(aicwf_dbg_level_get);

	if (get) {
		int mask = get();

		symbol_put(aicwf_dbg_level_get);
		return mask;
	}
	return READ_ONCE(aicwf_dbg_level_bsp);
}

static void apply_mask(int mask)
{
	void (*set)(int) = symbol_get(aicwf_dbg_level_set);

	WRITE_ONCE(aicwf_dbg_level_bsp, mask);
	level_written = true;
	if (set) {
		set(mask);
		symbol_put(aicwf_dbg_level_set);
	}
}

static ssize_t aic8800_level_show(struct config_item *item, char *page)
{
	int mask = current_mask();
	int level = mask_to_level(mask);

	if (level >= 0)
		return sysfs_emit(page, "%d\n", level);
	return sysfs_emit(page, "%d\n", mask);
}

static ssize_t aic8800_level_store(struct config_item *item, const char *page,
				   size_t count)
{
	unsigned int value;
	int ret = kstrtouint(page, 10, &value);

	if (ret)
		return ret;
	if (value >= ARRAY_SIZE(aic8800_level_mask))
		return -ERANGE;
	apply_mask(aic8800_level_mask[value]);
	return count;
}
CONFIGFS_ATTR(aic8800_, level);

static struct configfs_attribute *aic8800_attrs[] = {
	&aic8800_attr_level,
	NULL,
};

static const struct config_item_type aic8800_type = {
	.ct_attrs = aic8800_attrs,
	.ct_owner = THIS_MODULE,
};

static struct configfs_subsystem aic8800_subsys = {
	.su_group = {
		.cg_item = {
			.ci_namebuf = "aic8800",
			.ci_type = &aic8800_type,
		},
	},
};

int aic8800_log_configfs_init(void)
{
	int ret;

	if (configfs_live)
		return 0;
	config_group_init(&aic8800_subsys.su_group);
	mutex_init(&aic8800_subsys.su_mutex);
	ret = configfs_register_subsystem(&aic8800_subsys);
	if (ret)
		return ret;
	configfs_live = true;
	return 0;
}

void aic8800_log_configfs_exit(void)
{
	if (!configfs_live)
		return;
	configfs_unregister_subsystem(&aic8800_subsys);
	configfs_live = false;
}
