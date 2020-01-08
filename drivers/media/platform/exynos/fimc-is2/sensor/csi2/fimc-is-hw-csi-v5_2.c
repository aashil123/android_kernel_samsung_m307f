/*
 * Samsung Exynos SoC series FIMC-IS2 driver
 *
 * exynos fimc-is2 hw csi control functions
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#if IS_ENABLED(CONFIG_EXYNOS_OTP)
#include <linux/exynos_otp.h>
#endif

#include "fimc-is-hw-api-common.h"
#include "fimc-is-config.h"
#include "fimc-is-type.h"
#include "fimc-is-regs.h"
#include "fimc-is-device-csi.h"
#include "fimc-is-hw.h"
#include "fimc-is-hw-csi-v5_2.h"
#include "fimc-is-device-sensor.h"

void csi_hw_phy_otp_config(u32 __iomem *base_reg, u32 instance)
{
#if IS_ENABLED(CONFIG_EXYNOS_OTP)
#endif
}

int csi_hw_reset(u32 __iomem *base_reg)
{
	int ret = 0;
	u32 retry = 10;

	fimc_is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
			&csi_fields[CSIS_F_SW_RESET], 1);

	while (--retry) {
		udelay(10);
		if (fimc_is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
			&csi_fields[CSIS_F_SW_RESET]) != 1)
			break;
	}

	if (!retry) {
		err("reset is fail(%d)", retry);
		ret = -EINVAL;
		goto p_err;
	}

p_err:
	return ret;
}

int csi_hw_s_settle(u32 __iomem *base_reg,
	u32 settle)
{
	return 0;
}

int csi_hw_s_phy_default_value(u32 __iomem *base_reg, u32 instance)
{
	return 0;
}

int csi_hw_s_lane(u32 __iomem *base_reg,
	struct fimc_is_image *image, u32 lanes, u32 mipi_speed)
{
	u32 val = fimc_is_hw_get_reg(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL]);
	u32 lane;

	/* lane number */
	val = fimc_is_hw_set_field_value(val, &csi_fields[CSIS_F_LANE_NUMBER], lanes);
	fimc_is_hw_set_reg(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL], val);

	switch (lanes) {
	case CSI_DATA_LANES_1:
		/* lane 0 */
		lane = (0x1);
		break;
	case CSI_DATA_LANES_2:
		/* lane 0 + lane 1 */
		lane = (0x3);
		break;
	case CSI_DATA_LANES_3:
		/* lane 0 + lane 1 + lane 2 */
		lane = (0x7);
		break;
	case CSI_DATA_LANES_4:
		/* lane 0 + lane 1 + lane 2 + lane 3 */
		lane = (0xF);
		break;
	default:
		err("lanes is invalid(%d)", lanes);
		lane = (0xF);
		break;
	}

	fimc_is_hw_set_field(base_reg, &csi_regs[CSIS_R_PHY_CMN_CTRL],
			&csi_fields[CSIS_F_ENABLE_DAT], lane);

	return 0;
}

int csi_hw_s_control(u32 __iomem *base_reg, u32 id, u32 value)
{
	switch (id) {
	case CSIS_CTRL_INTERLEAVE_MODE:
		/* interleave mode */
		fimc_is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
				&csi_fields[CSIS_F_INTERLEAVE_MODE], value);
		break;
	case CSIS_CTRL_LINE_RATIO:
		/* line irq ratio */
		fimc_is_hw_set_field(base_reg, &csi_regs[CSIS_R_LINE_INTR_CH0],
				&csi_fields[CSIS_F_LINE_INTR_CH_N], value);
		break;
	case CSIS_CTRL_DMA_ABORT_REQ:
		/* dma abort req */
		fimc_is_hw_set_field(base_reg, &csi_vcdma_cmn_regs[CSIS_R_DMA_CMN_CTRL],
				&csi_vcdma_cmn_fields[CSIS_F_DMA_ABORT_REQ], value);
		break;
	case CSIS_CTRL_ENABLE_LINE_IRQ:
		fimc_is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_INT_MSK1],
				&csi_fields[CSIS_F_MSK_LINE_END], value);
		break;
	case CSIS_CTRL_PIXEL_ALIGN_MODE:
		fimc_is_hw_set_reg(base_reg, &csi_regs[CSIS_R_DEBUG_OPTION_SUITE], value);
		break;
	default:
		err("control id is invalid(%d)", id);
		break;
	}

	return 0;
}

