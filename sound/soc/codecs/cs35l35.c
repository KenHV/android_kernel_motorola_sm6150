/*
 * cs35l35.c -- CS35L35 ALSA SoC audio driver
 *
 * Copyright 2016 Cirrus Logic, Inc.
 *
 * Author: Brian Austin <brian.austin@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio/consumer.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/regmap.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <linux/gpio.h>
#include <sound/initval.h>
#include <sound/tlv.h>
#include <sound/cs35l35.h>
#include <linux/of_irq.h>
#include <linux/completion.h>

#include "cs35l35.h"

static const struct reg_default cs35l35_reg[] = {
	{CS35L35_PWRCTL1,		0x01},
	{CS35L35_PWRCTL2,		0x11},
	{CS35L35_PWRCTL3,		0x00},
	{CS35L35_CLK_CTL1,		0x04},
	{CS35L35_CLK_CTL2,		0x10},
	{CS35L35_CLK_CTL3,		0xCF},
	{CS35L35_SP_FMT_CTL1,		0x20},
	{CS35L35_SP_FMT_CTL2,		0x00},
	{CS35L35_SP_FMT_CTL3,		0x02},
	{CS35L35_MAG_COMP_CTL,		0x00},
	{CS35L35_AMP_INP_DRV_CTL,	0x01},
	{CS35L35_AMP_DIG_VOL_CTL,	0x12},
	{CS35L35_AMP_DIG_VOL,		0x00},
	{CS35L35_ADV_DIG_VOL,		0x00},
	{CS35L35_PROTECT_CTL,		0x06},
	{CS35L35_AMP_GAIN_AUD_CTL,	0x13},
	{CS35L35_AMP_GAIN_PDM_CTL,	0x00},
	{CS35L35_AMP_GAIN_ADV_CTL,	0x00},
	{CS35L35_GPI_CTL,		0x00},
	{CS35L35_BST_CVTR_V_CTL,	0x00},
	{CS35L35_BST_PEAK_I,		0x07},
	{CS35L35_BST_RAMP_CTL,		0x85},
	{CS35L35_BST_CONV_COEF_1,	0x20},
	{CS35L35_BST_CONV_COEF_2,	0x20},
	{CS35L35_BST_CONV_SLOPE_COMP,	0x47},
	{CS35L35_BST_CONV_SW_FREQ,	0x04},
	{CS35L35_CLASS_H_CTL,		0x0B},
	{CS35L35_CLASS_H_HEADRM_CTL,	0x0B},
	{CS35L35_CLASS_H_RELEASE_RATE,	0x08},
	{CS35L35_CLASS_H_FET_DRIVE_CTL, 0x41},
	{CS35L35_CLASS_H_VP_CTL,	0xC5},
	{CS35L35_VPBR_CTL,		0x0A},
	{CS35L35_VPBR_VOL_CTL,		0x09},
	{CS35L35_VPBR_TIMING_CTL,	0x6A},
	{CS35L35_VPBR_MODE_VOL_CTL,	0x00},
	{CS35L35_SPKR_MON_CTL,		0xC0},
	{CS35L35_IMON_SCALE_CTL,	0x30},
	{CS35L35_AUDIN_RXLOC_CTL,	0x00},
	{CS35L35_ADVIN_RXLOC_CTL,	0x80},
	{CS35L35_VMON_TXLOC_CTL,	0x00},
	{CS35L35_IMON_TXLOC_CTL,	0x80},
	{CS35L35_VPMON_TXLOC_CTL,	0x04},
	{CS35L35_VBSTMON_TXLOC_CTL,	0x84},
	{CS35L35_VPBR_STATUS_TXLOC_CTL,	0x04},
	{CS35L35_ZERO_FILL_LOC_CTL,	0x00},
	{CS35L35_AUDIN_DEPTH_CTL,	0x0F},
	{CS35L35_SPKMON_DEPTH_CTL,	0x0F},
	{CS35L35_SUPMON_DEPTH_CTL,	0x0F},
	{CS35L35_ZEROFILL_DEPTH_CTL,	0x00},
	{CS35L35_MULT_DEV_SYNCH1,	0x02},
	{CS35L35_MULT_DEV_SYNCH2,	0x80},
	{CS35L35_PROT_RELEASE_CTL,	0x00},
	{CS35L35_DIAG_MODE_REG_LOCK,	0x00},
	{CS35L35_DIAG_MODE_CTL_1,	0x40},
	{CS35L35_DIAG_MODE_CTL_2,	0x00},
	{CS35L35_INT_MASK_1,		0xFF},
	{CS35L35_INT_MASK_2,		0xFF},
	{CS35L35_INT_MASK_3,		0xFF},
	{CS35L35_INT_MASK_4,		0xFF},

};

static bool cs35l35_volatile_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS35L35_DEVID_AB ... CS35L35_REV_ID:
	case CS35L35_INT_STATUS_1:
	case CS35L35_INT_STATUS_2:
	case CS35L35_INT_STATUS_3:
	case CS35L35_INT_STATUS_4:
	case CS35L35_PLL_STATUS:
	case CS35L35_OTP_TRIM_STATUS:
		return true;
	default:
		return false;
	}
}

static bool cs35l35_readable_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS35L35_DEVID_AB ... CS35L35_PWRCTL3:
	case CS35L35_CLK_CTL1 ... CS35L35_SP_FMT_CTL3:
	case CS35L35_MAG_COMP_CTL ... CS35L35_AMP_GAIN_AUD_CTL:
	case CS35L35_AMP_GAIN_PDM_CTL ... CS35L35_BST_PEAK_I:
	case CS35L35_BST_RAMP_CTL ... CS35L35_BST_CONV_SW_FREQ:
	case CS35L35_CLASS_H_CTL ... CS35L35_CLASS_H_VP_CTL:
	case CS35L35_CLASS_H_STATUS:
	case CS35L35_VPBR_CTL ... CS35L35_VPBR_MODE_VOL_CTL:
	case CS35L35_VPBR_ATTEN_STATUS:
	case CS35L35_SPKR_MON_CTL:
	case CS35L35_IMON_SCALE_CTL ... CS35L35_ZEROFILL_DEPTH_CTL:
	case CS35L35_MULT_DEV_SYNCH1 ... CS35L35_PROT_RELEASE_CTL:
	case CS35L35_DIAG_MODE_REG_LOCK ... CS35L35_DIAG_MODE_CTL_2:
	case CS35L35_INT_MASK_1 ... CS35L35_PLL_STATUS:
	case CS35L35_OTP_TRIM_STATUS:
		return true;
	default:
		return false;
	}
}

static bool cs35l35_precious_register(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case CS35L35_INT_STATUS_1:
	case CS35L35_INT_STATUS_2:
	case CS35L35_INT_STATUS_3:
	case CS35L35_INT_STATUS_4:
	case CS35L35_PLL_STATUS:
	case CS35L35_OTP_TRIM_STATUS:
		return true;
	default:
		return false;
	}
}

static int cs35l35_main_amp_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct cs35l35_private *cs35l35 = snd_soc_codec_get_drvdata(codec);
	unsigned int reg[4];
	int i;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		regmap_update_bits(cs35l35->regmap, CS35L35_BST_CVTR_V_CTL,
			CS35L35_BST_CTL_MASK, 0x41);
		break;
	case SND_SOC_DAPM_POST_PMU:
		usleep_range(5000, 5100);
		/* If PDM mode we must use VP
		 * for Voltage control
		 */
		if (cs35l35->pdm_mode)
			regmap_update_bits(cs35l35->regmap,
					CS35L35_BST_CVTR_V_CTL,
					CS35L35_BST_CTL_MASK,
					0 << CS35L35_BST_CTL_SHIFT);
		for (i = 0; i < 2; i++)
			regmap_bulk_read(cs35l35->regmap, CS35L35_INT_STATUS_1,
					&reg, ARRAY_SIZE(reg));

		regmap_update_bits(cs35l35->regmap, CS35L35_PROTECT_CTL,
			CS35L35_AMP_MUTE_MASK,
			0 << CS35L35_AMP_MUTE_SHIFT);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		regmap_update_bits(cs35l35->regmap, CS35L35_PROTECT_CTL,
			CS35L35_AMP_MUTE_MASK, 1 << CS35L35_AMP_MUTE_SHIFT);
		regmap_update_bits(cs35l35->regmap, CS35L35_BST_CVTR_V_CTL,
			CS35L35_BST_CTL_MASK, 0x00);
		break;
	case SND_SOC_DAPM_POST_PMD:
		usleep_range(5000, 5100);
		/* If PDM mode we should switch back to pdata value
		 * for Voltage control when we go down
		 */
		if (cs35l35->pdm_mode)
			regmap_update_bits(cs35l35->regmap,
					CS35L35_BST_CVTR_V_CTL,
					CS35L35_BST_CTL_MASK,
					cs35l35->pdata.bst_vctl
					<< CS35L35_BST_CTL_SHIFT);

		break;
	default:
		pr_err("Invalid event = 0x%x\n", event);
	}
	return 0;
}

