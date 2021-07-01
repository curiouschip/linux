/*
 * Driver Pip DAC
 * Author:
 *	Jason Frame <jwf@jasonframe.co.uk>
 *	Copyright 2020
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/notifier.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <sound/control.h>

#include "../codecs/tlv320aic31xx.h"

/*
 * These are the actual connectors on the board
 */
static const struct snd_soc_dapm_widget pip_dac_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Pip Headphone Jack", NULL),
	SND_SOC_DAPM_SPK("Pip Builtin Speaker", NULL),
	SND_SOC_DAPM_MIC("Pip Builtin Mic", NULL),
};

/*
 * Map board connectors to codec widgets
 * NB - format is { SINK, CONTROL, SOURCE }
 */
static const struct snd_soc_dapm_route pip_dac_audio_map[] = {
	/* Outputs */
	{"Pip Headphone Jack", NULL, "HPL"},
	{"Pip Headphone Jack", NULL, "HPR"},
	{"Pip Builtin Speaker", NULL, "SPK"},

	/* Inputs */
	{"MIC1RP", NULL, "Pip Builtin Mic"},
	{"MIC1RP", NULL, "MICBIAS"},
	{"Pip Builtin Mic", NULL, "MICBIAS"},
};

static struct snd_soc_jack_pin headset_pins[] = {
	{
		.pin = "Pip Headphone Jack",
		.mask = SND_JACK_HEADPHONE,
	},
	{
		.pin = "Pip Builtin Speaker",
		.mask = SND_JACK_HEADPHONE,
		.invert = 1,
	},
};

// /* Setup DAPM widgets and paths */
// static const struct snd_soc_dapm_widget pip_dac_dapm_widgets[] = {
// 	SND_SOC_DAPM_HP("Headphone Jack", NULL),
// 	SND_SOC_DAPM_LINE("Line Out", NULL),
// 	SND_SOC_DAPM_LINE("Line In", NULL),
// 	SND_SOC_DAPM_INPUT("CM_L"),
// 	SND_SOC_DAPM_INPUT("CM_R"),
// };

// static const struct snd_soc_dapm_route pip_dac_audio_map[] = {
// 	/* Line Inputs are connected to
// 	 * (IN1_L | IN1_R)
// 	 * (IN2_L | IN2_R)
// 	 * (IN3_L | IN3_R)
// 	 */
// 	{"IN1_L", NULL, "Line In"},
// 	{"IN1_R", NULL, "Line In"},
// 	{"IN2_L", NULL, "Line In"},
// 	{"IN2_R", NULL, "Line In"},
// 	{"IN3_L", NULL, "Line In"},
// 	{"IN3_R", NULL, "Line In"},

// 	/* Mic is connected to IN2_L and IN2_R */
// 	{"Left ADC", NULL, "Mic Bias"},
// 	{"Right ADC", NULL, "Mic Bias"},

// 	/* Headphone connected to HPL, HPR */
// 	{"Headphone Jack", NULL, "HPL"},
// 	{"Headphone Jack", NULL, "HPR"},

// 	/* Speakers connected to LOL and LOR */
// 	{"Line Out", NULL, "LOL"},
// 	{"Line Out", NULL, "LOR"},
// };


static int headset_status_changed(struct notifier_block *nb, unsigned long action, void *data) {
	printk(KERN_INFO "headphone status changed: %ld\n", action);
	return 0;
}



/*
 * I think this function is used to do any one-time codec-specific config
 */