int csi_hw_s_config(u32 __iomem *base_reg,
	u32 vc, struct fimc_is_vci_config *config, u32 width, u32 height)
{
	int ret = 0;
	u32 val, parallel;
	u32 pd_mode;
	u32 pixel_mode = CSIS_PIXEL_MODE_DUAL;
	struct fimc_is_sensor_cfg *sensor_cfg;

	sensor_cfg = container_of(config, struct fimc_is_sensor_cfg, input[vc]);
	pd_mode = sensor_cfg->pd_mode;

	if (vc > CSI_VIRTUAL_CH_3) {
		err("invalid vc(%d)", vc);
		ret = -EINVAL;
		goto p_err;
	}

	if ((config->hwformat == HW_FORMAT_YUV420_8BIT) ||
		(config->hwformat == HW_FORMAT_YUV422_8BIT))
		parallel = 1;
	else
		parallel = 0;

	if (vc == CSI_VIRTUAL_CH_0) {
		if (pd_mode == PD_MOD1 || pd_mode == PD_MSPD)
			pixel_mode = CSIS_PIXEL_MODE_QUAD;
	} else if (vc == CSI_VIRTUAL_CH_1) {
		if (pd_mode == PD_MOD3)
			pixel_mode = CSIS_PIXEL_MODE_QUAD;
	}

	val = fimc_is_hw_get_reg(base_reg, &csi_regs[CSIS_R_ISP_CONFIG_CH0 + (vc * 3)]);
	val = fimc_is_hw_set_field_value(val, &csi_fields[CSIS_F_VIRTUAL_CHANNEL], config->map);
	val = fimc_is_hw_set_field_value(val, &csi_fields[CSIS_F_DATAFORMAT], config->hwformat);
	val = fimc_is_hw_set_field_value(val, &csi_fields[CSIS_F_DECOMP_EN], config->hwformat >> DECOMP_EN_BIT);
	val = fimc_is_hw_set_field_value(val, &csi_fields[CSIS_F_DECOMP_PREDICT],
		config->hwformat >> DECOMP_PREDICT_BIT);
	val = fimc_is_hw_set_field_value(val, &csi_fields[CSIS_F_POTF_ONLY], parallel);
	val = fimc_is_hw_set_field_value(val, &csi_fields[CSIS_F_PIXEL_MODE], pixel_mode);
	fimc_is_hw_set_reg(base_reg, &csi_regs[CSIS_R_ISP_CONFIG_CH0 + (vc * 3)], val);

	val = fimc_is_hw_get_reg(base_reg, &csi_regs[CSIS_R_ISP_RESOL_CH0 + (vc * 3)]);
	val = fimc_is_hw_set_field_value(val, &csi_fields[CSIS_F_VRESOL], height);
	val = fimc_is_hw_set_field_value(val, &csi_fields[CSIS_F_HRESOL], width);
	fimc_is_hw_set_reg(base_reg, &csi_regs[CSIS_R_ISP_RESOL_CH0 + (vc * 3)], val);

	if (config->hwformat & (1 << DECOMP_EN_BIT)) {
		val = fimc_is_hw_get_reg(base_reg, &csi_regs[CSIS_R_ISP_SYNC_CH0 + (vc * 3)]);
		val = fimc_is_hw_set_field_value(val, &csi_fields[CSIS_F_HSYNC_LINTV], 7);
		fimc_is_hw_set_reg(base_reg, &csi_regs[CSIS_R_ISP_SYNC_CH0 + (vc * 3)], val);
	}

p_err:
	return ret;
}

#ifdef CONFIG_USE_SENSOR_GROUP
int csi_hw_s_config_dma(u32 __iomem *base_reg, u32 vc, struct fimc_is_frame_cfg *cfg, u32 hwformat)
#else
int csi_hw_s_config_dma(u32 __iomem *base_reg, u32 vc, struct fimc_is_image *image, u32 hwformat)
#endif
{
	int ret = 0;
	u32 val;
	u32 dma_dim = 0;
	u32 dma_pack12 = 0;
	u32 dma_format = 3; /* reserved value */
	u32 dma_stg_mode;

	if (vc > CSI_VIRTUAL_CH_3) {
		err("invalid vc(%d)", vc);
		ret = -EINVAL;
		goto p_err;
	}

#ifdef CONFIG_USE_SENSOR_GROUP
	if (!cfg->format) {
		err("cfg->format is null");
		ret = -EINVAL;
		goto p_err;
	}

	if (cfg->format->pixelformat == V4L2_PIX_FMT_SBGGR10 ||
		cfg->format->pixelformat == V4L2_PIX_FMT_SBGGR12 ||
		cfg->format->pixelformat == V4L2_PIX_FMT_PRIV_MAGIC)
#else
	if (image->format.pixelformat == V4L2_PIX_FMT_SBGGR10 ||
		image->format.pixelformat == V4L2_PIX_FMT_SBGGR12 ||
		image->format.pixelformat == V4L2_PIX_FMT_PRIV_MAGIC)
#endif
		dma_pack12 = CSIS_REG_DMA_PACK12;
	else
		dma_pack12 = CSIS_REG_DMA_NORMAL;

#ifdef CONFIG_USE_SENSOR_GROUP
	switch (cfg->format->pixelformat)
#else
	switch (image->format.pixelformat)
#endif
	{
	case V4L2_PIX_FMT_SGRBG8:
	case V4L2_PIX_FMT_SBGGR8:
		dma_dim = CSIS_REG_DMA_1D_DMA;
		break;
	default:
		dma_dim = CSIS_REG_DMA_2D_DMA;
		break;
	}

	switch (hwformat) {
	case HW_FORMAT_RAW10:
	case HW_FORMAT_RAW6_DA:
	case HW_FORMAT_RAW7_DS:
	case HW_FORMAT_RAW8_DS:
		dma_format = 0;
		dma_stg_mode = CSIS_REG_DMA_STG_LEGACY_MODE;
		break;
	case HW_FORMAT_RAW12:
		dma_format = 1;
		dma_stg_mode = CSIS_REG_DMA_STG_LEGACY_MODE;
		break;
	case HW_FORMAT_RAW14:
	case HW_FORMAT_RAW10_DA:
		dma_format = 2;
		dma_stg_mode = CSIS_REG_DMA_STG_LEGACY_MODE;
		break;
	case HW_FORMAT_USER:
	case HW_FORMAT_EMBEDDED_8BIT:
	case HW_FORMAT_YUV420_8BIT:
	case HW_FORMAT_YUV420_10BIT:
	case HW_FORMAT_YUV422_8BIT:
	case HW_FORMAT_YUV422_10BIT:
	case HW_FORMAT_RGB565:
	case HW_FORMAT_RGB666:
	case HW_FORMAT_RGB888:
	case HW_FORMAT_RAW6:
	case HW_FORMAT_RAW7:
	case HW_FORMAT_RAW8:
		dma_stg_mode = CSIS_REG_DMA_STG_PACKET_MODE;
		break;
	default:
		err("invalid data format (%02X)", hwformat);
		ret = -EINVAL;
		goto p_err;
	}

	val = fimc_is_hw_get_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_FMT]);
	val = fimc_is_hw_set_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_DIM], dma_dim);
	val = fimc_is_hw_set_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_PACK12], dma_pack12);
	val = fimc_is_hw_set_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_STORAGE_MODE], dma_stg_mode);

	if (dma_stg_mode == CSIS_REG_DMA_STG_LEGACY_MODE) {
		/* below setting is only valid for storage legacy mode */
		val = fimc_is_hw_set_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_DATAFORMAT], dma_format);
		fimc_is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_FMT], val);

		val = fimc_is_hw_get_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_RESOL]);