static DECLARE_TLV_DB_SCALE(amp_gain_tlv, 0, 1, 1);
static DECLARE_TLV_DB_SCALE(dig_vol_tlv, -10200, 50, 0);

static const struct snd_kcontrol_new cs35l35_aud_controls[] = {
	SOC_SINGLE_SX_TLV("Digital Audio Volume", CS35L35_AMP_DIG_VOL,
		      0, 0x34, 0xE4, dig_vol_tlv),
	SOC_SINGLE_TLV("AMP Audio Gain", CS35L35_AMP_GAIN_AUD_CTL, 0, 19, 0,
			amp_gain_tlv),
	SOC_SINGLE_TLV("AMP PDM Gain", CS35L35_AMP_GAIN_PDM_CTL, 0, 19, 0,
			amp_gain_tlv),
};

static const struct snd_kcontrol_new cs35l35_adv_controls[] = {
	SOC_SINGLE_SX_TLV("Digital Advisory Volume", CS35L35_ADV_DIG_VOL,
		      0, 0x34, 0xE4, dig_vol_tlv),
	SOC_SINGLE_TLV("AMP Advisory Gain", CS35L35_AMP_GAIN_ADV_CTL, 0, 19, 0,
			amp_gain_tlv),
};

static const char * const cs35l35_clksel_text[] = {
	"MCLK",
	"PDM",
};

static SOC_ENUM_SINGLE_DECL(clksel_enum, SND_SOC_NOPM, 0,
		cs35l35_clksel_text);

static const struct snd_kcontrol_new cs35l35_clksel_mux[] = {
	SOC_DAPM_ENUM("CLKSEL Mux", clksel_enum),
};

static const char * const cs35l35_pdm_en_text[] = {
	"On",
	"Off",
};

static SOC_ENUM_SINGLE_DECL(pdm_en_enum, SND_SOC_NOPM, 0,
		cs35l35_pdm_en_text);

static const struct snd_kcontrol_new cs35l35_pdm_en_mux[] = {
	SOC_DAPM_ENUM("PDM MUX", pdm_en_enum),
};

static int cs35l35_reset_and_sync(struct cs35l35_private *priv, bool pdm)
{
	int ret = 0;

	if (!priv->reset_gpio)
		return 0;

	gpiod_set_value_cansleep(priv->reset_gpio, 0);
	usleep_range(2000, 2100);
	regcache_cache_only(priv->regmap, true);
	gpiod_set_value_cansleep(priv->reset_gpio, 1);
	usleep_range(1000, 1100);
	regcache_cache_only(priv->regmap, true);
	regcache_mark_dirty(priv->regmap);

	if (pdm) {
		regmap_update_bits(priv->regmap, CS35L35_AMP_INP_DRV_CTL,
			CS35L35_PDM_MODE_MASK, CS35L35_PDM_MODE_MASK);
		ret = regmap_update_bits(priv->regmap,
			CS35L35_CLK_CTL1,
			CS35L35_CLK_SOURCE_MASK,
			CS35L35_CLK_SOURCE_PDM);
		if (ret != 0)
			pr_err("%s regmap write failed %d\n",
				__func__, ret);

		ret = regmap_update_bits(priv->regmap,
			CS35L35_CLK_CTL2, CS35L35_CLK_DIV_MASK, 0);
		if (ret != 0)
			pr_err("%s regmap write failed %d\n",
				__func__, ret);

	} else {
		regmap_update_bits(priv->regmap, CS35L35_AMP_INP_DRV_CTL,
			CS35L35_PDM_MODE_MASK, 0);
		ret = regmap_update_bits(priv->regmap,
			CS35L35_CLK_CTL1,
			CS35L35_CLK_SOURCE_MASK,
			0);
		if (ret != 0)
			pr_err("%s regmap write failed %d\n",
				__func__, ret);
		ret = regmap_update_bits(priv->regmap,
			CS35L35_CLK_CTL2, CS35L35_CLK_DIV_MASK, 1);
		if (ret != 0)
			pr_err("%s regmap write failed %d\n",
				__func__, ret);
	}
	regcache_cache_only(priv->regmap, false);
	regcache_sync(priv->regmap);

	return ret;
}

static int cs35l35_mclk_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct cs35l35_private *cs35l35 = snd_soc_codec_get_drvdata(codec);
	int ret = 0;

	switch (event) {
	case SND_SOC_DAPM_POST_PMU:
		regmap_update_bits(cs35l35->regmap, CS35L35_PWRCTL1,
					CS35L35_DISCHG_FILT_MASK, 0);
		regmap_update_bits(cs35l35->regmap, CS35L35_PWRCTL1,
					CS35L35_PDN_ALL_MASK, 0);
		break;
	case SND_SOC_DAPM_PRE_PMU:
		cs35l35->i2s_enabled = true;
		regmap_update_bits(cs35l35->regmap, CS35L35_AMP_DIG_VOL_CTL,
					2, 2);
		if (cs35l35->pdm_mclk_switch) {
			cs35l35->pdm_mclk_switch = false;
			return cs35l35_reset_and_sync(cs35l35, false);
		}
		break;
	case SND_SOC_DAPM_PRE_PMD:
		cs35l35->i2s_enabled = false;
		if (cs35l35->pdm_mode) {
			cs35l35->pdm_mclk_switch = true;
			return cs35l35_reset_and_sync(cs35l35, true);
		} else {
			regmap_update_bits(cs35l35->regmap,
				CS35L35_AMP_DIG_VOL_CTL, 2, 0);
			regmap_update_bits(cs35l35->regmap, CS35L35_PWRCTL1,
					  CS35L35_PDN_ALL_MASK, 1);
			regmap_update_bits(cs35l35->regmap, CS35L35_PWRCTL1,
					CS35L35_DISCHG_FILT_MASK,
					1 << CS35L35_DISCHG_FILT_SHIFT);
			usleep_range(4000, 4010);
			ret = wait_for_completion_timeout(&cs35l35->pdn_done,
							msecs_to_jiffies(100));
			if (ret == 0) {
				pr_err("TIMEOUT PDN_DONE did not complete\n");
				ret = -ETIMEDOUT;
			}
		}
		break;
	default:
		pr_err("%s Invalid Event %d\n", __func__, event);
		return -EINVAL;
	}
	return ret;
}

static int cs35l35_pdm_event(struct snd_soc_dapm_widget *w,
		struct snd_kcontrol *kcontrol, int event)
{
	struct snd_soc_codec *codec = snd_soc_dapm_to_codec(w->dapm);
	struct cs35l35_private *cs35l35 = snd_soc_codec_get_drvdata(codec);
	int ret;

	if (cs35l35->i2s_enabled)
		return 0;

	switch (event) {
	case SND_SOC_DAPM_PRE_PMU:
		regmap_update_bits(cs35l35->regmap, CS35L35_AMP_DIG_VOL_CTL,
					2, 2);
		if (!cs35l35->pdm_mclk_switch) {
			cs35l35->pdm_mclk_switch = true;
			return cs35l35_reset_and_sync(cs35l35, true);
		}
		regmap_update_bits(cs35l35->regmap,
			CS35L35_CLK_CTL2, CS35L35_CLK_DIV_MASK, 0);
		break;
	case SND_SOC_DAPM_POST_PMU:
		regmap_update_bits(cs35l35->regmap, CS35L35_PWRCTL1,
					CS35L35_DISCHG_FILT_MASK, 0);
		regmap_update_bits(cs35l35->regmap, CS35L35_PWRCTL1,
					CS35L35_PDN_ALL_MASK, 0);
		break;
	case SND_SOC_DAPM_PRE_PMD:
		regmap_update_bits(cs35l35->regmap, CS35L35_AMP_DIG_VOL_CTL,
					2, 0);
		regmap_update_bits(cs35l35->regmap, CS35L35_PWRCTL1,
					  CS35L35_PDN_ALL_MASK, 1);
		regmap_update_bits(cs35l35->regmap, CS35L35_PWRCTL1,
					CS35L35_DISCHG_FILT_MASK,
					1 << CS35L35_DISCHG_FILT_SHIFT);
		usleep_range(4000, 4010);
		ret = wait_for_completion_timeout(&cs35l35->pdn_done,
							msecs_to_jiffies(100));
		if (ret == 0) {
			pr_err("TIMEOUT PDN_DONE did not complete in 100ms\n");
			ret = -ETIMEDOUT;
		}
		break;
	default:
		pr_err("%s Invalid Event %d\n", __func__, event);
		return -EINVAL;
	}
	return 0;
}

