/*
 *  Basic SMB136 Battery Charger Driver
 *
 *  Copyright (C) 2016 OMAP4 AOSP Project
 *  Copyright (C) 2011 Samsung Electronics
 *  Ikkeun Kim <iks.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/power/smb136-charger.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/battery.h>
#include <linux/mfd/stmpe811.h>

/* SMB136 Registers. */
#define SMB136_CHARGE_CURRENT			0x00
#define SMB136_INPUT_CURRENTLIMIT		0x01
#define SMB136_FLOAT_VOLTAGE			0x02
#define SMB136_CHARGE_CONTROL_A			0x03
#define SMB136_CHARGE_CONTROL_B			0x04
#define SMB136_PIN_ENABLE_CONTROL		0x05
#define SMB136_OTG_CONTROL			0x06
#define SMB136_SAFTY				0x09

#define SMB136_COMMAND_A			0x31
#define SMB136_STATUS_D				0x35

#define SMB136_STATUS_E				0x36
#define	CHARGE_CYCLE_COUNT_STAT				BIT(7)
#define	CHARGER_TERM_STAT					BIT(6)
#define	SAFETY_TIMER_STAT_MASK				((unsigned char)(((1 << 2) - 1) << 4))
#define	CHARGER_ERROR_STAT					BIT(3)
#define	CHARGING_STAT_E						((unsigned char)(((1 << 2) - 1) << 1))
#define	CHARGING_EN							BIT(0)

#define	NOT_CHARGING_STATUS						0x0
#define PRE_CHARGE_STATUS						0x1
#define FAST_CHARGE_STATUS						0x2
#define TAPER_CHARGE_STATUS						0x3

#define GPIO_TA_NCONNECTED		32

struct smb136_charger {
	struct i2c_client	*client;
	struct power_supply	*mains;
	struct power_supply	*usb;
	bool			mains_online;
	bool			usb_online;
	bool			usb_hc_mode;
	const struct smb136_charger_platform_data *pdata;
};

static int smb136_i2c_read(struct i2c_client *client, u8 reg)
{
	int ret;
    printk("smb136_i2c_read");

	if (!client)
		return -ENODEV;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return -EIO;
	}

	return ret;
}

static int smb136_i2c_write(struct i2c_client *client, u8 reg, u8 data)
{
	int ret;
    printk("smb136_i2c_write");

	if (!client)
		return -ENODEV;

	ret = i2c_smbus_write_byte_data(client, reg, data);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static void smb136_update_charger(struct smb136_charger *smb)
{
    printk("smb136_update_charger");
	if (smb->usb_hc_mode) {
		/* HC mode */
		smb136_i2c_write(smb->client, SMB136_COMMAND_A, 0x8c);

		/* Set charging current limit to 1.5A */
		smb136_i2c_write(smb->client, SMB136_CHARGE_CURRENT, 0xF4);

		dev_info(&smb->client->dev,
			"charging current limit set to 1.5A\n");
	} else if (smb->usb_online) {
		/* USBIN 500mA mode */
		smb136_i2c_write(smb->client, SMB136_COMMAND_A, 0x88);

		/* Set charging current limit to 500mA */
		smb136_i2c_write(smb->client, SMB136_CHARGE_CURRENT, 0x14);

		dev_info(&smb->client->dev,
			"charging current limit set to 0.5A\n");
	} else {
		/* USB 100mA Mode, USB5/1 Current Levels */
		/* Prevent in-rush current */
		smb136_i2c_write(smb->client, SMB136_COMMAND_A, 0x80);
		udelay(10);

		/* Set charge current to 100mA */
		/* Prevent in-rush current */
		smb136_i2c_write(smb->client, SMB136_CHARGE_CURRENT, 0x14);
		udelay(10);
	}
}

static int smb136_mains_get_property(struct power_supply *psy,
				     enum power_supply_property prop,
				     union power_supply_propval *val)
{
	struct smb136_charger *smb = power_supply_get_drvdata(psy);
    printk("smb136_mains_get_property");

	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = smb->mains_online;
		return 0;
	default:
		return -EINVAL;
	}
	return -EINVAL;
}

static int smb136_mains_set_property(struct power_supply *psy,
				     enum power_supply_property prop,
				     const union power_supply_propval *val)
{
	struct smb136_charger *smb = power_supply_get_drvdata(psy);
	bool oldval;

    printk("smb136_mains_set_property");
	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		oldval = smb->mains_online;

		smb->mains_online = val->intval;

		if (smb->mains_online != oldval)
			power_supply_changed(psy);
		return 0;
	default:
		return -EINVAL;
	}

	return -EINVAL;
}