#ifdef CONFIG_USE_SENSOR_GROUP
		val = fimc_is_hw_set_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_RESOL], cfg->width);
#else
		val = fimc_is_hw_set_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_RESOL], image->window.width);
#endif
		fimc_is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_RESOL], val);
	} else if (dma_stg_mode == CSIS_REG_DMA_STG_PACKET_MODE) {
		fimc_is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_FMT], val);
	}

	fimc_is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_FCNTSEQ], 0);

p_err:
	return ret;
}

int csi_hw_s_irq_msk(u32 __iomem *base_reg, bool on)
{
	u32 otf_msk;
	u32 otf_msk1;

	/* default setting */
	if (on) {
		/* base interrupt setting */
		otf_msk = CSIS_IRQ_MASK0;
		otf_msk1 = CSIS_IRQ_MASK1;
#if defined(SUPPORTED_EARLYBUF_DONE_SW) || defined(SUPPORTED_EARLYBUF_DONE_HW)
		otf_msk = fimc_is_hw_set_field_value(otf_msk, &csi_fields[CSIS_F_FRAMEEND], 0x0);
#endif
	} else {
		otf_msk = 0;
		otf_msk1 = 0;
	}

	fimc_is_hw_set_reg(base_reg, &csi_regs[CSIS_R_CSIS_INT_MSK0], otf_msk);
	fimc_is_hw_set_reg(base_reg, &csi_regs[CSIS_R_CSIS_INT_MSK1], otf_msk1);

	return 0;
}

static void csi_hw_g_err_types_from_err(u32 err_src0, u32 err_src1, u32 *err_id)
{
	int i = 0;
	u32 sot_hs_err = 0;
	u32 lost_fs_err = 0;
	u32 lost_fe_err = 0;
	u32 ovf_err = 0;
	u32 wrong_cfg_err = 0;
	u32 err_ecc_err = 0;
	u32 crc_err = 0;
	u32 err_id_err = 0;
	u32 vresol_err = 0;
	u32 hresol_err = 0;
#ifdef USE_SENSOR_IF_CPHY
	u32 mal_crc_err = 0;
	u32 inval_code_hs = 0;
	u32 sot_sync_hs = 0
#endif

	sot_hs_err    = fimc_is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_ERR_SOT_HS]);
	lost_fs_err   = fimc_is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_ERR_LOST_FS]);
	lost_fe_err   = fimc_is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_ERR_LOST_FE]);
	ovf_err       = fimc_is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_ERR_OVER]);
	wrong_cfg_err = fimc_is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_ERR_WRONG_CFG]);
	err_ecc_err   = fimc_is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_ERR_ECC]);
	crc_err       = fimc_is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_ERR_CRC_DPHY]);
	err_id_err    = fimc_is_hw_get_field_value(err_src0, &csi_fields[CSIS_F_ERR_ID]);

	vresol_err    = fimc_is_hw_get_field_value(err_src1, &csi_fields[CSIS_F_MSK_VRESOL_MISMATCH]);
	hresol_err    = fimc_is_hw_get_field_value(err_src1, &csi_fields[CSIS_F_MSK_HRESOL_MISMATCH]);

#ifdef USE_SENSOR_IF_CPHY
	inval_code_hs = fimc_is_hw_get_field_value(err_src1, &csi_fields[CSIS_F_RXINVALIDCODEHS]);
	sot_sync_hs   = fimc_is_hw_get_field_value(err_src1, &csi_fields[CSIS_F_ERRSOTSYNCHS]);
	mal_crc_err   = fimc_is_hw_get_field_value(err_src1, &csi_fields[CSIS_F_MAL_CRC]);
	crc_err      |= fimc_is_hw_get_field_value(err_src1, &csi_fields[CSIS_F_ERR_CRC_CPHY]);
#endif

	for (i = 0; i < CSI_VIRTUAL_CH_MAX; i++) {
		err_id[i] |= ((sot_hs_err    & (1 << i)) ? (1 << CSIS_ERR_SOT_VC) : 0);
		err_id[i] |= ((lost_fs_err   & (1 << i)) ? (1 << CSIS_ERR_LOST_FS_VC) : 0);
		err_id[i] |= ((lost_fe_err   & (1 << i)) ? (1 << CSIS_ERR_LOST_FE_VC) : 0);
		err_id[i] |= ((ovf_err       & (1 << i)) ? (1 << CSIS_ERR_OVERFLOW_VC) : 0);
		err_id[i] |= ((wrong_cfg_err & (1 << i)) ? (1 << CSIS_ERR_WRONG_CFG) : 0);
		err_id[i] |= ((err_ecc_err   & (1 << i)) ? (1 << CSIS_ERR_ECC) : 0);
		err_id[i] |= ((crc_err       & (1 << i)) ? (1 << CSIS_ERR_CRC) : 0);
		err_id[i] |= ((err_id_err    & (1 << i)) ? (1 << CSIS_ERR_ID) : 0);

		err_id[i] |= ((vresol_err    & (1 << i)) ? (1 << CSIS_ERR_VRESOL_MISMATCH) : 0);
		err_id[i] |= ((hresol_err    & (1 << i)) ? (1 << CSIS_ERR_HRESOL_MISMATCH) : 0);
#ifdef USE_SENSOR_IF_CPHY
		err_id[i] |= ((inval_code_hs & (1 << i)) ? (1 << CSIS_ERR_INVALID_CODE_HS) : 0);
		err_id[i] |= ((sot_sync_hs & (1 << i)) ? (1 << CSIS_ERR_SOT_SYNC_HS) : 0);
		err_id[i] |= ((mal_crc_err & (1 << i)) ? (1 << CSIS_ERR_MAL_CRC) : 0);
#endif
	}
}