static const struct snd_soc_dapm_widget cs35l35_dapm_widgets[] = {
	SND_SOC_DAPM_AIF_IN("SDIN", NULL, 0, CS35L35_PWRCTL3, 1, 1),
	SND_SOC_DAPM_AIF_OUT("SDOUT", NULL, 0, CS35L35_PWRCTL3, 2, 1),

	SND_SOC_DAPM_SUPPLY_S("EXTCLK", 1, SND_SOC_NOPM, 0, 0,
		cs35l35_mclk_event, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY_S("PDMCLK", 2, SND_SOC_NOPM, 0, 0,
		cs35l35_pdm_event, SND_SOC_DAPM_PRE_PMU |
			SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_SUPPLY("MSTRCLK", CS35L35_CLK_CTL1, 2, 1,
		NULL, 0),

	SND_SOC_DAPM_OUTPUT("SPK"),

	SND_SOC_DAPM_INPUT("VP"),
	SND_SOC_DAPM_INPUT("VBST"),
	SND_SOC_DAPM_INPUT("ISENSE"),
	SND_SOC_DAPM_INPUT("VSENSE"),

	SND_SOC_DAPM_PGA("MCLK Select", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_PGA("PDM Select", SND_SOC_NOPM, 0, 0, NULL, 0),
	SND_SOC_DAPM_MUX("CLKSEL MUX", SND_SOC_NOPM, 0, 0, cs35l35_clksel_mux),
	SND_SOC_DAPM_MUX("PDM Mux", SND_SOC_NOPM, 0, 0, cs35l35_pdm_en_mux),

	SND_SOC_DAPM_ADC("VMON ADC", NULL, CS35L35_PWRCTL2, 7, 1),
	SND_SOC_DAPM_ADC("IMON ADC", NULL, CS35L35_PWRCTL2, 6, 1),
	SND_SOC_DAPM_ADC("VPMON ADC", NULL, CS35L35_PWRCTL3, 3, 1),
	SND_SOC_DAPM_ADC("VBSTMON ADC", NULL, CS35L35_PWRCTL3, 4, 1),
	SND_SOC_DAPM_ADC("CLASS H", NULL, CS35L35_PWRCTL2, 5, 1),

	SND_SOC_DAPM_OUT_DRV_E("Main AMP", CS35L35_PWRCTL2, 0, 1, NULL, 0,
		cs35l35_main_amp_event, SND_SOC_DAPM_PRE_PMU |
				SND_SOC_DAPM_POST_PMD | SND_SOC_DAPM_POST_PMU |
				SND_SOC_DAPM_PRE_PMD),

	SND_SOC_DAPM_OUT_DRV("RECT FET", CS35L35_PWRCTL2, 1, 0, NULL, 0),
	SND_SOC_DAPM_OUT_DRV("BOOST", CS35L35_PWRCTL2, 2, 1, NULL, 0),
};

static const struct snd_soc_dapm_route cs35l35_audio_map[] = {
	{"VPMON ADC", NULL, "VP"},
	{"VBSTMON ADC", NULL, "VBST"},
	{"IMON ADC", NULL, "ISENSE"},
	{"VMON ADC", NULL, "VSENSE"},
	{"SDOUT", NULL, "IMON ADC"},
	{"SDOUT", NULL, "VMON ADC"},
	{"SDOUT", NULL, "VBSTMON ADC"},
	{"SDOUT", NULL, "VPMON ADC"},
	{"AMP Capture", NULL, "SDOUT"},

	{"SDIN", NULL, "AMP Playback"},
	{"CLASS H", NULL, "SDIN"},
	{"BOOST", NULL, "CLASS H"},
	{"MCLK Select", NULL, "BOOST"},
	{"Main AMP", NULL, "MCLK Select"},

	{"PDM Mux", "On", "PDM Playback"},
	{"RECT FET", NULL, "PDM Mux"},
	{"PDM Select", NULL, "RECT FET"},
	{"CLKSEL MUX", "PDM", "PDM Select"},
	{"Main AMP", NULL, "CLKSEL MUX"},

	{"SPK", NULL, "Main AMP"},

	{"MCLK Select", NULL, "EXTCLK"},
	{"PDM Select", NULL, "PDMCLK"},
	{"MCLK Select", NULL, "MSTRCLK"},
	{"PDM Select", NULL, "MSTRCLK"},
};

static int cs35l35_set_dai_fmt(struct snd_soc_dai *codec_dai, unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct cs35l35_private *cs35l35 = snd_soc_codec_get_drvdata(codec);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		regmap_update_bits(cs35l35->regmap, CS35L35_CLK_CTL1,
				    CS35L35_MS_MASK, 1 << CS35L35_MS_SHIFT);
		cs35l35->slave_mode = false;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		regmap_update_bits(cs35l35->regmap, CS35L35_CLK_CTL1,
				    CS35L35_MS_MASK, 0 << CS35L35_MS_SHIFT);
		cs35l35->slave_mode = true;
		break;
	default:
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		cs35l35->tdm_mode = true;
		break;
	case SND_SOC_DAIFMT_I2S:
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int cs35l35_set_pdm_dai_fmt(struct snd_soc_dai *codec_dai,
	unsigned int fmt)
{
	struct snd_soc_codec *codec = codec_dai->codec;
	struct cs35l35_private *cs35l35 = snd_soc_codec_get_drvdata(codec);

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		cs35l35->slave_mode = true;
		break;
	default:
		dev_err(codec->dev, "PDM format is slave mode only\n");
		return -EINVAL;
	}

	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_PDM:
		break;
	default:
		dev_err(codec->dev, "Not set to PDM format\n");
		return -EINVAL;
	}
	return 0;
}

struct cs35l35_sysclk_config {
	int sysclk;
	int srate;
	u8 clk_cfg;
};

static struct cs35l35_sysclk_config cs35l35_clk_ctl[] = {

	/* SYSCLK, Sample Rate, Serial Port Cfg */
	{5644800, 44100, 0x00},
	{5644800, 88200, 0x40},
	{6144000, 48000, 0x10},
	{6144000, 96000, 0x50},
	{11289600, 44100, 0x01},
	{11289600, 88200, 0x41},
	{11289600, 176400, 0x81},
	{12000000, 44100, 0x03},
	{12000000, 48000, 0x13},
	{12000000, 88200, 0x43},
	{12000000, 96000, 0x53},
	{12000000, 176400, 0x83},
	{12000000, 192000, 0x93},
	{12288000, 48000, 0x11},
	{12288000, 96000, 0x51},
	{12288000, 192000, 0x91},
	{13000000, 44100, 0x07},
	{13000000, 48000, 0x17},
	{13000000, 88200, 0x47},
	{13000000, 96000, 0x57},
	{13000000, 176400, 0x87},
	{13000000, 192000, 0x97},
	{22579200, 44100, 0x02},
	{22579200, 88200, 0x42},
	{22579200, 176400, 0x82},
	{24000000, 44100, 0x0B},
	{24000000, 48000, 0x1B},
	{24000000, 88200, 0x4B},
	{24000000, 96000, 0x5B},
	{24000000, 176400, 0x8B},
	{24000000, 192000, 0x9B},
	{24576000, 48000, 0x12},
	{24576000, 96000, 0x52},
	{24576000, 192000, 0x92},
	{26000000, 44100, 0x0F},
	{26000000, 48000, 0x1F},
	{26000000, 88200, 0x4F},
	{26000000, 96000, 0x5F},
	{26000000, 176400, 0x8F},
	{26000000, 192000, 0x9F},
};

static int cs35l35_get_clk_config(int sysclk, int srate)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(cs35l35_clk_ctl); i++) {
		if (cs35l35_clk_ctl[i].sysclk == sysclk &&
			cs35l35_clk_ctl[i].srate == srate)
			return cs35l35_clk_ctl[i].clk_cfg;
	}
	return -EINVAL;
}

