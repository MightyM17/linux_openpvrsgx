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
	printk("prepare began");
        ret = twl_i2c_write_u8(TWL_MODULE_PM_RECEIVER,
                        TWL6030_GRP_CON << TWL6030_CFG_STATE_GRP_SHIFT |
                        TWL6030_CFG_STATE_ON,
                        TWL6030_PM_RECEIVER_CLK32KAUDIO_CFG_STATE);

        return ret;
	printk("prepare ended");
	return true;
}

void twl6030_clk32kaudio_unprepare(struct clk_hw *hw)
{
        twl_i2c_write_u8(TWL_MODULE_PM_RECEIVER,
                TWL6030_GRP_CON << TWL6030_CFG_STATE_GRP_SHIFT |
                TWL6030_CFG_STATE_OFF,
                TWL6030_PM_RECEIVER_CLK32KAUDIO_CFG_STATE);
	printk("unprepare ended");
}

static int twl6030_clk32kaudio_is_prepared(struct clk_hw *hw)
{
        u8 is_prepared;

        twl_i2c_read_u8(TWL_MODULE_PM_RECEIVER, &is_prepared,
        TWL6030_PM_RECEIVER_CLK32KAUDIO_CFG_STATE);

        return is_prepared & TWL6030_CFG_STATE_ON;
	printk("isprepare ended");
	return true;
}

static const struct clk_ops twl6030_clk32kaudio_ops = {
        .prepare = twl6030_clk32kaudio_prepare,
        .unprepare = twl6030_clk32kaudio_unprepare,
        .is_prepared = twl6030_clk32kaudio_is_prepared,
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

MODULE_AUTHOR("Stefan Assmann <sassmann-llIHtaV5axyzQB+***@public.gmane.org>");
MODULE_DESCRIPTION("clock driver for TI SoC based boards with twl6030");
MODULE_LICENSE("GPL v2");