static bool csi_hw_g_value_of_err(u32 __iomem *base_reg, u32 otf_src0, u32 otf_src1, u32 *err_id)
{
	u32 err_src0 = (otf_src0 & CSIS_ERR_MASK0);
	u32 err_src1 = (otf_src1 & CSIS_ERR_MASK1);
	bool err_flag = false;

	if (err_src0 || err_src1) {
		csi_hw_g_err_types_from_err(err_src0, err_src1, err_id);
		err_flag = true;
	}

	return err_flag;
}

int csi_hw_g_irq_src(u32 __iomem *base_reg, struct csis_irq_src *src, bool clear)
{
	u32 otf_src0;
	u32 otf_src1;

	otf_src0 = fimc_is_hw_get_reg(base_reg, &csi_regs[CSIS_R_CSIS_INT_SRC0]);
	otf_src1 = fimc_is_hw_get_reg(base_reg, &csi_regs[CSIS_R_CSIS_INT_SRC1]);

	if (clear) {
		fimc_is_hw_set_reg(base_reg, &csi_regs[CSIS_R_CSIS_INT_SRC0], otf_src0);
		fimc_is_hw_set_reg(base_reg, &csi_regs[CSIS_R_CSIS_INT_SRC1], otf_src1);
	}

#ifdef CONFIG_CSIS_V4_0
	/* HACK: For dual scanario in EVT0, we should use only DMA interrupt */
	src->otf_start = src->dma_start;
	src->otf_end = src->dma_end;
#else
	src->otf_start = fimc_is_hw_get_field_value(otf_src0, &csi_fields[CSIS_F_FRAMESTART]);
	src->otf_end = fimc_is_hw_get_field_value(otf_src0, &csi_fields[CSIS_F_FRAMEEND]);
#endif
	src->line_end = fimc_is_hw_get_field_value(otf_src1, &csi_fields[CSIS_F_MSK_LINE_END]);
	src->err_flag = csi_hw_g_value_of_err(base_reg, otf_src0, otf_src1, (u32 *)src->err_id);

	return 0;
}

void csi_hw_s_frameptr(u32 __iomem *base_reg, u32 vc, u32 number, bool clear)
{
	u32 frame_ptr = 0;
	u32 val = fimc_is_hw_get_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_CTRL]);

	frame_ptr = fimc_is_hw_get_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_UPDT_FRAMEPTR]);
	if (clear)
		frame_ptr |= (1 << number);
	else
		frame_ptr &= ~(1 << number);
	val = fimc_is_hw_set_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_UPDT_FRAMEPTR], frame_ptr);
	fimc_is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_CTRL], val);
}

u32 csi_hw_g_frameptr(u32 __iomem *base_reg, u32 vc)
{
	u32 frame_ptr = 0;
	u32 val = fimc_is_hw_get_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_ACT_CTRL]);

	frame_ptr = fimc_is_hw_get_field_value(val, &csi_vcdma_fields[CSIS_F_ACTIVE_DMA_N_FRAMEPTR]);

	return frame_ptr;
}

void csi_hw_s_dma_addr(u32 __iomem *base_reg, u32 vc, u32 number, u32 addr)
{
	u32 val = fimc_is_hw_get_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_FCNTSEQ]);

	csi_hw_s_frameptr(base_reg, vc, number, false);

	fimc_is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_ADDR1 + number], addr);

	val |= 1 << number;
	fimc_is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_FCNTSEQ], val);
}

void csi_hw_s_multibuf_dma_addr(u32 __iomem *base_reg, u32 vc, u32 number, u32 addr)
{
	u32 val = fimc_is_hw_get_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_FCNTSEQ]);

	fimc_is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_ADDR1 + number], addr);

	val |= 1 << number;
	fimc_is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_FCNTSEQ], val);
}

void csi_hw_s_output_dma(u32 __iomem *base_reg, u32 vc, bool enable)
{
	u32 val = fimc_is_hw_get_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_CTRL]);

	val = fimc_is_hw_set_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_ENABLE], enable);
	val = fimc_is_hw_set_field_value(val, &csi_vcdma_fields[CSIS_F_DMA_N_UPDT_PTR_EN], enable);
	fimc_is_hw_set_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_CTRL], val);
}

bool csi_hw_g_output_dma_enable(u32 __iomem *base_reg, u32 vc)
{
	/* if DMA_DISABLE field value is 1, this means dma output is disabled */
	if (fimc_is_hw_get_field(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_CTRL],
			&csi_vcdma_fields[CSIS_F_DMA_N_ENABLE]))
		return true;
	else
		return false;
}

bool csi_hw_g_output_cur_dma_enable(u32 __iomem *base_reg, u32 vc)
{
	u32 val = fimc_is_hw_get_reg(base_reg, &csi_vcdma_regs[CSIS_R_DMA0_ACT_CTRL]);
	/* if DMA_ENABLE field value is 1, this means dma output is enabled */
	bool dma_enable = fimc_is_hw_get_field_value(val, &csi_vcdma_fields[CSIS_F_ACTIVE_DMA_N_ENABLE]);

	return dma_enable;
}