static int cs35l35_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct cs35l35_private *cs35l35 = snd_soc_codec_get_drvdata(codec);
	struct classh_cfg *classh = &cs35l35->pdata.classh_algo;
	int srate = params_rate(params);
	u8 sp_sclks;
	int audin_format = CS35L35_SDIN_DEPTH_16;
	int errata_chk;

	int clk_ctl = cs35l35_get_clk_config(cs35l35->sysclk, srate);
	if (clk_ctl < 0) {
		dev_err(codec->dev, "Invalid CLK:Rate %d:%d\n",
			cs35l35->sysclk, srate);
		return -EINVAL;
	}

	regmap_update_bits(cs35l35->regmap, CS35L35_CLK_CTL2,
			  CS35L35_CLK_CTL2_MASK, clk_ctl);

	/* Rev A0 Errata
	 *
	 * When configured for the weak-drive detection path (CH_WKFET_DIS = 0)
	 * the Class H algorithm does not enable weak-drive operation for
	 * nonzero values of CH_WKFET_DELAY if SP_RATE = 01 or 10
	 *
	 */
	errata_chk = clk_ctl & CS35L35_SP_RATE_MASK;

	if (classh->classh_wk_fet_disable == 0x00 &&
		(errata_chk == 0x01 || errata_chk == 0x03))
			regmap_update_bits(cs35l35->regmap,
				CS35L35_CLASS_H_FET_DRIVE_CTL,
				CS35L35_CH_WKFET_DEL_MASK,
				0);

/*
 * You can pull more Monitor data from the SDOUT pin than going to SDIN
 * Just make sure your SCLK is fast enough to fill the frame
 */
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		switch (params_width(params)) {
		case 8:
		case 16:
			audin_format = CS35L35_SDIN_DEPTH_16;
			break;
		case 24:
			audin_format = CS35L35_SDIN_DEPTH_24;
			break;
		default:
			dev_err(codec->dev, "Unsupported Width %d\n",
				params_width(params));
		}
		regmap_update_bits(cs35l35->regmap,
					CS35L35_AUDIN_DEPTH_CTL,
					CS35L35_AUDIN_DEPTH_MASK,
					audin_format <<
					CS35L35_AUDIN_DEPTH_SHIFT);
		if (cs35l35->pdata.stereo)
			regmap_update_bits(cs35l35->regmap,
					CS35L35_AUDIN_DEPTH_CTL,
					CS35L35_ADVIN_DEPTH_MASK,
					audin_format <<
					CS35L35_ADVIN_DEPTH_SHIFT);
	}

	if (cs35l35->pdm_mode)
		return 0;

/* We have to take the SCLK to derive num sclks
 * to configure the CLOCK_CTL3 register correctly
 */
	if ((cs35l35->sclk / srate) % 4) {
		dev_err(codec->dev, "Unsupported sclk/fs ratio %d:%d\n",
					cs35l35->sclk, srate);
		return -EINVAL;
	}
	sp_sclks = ((cs35l35->sclk / srate) / 4) - 1;

	/* Only certain ratios are supported in I2S Slave Mode */
	if (cs35l35->slave_mode) {
		switch (sp_sclks) {
		case CS35L35_SP_SCLKS_32FS:
		case CS35L35_SP_SCLKS_48FS:
		case CS35L35_SP_SCLKS_64FS:
			regmap_update_bits(cs35l35->regmap,
				CS35L35_CLK_CTL3,
				CS35L35_SP_SCLKS_MASK, sp_sclks <<
				CS35L35_SP_SCLKS_SHIFT);
		break;
		default:
			dev_err(codec->dev, "ratio not supported\n");
			return -EINVAL;
		};
	} else {
		/* Only certain ratios supported in I2S MASTER Mode */
		switch (sp_sclks) {
		case CS35L35_SP_SCLKS_32FS:
		case CS35L35_SP_SCLKS_64FS:
			regmap_update_bits(cs35l35->regmap,
				CS35L35_CLK_CTL3,
				CS35L35_SP_SCLKS_MASK, sp_sclks <<
				CS35L35_SP_SCLKS_SHIFT);
		break;
		default:
			dev_err(codec->dev, "ratio not supported\n");
			return -EINVAL;
		};
	}
	return 0;
}

static int cs35l35_pcm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct cs35l35_private *cs35l35 = snd_soc_codec_get_drvdata(codec);

	cs35l35->pdm_mode = false;

	return cs35l35_hw_params(substream, params, dai);
}

static int cs35l35_pdm_hw_params(struct snd_pcm_substream *substream,
				 struct snd_pcm_hw_params *params,
				 struct snd_soc_dai *dai)
{
	struct snd_soc_codec *codec = dai->codec;
	struct cs35l35_private *cs35l35 = snd_soc_codec_get_drvdata(codec);

	cs35l35->pdm_mode = true;

	return cs35l35_hw_params(substream, params, dai);
}

static const unsigned int cs35l35_src_rates[] = {
	8000, 16000, 44100, 48000, 88200, 96000, 176400, 192000
};

static const struct snd_pcm_hw_constraint_list cs35l35_constraints = {
	.count  = ARRAY_SIZE(cs35l35_src_rates),
	.list   = cs35l35_src_rates,
};

static int cs35l35_pcm_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	if (substream->runtime)
		snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE, &cs35l35_constraints);

	return 0;
}

static const unsigned int cs35l35_pdm_rates[] = {
	44100, 48000, 88200, 96000
};

static const struct snd_pcm_hw_constraint_list cs35l35_pdm_constraints = {
	.count  = ARRAY_SIZE(cs35l35_pdm_rates),
	.list   = cs35l35_pdm_rates,
};

static int cs35l35_pdm_startup(struct snd_pcm_substream *substream,
			       struct snd_soc_dai *dai)
{
	if (substream->runtime)
		snd_pcm_hw_constraint_list(substream->runtime, 0,
			SNDRV_PCM_HW_PARAM_RATE,
			&cs35l35_pdm_constraints);

	return 0;
}

static int cs35l35_dai_set_sysclk(struct snd_soc_dai *dai,
				int clk_id, unsigned int freq, int dir)
{
	struct snd_soc_codec *codec = dai->codec;
	struct cs35l35_private *cs35l35 = snd_soc_codec_get_drvdata(codec);

	/* Need the SCLK Frequency */
	cs35l35->sclk = freq;

	return 0;
}

static const struct snd_soc_dai_ops cs35l35_ops = {
	.startup = cs35l35_pcm_startup,
	.set_fmt = cs35l35_set_dai_fmt,
	.hw_params = cs35l35_pcm_hw_params,
	.set_sysclk = cs35l35_dai_set_sysclk,
};

static const struct snd_soc_dai_ops cs35l35_pdm_ops = {
	.startup = cs35l35_pdm_startup,
	.set_fmt = cs35l35_set_pdm_dai_fmt,
	.hw_params = cs35l35_pdm_hw_params,
};

static struct snd_soc_dai_driver cs35l35_dai[] = {
	{
		.name = "cs35l35-pcm",
		.id = 0,
		.playback = {
			.stream_name = "AMP Playback",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_KNOT,
			.formats = CS35L35_FORMATS,
		},
		.capture = {
			.stream_name = "AMP Capture",
			.channels_min = 1,
			.channels_max = 8,
			.rates = SNDRV_PCM_RATE_KNOT,
			.formats = CS35L35_FORMATS,
		},
		.ops = &cs35l35_ops,
		.symmetric_rates = 1,
	},
	{
		.name = "cs35l35-pdm",
		.id = 1,
		.playback = {
			.stream_name = "PDM Playback",
			.channels_min = 1,
			.channels_max = 2,
			.rates = SNDRV_PCM_RATE_KNOT,
			.formats = CS35L35_FORMATS,
		},
		.ops = &cs35l35_pdm_ops,
	},
};

