// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Driver for I2C connected Hynitron CST820 Touchscreen
 *
 * Resolution configurable via DT (default 460x460)
 * Supports optional reset-gpios and interrupt (IRQ).
 * When IRQ is not present, falls back to polling.
 *
 * This example is from the open-source sharing by engineers of Yuying Optoelectronics (鱼鹰光电)
 * on Github.com/osptek. Welcome to provide improvement suggestions.
 *
 * 本例程来源于鱼鹰光电的工程师的开源分享 Github.com/osptek，欢迎提出改进意见
 */
 
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio/consumer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/touchscreen.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/workqueue.h>

#define CST820_DATA_REG		0x02
#define CST820_CHIP_ID_REG	0xA7
#define CST820_FW_VER_REG	0xA9

#define CST820_MAX_POINTS	2

#define CST820_DEFAULT_MAX_X	460
#define CST820_DEFAULT_MAX_Y	460

#define CST820_POLL_INTERVAL_MS	20

/* Touch data layout starting at register 0x02
 * byte0: point number
 * byte1: X[11:8] in low 4 bits
 * byte2: X[7:0]
 * byte3: Y[11:8] in low 4 bits
 * byte4: Y[7:0]
 */
struct cst820_point_data {
	u8 num;
	u8 x_h;
	u8 x_l;
	u8 y_h;
	u8 y_l;
} __packed;

struct cst820_priv {
	struct i2c_client *client;
	struct gpio_desc *reset;
	struct input_dev *input;
	struct delayed_work poll_work;
	struct touchscreen_properties prop;
	bool use_polling;
};

static int cst820_i2c_read(struct cst820_priv *priv, u8 reg,
			   void *buf, size_t len)
{
	struct i2c_msg xfer[] = {
		{
			.addr = priv->client->addr,
			.flags = 0,
			.buf = &reg,
			.len = 1,
		},
		{
			.addr = priv->client->addr,
			.flags = I2C_M_RD,
			.buf = buf,
			.len = len,
		},
	};
	int ret;

	ret = i2c_transfer(priv->client->adapter, xfer, ARRAY_SIZE(xfer));
	if (ret != ARRAY_SIZE(xfer)) {
		ret = ret < 0 ? ret : -EIO;
		dev_err_ratelimited(&priv->client->dev, "i2c read err: %d\n", ret);
		return ret;
	}

	return 0;
}

static void cst820_report(struct cst820_priv *priv, u16 x, u16 y, bool pressed)
{
	if (pressed) {
		touchscreen_report_pos(priv->input, &priv->prop, x, y, false);
		input_report_key(priv->input, BTN_TOUCH, 1);
	} else {
		input_report_key(priv->input, BTN_TOUCH, 0);
	}
	input_sync(priv->input);
}

static void cst820_handle_touch(struct cst820_priv *priv)
{
	struct cst820_point_data data;
	u16 x, y;
	u8 num;
	int ret;

	ret = cst820_i2c_read(priv, CST820_DATA_REG, &data, sizeof(data));
	if (ret)
		return;

	num = data.num;
	if (num > CST820_MAX_POINTS)
		num = CST820_MAX_POINTS;

	if (num == 0) {
		cst820_report(priv, 0, 0, false);
		return;
	}

	x = ((data.x_h & 0x0F) << 8) | data.x_l;
	y = ((data.y_h & 0x0F) << 8) | data.y_l;

	dev_dbg(&priv->client->dev, "points=%u x=%u y=%u\n", num, x, y);

	cst820_report(priv, x, y, true);
}

static irqreturn_t cst820_irq_handler(int irq, void *cookie)
{
	struct cst820_priv *priv = cookie;

	cst820_handle_touch(priv);
	return IRQ_HANDLED;
}

static void cst820_poll_work(struct work_struct *work)
{
	struct cst820_priv *priv = container_of(work, struct cst820_priv,
						poll_work.work);

	cst820_handle_touch(priv);
	schedule_delayed_work(&priv->poll_work,
			      msecs_to_jiffies(CST820_POLL_INTERVAL_MS));
}

static void cst820_reset(struct cst820_priv *priv)
{
	if (!priv->reset)
		return;

	gpiod_set_value_cansleep(priv->reset, 1);
	msleep(200);
	gpiod_set_value_cansleep(priv->reset, 0);
	msleep(200);
}