void csi_hw_set_start_addr(u32 __iomem *base_reg, u32 number, u32 addr)
{
	u32 __iomem *target_reg;

	if (number == 0) {
		target_reg = base_reg + TO_WORD_OFFSET(0x30);
	} else {
		number--;
		target_reg = base_reg + TO_WORD_OFFSET(0x200 + (0x4*number));
	}

	writel(addr, target_reg);
}

int csi_hw_dma_common_reset(void)
{
	return 0;
}

#define GET_DMA_CH(x, y)	((x) & (1 << (y)))
int csi_hw_s_dma_common_dynamic(u32 __iomem *base_reg, size_t size, unsigned int dma_ch)
{
	u32 val;
	u32 sram0_split;
	u32 sram1_split;
	u32 max;

	if (!base_reg)
		return 0;

	/* Common DMA Arbitration Priority register */
	/* CSIS_DMA_F_DMA_ARB_PRI_1 : 1 = CSIS2 DMA has a high priority */
	/* CSIS_DMA_F_DMA_ARB_PRI_1 : 2 = CSIS3 DMA has a high priority */
	/* CSIS_DMA_F_DMA_ARB_PRI_0 : 1 = CSIS0 DMA has a high priority */
	/* CSIS_DMA_F_DMA_ARB_PRI_0 : 2 = CSIS1 DMA has a high priority */
	val = fimc_is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_ARB_PRI]);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_ARB_PRI_1], 0x1);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_ARB_PRI_0], 0x2);
	fimc_is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_ARB_PRI], val);

	/* Common DMA Control register */
	/* CSIS_DMA_F_IP_PROCESSING : 1 = Q-channel clock enable  */
	/* CSIS_DMA_F_IP_PROCESSING : 0 = Q-channel clock disable */
	val = fimc_is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_CTRL]);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_IP_PROCESSING], 0x1);
	fimc_is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_CTRL], val);

	/* Common DMA SRAM split register */
	/* CSIS_DMA_F_DMA_SRAM1_SPLIT : internal SRAM1 is 10KB (640 * 16 bytes) */
	/* CSIS_DMA_F_DMA_SRAM0_SPLIT : internal SRAM0 is 10KB (640 * 16 bytes) */
	/* This register can be set between 0 to 640 */
	max = (u32)(size / 16);
	sram0_split = max / 2;
	sram1_split = max / 2;

	/* FIXME */
	if (GET_DMA_CH(dma_ch, 0) && !GET_DMA_CH(dma_ch, 1))
		sram0_split = max;
	else if (!GET_DMA_CH(dma_ch, 0) && GET_DMA_CH(dma_ch, 1))
		sram0_split = 4;

	if (GET_DMA_CH(dma_ch, 2) && !GET_DMA_CH(dma_ch, 3))
		sram1_split = max;
	else if (!GET_DMA_CH(dma_ch, 2) && GET_DMA_CH(dma_ch, 3))
		sram1_split = 4;

	if (GET_DMA_CH(dma_ch, 4))
		sram0_split = max;

	val = fimc_is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_SRAM_SPLIT]);
	/* FIXME */
	/*
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_SRAM1_SPLIT], sram1_split);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_SRAM0_SPLIT], sram0_split);
	*/
	/* FIXME: only 36KB */
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_SRAM1_SPLIT], 0x480);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_SRAM0_SPLIT], 0x480);
	fimc_is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_SRAM_SPLIT], val);

	/* Common DMA Martix register */
	/* CSIS_DMA_F_DMA_MATRIX : Under Table see */
	/*       CSIS0      CSIS1      CSIS2      CSIS3  */
	/* 0  : SRAM0_0    SRAM0_0    SRAM1_0    SRAM1_1 */
	/* 2  : SRAM0_0    SRAM1_0    SRAM0_1    SRAM1_1 */
	/* 5  : SRAM0_0    SRAM1_1    SRAM1_0    SRAM0_1 */
	/* 14 : SRAM1_0    SRAM0_1    SRAM0_0    SRAM1_1 */
	/* 16 : SRAM1_0    SRAM1_1    SRAM0_0    SRAM0_1 */
	/* 17 : SRAM1_0    SRAM1_1    SRAM0_1    SRAM0_0 */
	/* 22 : SRAM1_1    SRAM1_0    SRAM0_0    SRAM0_1 */
	/* 23 : SRAM1_1    SRAM1_0    SRAM0_1    SRAM0_0 */
	val = fimc_is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_MATRIX]);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_MATRIX], 0x0);
	fimc_is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_MATRIX], val);

	return 0;
}