static int cs35l35_codec_set_sysclk(struct snd_soc_codec *codec,
				int clk_id, int source, unsigned int freq,
				int dir)
{
	struct cs35l35_private *cs35l35 = snd_soc_codec_get_drvdata(codec);
	int clksrc;

	switch (clk_id) {
	case 0:
		clksrc = CS35L35_CLK_SOURCE_MCLK;
		break;
	case 1:
		clksrc = CS35L35_CLK_SOURCE_SCLK;
		break;
	case 2:
		clksrc = CS35L35_CLK_SOURCE_PDM;
		break;
	default:
		dev_err(codec->dev, "Invalid CLK Source\n");
		return -EINVAL;
	};

	switch (freq) {
	case 5644800:
	case 6144000:
	case 11289600:
	case 12000000:
	case 12288000:
	case 13000000:
	case 22579200:
	case 24000000:
	case 24576000:
	case 26000000:
		cs35l35->sysclk = freq;
		break;
	default:
		dev_err(codec->dev, "Invalid CLK Frequency\n");
		return -EINVAL;
	}

	regmap_update_bits(cs35l35->regmap, CS35L35_CLK_CTL1,
				CS35L35_CLK_SOURCE_MASK,
				clksrc << CS35L35_CLK_SOURCE_SHIFT);

	return 0;
}

static int cs35l35_codec_probe(struct snd_soc_codec *codec)
{
	struct snd_soc_dapm_context *dapm = snd_soc_codec_get_dapm(codec);
	struct cs35l35_private *cs35l35 = snd_soc_codec_get_drvdata(codec);
	struct classh_cfg *classh = &cs35l35->pdata.classh_algo;
	struct monitor_cfg *monitor_config = &cs35l35->pdata.mon_cfg;
	int ret = 0;

	/* Set Platform Data */
	if (cs35l35->pdata.bst_vctl)
		regmap_update_bits(cs35l35->regmap, CS35L35_BST_CVTR_V_CTL,
				CS35L35_BST_CTL_MASK,
				cs35l35->pdata.bst_vctl);

	if (cs35l35->pdata.gain_zc)
		regmap_update_bits(cs35l35->regmap, CS35L35_PROTECT_CTL,
				CS35L35_AMP_GAIN_ZC_MASK,
				cs35l35->pdata.gain_zc <<
				CS35L35_AMP_GAIN_ZC_SHIFT);

	if (cs35l35->pdata.aud_channel)
		regmap_update_bits(cs35l35->regmap,
				CS35L35_AUDIN_RXLOC_CTL,
				CS35L35_AUD_IN_LR_MASK,
				cs35l35->pdata.aud_channel <<
				CS35L35_AUD_IN_LR_SHIFT);

	if (cs35l35->pdata.stereo) {
		regmap_update_bits(cs35l35->regmap,
				CS35L35_ADVIN_RXLOC_CTL,
				CS35L35_ADV_IN_LR_MASK,
				cs35l35->pdata.adv_channel <<
				CS35L35_ADV_IN_LR_SHIFT);
		if (cs35l35->pdata.shared_bst)
			regmap_update_bits(cs35l35->regmap, CS35L35_CLASS_H_CTL,
					CS35L35_CH_STEREO_MASK,
					1 << CS35L35_CH_STEREO_SHIFT);
		ret = snd_soc_add_codec_controls(codec, cs35l35_adv_controls,
					ARRAY_SIZE(cs35l35_adv_controls));
		if (ret)
			return ret;
	}

	if (cs35l35->pdata.sp_drv_str)
		regmap_update_bits(cs35l35->regmap, CS35L35_CLK_CTL1,
				CS35L35_SP_DRV_MASK,
				cs35l35->pdata.sp_drv_str <<
				CS35L35_SP_DRV_SHIFT);

	if (classh->classh_algo_enable) {
		if (classh->classh_bst_override)
			regmap_update_bits(cs35l35->regmap,
					CS35L35_CLASS_H_CTL,
					CS35L35_CH_BST_OVR_MASK,
					classh->classh_bst_override <<
					CS35L35_CH_BST_OVR_SHIFT);
		if (classh->classh_bst_max_limit)
			regmap_update_bits(cs35l35->regmap,
					CS35L35_CLASS_H_CTL,
					CS35L35_CH_BST_LIM_MASK,
					classh->classh_bst_max_limit <<
					CS35L35_CH_BST_LIM_SHIFT);
		if (classh->classh_mem_depth)
			regmap_update_bits(cs35l35->regmap,
					CS35L35_CLASS_H_CTL,
					CS35L35_CH_MEM_DEPTH_MASK,
					classh->classh_mem_depth <<
					CS35L35_CH_MEM_DEPTH_SHIFT);
		if (classh->classh_headroom)
			regmap_update_bits(cs35l35->regmap,
					CS35L35_CLASS_H_HEADRM_CTL,
					CS35L35_CH_HDRM_CTL_MASK,
					classh->classh_headroom <<
					CS35L35_CH_HDRM_CTL_SHIFT);
		if (classh->classh_release_rate)
			regmap_update_bits(cs35l35->regmap,
					CS35L35_CLASS_H_RELEASE_RATE,
					CS35L35_CH_REL_RATE_MASK,
					classh->classh_release_rate <<
					CS35L35_CH_REL_RATE_SHIFT);
		if (classh->classh_wk_fet_disable)
			regmap_update_bits(cs35l35->regmap,
					CS35L35_CLASS_H_FET_DRIVE_CTL,
					CS35L35_CH_WKFET_DIS_MASK,
					classh->classh_wk_fet_disable <<
					CS35L35_CH_WKFET_DIS_SHIFT);
		if (classh->classh_wk_fet_delay)
			regmap_update_bits(cs35l35->regmap,
					CS35L35_CLASS_H_FET_DRIVE_CTL,
					CS35L35_CH_WKFET_DEL_MASK,
					classh->classh_wk_fet_delay <<
					CS35L35_CH_WKFET_DEL_SHIFT);
		if (classh->classh_wk_fet_thld)
			regmap_update_bits(cs35l35->regmap,
					   CS35L35_CLASS_H_FET_DRIVE_CTL,
					CS35L35_CH_WKFET_THLD_MASK,
					classh->classh_wk_fet_thld <<
					CS35L35_CH_WKFET_THLD_SHIFT);
		if (classh->classh_vpch_auto)
			regmap_update_bits(cs35l35->regmap,
					CS35L35_CLASS_H_VP_CTL,
					CS35L35_CH_VP_AUTO_MASK,
					classh->classh_vpch_auto <<
					CS35L35_CH_VP_AUTO_SHIFT);
		if (classh->classh_vpch_rate)
			regmap_update_bits(cs35l35->regmap,
					CS35L35_CLASS_H_VP_CTL,
					CS35L35_CH_VP_RATE_MASK,
					classh->classh_vpch_rate <<
					CS35L35_CH_VP_RATE_SHIFT);
		if (classh->classh_vpch_man)
			regmap_update_bits(cs35l35->regmap,
					CS35L35_CLASS_H_VP_CTL,
					CS35L35_CH_VP_MAN_MASK,
					classh->classh_vpch_man <<
					CS35L35_CH_VP_MAN_SHIFT);
	}

	if (monitor_config->is_present) {
		if (monitor_config->vmon_specs) {
			regmap_update_bits(cs35l35->regmap,
					CS35L35_SPKMON_DEPTH_CTL,
					CS35L35_VMON_DEPTH_MASK,
					monitor_config->vmon_dpth <<
					CS35L35_VMON_DEPTH_SHIFT);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_VMON_TXLOC_CTL,
					CS35L35_MON_TXLOC_MASK,
					monitor_config->vmon_loc <<
					CS35L35_MON_TXLOC_SHIFT);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_VMON_TXLOC_CTL,
					CS35L35_MON_FRM_MASK,
					monitor_config->vmon_frm <<
					CS35L35_MON_FRM_SHIFT);
		}
		if (monitor_config->imon_specs) {
			regmap_update_bits(cs35l35->regmap,
					CS35L35_SPKMON_DEPTH_CTL,
					CS35L35_IMON_DEPTH_MASK,
					monitor_config->imon_dpth <<
					CS35L35_IMON_DEPTH_SHIFT);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_IMON_TXLOC_CTL,
					CS35L35_MON_TXLOC_MASK,
					monitor_config->imon_loc <<
					CS35L35_MON_TXLOC_SHIFT);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_IMON_TXLOC_CTL,
					CS35L35_MON_FRM_MASK,
					monitor_config->imon_frm <<
					CS35L35_MON_FRM_SHIFT);
		}
		if (monitor_config->vpmon_specs) {
			regmap_update_bits(cs35l35->regmap,
					CS35L35_SUPMON_DEPTH_CTL,
					CS35L35_VPMON_DEPTH_MASK,
					monitor_config->vpmon_dpth <<
					CS35L35_VPMON_DEPTH_SHIFT);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_VPMON_TXLOC_CTL,
					CS35L35_MON_TXLOC_MASK,
					monitor_config->vpmon_loc <<
					CS35L35_MON_TXLOC_SHIFT);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_VPMON_TXLOC_CTL,
					CS35L35_MON_FRM_MASK,
					monitor_config->vpmon_frm <<
					CS35L35_MON_FRM_SHIFT);
		}
		if (monitor_config->vbstmon_specs) {
			regmap_update_bits(cs35l35->regmap,
					CS35L35_SUPMON_DEPTH_CTL,
					CS35L35_VBSTMON_DEPTH_MASK,
					monitor_config->vpmon_dpth <<
					CS35L35_VBSTMON_DEPTH_SHIFT);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_VBSTMON_TXLOC_CTL,
					CS35L35_MON_TXLOC_MASK,
					monitor_config->vbstmon_loc <<
					CS35L35_MON_TXLOC_SHIFT);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_VBSTMON_TXLOC_CTL,
					CS35L35_MON_FRM_MASK,
					monitor_config->vbstmon_frm <<
					CS35L35_MON_FRM_SHIFT);
		}
		if (monitor_config->vpbrstat_specs) {
			regmap_update_bits(cs35l35->regmap,
					CS35L35_SUPMON_DEPTH_CTL,
					CS35L35_VPBRSTAT_DEPTH_MASK,
					monitor_config->vpbrstat_dpth <<
					CS35L35_VPBRSTAT_DEPTH_SHIFT);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_VPBR_STATUS_TXLOC_CTL,
					CS35L35_MON_TXLOC_MASK,
					monitor_config->vpbrstat_loc <<
					CS35L35_MON_TXLOC_SHIFT);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_VPBR_STATUS_TXLOC_CTL,
					CS35L35_MON_FRM_MASK,
					monitor_config->vpbrstat_frm <<
					CS35L35_MON_FRM_SHIFT);
		}
		if (monitor_config->zerofill_specs) {
			regmap_update_bits(cs35l35->regmap,
					CS35L35_SUPMON_DEPTH_CTL,
					CS35L35_ZEROFILL_DEPTH_MASK,
					monitor_config->zerofill_dpth <<
					CS35L35_ZEROFILL_DEPTH_SHIFT);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_ZERO_FILL_LOC_CTL,
					CS35L35_MON_TXLOC_MASK,
					monitor_config->zerofill_loc <<
					CS35L35_MON_TXLOC_SHIFT);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_ZERO_FILL_LOC_CTL,
					CS35L35_MON_FRM_MASK,
					monitor_config->zerofill_frm <<
					CS35L35_MON_FRM_SHIFT);
		}
	}

	snd_soc_dapm_ignore_suspend(dapm, "SDIN");
	snd_soc_dapm_ignore_suspend(dapm, "SDOUT");
	snd_soc_dapm_ignore_suspend(dapm, "SPK");
	snd_soc_dapm_ignore_suspend(dapm, "VP");
	snd_soc_dapm_ignore_suspend(dapm, "VBST");
	snd_soc_dapm_ignore_suspend(dapm, "ISENSE");
	snd_soc_dapm_ignore_suspend(dapm, "VSENSE");
	snd_soc_dapm_ignore_suspend(dapm, "Main AMP");

	return ret;
}