static int cst820_check_chip(struct cst820_priv *priv)
{
	u8 chip_id = 0;
	u8 fw_ver = 0;
	int ret;

	ret = cst820_i2c_read(priv, CST820_CHIP_ID_REG, &chip_id, 1);
	if (ret)
		return ret;

	ret = cst820_i2c_read(priv, CST820_FW_VER_REG, &fw_ver, 1);
	if (ret)
		return ret;

	dev_info(&priv->client->dev, "CST820 chip id: 0x%02x, fw: 0x%02x\n",
		 chip_id, fw_ver);

	if (chip_id != 0xB7)
		dev_warn(&priv->client->dev,
			 "unexpected chip id 0x%02x (expected 0xB7)\n", chip_id);

	return 0;
}

static int cst820_register_input(struct cst820_priv *priv)
{
	struct device *dev = &priv->client->dev;
	u32 max_x = CST820_DEFAULT_MAX_X;
	u32 max_y = CST820_DEFAULT_MAX_Y;

	priv->input = devm_input_allocate_device(dev);
	if (!priv->input)
		return -ENOMEM;

	priv->input->name = "Hynitron CST820 Touchscreen";
	priv->input->phys = "input/ts";
	priv->input->id.bustype = BUS_I2C;

	input_set_drvdata(priv->input, priv);

	device_property_read_u32(dev, "touchscreen-size-x", &max_x);
	device_property_read_u32(dev, "touchscreen-size-y", &max_y);

	input_set_abs_params(priv->input, ABS_X, 0, max_x, 0, 0);
	input_set_abs_params(priv->input, ABS_Y, 0, max_y, 0, 0);
	input_set_capability(priv->input, EV_KEY, BTN_TOUCH);

	touchscreen_parse_properties(priv->input, false, &priv->prop);

	dev_info(dev, "touchscreen size: %u x %u\n", max_x, max_y);

	return input_register_device(priv->input);
}

static int cst820_probe(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct cst820_priv *priv;
	int error;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->client = client;
	i2c_set_clientdata(client, priv);

	priv->reset = devm_gpiod_get_optional(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(priv->reset))
		return dev_err_probe(dev, PTR_ERR(priv->reset),
				     "failed to get reset gpio\n");

	if (priv->reset) {
		cst820_reset(priv);
		dev_info(dev, "using reset gpio\n");
	} else {
		dev_info(dev, "no reset gpio, skip hardware reset\n");
	}

	error = cst820_check_chip(priv);
	if (error)
		dev_warn(dev, "chip check failed: %d (continue anyway)\n", error);

	error = cst820_register_input(priv);
	if (error)
		return dev_err_probe(dev, error, "input register failed\n");

	if (client->irq > 0) {
		error = devm_request_threaded_irq(dev, client->irq,
						  NULL, cst820_irq_handler,
						  IRQF_ONESHOT,
						  dev_driver_string(dev), priv);
		if (error) {
			dev_warn(dev, "irq request failed (%d), fallback to polling\n",
				 error);
			priv->use_polling = true;
		} else {
			dev_info(dev, "using interrupt mode (irq %d)\n", client->irq);
		}
	} else {
		dev_info(dev, "no irq specified, using polling mode\n");
		priv->use_polling = true;
	}

	if (priv->use_polling) {
		INIT_DELAYED_WORK(&priv->poll_work, cst820_poll_work);
		schedule_delayed_work(&priv->poll_work,
				      msecs_to_jiffies(CST820_POLL_INTERVAL_MS));
	}

	return 0;
}

static void cst820_remove(struct i2c_client *client)
{
	struct cst820_priv *priv = i2c_get_clientdata(client);

	if (priv->use_polling)
		cancel_delayed_work_sync(&priv->poll_work);
}

static const struct i2c_device_id cst820_id[] = {
	{ .name = "cst820", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, cst820_id);

static const struct of_device_id cst820_of_match[] = {
	{ .compatible = "hynitron,cst820" },
	{ }
};
MODULE_DEVICE_TABLE(of, cst820_of_match);

static struct i2c_driver cst820_driver = {
	.driver = {
		.name = "hynitron-cst820",
		.of_match_table = cst820_of_match,
	},
	.id_table = cst820_id,
	.probe = cst820_probe,
	.remove = cst820_remove,
};

module_i2c_driver(cst820_driver);

MODULE_DESCRIPTION("Hynitron CST820 Touchscreen Driver");
MODULE_LICENSE("GPL");