static int pip_dac_card_init(struct snd_soc_pcm_runtime *rtd)
{
	int ret = 0;
	struct snd_soc_component *component;
	static struct snd_soc_jack headset_jack;
	static struct notifier_block nb;

	printk(KERN_INFO "enter init\n");

	// /* TODO: init of the codec specific dapm data, ignore suspend/resume */
	component = rtd->codec_dai->component;
	
    //ret = snd_soc_component_update_bits(component, AIC31XX_HSDETECT, 0x07 << 2, 5 << 2);
	//printk(KERN_INFO "update glitch detection result: %d\n", ret);
	
	// Set M-terminal with CM input, feed forward resistance 21k
	//ret = snd_soc_component_update_bits(component, AIC31XX_MICPGAMI, 0x03 << 6, 0x02 << 6);
	//printk(KERN_INFO "update mic result: %d\n", ret);
	
	ret = snd_soc_card_jack_new(rtd->card, "Headset Detect", SND_JACK_HEADSET, &headset_jack, NULL, 0);
	if (ret) {
		printk(KERN_INFO "failed to create jack: %d\n", ret);
		return ret;
	}

	ret = snd_soc_jack_add_pins(&headset_jack, ARRAY_SIZE(headset_pins), headset_pins);
	if (ret) {
		printk(KERN_INFO "failed to add jack pins: %d\n", ret);
		return ret;
	}

	printk(KERN_INFO "adding notifier\n");
	nb.notifier_call = headset_status_changed;
	nb.priority = 1;
	snd_soc_jack_notifier_register(&headset_jack, &nb);

	printk(KERN_INFO "setting jack\n");

	ret = snd_soc_component_set_jack(component, &headset_jack, NULL);	
	if (ret) {
		printk(KERN_INFO "failed to attach jack to codec: %d\n", ret);
		return ret;
	}

	// Set glitch detection to max
	// Note - this must be done *after* setting the jack because jack connection handler
	// in the codec driver will overwrite these bits with zeroes.
    //ret = snd_soc_component_write(component, AIC31XX_HSDETECT, 0x07 << 2, 5 << 2);
    //ret = snd_soc_component_write(component, AIC31XX_HSDETECT, 5 << 2);
	//printk(KERN_INFO "update glitch detection result: %d\n", ret);

	printk(KERN_INFO "init done!\n");

	return 0;
}

// 	// snd_soc_component_update_bits(component, AIC32X4_MICBIAS, 0x78,
// 	// 			      AIC32X4_MICBIAS_LDOIN |
// 	// 			      AIC32X4_MICBIAS_2075V);
// 	// snd_soc_component_update_bits(component, AIC32X4_PWRCFG, 0x08,
// 	// 			      AIC32X4_AVDDWEAKDISABLE);
// 	// snd_soc_component_update_bits(component, AIC32X4_LDOCTL, 0x01,
// 	// 			      AIC32X4_LDOCTLEN);

// 	return 0;
// }

/*
 * I have no idea what this function is for.
 * It's registered as part of the ops struct field.
 */
static int pip_dac_card_startup(struct snd_pcm_substream *substream)
{
	//struct snd_pcm_runtime *runtime = substream->runtime;

	// /*
	//  * Set codec to 48Khz Sampling, Stereo I/O and 16 bit audio
	//  */
	// runtime->hw.channels_max = 2;
	// snd_pcm_hw_constraint_list(runtime, 0, SNDRV_PCM_HW_PARAM_CHANNELS,
	// 			   &audiosense_constraints_ch);

	// runtime->hw.formats = SNDRV_PCM_FMTBIT_S16_LE;
	// snd_pcm_hw_constraint_msbits(runtime, 0, 16, 16);


	// snd_pcm_hw_constraint_list(substream->runtime, 0,
	// 			   SNDRV_PCM_HW_PARAM_RATE,
	// 			   &audiosense_constraints_rates);
	return 0;
}

/*
 * Set up the clocks for a specific playback in here
 */
static int pip_dac_card_hw_params(
		struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	int ret = 0;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;

	/* Set the codec to use a 12MHz clock! */
	ret = snd_soc_dai_set_sysclk(codec_dai, AIC31XX_PLL_CLKIN_BCKL,
				     12000000, SND_SOC_CLOCK_IN);
	if (ret) {
		dev_err(rtd->card->dev,
			"could not set codec driver clock params\n");
		return ret;
	}
	
	return 0;
}

static struct snd_soc_ops pip_dac_card_ops = {
	.startup 	= pip_dac_card_startup,
	.hw_params 	= pip_dac_card_hw_params,
};