int csi_hw_s_dma_common(u32 __iomem *base_reg)
{
	u32 val;

	if (!base_reg)
		return 0;

	/* Common DMA Arbitration Priority register */
	/* CSIS_DMA_F_DMA_ARB_PRI_1 : 1 = CSIS2 DMA has a high priority */
	/* CSIS_DMA_F_DMA_ARB_PRI_1 : 2 = CSIS3 DMA has a high priority */
	/* CSIS_DMA_F_DMA_ARB_PRI_0 : 1 = CSIS0 DMA has a high priority */
	/* CSIS_DMA_F_DMA_ARB_PRI_0 : 2 = CSIS1 DMA has a high priority */
	val = fimc_is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_ARB_PRI]);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_ARB_PRI_1], 0x1);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_ARB_PRI_0], 0x2);
	fimc_is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_ARB_PRI], val);

	/* Common DMA Control register */
	/* CSIS_DMA_F_IP_PROCESSING : 1 = Q-channel clock enable  */
	/* CSIS_DMA_F_IP_PROCESSING : 0 = Q-channel clock disable */
	val = fimc_is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_CTRL]);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_IP_PROCESSING], 0x1);
	fimc_is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_CTRL], val);

	/* Common DMA SRAM split register */
	/* CSIS_DMA_F_DMA_SRAM1_SPLIT : internal SRAM1 is 10KB (640 * 16 bytes) */
	/* CSIS_DMA_F_DMA_SRAM0_SPLIT : internal SRAM0 is 10KB (640 * 16 bytes) */
	/* This register can be set between 0 to 640 */
	val = fimc_is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_SRAM_SPLIT]);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_SRAM1_SPLIT], 0x140);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_SRAM0_SPLIT], 0x140);
	fimc_is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_SRAM_SPLIT], val);

	/* Common DMA Martix register */
	/* CSIS_DMA_F_DMA_MATRIX : Under Table see */
	/*       CSIS0      CSIS1      CSIS2      CSIS3  */
	/* 0  : SRAM0_0    SRAM0_0    SRAM1_0    SRAM1_1 */
	/* 2  : SRAM0_0    SRAM1_0    SRAM0_1    SRAM1_1 */
	/* 5  : SRAM0_0    SRAM1_1    SRAM1_0    SRAM0_1 */
	/* 14 : SRAM1_0    SRAM0_1    SRAM0_0    SRAM1_1 */
	/* 16 : SRAM1_0    SRAM1_1    SRAM0_0    SRAM0_1 */
	/* 17 : SRAM1_0    SRAM1_1    SRAM0_1    SRAM0_0 */
	/* 22 : SRAM1_1    SRAM1_0    SRAM0_0    SRAM0_1 */
	/* 23 : SRAM1_1    SRAM1_0    SRAM0_1    SRAM0_0 */
	val = fimc_is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_MATRIX]);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_DMA_MATRIX], 0x0);
	fimc_is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_COMMON_DMA_MATRIX], val);

	return 0;
}

int csi_hw_s_dma_common_pattern_enable(u32 __iomem *base_reg,
	u32 width, u32 height, u32 fps, u32 clk)
{
	u32 val;
	int clk_mhz;
	int vvalid;
	int vblank;
	int vblank_size;
	u32 hblank = 70;	/* This value should be guided according to 3AA HW constrain. */
	u32 v_to_hblank = 0x80;	/* This value should be guided according to 3AA HW constrain. */
	u32 h_to_vblank = 0x40;	/* This value should be guided according to 3AA HW constrain. */

	if (!width || (width % 8 != 0)) {
		err("A width(%d) is not aligned to 8", width);
		return -EINVAL;
	}

	clk_mhz = clk / 1000000;

	/*
	 * V-valid Calculation:
	 * The unit of v-valid is usec.
	 * 2 means 2ppc.
	 */
	vvalid = (width * height) / (clk_mhz * 2);

	/*
	 * V-blank Calculation:
	 * The unit of v-blank is usec.
	 * v-blank operates with 1ppc.
	 */
	vblank = ((1000000 / fps) - vvalid);
	if (vblank < 0) {
		vblank = 1000; /* 1000 us */
		info("FPS is too high. So, FPS is adjusted forcely. vvalid(%d us), vblank(%d us)\n",
			vvalid, vblank);
	}
	vblank_size = vblank * clk_mhz;

	val = fimc_is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_TEST_PATTERN_CTRL]);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_VTOHBLANK], v_to_hblank);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_HBLANK], hblank);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_HTOVBALNK], h_to_vblank);
	fimc_is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_TEST_PATTERN_CTRL], val);

	val = fimc_is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_TEST_PATTERN_RESOL]);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_TP_VSIZE], height);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_TP_HSIZE], width);
	fimc_is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_TEST_PATTERN_RESOL], val);

	val = fimc_is_hw_get_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_TEST_PATTERN_ENABLE]);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_PPC_MODE], CSIS_PIXEL_MODE_DUAL);
	val = fimc_is_hw_set_field_value(val, &csi_dma_fields[CSIS_DMA_F_VBLANK], vblank_size);
	fimc_is_hw_set_reg(base_reg, &csi_dma_regs[CSIS_DMA_R_TEST_PATTERN_ENABLE], val);

	fimc_is_hw_set_field(base_reg, &csi_dma_regs[CSIS_DMA_R_TEST_PATTERN_ENABLE],
			&csi_dma_fields[CSIS_DMA_F_TESTPATTERN], 1);

	info("Enable Pattern Generator: size(%d x %d), fps(%d), clk(%d Hz), vvalid(%d us), vblank(%d us)\n",
		width, height, fps, clk, vvalid, vblank);

	return 0;
}

void csi_hw_s_dma_common_pattern_disable(u32 __iomem *base_reg)
{
	fimc_is_hw_set_field(base_reg, &csi_dma_regs[CSIS_DMA_R_TEST_PATTERN_ENABLE],
		&csi_dma_fields[CSIS_DMA_F_TESTPATTERN], 0);
}

int csi_hw_enable(u32 __iomem *base_reg)
{
	/* update shadow */
	fimc_is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
			&csi_fields[CSIS_F_UPDATE_SHADOW], 0xF);

	/* PHY selection */
#ifdef USE_SENSOR_IF_CPHY
	fimc_is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
			&csi_fields[CSIS_F_PHY_SEL], 1);
#else
	fimc_is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
			&csi_fields[CSIS_F_PHY_SEL], 0);
#endif

	/* PHY on */
	fimc_is_hw_set_field(base_reg, &csi_regs[CSIS_R_PHY_CMN_CTRL],
			&csi_fields[CSIS_F_ENABLE_CLK], 1);

	/* Q-channel disable */