static int smb136_mains_property_is_writeable(struct power_supply *psy,
					     enum power_supply_property prop)
{
    printk("smb136_mains_property_is_writeable");
	return 0;
}

static enum power_supply_property smb136_mains_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int smb136_usb_get_property(struct power_supply *psy,
				   enum power_supply_property prop,
				   union power_supply_propval *val)
{
	struct smb136_charger *smb = power_supply_get_drvdata(psy);

    printk("smb136_usb_get_property");
	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = smb->usb_online;
		return 0;

	case POWER_SUPPLY_PROP_USB_HC:
		val->intval = smb->usb_hc_mode;
		return 0;

	default:
		break;
	}
	return -EINVAL;
}

static int smb136_usb_set_property(struct power_supply *psy,
				   enum power_supply_property prop,
				   const union power_supply_propval *val)
{
	int ret = -EINVAL;
	struct smb136_charger *smb = power_supply_get_drvdata(psy);
	bool oldval;

    printk("smb136_usb_set_property");
	switch (prop) {
	case POWER_SUPPLY_PROP_ONLINE:
		oldval = smb->usb_online;
		smb->usb_online = val->intval;

		if (smb->usb_online != oldval)
			power_supply_changed(psy);
		ret = 0;
		break;
	case POWER_SUPPLY_PROP_USB_HC:
		smb->usb_hc_mode = val->intval;
		break;
	default:
		break;
	}

	smb136_update_charger(smb);

	return ret;
}

static int smb136_usb_property_is_writeable(struct power_supply *psy,
					    enum power_supply_property prop)
{
    printk("smb136_usb_property_is_writeable");
	switch (prop) {
	case POWER_SUPPLY_PROP_USB_HC:
		return 1;
	default:
		break;
	}

	return 0;
}

static int smb136_get_battery_info(struct smb136_charger *smb)
{
	struct smb136_charger_platform_data *pdata = (void *)smb->pdata;
	struct power_supply_battery_info info = {};
	struct power_supply *supply;
	int err;

	if (smb->mains)
		supply = smb->mains;
	else
		supply = smb->usb;

	err = power_supply_get_battery_info(supply, &info);
	if (err == -ENXIO || err == -ENODEV)
		return 0;
	if (err)
		return err;

	return 0;
}

static struct smb136_charger_platform_data
			*smb136_get_platdata(struct device *dev)
{
	struct smb136_charger_platform_data *pdata;

	if (dev->of_node) {
		pdata = devm_kzalloc(dev, sizeof(*pdata), GFP_KERNEL);
	} else {
		pdata = dev_get_platdata(dev);
	}

	return pdata;
}

static void smb136_start_charger(int cable_type)
{
	struct power_supply *smb_usb;
	struct power_supply *smb_mains;
	union power_supply_propval usb_online = { cable_type == CABLE_TYPE_USB ? 1 : 0 };
	union power_supply_propval ac_online = { cable_type == CABLE_TYPE_AC ? 1 : 0 };

    printk("smb136_start_charger, cable_type is %x", cable_type );
	smb_usb = power_supply_get_by_name("smb136-usb");
	smb_mains = power_supply_get_by_name("smb136-mains");

	if (smb_mains) {
        printk("if (smb_mains)");
		smb136_mains_set_property(
			smb_mains,
			POWER_SUPPLY_PROP_ONLINE,
			&ac_online);
	}

	if (smb_usb) {
        printk("if (smb_usb)");
		smb136_usb_set_property(
			smb_usb,
			POWER_SUPPLY_PROP_ONLINE,
			&usb_online);
		smb136_usb_set_property(
			smb_usb,
			POWER_SUPPLY_PROP_USB_HC,
			&ac_online);
	}
}