static struct snd_soc_codec_driver soc_codec_dev_cs35l35 = {
	.probe = cs35l35_codec_probe,
	.set_sysclk = cs35l35_codec_set_sysclk,

	.component_driver = {
		.dapm_widgets = cs35l35_dapm_widgets,
		.num_dapm_widgets = ARRAY_SIZE(cs35l35_dapm_widgets),

		.dapm_routes = cs35l35_audio_map,
		.num_dapm_routes = ARRAY_SIZE(cs35l35_audio_map),

		.controls = cs35l35_aud_controls,
		.num_controls = ARRAY_SIZE(cs35l35_aud_controls),
	}
};

static struct regmap_config cs35l35_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = CS35L35_MAX_REGISTER,
	.reg_defaults = cs35l35_reg,
	.num_reg_defaults = ARRAY_SIZE(cs35l35_reg),
	.volatile_reg = cs35l35_volatile_register,
	.readable_reg = cs35l35_readable_register,
	.precious_reg = cs35l35_precious_register,
	.cache_type = REGCACHE_RBTREE,
	.use_single_rw = true,
};

static irqreturn_t cs35l35_irq(int irq, void *data)
{
		struct cs35l35_private *cs35l35 = data;
	struct i2c_client *i2c_client = cs35l35->i2c_client;
	unsigned int sticky1, sticky2, sticky3, sticky4;
	unsigned int mask1, mask2, mask3, mask4, current1;

	/* ack the irq by reading all status registers */
	regmap_read(cs35l35->regmap, CS35L35_INT_STATUS_4, &sticky4);
	regmap_read(cs35l35->regmap, CS35L35_INT_STATUS_3, &sticky3);
	regmap_read(cs35l35->regmap, CS35L35_INT_STATUS_2, &sticky2);
	regmap_read(cs35l35->regmap, CS35L35_INT_STATUS_1, &sticky1);

	regmap_read(cs35l35->regmap, CS35L35_INT_MASK_4, &mask4);
	regmap_read(cs35l35->regmap, CS35L35_INT_MASK_3, &mask3);
	regmap_read(cs35l35->regmap, CS35L35_INT_MASK_2, &mask2);
	regmap_read(cs35l35->regmap, CS35L35_INT_MASK_1, &mask1);

	/* Check to see if unmasked bits are active */
	if (!(sticky1 & ~mask1) && !(sticky2 & ~mask2) && !(sticky3 & ~mask3)
			&& !(sticky4 & ~mask4))
		return IRQ_NONE;

	if (sticky2 & CS35L35_PDN_DONE)
		complete(&cs35l35->pdn_done);

	/* read the current values */
	regmap_read(cs35l35->regmap, CS35L35_INT_STATUS_1, &current1);

	/* handle the interrupts */
	if (sticky1 & CS35L35_CAL_ERR) {
		dev_err(&i2c_client->dev, "Calibration Error\n");

		/* error is no longer asserted; safe to reset */
		if (!(current1 & CS35L35_CAL_ERR)) {
			dev_dbg(&i2c_client->dev, "Cal error release\n");
			regmap_update_bits(cs35l35->regmap,
					CS35L35_PROT_RELEASE_CTL,
					CS35L35_CAL_ERR_RLS, 0);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_PROT_RELEASE_CTL,
					CS35L35_CAL_ERR_RLS,
					CS35L35_CAL_ERR_RLS);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_PROT_RELEASE_CTL,
					CS35L35_CAL_ERR_RLS, 0);
		}
	}

	if (sticky1 & CS35L35_AMP_SHORT) {
		/* error is no longer asserted; safe to reset */
		if (!(current1 & CS35L35_AMP_SHORT)) {
			dev_dbg(&i2c_client->dev,
				"Amp short error release\n");
			regmap_update_bits(cs35l35->regmap,
					CS35L35_PROT_RELEASE_CTL,
					CS35L35_SHORT_RLS, 0);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_PROT_RELEASE_CTL,
					CS35L35_SHORT_RLS,
					CS35L35_SHORT_RLS);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_PROT_RELEASE_CTL,
					CS35L35_SHORT_RLS, 0);
		}
	}

	if (sticky1 & CS35L35_OTW) {
		dev_err(&i2c_client->dev, "Over temperature warning\n");

		/* error is no longer asserted; safe to reset */
		if (!(current1 & CS35L35_OTW)) {
			dev_dbg(&i2c_client->dev,
				"Over temperature warning release\n");
			regmap_update_bits(cs35l35->regmap,
					CS35L35_PROT_RELEASE_CTL,
					CS35L35_OTW_RLS, 0);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_PROT_RELEASE_CTL,
					CS35L35_OTW_RLS,
					CS35L35_OTW_RLS);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_PROT_RELEASE_CTL,
					CS35L35_OTW_RLS, 0);
		}
	}

	if (sticky1 & CS35L35_OTE) {
		dev_crit(&i2c_client->dev, "Over temperature error\n");

		/* error is no longer asserted; safe to reset */
		if (!(current1 & CS35L35_OTE)) {
			dev_dbg(&i2c_client->dev,
				"Over temperature error release\n");
			regmap_update_bits(cs35l35->regmap,
					CS35L35_PROT_RELEASE_CTL,
					CS35L35_OTE_RLS, 0);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_PROT_RELEASE_CTL,
					CS35L35_OTE_RLS,
					CS35L35_OTE_RLS);
			regmap_update_bits(cs35l35->regmap,
					CS35L35_PROT_RELEASE_CTL,
					CS35L35_OTE_RLS, 0);
		}
	}

	if (sticky3 & CS35L35_BST_HIGH) {
		dev_crit(&i2c_client->dev, "VBST error: powering off!\n");
		regmap_update_bits(cs35l35->regmap, CS35L35_PWRCTL2,
			CS35L35_PDN_AMP, CS35L35_PDN_AMP);
		regmap_update_bits(cs35l35->regmap, CS35L35_PWRCTL1,
			CS35L35_PDN_ALL, CS35L35_PDN_ALL);
	}

	if (sticky3 & CS35L35_LBST_SHORT) {
		dev_crit(&i2c_client->dev, "LBST error: powering off!\n");
		regmap_update_bits(cs35l35->regmap, CS35L35_PWRCTL2,
			CS35L35_PDN_AMP, CS35L35_PDN_AMP);
		regmap_update_bits(cs35l35->regmap, CS35L35_PWRCTL1,
			CS35L35_PDN_ALL, CS35L35_PDN_ALL);
	}

	if (sticky2 & CS35L35_VPBR_ERR)
		dev_err(&i2c_client->dev, "Error: Reactive Brownout\n");

	if (sticky4 & CS35L35_VMON_OVFL)
		dev_err(&i2c_client->dev, "Error: VMON overflow\n");

	if (sticky4 & CS35L35_IMON_OVFL)
		dev_err(&i2c_client->dev, "Error: IMON overflow\n");

	return IRQ_HANDLED;
}