#ifdef ENABLE_HWACG_CONTROL
	fimc_is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
			&csi_fields[CSIS_F_QCHANNEL_EN], 0);
#endif

	/* csi enable */
	fimc_is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
			&csi_fields[CSIS_F_CSI_EN], 1);

	return 0;
}

int csi_hw_disable(u32 __iomem *base_reg)
{
	/* PHY off */
	fimc_is_hw_set_field(base_reg, &csi_regs[CSIS_R_PHY_CMN_CTRL],
			&csi_fields[CSIS_F_ENABLE_CLK], 0);
	fimc_is_hw_set_field(base_reg, &csi_regs[CSIS_R_PHY_CMN_CTRL],
			&csi_fields[CSIS_F_ENABLE_DAT], 0);

	/* csi disable */
	fimc_is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
			&csi_fields[CSIS_F_CSI_EN], 0);

	/* Q-channel enable */
#ifdef ENABLE_HWACG_CONTROL
	fimc_is_hw_set_field(base_reg, &csi_regs[CSIS_R_CSIS_CMN_CTRL],
			&csi_fields[CSIS_F_QCHANNEL_EN], 1);
#endif

	return 0;
}

int csi_hw_dump(u32 __iomem *base_reg)
{
	info("CSI Core SFR DUMP (v%d.%d[.%d[.%d]])\n",
		fimc_is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_VERSION],
			&csi_fields[CSIS_F_MAJOR]),
		fimc_is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_VERSION],
			&csi_fields[CSIS_F_MINOR]),
		fimc_is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_VERSION],
			&csi_fields[CSIS_F_REVISION]),
		fimc_is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_VERSION],
			&csi_fields[CSIS_F_DERIVATIVE]));

	fimc_is_hw_dump_regs(base_reg, csi_regs, CSIS_REG_CNT);

	return 0;
}

int csi_hw_vcdma_dump(u32 __iomem *base_reg)
{
	info("CSI VC/DMA SFR DUMP (v%d.%d[.%d[.%d]])\n",
		fimc_is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_VERSION],
			&csi_fields[CSIS_F_MAJOR]),
		fimc_is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_VERSION],
			&csi_fields[CSIS_F_MINOR]),
		fimc_is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_VERSION],
			&csi_fields[CSIS_F_REVISION]),
		fimc_is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_VERSION],
			&csi_fields[CSIS_F_DERIVATIVE]));

	fimc_is_hw_dump_regs(base_reg, csi_vcdma_regs, CSIS_VCDMA_REG_CNT);

	return 0;
}

int csi_hw_vcdma_cmn_dump(u32 __iomem *base_reg)
{
	info("CSI VC/DMA Common SFR DUMP (v%d.%d[.%d[.%d]])\n",
		fimc_is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_VERSION],
			&csi_fields[CSIS_F_MAJOR]),
		fimc_is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_VERSION],
			&csi_fields[CSIS_F_MINOR]),
		fimc_is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_VERSION],
			&csi_fields[CSIS_F_REVISION]),
		fimc_is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_VERSION],
			&csi_fields[CSIS_F_DERIVATIVE]));

	fimc_is_hw_dump_regs(base_reg, csi_vcdma_cmn_regs, CSIS_VCDMA_CMN_REG_CNT);

	return 0;
}

int csi_hw_phy_dump(u32 __iomem *base_reg, u32 instance)
{
	info("CSI PHY SFR DUMP (v%d.%d[.%d[.%d]])\n",
		fimc_is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_VERSION],
			&csi_fields[CSIS_F_MAJOR]),
		fimc_is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_VERSION],
			&csi_fields[CSIS_F_MINOR]),
		fimc_is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_VERSION],
			&csi_fields[CSIS_F_REVISION]),
		fimc_is_hw_get_field(base_reg, &csi_regs[CSIS_R_CSIS_VERSION],
			&csi_fields[CSIS_F_DERIVATIVE]));

	fimc_is_hw_dump_regs(base_reg, csi_phy_regs[instance], PHY_REG_CNT);

	return 0;
}

int csi_hw_common_dma_dump(u32 __iomem *base_reg)
{
	info("CSI COMMON DMA SFR DUMP (v5.1)\n");
	fimc_is_hw_dump_regs(base_reg, csi_dma_regs, CSIS_DMA_REG_CNT);

	return 0;
}

int csi_hw_s_dma_irq_msk(u32 __iomem *base_reg, bool on)
{
	u32 dma_msk;

	/* default setting */
	if (on) {
		/* base interrupt setting */
		dma_msk = CSIS_DMA_IRQ_MASK;
	} else {
		dma_msk = 0;
	}

	fimc_is_hw_set_reg(base_reg, &csi_vcdma_cmn_regs[CSIS_R_DMA_INT_MASK], dma_msk);

	return 0;
}