static struct snd_soc_dai_link_component pip_dac_cpu = {
	.dai_name = "bcm2708-i2s.0"
};

static struct snd_soc_dai_link_component pip_dac_codec = {
	.name = "tlv320aic31xx-codec.1-0018",
	.dai_name = "tlv320aic31xx-hifi"
};

static struct snd_soc_dai_link_component pip_dac_platform = {
	.name = "bcm2708-i2s.0"
};

static struct snd_soc_dai_link pip_dac_card_dai[] = {
	{
		.name           = "TLV320AIC3100 Audio",
		.stream_name    = "TLV320AIC3100 Hifi Audio",
		.cpus = &pip_dac_cpu,
		.num_cpus = 1,
		.codecs = &pip_dac_codec,
		.num_codecs = 1,
		.platforms = &pip_dac_platform,
		.num_platforms = 1,
		// .dai_fmt        = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
		// 	SND_SOC_DAIFMT_CBM_CFM,
		.dai_fmt        = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_NB_NF |
		SND_SOC_DAIFMT_CBS_CFS,
		.ops            = &pip_dac_card_ops,
		.init           = pip_dac_card_init,
	},
};

static struct snd_soc_card pip_dac_card = {
	.name				= "pip-dac",
	.driver_name		= "pip-dac",
	.owner				= THIS_MODULE,
	.dai_link			= pip_dac_card_dai,
	.num_links			= ARRAY_SIZE(pip_dac_card_dai),
	.dapm_widgets		= pip_dac_dapm_widgets,
	.num_dapm_widgets 	= ARRAY_SIZE(pip_dac_dapm_widgets),
	.dapm_routes		= pip_dac_audio_map,
	.num_dapm_routes  	= ARRAY_SIZE(pip_dac_audio_map),
};

static int pip_dac_card_probe(struct platform_device *pdev)
{
	struct snd_soc_card *card = &pip_dac_card;
	struct snd_soc_dai_link *dai = &pip_dac_card_dai[0];
	struct device_node *i2s_node = pdev->dev.of_node;
	int ret = 0;

	card->dev = &pdev->dev;

	if (!dai) {
		dev_err(&pdev->dev, "DAI not found. Missing or Invalid\n");
		return -EINVAL;
	}

	i2s_node = of_parse_phandle(pdev->dev.of_node, "i2s-controller", 0);
	if (!i2s_node) {
		dev_err(&pdev->dev,
			"Property 'i2s-controller' missing or invalid\n");
		return -EINVAL;
	}

	dai->cpus[0].dai_name = NULL;
	dai->cpus[0].of_node = i2s_node;
	dai->platforms[0].name = NULL;
	dai->platforms[0].of_node = i2s_node;

	of_node_put(i2s_node);

	printk(KERN_INFO "registering card!\n");

	ret = snd_soc_register_card(card);
	if (ret && ret != -EPROBE_DEFER)
		dev_err(&pdev->dev,
			"snd_soc_register_card() failed: %d\n", ret);

	printk(KERN_INFO "pip dac registered: %d\n", ret);

	return ret;
}

static int pip_dac_card_remove(struct platform_device *pdev)
{
	struct snd_soc_card *card = platform_get_drvdata(pdev);
	return snd_soc_unregister_card(card);
}

static const struct of_device_id pip_dac_card_of_match[] = {
	{ .compatible = "pip,pip-dac", },
	{},
};
MODULE_DEVICE_TABLE(of, pip_dac_card_of_match);

static struct platform_driver pip_dac_card_driver = {
	.driver = {
		.name = "pip-dac-snd-card",
		.owner = THIS_MODULE,
		.of_match_table = pip_dac_card_of_match,
	},
	.probe = pip_dac_card_probe,
	.remove = pip_dac_card_remove,
};

module_platform_driver(pip_dac_card_driver);

MODULE_AUTHOR("Jason Frame <jwf@jasonframe.co.uk>");
MODULE_DESCRIPTION("Pip driver for TLV320AIC31xx Audio");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:pip-dac");