static int smb136_hw_init(struct smb136_charger *smb)
{
    struct power_supply *smb_usb;
    printk("smb136_hw_init");
	/* Change USB5/1/HC Control from Pin to I2C */
	smb136_i2c_write(smb->client, SMB136_PIN_ENABLE_CONTROL, 0x8);

	/* Disable Automatic Input Current Limit */
	/* Set it to 1.3A */
	smb136_i2c_write(smb->client, SMB136_INPUT_CURRENTLIMIT, 0xE6);

	/* Automatic Recharge Disabed */
	smb136_i2c_write(smb->client, SMB136_CHARGE_CONTROL_A, 0x8c);

	/* Safty timer Disabled */
	smb136_i2c_write(smb->client, SMB136_CHARGE_CONTROL_B, 0x28);

	/* Disable USB D+/D- Detection */
	smb136_i2c_write(smb->client, SMB136_OTG_CONTROL, 0x28);

	/* Set Output Polarity for STAT */
	smb136_i2c_write(smb->client, SMB136_FLOAT_VOLTAGE, 0xCA);

	/* Re-load Enable */
	smb136_i2c_write(smb->client, SMB136_SAFTY, 0x4b);

    //smb136_start_charger();

	return 0;
}

int espresso_cable_type = CABLE_TYPE_NONE;
#define CABLE_DETECT_VALUE	1150
enum espresso_adc_ch {
	REMOTE_SENSE = 0,
	ADC_CHECK_1,	/* TA detection */
	ACCESSORY_ID,	/* OTG detection */
	EAR_ADC_35,	/* Earjack detection */
};
#define ADC_CHANNEL_IN0		4
#define ADC_CHANNEL_IN1		5
#define ADC_CHANNEL_IN2		6
#define ADC_CHANNEL_IN3		7
#define MAX_ADC_VAL	4096
#define MIN_ADC_VAL	0

int omap4_espresso_get_adc(enum espresso_adc_ch ch)
{
	int adc;
	int i;
	int adc_tmp;
	int adc_min = MAX_ADC_VAL;
	int adc_max = MIN_ADC_VAL;
	int adc_sum = 0;
	u8 stmpe811_ch = ADC_CHANNEL_IN2;

	/*if (ch == REMOTE_SENSE)
		stmpe811_ch = ADC_CHANNEL_IN1;
	else if (ch == ADC_CHECK_1)
		stmpe811_ch = ADC_CHANNEL_IN2;
	else if (ch == ACCESSORY_ID)
		stmpe811_ch = ADC_CHANNEL_IN3;
	else if (ch == EAR_ADC_35)
		stmpe811_ch = ADC_CHANNEL_IN0;*/

	if (ch == ADC_CHECK_1) {
		/* HQRL Standard defines that time margin from Vbus5V detection
		 * to ADC_CHECK_1 voltage up should be more than 400ms.
		 */
		msleep(400);	/* delay for unstable cable connection */

		//espresso_gpio_set_for_adc_check_1();
        gpio_set_value(154, 0);
        gpio_set_value(60, 1);
		msleep(150);	/* delay for slow increase of line voltage */

		for (i = 0; i < 5; i++) {
			usleep_range(5000, 5500);
			adc_tmp = stmpe811_adc_get_value(stmpe811_ch);
			pr_info("adc_check_1 adc=%d\n", adc_tmp);
			adc_sum += adc_tmp;
			if (adc_max < adc_tmp)
				adc_max = adc_tmp;

			if (adc_min > adc_tmp)
				adc_min = adc_tmp;
		}
		//espresso_gpio_rel_for_adc_check_1();
		gpio_set_value(154, 1);
        gpio_set_value(60, 0);
		adc = (adc_sum - adc_max - adc_min) / 3;
	} else {
        printk("smb136 uh not ADC_CHECK_1");
		adc = stmpe811_adc_get_value(stmpe811_ch);
    }

	return adc;
}

int check_charger_type(void)
{
	int cable_type;
	short adc;

	if (gpio_is_valid(GPIO_TA_NCONNECTED) && gpio_get_value(GPIO_TA_NCONNECTED))
		return CABLE_TYPE_NONE;

	adc = omap4_espresso_get_adc(ADC_CHECK_1);
	cable_type = adc > 750 ? //cable_type = adc > CABLE_DETECT_VALUE ?
			CABLE_TYPE_AC :
			CABLE_TYPE_USB;

	pr_info("%s: Charger type is [%s], adc = %d\n",
		__func__,
		cable_type == CABLE_TYPE_AC ? "AC" : "USB",
		adc);

	return cable_type;
}

