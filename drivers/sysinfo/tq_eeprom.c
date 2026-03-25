// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (c) 2014-2026 TQ-Systems GmbH <u-boot@ew.tq-group.com>,
 * D-82229 Seefeld, Germany.
 * Author: Nora Schiffer
 */

#include <display_options.h>
#include <dm.h>
#include <i2c_eeprom.h>
#include <log.h>
#include <net.h>
#include <dm/device_compat.h>
#include <linux/ctype.h>
#include <linux/sizes.h>
#include <sysinfo/tq_eeprom.h>

#define TQ_EE_RSV0_BYTES		0x20
#define TQ_EE_RSV1_BYTES		10
#define TQ_EE_SERIAL_BYTES		8
#define TQ_EE_RSV2_BYTES		8
#define TQ_EE_BDID_BYTES		0x40

struct tq_eeprom_data {
	u8 rsv0[TQ_EE_RSV0_BYTES];
	u8 mac[ETH_ALEN];		/* 0x20 ... 0x25 */
	u8 rsv1[TQ_EE_RSV1_BYTES];
	u8 serial[TQ_EE_SERIAL_BYTES];	/* 0x30 ... 0x37 */
	u8 rsv2[TQ_EE_RSV2_BYTES];
	u8 id[TQ_EE_BDID_BYTES];	/* 0x40 ... 0x7f */
};

static_assert(sizeof(struct tq_eeprom_data) == 0x80,
	      "struct tq_eeprom_data has incorrect size");

/**
 * struct sysinfo_tq_eeprom_priv - sysinfo private data
 */
struct sysinfo_tq_eeprom_priv {
	struct udevice *i2c_eeprom;
	int offset;

	/* Reserve extra space for \0 in id and serial */
	char id[TQ_EE_BDID_BYTES + 1];
	char serial[TQ_EE_SERIAL_BYTES + 1];
	u8 mac[ETH_ALEN];
};

static void tq_eeprom_parse_id(struct udevice *dev, const struct tq_eeprom_data *data)
{
	struct sysinfo_tq_eeprom_priv *priv = dev_get_priv(dev);
	int i;

	for (i = 0; i < sizeof(data->id); i++) {
		if (!(isprint(data->id[i]) && isascii(data->id[i])))
			break;
	}

	if (i == 0)
		dev_warn(dev, "no valid model name in EEPROM\n");

	snprintf(priv->id, sizeof(priv->id), "%.*s", i, data->id);
}

static int tq_eeprom_serial_len_old(const struct tq_eeprom_data *data)
{
	int i;

	for (i = 0; i < sizeof(data->serial); i++) {
		if (!isdigit(data->serial[i]))
			break;
	}

	return i;
}

static int tq_eeprom_serial_len_new(const struct tq_eeprom_data *data)
{
	int i;

	for (i = 0; i < sizeof(data->serial); i++) {
		if (!(isdigit(data->serial[i]) || isupper(data->serial[i])))
			break;
	}

	return i;
}

static void tq_eeprom_parse_serial(struct udevice *dev, const struct tq_eeprom_data *data)
{
	struct sysinfo_tq_eeprom_priv *priv = dev_get_priv(dev);
	int len;

	if (data->serial[0] == 'T' && data->serial[1] == 'Q')
		len = tq_eeprom_serial_len_new(data);
	else
		len = tq_eeprom_serial_len_old(data);

	/* For now, only serial numbers with the exact size of the field are accepted */
	if (len != sizeof(data->serial)) {
		dev_warn(dev, "no valid serial number in EEPROM\n");
		len = 0;
	}

	snprintf(priv->serial, sizeof(priv->serial), "%.*s", len, data->serial);
}

static int tq_eeprom_dump(const struct sysinfo_tq_eeprom_priv *priv)
{
	printf("TQ EEPROM:\n");
	printf("  ID:  %s\n", priv->id[0] ? priv->id : "<invalid>");
	printf("  SN:  %s\n", priv->serial[0] ? priv->serial : "<invalid>");
	printf("  MAC: ");
	if (is_valid_ethaddr(priv->mac))
		printf("%pM\n", priv->mac);
	else
		printf("<invalid>\n");

	return 0;
}

static int sysinfo_tq_eeprom_detect(struct udevice *dev)
{
	struct sysinfo_tq_eeprom_priv *priv = dev_get_priv(dev);
	struct tq_eeprom_data data;
	int ret;

	ret = i2c_eeprom_read(priv->i2c_eeprom, priv->offset,
			      (u8 *)&data, sizeof(data));
	if (ret < 0) {
		dev_err(dev, "EEPROM read failed: %d\n", ret);
		return -EIO;
	}

	tq_eeprom_parse_id(dev, &data);
	tq_eeprom_parse_serial(dev, &data);
	memcpy(priv->mac, data.mac, ETH_ALEN);

	if (!IS_ENABLED(CONFIG_SPL_BUILD))
		tq_eeprom_dump(priv);

	return 0;
}

static int sysinfo_tq_eeprom_get_str(struct udevice *dev, int id, size_t size, char *val)
{
	struct sysinfo_tq_eeprom_priv *priv = dev_get_priv(dev);

	switch (id) {
	case SYSID_TQ_MODEL:
		if (!priv->id[0])
			return -ENODATA;

		strlcpy(val, priv->id, size);
		return 0;

	case SYSID_TQ_SERIAL:
		if (!priv->serial[0])
			return -ENODATA;

		strlcpy(val, priv->serial, size);
		return 0;

	default:
		return -EINVAL;
	}
}

static int sysinfo_tq_eeprom_get_data(struct udevice *dev, int id, void **data, size_t *size)
{
	struct sysinfo_tq_eeprom_priv *priv = dev_get_priv(dev);

	switch (id) {
	case SYSID_TQ_MAC_ADDR:
		if (!is_valid_ethaddr(priv->mac))
			return -ENODATA;

		*data = priv->mac;
		*size = sizeof(priv->mac);

		return 0;

	default:
		return -EINVAL;
	}
}

static const struct sysinfo_ops sysinfo_tq_eeprom_ops = {
	.detect = sysinfo_tq_eeprom_detect,
	.get_str = sysinfo_tq_eeprom_get_str,
	.get_data = sysinfo_tq_eeprom_get_data,
};

static int sysinfo_tq_eeprom_probe(struct udevice *dev)
{
	struct sysinfo_tq_eeprom_priv *priv = dev_get_priv(dev);
	int ret;

	ret = uclass_get_device_by_phandle(UCLASS_I2C_EEPROM, dev, "i2c-eeprom",
					   &priv->i2c_eeprom);
	if (ret) {
		dev_err(dev, "i2c-eeprom backing device not found: %d\n", ret);
		return ret;
	}

	priv->offset = dev_read_u32_default(dev, "offset", 0);

	return 0;
}

static const struct udevice_id sysinfo_tq_eeprom_ids[] = {
	{ .compatible = "tq,eeprom-sysinfo" },
	{ /* sentinel */ }
};

U_BOOT_DRIVER(sysinfo_tq_eeprom) = {
	.name           = "sysinfo_tq_eeprom",
	.id             = UCLASS_SYSINFO,
	.of_match       = sysinfo_tq_eeprom_ids,
	.ops		= &sysinfo_tq_eeprom_ops,
	.priv_auto	= sizeof(struct sysinfo_tq_eeprom_priv),
	.probe          = sysinfo_tq_eeprom_probe,
};