static int cs35l35_handle_of_data(struct i2c_client *i2c_client,
				struct cs35l35_platform_data *pdata)
{
	struct device_node *np = i2c_client->dev.of_node;
	struct device_node *classh, *signal_format;
	struct classh_cfg *classh_config = &pdata->classh_algo;
	struct monitor_cfg *monitor_config = &pdata->mon_cfg;
	unsigned int val32 = 0;
	u8 monitor_array[3];
	int ret = 0;

	if (!np)
		return 0;

	pdata->bst_pdn_fet_on = of_property_read_bool(np, "boost-pdn-fet-on");

	if (of_property_read_u32(np, "boost-ctl-millivolt", &val32) >= 0)
		pdata->bst_vctl = val32;

	if (of_property_read_u32(np, "sp-drv-strength", &val32) >= 0)
		pdata->sp_drv_str = val32;

	if (of_property_read_u32(np, "audio-channel", &val32) >= 0)
		pdata->aud_channel = val32;

	pdata->stereo = of_property_read_bool(np, "stereo-config");
	if (pdata->stereo) {
		if (of_property_read_u32(np, "audio-channel", &val32) >= 0)
			pdata->aud_channel = val32;

		if (of_property_read_u32(np, "advisory-channel",
					&val32) >= 0)
			pdata->adv_channel = val32;

		pdata->shared_bst = of_property_read_bool(np, "shared-boost");
	}

	pdata->gain_zc = of_property_read_bool(np, "amp-gain-zc");

	classh = of_get_child_by_name(np, "classh-internal-algo");
	classh_config->classh_algo_enable = classh ? true : false;

	if (classh_config->classh_algo_enable) {
		classh_config->classh_bst_override = of_property_read_bool(np,
					"classh-bst-overide");

		if (of_property_read_u32(classh, "classh-bst-max-limit",
					&val32) >= 0)
			classh_config->classh_bst_max_limit = val32;
		if (of_property_read_u32(classh, "classh-mem-depth",
					&val32) >= 0)
			classh_config->classh_mem_depth = val32;
		if (of_property_read_u32(classh, "classh-release-rate",
					&val32) >= 0)
			classh_config->classh_release_rate = val32;
		if (of_property_read_u32(classh, "classh-headroom",
					&val32) >= 0)
			classh_config->classh_headroom = val32;
		if (of_property_read_u32(classh, "classh-wk-fet-disable",
					&val32) >= 0)
			classh_config->classh_wk_fet_disable = val32;
		if (of_property_read_u32(classh, "classh-wk-fet-delay",
					&val32) >= 0)
			classh_config->classh_wk_fet_delay = val32;
		if (of_property_read_u32(classh, "classh-wk-fet-thld",
					&val32) >= 0)
			classh_config->classh_wk_fet_thld = val32;
		if (of_property_read_u32(classh, "classh-vpch-auto",
					&val32) >= 0)
			classh_config->classh_vpch_auto = val32;
		if (of_property_read_u32(classh, "classh-vpch-rate",
					&val32) >= 0)
			classh_config->classh_vpch_rate = val32;
		if (of_property_read_u32(classh, "classh-vpch-man",
					&val32) >= 0)
			classh_config->classh_vpch_man = val32;
	}
	of_node_put(classh);

	/* frame depth location */
	signal_format = of_get_child_by_name(np, "monitor-signal-format");
	monitor_config->is_present = signal_format ? true : false;
	if (monitor_config->is_present) {
		ret = of_property_read_u8_array(signal_format, "imon",
				   monitor_array, ARRAY_SIZE(monitor_array));
		if (!ret) {
			monitor_config->imon_specs = true;
			monitor_config->imon_dpth = monitor_array[0];
			monitor_config->imon_loc = monitor_array[1];
			monitor_config->imon_frm = monitor_array[2];
		}
		ret = of_property_read_u8_array(signal_format, "vmon",
				   monitor_array, ARRAY_SIZE(monitor_array));
		if (!ret) {
			monitor_config->vmon_specs = true;
			monitor_config->vmon_dpth = monitor_array[0];
			monitor_config->vmon_loc = monitor_array[1];
			monitor_config->vmon_frm = monitor_array[2];
		}
		ret = of_property_read_u8_array(signal_format, "vpmon",
				   monitor_array, ARRAY_SIZE(monitor_array));
		if (!ret) {
			monitor_config->vpmon_specs = true;
			monitor_config->vpmon_dpth = monitor_array[0];
			monitor_config->vpmon_loc = monitor_array[1];
			monitor_config->vpmon_frm = monitor_array[2];
		}
		ret = of_property_read_u8_array(signal_format, "vbstmon",
				   monitor_array, ARRAY_SIZE(monitor_array));
		if (!ret) {
			monitor_config->vbstmon_specs = true;
			monitor_config->vbstmon_dpth = monitor_array[0];
			monitor_config->vbstmon_loc = monitor_array[1];
			monitor_config->vbstmon_frm = monitor_array[2];
		}
		ret = of_property_read_u8_array(signal_format, "vpbrstat",
				   monitor_array, ARRAY_SIZE(monitor_array));
		if (!ret) {
			monitor_config->vpbrstat_specs = true;
			monitor_config->vpbrstat_dpth = monitor_array[0];
			monitor_config->vpbrstat_loc = monitor_array[1];
			monitor_config->vpbrstat_frm = monitor_array[2];
		}
		ret = of_property_read_u8_array(signal_format, "zerofill",
				   monitor_array, ARRAY_SIZE(monitor_array));
		if (!ret) {
			monitor_config->zerofill_specs = true;
			monitor_config->zerofill_dpth = monitor_array[0];
			monitor_config->zerofill_loc = monitor_array[1];
			monitor_config->zerofill_frm = monitor_array[2];
		}
	}
	of_node_put(signal_format);

	return 0;
}