static irqreturn_t smb136_interrupt(int irq, void *smb_chip)
{
	struct smb136_charger *chip = smb_chip;
    struct power_supply *smb_usb;
	int val, reg, ret;

	u8 chg_status = 0;

	printk("%s\n", __func__);

	msleep(100);

	val = gpio_get_value(GPIO_TA_NCONNECTED);
	if (val < 0) {
		pr_err("usb ta_nconnected: gpio_get_value error %d\n", val);
		return IRQ_HANDLED;
	}

	reg = SMB136_STATUS_E;
	val = smb136_i2c_read(chip->client, reg);
	if (val >= 0) {
		chg_status = (u8)val;
		pr_info("%s : reg (0x%x) = 0x%x\n", __func__, reg, chg_status);
        smb136_start_charger(espresso_cable_type);
	}
	if(!val)
        espresso_cable_type = check_charger_type();

	/*if(chip->pdata->chg_intr_trigger)
		chip->pdata->chg_intr_trigger((int)(chg_status&0x1));*/

	/* clear IRQ */
	reg = 0x30;
	if (smb136_i2c_write(chip->client, reg, 0xff) < 0) {
		pr_err("%s : irq clear error!\n", __func__);
	}

	return IRQ_HANDLED;
}

static enum power_supply_property smb136_usb_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_USB_HC,
};

static const struct power_supply_desc smb136_mains_desc = {
	.name		= "smb136-mains",
	.type		= POWER_SUPPLY_TYPE_MAINS,
	.get_property	= smb136_mains_get_property,
	.set_property	= smb136_mains_set_property,
	.property_is_writeable	= smb136_mains_property_is_writeable,
	.properties	= smb136_mains_properties,
	.num_properties	= ARRAY_SIZE(smb136_mains_properties),
};

static const struct power_supply_desc smb136_usb_desc = {
	.name		= "smb136-usb",
	.type		= POWER_SUPPLY_TYPE_USB,
	.get_property	= smb136_usb_get_property,
	.set_property	= smb136_usb_set_property,
	.property_is_writeable	= smb136_usb_property_is_writeable,
	.properties	= smb136_usb_properties,
	.num_properties	= ARRAY_SIZE(smb136_usb_properties),
};

static int smb136_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	//static char *battery[] = { "max17042-battery" };
    struct power_supply_config mains_usb_cfg = {};
	struct device *dev = &client->dev;
	struct smb136_charger *smb;
	int ret, irq, val;

    printk("smb136: probe started");

	smb = devm_kzalloc(dev, sizeof(*smb), GFP_KERNEL);
	if (!smb)
		return -ENOMEM;

	smb->pdata = smb136_get_platdata(dev);
	if (!smb->pdata)
		return -ENODEV;

	i2c_set_clientdata(client, smb);

	smb->client = client;

    //mains_usb_cfg.supplied_to = battery;
    //mains_usb_cfg.num_supplicants = ARRAY_SIZE(battery);
    mains_usb_cfg.drv_data = smb;
    mains_usb_cfg.of_node = dev->of_node;

	smb->mains = devm_power_supply_register(dev, &smb136_mains_desc,
						   &mains_usb_cfg);

	if (IS_ERR(smb->mains))
		return PTR_ERR(smb->mains);

	smb->usb = devm_power_supply_register(dev, &smb136_usb_desc,
					 &mains_usb_cfg);
	if (IS_ERR(smb->usb))
		return PTR_ERR(smb->usb);

	ret = smb136_get_battery_info(smb);
	if (ret)
		return ret;

	irq = gpio_to_irq(GPIO_TA_NCONNECTED);
	val = gpio_get_value(GPIO_TA_NCONNECTED);

	ret = request_threaded_irq(client->irq, NULL, smb136_interrupt, IRQF_ONESHOT,
			"smb136", smb);
	if (ret < 0) {
		printk("request irq %d failed for gpio %d\n",
				irq, GPIO_TA_NCONNECTED);
	}

    ret = smb136_hw_init(smb);
	if (ret < 0)
		return ret;

	printk("smb136 probed\n");

	return 0;
}

static int smb136_remove(struct i2c_client *client)
{
	struct smb136_charger *smb = i2c_get_clientdata(client);
    //Doesnt work :(
	return 0;
}

static const struct i2c_device_id smb136_id[] = {
	{ "smb136", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, smb136_id);

static const struct of_device_id smb136_of_match[] = {
	{ .compatible = "summit,smb136" },
	{ },
};
MODULE_DEVICE_TABLE(of, smb136_of_match);

static struct i2c_driver smb136_i2c_driver = {
	.driver = {
		.name	= "smb136",
		.of_match_table = smb136_of_match,
	},
	.id_table	= smb136_id,
	.probe	= smb136_probe,
	.remove	= smb136_remove,
};

module_i2c_driver(smb136_i2c_driver);

MODULE_DESCRIPTION("SMB136 battery charger driver");
MODULE_LICENSE("GPL");
