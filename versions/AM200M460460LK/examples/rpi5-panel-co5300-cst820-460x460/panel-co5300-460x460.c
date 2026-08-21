/*
 * CO5300 460x460 1-Lane MIPI-DSI Panel Driver for Raspberry Pi 5
 * Adapted for 2.0-inch rectangular AMOLED (Chipone CO5300)
 *
 */

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <linux/backlight.h>
#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <video/mipi_display.h>

struct power_on_timing {
	unsigned long post_reset;
	unsigned long reset_low;
	unsigned long after_reset;
	unsigned long slpout;
};

struct co5300_desc {
	const struct drm_display_mode *mode;
	unsigned int lanes;
	unsigned long flags;
	enum mipi_dsi_pixel_format format;
	int (*init_sequence)(struct mipi_dsi_device *dsi);
	const struct power_on_timing *pwr_timing;
	bool do_sw_reset;
};

struct co5300 {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	const struct co5300_desc *desc;
	struct gpio_desc *reset;
	enum drm_panel_orientation orientation;
};

static inline struct co5300 *to_co5300(struct drm_panel *panel)
{
	return container_of(panel, struct co5300, panel);
}

/* ==================== CO5300 初始化序列 ==================== */
static int co5300_460x460_init_sequence(struct mipi_dsi_device *dsi)
{
	struct mipi_dsi_multi_context ctx = { .dsi = dsi };

	/* Optional: keep 1-lane setting (safe even if already default) */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xFE, 0x80);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0x03, 0x00);	/* 1-lane */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xFE, 0x00);

	/* Your sequence */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xFE, 0x00);

	/* 0x35 TE is commented out in your code → skipped */

	mipi_dsi_dcs_write_seq_multi(&ctx, 0x3A, 0x77);	/* RGB888 */

	mipi_dsi_dcs_write_seq_multi(&ctx, 0x53, 0x20);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0x51, 0xFF);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0x63, 0xFF);

	/* CASET: start=0x000A, end=0x01D5  → 460 columns with offset 10 */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0x2A, 0x00, 0x0A, 0x01, 0xD5);

	/* PASET: start=0x0000, end=0x01CB  → 460 rows */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0x2B, 0x00, 0x00, 0x01, 0xCB);

	/* Sleep Out + 120 ms */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0x11);
	msleep(120);

	/* Display ON */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0x29);

	return ctx.accum_err;
}
/* ======================================================= */

static int co5300_prepare(struct drm_panel *panel)
{
	struct co5300 *co5300 = to_co5300(panel);
	struct mipi_dsi_multi_context ctx = { .dsi = co5300->dsi };

	if (co5300->reset) {
		gpiod_set_value_cansleep(co5300->reset, 1);
		msleep(co5300->desc->pwr_timing->post_reset);
		gpiod_set_value_cansleep(co5300->reset, 0);
		msleep(co5300->desc->pwr_timing->reset_low);
		gpiod_set_value_cansleep(co5300->reset, 1);
		msleep(co5300->desc->pwr_timing->after_reset);
	}

	if (co5300->desc->do_sw_reset) {
		mipi_dsi_dcs_soft_reset_multi(&ctx);
		msleep(co5300->desc->pwr_timing->after_reset);
	}

	if (co5300->desc->init_sequence) {
		int ret = co5300->desc->init_sequence(co5300->dsi);
		if (ret)
			return ret;
	} else {
		/* 仅在没有自定义 init 序列时才发 Sleep Out */
		mipi_dsi_dcs_exit_sleep_mode_multi(&ctx);
		msleep(co5300->desc->pwr_timing->slpout);
	}

	return ctx.accum_err;
}

static int co5300_enable(struct drm_panel *panel)
{
	struct mipi_dsi_multi_context ctx = { .dsi = to_mipi_dsi_device(panel->dev) };
	mipi_dsi_dcs_set_display_on_multi(&ctx);
	return ctx.accum_err;
}

static int co5300_disable(struct drm_panel *panel)
{
	struct mipi_dsi_multi_context ctx = { .dsi = to_mipi_dsi_device(panel->dev) };
	mipi_dsi_dcs_set_display_off_multi(&ctx);
	return ctx.accum_err;
}

static int co5300_unprepare(struct drm_panel *panel)
{
	struct co5300 *co5300 = to_co5300(panel);
	struct mipi_dsi_multi_context ctx = { .dsi = co5300->dsi };

	mipi_dsi_dcs_enter_sleep_mode_multi(&ctx);
	if (co5300->reset)
		gpiod_set_value_cansleep(co5300->reset, 0);

	return ctx.accum_err;
}

