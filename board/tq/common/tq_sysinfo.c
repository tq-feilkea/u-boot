// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Common sysinfo helpers for TQ-Systems SOMs
 *
 * Copyright (c) 2020-2026 TQ-Systems GmbH <u-boot@ew.tq-group.com>
 * D-82229 Seefeld, Germany.
 * Author: Nora Schiffer
 */

#include <env.h>
#include <fdt_support.h>
#include <mtd_node.h>
#include <net.h>
#include <spi_flash.h>
#include <sysinfo/tq_eeprom.h>

#define MAX_NAME_LENGTH	80

void tq_common_sysinfo_setup(void)
{
	struct udevice *sysinfo;
	size_t macaddr_size;
	void *macaddr;
	char buf[MAX_NAME_LENGTH] = "";
	int ret;

	ret = sysinfo_get_and_detect(&sysinfo);
	if (ret) {
		pr_err("Failed to get sysinfo data: %d\n", ret);
		return;
	}

	if (!sysinfo_get_str(sysinfo, SYSID_TQ_MODEL, sizeof(buf), buf))
		env_set_runtime("boardtype", buf);

	if (!sysinfo_get_str(sysinfo, SYSID_TQ_SERIAL, sizeof(buf), buf))
		env_set_runtime("serial#", buf);
}