int csi_hw_g_dma_irq_src_vc(u32 __iomem *base_reg, struct csis_irq_src *src, u32 vc_phys, bool clear)
{
	u32 dma_src;
	u32 mask = 0;

	dma_src = fimc_is_hw_get_reg(base_reg, &csi_vcdma_cmn_regs[CSIS_R_DMA_INT_SRC]);

	mask |= 1 << (csi_vcdma_cmn_fields[CSIS_F_DMA_ABORT_DONE].bit_start);
	mask |= 1 << (csi_vcdma_cmn_fields[CSIS_F_DMA_ERROR].bit_start);
	mask |= 1 << (csi_vcdma_cmn_fields[CSIS_F_DMA_OTF_OVERLAP].bit_start + vc_phys);
	mask |= 1 << (csi_vcdma_cmn_fields[CSIS_F_DMA_FRM_END].bit_start + vc_phys);
	mask |= 1 << (csi_vcdma_cmn_fields[CSIS_F_DMA_FRM_START].bit_start + vc_phys);
	mask |= 1 << (csi_vcdma_cmn_fields[CSIS_F_DMA_LINE_END].bit_start + vc_phys);
	dma_src = dma_src & mask;

	if (clear)
		fimc_is_hw_set_reg(base_reg, &csi_vcdma_cmn_regs[CSIS_R_DMA_INT_SRC], dma_src);

	src->dma_start = fimc_is_hw_get_field_value(dma_src, &csi_vcdma_cmn_fields[CSIS_F_DMA_FRM_START]);
	src->dma_end = fimc_is_hw_get_field_value(dma_src, &csi_vcdma_cmn_fields[CSIS_F_DMA_FRM_END]);

	if (dma_src & (1 << (csi_vcdma_cmn_fields[CSIS_F_DMA_OTF_OVERLAP].bit_start + vc_phys)))
		src->err_id[vc_phys] |= 1 << CSIS_ERR_OTF_OVERLAP;

	if (dma_src & (1 << csi_vcdma_cmn_fields[CSIS_F_DMA_ERROR].bit_start))
		src->err_id[vc_phys] |= 1 << CSIS_ERR_DMA_ERR_DMAFIFO_FULL;

	if (dma_src & (1 << csi_vcdma_cmn_fields[CSIS_F_DMA_ABORT_DONE].bit_start))
		src->err_id[vc_phys] |= 1 << CSIS_ERR_DMA_ABORT_DONE;

	return 0;
}

int csi_hw_s_config_dma_cmn(u32 __iomem *base_reg, u32 vc, u32 actual_vc, u32 hwformat)
{
	int ret = 0;
	u32 val;
	u32 otf_format;
	u32 dma_input_path;

	if (vc > CSI_VIRTUAL_CH_3) {
		err("invalid vc(%d)", vc);
		ret = -EINVAL;
		goto p_err;
	}

	if (vc == CSI_VIRTUAL_CH_0) {
		switch (hwformat) {
		case HW_FORMAT_RAW10:
		case HW_FORMAT_RAW6_DA:
		case HW_FORMAT_RAW7_DS:
		case HW_FORMAT_RAW8_DS:
			otf_format = 0;
			dma_input_path = CSIS_REG_DMA_INPUT_OTF;
			break;
		case HW_FORMAT_RAW12:
			otf_format = 1;
			dma_input_path = CSIS_REG_DMA_INPUT_OTF;
			break;
		case HW_FORMAT_RAW14:
		case HW_FORMAT_RAW10_DA:
			otf_format = 2;
			dma_input_path = CSIS_REG_DMA_INPUT_OTF;
			break;
		case HW_FORMAT_RAW8:
		case HW_FORMAT_USER:
			otf_format = 3;
			dma_input_path = CSIS_REG_DMA_INPUT_PRL;
			break;
		default:
			err("invalid data format (%02X)", hwformat);
			ret = -EINVAL;
			goto p_err;
		}

		val = fimc_is_hw_get_reg(base_reg, &csi_vcdma_cmn_regs[CSIS_R_OTF_FORMAT]);
		val = fimc_is_hw_set_field_value(val, &csi_vcdma_cmn_fields[CSIS_F_OTF_FORMAT], otf_format);
		fimc_is_hw_set_reg(base_reg, &csi_vcdma_cmn_regs[CSIS_R_OTF_FORMAT], val);

		val = fimc_is_hw_get_reg(base_reg, &csi_vcdma_cmn_regs[CSIS_R_DMA_DATA_CTRL]);
		val = fimc_is_hw_set_field_value(val, &csi_vcdma_cmn_fields[CSIS_F_DMA_INPUT_PATH_CH0],
			dma_input_path);
		fimc_is_hw_set_reg(base_reg, &csi_vcdma_cmn_regs[CSIS_R_DMA_DATA_CTRL], val);
	}

p_err:
	return ret;
}

int csi_hw_s_phy_config(u32 __iomem *base_reg,
	u32 lanes, u32 mipi_speed, u32 settle, u32 instance)
{
	return 0;
}

#define MAX_PHY_CFG 10
int csi_hw_s_phy_set(struct phy *phy, u32 lanes, u32 mipi_speed,
		u32 settle, u32 instance)
{
	int ret = 0;
	unsigned int phy_cfg[MAX_PHY_CFG];

	/*
	 * [0]: the version of PHY (major << 16 | minor)
	 * [1]: the type of PHY (mode << 16 | type)
	 * [2]: the number of lanes (zero-based)
	 * [3]: the data rate
	 * [4]: the settle value for the data rate
	 */

	phy_cfg[0] = 0x0502 << 16;
	/*
	 * Exynos9820 EVT0 4lanes: 0x0000
	 * Exynos9820 EVT0 2lanes: 0x0001
	 * Other 4lanes: 0x0002
	 * Other 2lanes: 0x0003
	 */
	if (IS_ENABLED(CONFIG_SOC_EXYNOS9820_EVT0))
		phy_cfg[0] |= (instance == CSI_ID_E ? 0x0001 :  0x0000);
	else
		phy_cfg[0] |= (instance == CSI_ID_E ? 0x0003 :  0x0002);

	phy_cfg[1] = 0xD << 16;
	if ((instance == CSI_ID_A) || (instance == CSI_ID_C))
		phy_cfg[1] |= 0xDC;
	else
		phy_cfg[1] |= 0xD;

	phy_cfg[2] = lanes;
	phy_cfg[3] = mipi_speed;
	phy_cfg[4] = settle;

	ret = phy_set(phy, 0, (void *)phy_cfg);
	if (ret) {
		err("failed to set PHY");
		return ret;
	}

	return ret;
}