/* Errata Rev A0 */
static const struct reg_sequence cs35l35_errata_patch[] = {
	{ 0x7F, 0x99 },
	{ 0x00, 0x99 },
	{ 0x52, 0x22 },
	{ 0x04, 0x14 },
	{ 0x6D, 0x44 },
	{ 0x24, 0x10 },
	{ 0x58, 0xC4 },
	{ 0x00, 0x98 },
	{ 0x18, 0x08 },
	{ 0x00, 0x00 },
	{ 0x7F, 0x00 },
};

static int cs35l35_i2c_probe(struct i2c_client *i2c_client,
			      const struct i2c_device_id *id)
{
	struct cs35l35_private *cs35l35;
	struct cs35l35_platform_data *pdata =
		dev_get_platdata(&i2c_client->dev);
	int i;
	int ret;
	unsigned int devid = 0;
	unsigned int reg;

	cs35l35 = devm_kzalloc(&i2c_client->dev,
			       sizeof(struct cs35l35_private),
			       GFP_KERNEL);
	if (!cs35l35) {
		dev_err(&i2c_client->dev, "could not allocate codec\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(i2c_client, cs35l35);
	cs35l35->i2c_client = i2c_client;
	cs35l35->regmap = devm_regmap_init_i2c(i2c_client, &cs35l35_regmap);
	if (IS_ERR(cs35l35->regmap)) {
		ret = PTR_ERR(cs35l35->regmap);
		dev_err(&i2c_client->dev, "regmap_init() failed: %d\n", ret);
		goto err;
	}

	for (i = 0; i < ARRAY_SIZE(cs35l35_supplies); i++)
		cs35l35->supplies[i].supply = cs35l35_supplies[i];
	cs35l35->num_supplies = ARRAY_SIZE(cs35l35_supplies);

	ret = devm_regulator_bulk_get(&i2c_client->dev,
			cs35l35->num_supplies,
			cs35l35->supplies);
	if (ret != 0) {
		dev_err(&i2c_client->dev,
			"Failed to request core supplies: %d\n",
			ret);
		return ret;
	}

	if (pdata) {
		cs35l35->pdata = *pdata;
	} else {
		pdata = devm_kzalloc(&i2c_client->dev,
				     sizeof(struct cs35l35_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&i2c_client->dev,
				"could not allocate pdata\n");
			return -ENOMEM;
		}
		if (i2c_client->dev.of_node) {
			ret = cs35l35_handle_of_data(i2c_client, pdata);
			if (ret != 0)
				return ret;

		}
		cs35l35->pdata = *pdata;
	}

	ret = regulator_bulk_enable(cs35l35->num_supplies,
					cs35l35->supplies);
	if (ret != 0) {
		dev_err(&i2c_client->dev,
			"Failed to enable core supplies: %d\n",
			ret);
		return ret;
	}

	/* returning NULL can be an option if in stereo mode */
	cs35l35->reset_gpio = devm_gpiod_get_optional(&i2c_client->dev,
		"reset", GPIOD_OUT_LOW);
	if (IS_ERR(cs35l35->reset_gpio))
		return PTR_ERR(cs35l35->reset_gpio);

	if (cs35l35->reset_gpio)
		gpiod_set_value_cansleep(cs35l35->reset_gpio, 1);

	init_completion(&cs35l35->pdn_done);

	ret = regmap_register_patch(cs35l35->regmap, cs35l35_errata_patch,
		ARRAY_SIZE(cs35l35_errata_patch));
	if (ret < 0) {
		dev_err(&i2c_client->dev, "Failed to apply errata patch\n");
		return ret;
	}

	cs35l35->irq_gpio = devm_gpiod_get_optional(&i2c_client->dev,
		"irq", GPIOD_IN);
	if (IS_ERR(cs35l35->irq_gpio))
		return PTR_ERR(cs35l35->irq_gpio);

	ret = devm_request_threaded_irq(&i2c_client->dev,
					gpiod_to_irq(cs35l35->irq_gpio),
					NULL, cs35l35_irq,
					IRQF_ONESHOT | IRQF_TRIGGER_LOW,
					"cs35l35", cs35l35);
	/* CS35L35 needs INT for PDN_DONE */
	if (ret != 0) {
		dev_err(&i2c_client->dev, "Failed to request IRQ: %d\n", ret);
		goto err;
	}
	/* initialize codec */
	ret = regmap_read(cs35l35->regmap, CS35L35_DEVID_AB, &reg);

	devid = (reg & 0xFF) << 12;
	ret = regmap_read(cs35l35->regmap, CS35L35_DEVID_CD, &reg);
	devid |= (reg & 0xFF) << 4;
	ret = regmap_read(cs35l35->regmap, CS35L35_DEVID_E, &reg);
	devid |= (reg & 0xF0) >> 4;

	if (devid != CS35L35_CHIP_ID) {
		dev_err(&i2c_client->dev,
			"CS35L35 Device ID (%X). Expected ID %X\n",
			devid, CS35L35_CHIP_ID);
		ret = -ENODEV;
		goto err;
	}

	ret = regmap_read(cs35l35->regmap, CS35L35_REV_ID, &reg);
	if (ret < 0) {
		dev_err(&i2c_client->dev, "Get Revision ID failed\n");
		goto err;
	}

	dev_info(&i2c_client->dev,
		 "Cirrus Logic CS35L35 (%x), Revision: %02X\n", devid,
		ret & 0xFF);

	/* Set the INT Masks for critical errors */
	regmap_write(cs35l35->regmap, CS35L35_INT_MASK_1,
				CS35L35_INT1_CRIT_MASK);
	regmap_write(cs35l35->regmap, CS35L35_INT_MASK_2,
				CS35L35_INT2_CRIT_MASK);
	regmap_write(cs35l35->regmap, CS35L35_INT_MASK_3,
				CS35L35_INT3_CRIT_MASK);
	regmap_write(cs35l35->regmap, CS35L35_INT_MASK_4,
				CS35L35_INT4_CRIT_MASK);

	regmap_update_bits(cs35l35->regmap, CS35L35_PWRCTL2,
			CS35L35_PWR2_PDN_MASK,
			CS35L35_PWR2_PDN_MASK);

	if (cs35l35->pdata.bst_pdn_fet_on)
		regmap_update_bits(cs35l35->regmap, CS35L35_PWRCTL2,
					CS35L35_PDN_BST_MASK,
					1 << CS35L35_PDN_BST_FETON_SHIFT);
	else
		regmap_update_bits(cs35l35->regmap, CS35L35_PWRCTL2,
					CS35L35_PDN_BST_MASK,
					1 << CS35L35_PDN_BST_FETOFF_SHIFT);

	regmap_update_bits(cs35l35->regmap, CS35L35_PWRCTL3,
			CS35L35_PWR3_PDN_MASK,
			CS35L35_PWR3_PDN_MASK);

	regmap_update_bits(cs35l35->regmap, CS35L35_PROTECT_CTL,
		CS35L35_AMP_MUTE_MASK, 1 << CS35L35_AMP_MUTE_SHIFT);

	ret =  snd_soc_register_codec(&i2c_client->dev,
			&soc_codec_dev_cs35l35, cs35l35_dai,
			ARRAY_SIZE(cs35l35_dai));
	if (ret < 0) {
		dev_err(&i2c_client->dev,
			"%s: Register codec failed\n", __func__);
		goto err;
	}

err:
	regulator_bulk_disable(cs35l35->num_supplies,
			       cs35l35->supplies);
	return ret;
}

static int cs35l35_i2c_remove(struct i2c_client *client)
{
	snd_soc_unregister_codec(&client->dev);
	kfree(i2c_get_clientdata(client));
	return 0;
}

static const struct of_device_id cs35l35_of_match[] = {
	{.compatible = "cirrus,cs35l35"},
	{},
};
MODULE_DEVICE_TABLE(of, cs35l35_of_match);

static const struct i2c_device_id cs35l35_id[] = {
	{"cs35l35", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, cs35l35_id);

static struct i2c_driver cs35l35_i2c_driver = {
	.driver = {
		.name = "cs35l35",
		.of_match_table = cs35l35_of_match,
	},
	.id_table = cs35l35_id,
	.probe = cs35l35_i2c_probe,
	.remove = cs35l35_i2c_remove,
};
module_i2c_driver(cs35l35_i2c_driver);

MODULE_DESCRIPTION("ASoC CS35L35 driver");
MODULE_AUTHOR("Brian Austin, Cirrus Logic Inc, <brian.austin@cirrus.com>");
MODULE_LICENSE("GPL");
