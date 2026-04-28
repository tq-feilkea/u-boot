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

__weak size_t tq_common_sysinfo_macaddr_num(void)
{
	return CONFIG_TQ_COMMON_SYSINFO_MACADDR_NUM;
}

static void tq_common_sysinfo_set_macaddrs(const u8 *macaddr)
{
	size_t i, macaddr_num = tq_common_sysinfo_macaddr_num();
	u8 macaddr_buf[ETH_ALEN];

	if (macaddr_num == 0)
		return;

	memcpy(macaddr_buf, macaddr, ETH_ALEN);

	for (i = 0; ; i++) {
		eth_env_set_enetaddr_by_index("eth", CONFIG_TQ_COMMON_SYSINFO_MACADDR_OFFSET + i,
					      macaddr_buf);
		if (i >= (macaddr_num - 1))
			break;

		if (++macaddr_buf[5])
			continue;
		if (++macaddr_buf[4])
			continue;
		if (++macaddr_buf[3])
			continue;

		printf("Warning: End of MAC address block\n");
		break;
	}
}

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

	if (!sysinfo_get_data(sysinfo, SYSID_TQ_MAC_ADDR, &macaddr, &macaddr_size) &&
	    macaddr_size == ETH_ALEN)
		tq_common_sysinfo_set_macaddrs(macaddr);
}
