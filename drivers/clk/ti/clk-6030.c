/*
* drivers/clk/ti/clk-6030.c
*
* Copyright (C) 2014 Stefan Assmann <sassmann-llIHtaV5axyzQB+***@public.gmane.org>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
* Clock driver for TI twl6030.
*
*/

#include <linux/module.h>
#include <linux/of.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/mfd/twl.h>
#include <linux/platform_device.h>

struct twl6030_desc {
        struct clk *clk;
        struct clk_hw hw;
};

static int twl6030_clk32kaudio_prepare(struct clk_hw *hw)
{
        int ret;
	printk("twl6030_clk32kaudio_prepare prepare began\n");
        ret = twl_i2c_write_u8(TWL_MODULE_PM_RECEIVER,
                        TWL6030_GRP_CON << TWL6030_CFG_STATE_GRP_SHIFT |
                        TWL6030_CFG_STATE_ON,
                        TWL6030_PM_RECEIVER_CLK32KAUDIO_CFG_STATE);
	printk("twl6030_clk32kaudio_prepare ended returned %d\n", ret);
        return ret;
}

void twl6030_clk32kaudio_unprepare(struct clk_hw *hw)
{
        twl_i2c_write_u8(TWL_MODULE_PM_RECEIVER,
                TWL6030_GRP_CON << TWL6030_CFG_STATE_GRP_SHIFT |
                TWL6030_CFG_STATE_OFF,
                TWL6030_PM_RECEIVER_CLK32KAUDIO_CFG_STATE);
	printk("twl6030_clk32kaudio_unprepare ended\n");
}

static int twl6030_clk32kaudio_is_prepared(struct clk_hw *hw)
{
        u8 is_prepared;

        twl_i2c_read_u8(TWL_MODULE_PM_RECEIVER, &is_prepared,
        TWL6030_PM_RECEIVER_CLK32KAUDIO_CFG_STATE);

	printk("twl6030_clk32kaudio_is_prepared ended returned %d\n",is_prepared & TWL6030_CFG_STATE_ON);
        return is_prepared & TWL6030_CFG_STATE_ON;
}

static unsigned long twl6030_clk32kaudio_recalc_rate(struct clk_hw *hw,
						unsigned long parent_rate)
{
	printk("twl6030_clk32kaudio_recalc_rate ended\n");
	return 32768;
}

static const struct clk_ops twl6030_clk32kaudio_ops = {
        .prepare = twl6030_clk32kaudio_prepare,
        .unprepare = twl6030_clk32kaudio_unprepare,
        .is_prepared = twl6030_clk32kaudio_is_prepared,
	.recalc_rate = twl6030_clk32kaudio_recalc_rate,
};

static void __init of_ti_twl6030_clk32kaudio_setup(struct device_node *node)
{
        struct twl6030_desc *clk_hw = NULL;
        struct clk_init_data init = { 0 };
        struct clk_lookup *clookup;
        struct clk *clk;

	printk("of_ti_twl6030_clk32kaudio_setup started\n");

        clookup = kzalloc(sizeof(*clookup), GFP_KERNEL);
        if (!clookup) {
                pr_err("%s: could not allocate clookup\n", __func__);
                return;
        }

	printk("of_ti_twl6030_clk32kaudio_setup clookup is good\n");

        clk_hw = kzalloc(sizeof(*clk_hw), GFP_KERNEL);

        if (!clk_hw) {
                pr_err("%s: could not allocate clk_hw\n", __func__);
                goto err_clk_hw;
        }

	printk("of_ti_twl6030_clk32kaudio_setup clkhw is good\n");

        clk_hw->hw.init = &init;

	printk("of_ti_twl6030_clk32kaudio_setup init o/\n");

        init.name = node->name;
        init.ops = &twl6030_clk32kaudio_ops;

	printk("of_ti_twl6030_clk32kaudio_setup more init o/\n");

        clk = clk_register(NULL, &clk_hw->hw);
        if (!IS_ERR(clk)) {
                clookup->con_id = kstrdup("clk32kaudio", GFP_KERNEL);
		printk("of_ti_twl6030_clk32kaudio_setup step 1\n");
                clookup->clk = clk;
		printk("of_ti_twl6030_clk32kaudio_setup step 2\n");
                clkdev_add(clookup);
		printk("of_ti_twl6030_clk32kaudio_setup step 3\n");

                return;
        }
	printk("of_ti_twl6030_clk32kaudio_setup failed?\n");

        kfree(clookup);
err_clk_hw:
        kfree(clk_hw);
}
CLK_OF_DECLARE(of_ti_twl6030_clk32kaudio, "ti,twl6030-clk32kaudio", of_ti_twl6030_clk32kaudio_setup);

static int of_twl6030_clk32kaudio_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct clk *clk;

	printk("clk32kaudio probe started\n");
	if (!node)
	{
		printk("clk32kaudio No node\n");
 		return -ENODEV;
	}

	clk = devm_clk_get(&pdev->dev, "clk32kaudio");
	if (IS_ERR(clk))
	{
		printk("clk32kaudio clk error %ld\n",PTR_ERR(clk));
		return PTR_ERR(clk);
	}

	return clk_prepare(clk);
}

static struct of_device_id of_twl6030_clk32kaudio_match_tbl[] = {
	{ .compatible = "ti,twl6030-clk32kaudio", },
	{},
};
MODULE_DEVICE_TABLE(of, of_twl6030_clk32kaudio_match_tbl);

static struct platform_driver twl6030_clk_driver = {
	.driver = {
		.name = "twl6030-clk32kaudio",
		.owner = THIS_MODULE,
		.of_match_table = of_twl6030_clk32kaudio_match_tbl,
	},
	.probe = of_twl6030_clk32kaudio_probe,
};
module_platform_driver(twl6030_clk_driver);

MODULE_AUTHOR("Stefan Assmann <sassmann-llIHtaV5axyzQB+***@public.gmane.org>");
MODULE_DESCRIPTION("clock driver for TI SoC based boards with twl6030");
MODULE_LICENSE("GPL v2");
