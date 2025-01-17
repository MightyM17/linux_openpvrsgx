/**
 * twl4030-pwrbutton.c - TWL4030 Power Button Input Driver
 *
 * Copyright (C) 2008-2009 Nokia Corporation
 *
 * Written by Peter De Schrijver <peter.de-schrijver@nokia.com>
 * Several fixes by Felipe Balbi <felipe.balbi@nokia.com>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mfd/twl.h>

#define PWR_PWRON_IRQ (1 << 0)

#define TWL4030_STS_HW_CONDITIONS 0x0f
#define TWL6030_STS_HW_CONDITIONS 0x21

static irqreturn_t powerbutton_irq(int irq, void *_pwr)
{
	struct input_dev *pwr = _pwr;
	int err;
	u8 value;
	
	if (twl_class_is_4030())
		err = twl_i2c_read_u8(TWL_MODULE_PM_MASTER, &value,
				TWL4030_STS_HW_CONDITIONS);
	else
		err = twl_i2c_read_u8(TWL6030_MODULE_ID0, &value,
				TWL6030_STS_HW_CONDITIONS);

	if (!err)  {
		pm_wakeup_event(pwr->dev.parent, 0);
		input_report_key(pwr, KEY_POWER, value & PWR_PWRON_IRQ);
		input_sync(pwr);
	} else {
		dev_err(pwr->dev.parent, "twl4030: i2c error %d while reading"
			" TWL4030 PM_MASTER STS_HW_CONDITIONS register\n", err);
	}

	return IRQ_HANDLED;
}

static int twl_pwrbutton_probe(struct platform_device *pdev)
{
	struct input_dev *pwr;
	int irq = platform_get_irq(pdev, 0);
	int err;

	pwr = devm_input_allocate_device(&pdev->dev);
	if (!pwr) {
		dev_err(&pdev->dev, "Can't allocate power button\n");
		return -ENOMEM;
	}


	pwr->evbit[0] = BIT_MASK(EV_KEY);
	pwr->keybit[BIT_WORD(KEY_POWER)] = BIT_MASK(KEY_POWER);
	pwr->name = "twl_pwrbutton";
	pwr->phys = "twl_pwrbutton/input0";
	pwr->dev.parent = &pdev->dev;

	if (twl_class_is_6030()) {
		twl6030_interrupt_unmask(TWL6030_PWRON_INT_MASK,
			REG_INT_MSK_LINE_A);
		twl6030_interrupt_unmask(TWL6030_PWRON_INT_MASK,
			REG_INT_MSK_STS_A);
	}

 	err = devm_request_threaded_irq(&pwr->dev, irq, NULL, powerbutton_irq,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING |
			IRQF_ONESHOT,
			"twl_pwrbutton", pwr);
	if (err < 0) {
		dev_err(&pdev->dev, "Can't get IRQ for pwrbutton: %d\n", err);
		return err;
	}

	err = input_register_device(pwr);
	if (err) {
		dev_err(&pdev->dev, "Can't register power button: %d\n", err);
		return err;
	}

 	platform_set_drvdata(pdev, pwr);
	device_init_wakeup(&pdev->dev, true);

	return 0;
}

static int twl_pwrbutton_remove(struct platform_device *pdev)
{
	if (twl_class_is_6030()) {
		twl6030_interrupt_mask(TWL6030_PWRON_INT_MASK,
			REG_INT_MSK_LINE_A);
		twl6030_interrupt_mask(TWL6030_PWRON_INT_MASK,
			REG_INT_MSK_STS_A);
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id twl_pwrbutton_dt_match_table[] = {
       { .compatible = "ti,twl4030-pwrbutton" },
       { .compatible = "ti,twl6030-pwrbutton" },
       {},
};
MODULE_DEVICE_TABLE(of, twl_pwrbutton_dt_match_table);
#endif

static struct platform_driver twl_pwrbutton_driver = {
	.probe		= twl_pwrbutton_probe,
 	.remove		= twl_pwrbutton_remove,
	.driver		= {
		.name	= "twl_pwrbutton",
		.of_match_table = of_match_ptr(twl_pwrbutton_dt_match_table),
	},
};
module_platform_driver(twl_pwrbutton_driver);

MODULE_ALIAS("platform:twl_pwrbutton");
MODULE_DESCRIPTION("Twl Power Button");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Peter De Schrijver <peter.de-schrijver@nokia.com>");
MODULE_AUTHOR("Felipe Balbi <felipe.balbi@nokia.com>");