static int co5300_get_modes(struct drm_panel *panel, struct drm_connector *connector)
{
	struct co5300 *co5300 = to_co5300(panel);
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, co5300->desc->mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;

	drm_connector_set_orientation_from_panel(connector, panel);
	return 1;
}

static enum drm_panel_orientation co5300_get_orientation(struct drm_panel *panel)
{
	return to_co5300(panel)->orientation;
}

static const struct drm_panel_funcs co5300_funcs = {
	.prepare = co5300_prepare,
	.enable = co5300_enable,
	.disable = co5300_disable,
	.unprepare = co5300_unprepare,
	.get_modes = co5300_get_modes,
	.get_orientation = co5300_get_orientation,
};

static const struct drm_display_mode co5300_mode = {
	.clock = 16000,			/* 16 MHz */

	.hdisplay = 460,
	.hsync_start = 460 + 32,		/* + front porch */
	.hsync_end   = 460 + 32 + 4,		/* + pulse width */
	.htotal      = 460 + 32 + 4 + 72,	/* + back porch = 568 */

	.vdisplay = 460,
	.vsync_start = 460 + 6,			/* + front porch */
	.vsync_end   = 460 + 6 + 2,		/* + pulse width */
	.vtotal      = 460 + 6 + 2 + 4,		/* + back porch = 472 */

	.width_mm = 51,
	.height_mm = 51,

	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

static const struct power_on_timing co5300_pwr_timing = {
	.post_reset = 20,
	.reset_low = 20,
	.after_reset = 120,
	.slpout = 400,
};

static const struct co5300_desc co5300_desc = {
	.mode = &co5300_mode,
	.lanes = 1,					/* 单通道 */
	.flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST | MIPI_DSI_MODE_LPM,
	.format = MIPI_DSI_FMT_RGB888,
	.init_sequence = co5300_460x460_init_sequence,
	.pwr_timing = &co5300_pwr_timing,
	.do_sw_reset = true,
};

static int co5300_probe(struct mipi_dsi_device *dsi)
{
	struct co5300 *co5300;
	const struct co5300_desc *desc;
	int ret;

	co5300 = devm_kzalloc(&dsi->dev, sizeof(*co5300), GFP_KERNEL);
	if (!co5300)
		return -ENOMEM;

	desc = of_device_get_match_data(&dsi->dev);
	dsi->mode_flags = desc->flags;
	dsi->format = desc->format;
	dsi->lanes = desc->lanes;

	co5300->panel.prepare_prev_first = true;
	co5300->reset = devm_gpiod_get_optional(&dsi->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(co5300->reset)) {
		dev_err(&dsi->dev, "Failed to get reset GPIO\n");
		return PTR_ERR(co5300->reset);
	}

	ret = of_drm_get_panel_orientation(dsi->dev.of_node, &co5300->orientation);
	if (ret < 0)
		co5300->orientation = DRM_MODE_PANEL_ORIENTATION_NORMAL;

	drm_panel_init(&co5300->panel, &dsi->dev, &co5300_funcs, DRM_MODE_CONNECTOR_DSI);

	ret = drm_panel_of_backlight(&co5300->panel);
	if (ret)
		return ret;

	drm_panel_add(&co5300->panel);

	mipi_dsi_set_drvdata(dsi, co5300);
	co5300->dsi = dsi;
	co5300->desc = desc;

	ret = mipi_dsi_attach(dsi);
	if (ret)
		drm_panel_remove(&co5300->panel);

	return ret;
}

static void co5300_remove(struct mipi_dsi_device *dsi)
{
	struct co5300 *co5300 = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&co5300->panel);
}

static const struct of_device_id co5300_of_match[] = {
	{ .compatible = "chipone,co5300-460x460", .data = &co5300_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, co5300_of_match);

static struct mipi_dsi_driver co5300_driver = {
	.probe = co5300_probe,
	.remove = co5300_remove,
	.driver = {
		.name = "panel-co5300-460x460",
		.of_match_table = co5300_of_match,
	},
};
module_mipi_dsi_driver(co5300_driver);

MODULE_AUTHOR("Adapted for CO5300 2.0-inch 460x460");
MODULE_DESCRIPTION("Chipone CO5300 460x460 1-Lane MIPI-DSI Panel Driver");
MODULE_LICENSE("GPL");
