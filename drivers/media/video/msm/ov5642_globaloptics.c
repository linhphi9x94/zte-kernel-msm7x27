/*
 * drivers/media/video/msm/ov5642_globaloptics.c
 *
 * Refer to drivers/media/video/msm/mt9d112.c
 * For IC OV5642 of Module GLOBALOPTICS: 5.0Mp, 1/4-Inch System-On-A-Chip (SOC) CMOS Digital Image Sensor
 *
 * Copyright (C) 2009-2010 ZTE Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Created by zhang.shengjie@zte.com.cn
 */
/*-----------------------------------------------------------------------------------------
  when         who      what, where, why                         comment tag
  --------     ----     -------------------------------------    --------------------------
  2011-06-24   lijing   optimize adaptor flow                    ZTE_CAM_LJ_20110624 
  2010-12-15   ygl      add the interface of touch AF for froyo  ZTE_YGL_CAM_20101214                        
  2010-12-09   jia      add failure process to avoid standby     ZTE_JIA_CAM_20101209
                        current exception problem
  2010-12-07   zt       set gain no mater value of               ZTE_ZT_CAM_20101203
                        g_preview_exposure under normal light or backlight
  2010-12-03   zt       modified for capturing under backlight   ZTE_ZT_CAM_20101203
  2010-11-23   ygl      add time delay of 400ms after sensor     ZTE_YGL_CAM_20101123
                        init to avoid green display when preview
                        mode is resumed after snapshot or review
  2010-10-26   lijing   fix bug of no preview image after        ZTE_LJ_CAM_20101026
                        changing effect mode repeatedly
  2010-10-26   zt       add the interface of exposure            ZTE_ZT_CAM_20101026_04
                        compensation for foryo
  2010-10-26   zt       update the config of saturation          ZTE_ZT_CAM_20101026
  2010-10-26   zt       update the config of MWB cloudy          ZTE_ZT_CAM_20101026
  2010-10-18   ygl      decrease time delay in                   ZTE_YGL_CAM_20101018
                        "ov5642_sensor_init" to pass CTS
  2010-09-29   zt       update the saturation parameter          ZTE_ZT_CAM_20100929
  2010-09-21   zt       update the saturation parameter          ZTE_ZT_CAM_20100921
  2010-09-14   zt       solve the problem that exit the          ZTE_ZT_CAM_20100914
                        camera application when adjusting 
                        the brightness due to register 0x5580;
                        modify setting of register 0x5580 for
                        contrast, saturation, and effect
  2010-09-08   jia      add exception process of i2c_del_driver  ZTE_JIA_CAM_20100908
  2010-09-07   jia      add process for variable initialization  JIA_CAM_20100907
  2010-09-06   zt       add the register configuration of        ZT_CAM_20100906
                        ISO, sharpness, antibanding and effect,
                        update the brightness configuration
  2010-08-31   jia      add functions of ISO, sharpness, and     JIA_CAM_20100831
                        antibanding
  2010-08-20   zt       modified the configuration               ZT_CAM_20100820
                        parameter of brightness
  2010-08-19   jia      fix slave address error for              JIA_CAM_20100819
                        CONFIG_SENSOR_ADAPTER
  2010-08-18   jia      refactor code for support of             JIA_CAM_20100818
                        CONFIG_SENSOR_ADAPTER,
                        add support for CONFIG_SENSOR_INFO,
                        add support for OV5642_PROBE_WORKQUEUE
  2010-08-13   zt       modified the frequency of MCLK           ZT_CAM_20100813_002
  2010-08-13   zt       modified the value of wb configration    ZT_CAM_20100813_001
  2010-06-08   jia      refactor process of                      JIA_CAM_20100608
                        "CONFIG_SENSOR_ADAPTER"
  2009-04-30   lijing   add for mt9t11x and ov5642 adapter       LIJING_CAM_20100430
  2010-04-29   zh.shj   Get FTM flag to adjust the               ZTE_MSM_CAMERA_ZHSHJ_001
                        initialize process of camera  
  2010-01-28   lijing   modify brighness for snapshot            ZTE_MSM_CAMERA_LIJING_001
  2010-01-28   lijing   adjust MCLK switch setting               ZTE_MSM_CAMERA_LIJING_001
    
  2009-12-11   jia.jia  decrease time delay during AF trigger,   ZTE_MSM_CAMERA_JIA_001
                        switch between preview and snapshot     
  2009-12-07   jia.jia  Remove functions called for              ZTE_CAMERA_JIA_001
                        turning on/off FLASH LED, which is
                        moved to 
                        vfe_process_QDSP_VFETASK_MSG_VFE_RESET_ACK
                        in HAL
  2009-12-02   jia.jia  Add function of AF with keypress         ZTE_CAMERA_JIA_001
                        Improve effects of preview and
                        snapshot
  2009-11-12   jia.jia  Refactor code for initialization         ZTE_MSM_CAMERA_JIA_001
  2009-11-02   zh.shj   Add AF,AWB & Brightness settings         ZTE_MSM_CAMERA_ZHSHJ_001 
  2009-10-24   jia.jia  Merged from kernel-v4515                 ZTE_MSM_CAMERA_JIA_001
------------------------------------------------------------------------------------------*/

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#include <media/msm_camera.h>
#include <mach/gpio.h>
#include "ov5642.h"
#include <linux/slab.h>

/*-----------------------------------------------------------------------------------------
 *
 * MACRO DEFINITION
 *
 *----------------------------------------------------------------------------------------*/
/*
 * To implement the parallel init process
 */
#define OV5642_PROBE_WORKQUEUE

#if defined(OV5642_PROBE_WORKQUEUE)
#include <linux/workqueue.h>
static struct platform_device *pdev_wq = NULL;
static struct workqueue_struct *ov5642_wq = NULL;
static void ov5642_workqueue(struct work_struct *work);
static DECLARE_WORK(ov5642_cb_work, ov5642_workqueue);
#endif /* defined(OV5642_PROBE_WORKQUEUE) */

#define ENOINIT 100 /*have not power up,so don't need to power down*/

/*
 * OV5642 Registers and their values
 */
/* Sensor I2C Board Name */
#define OV5642_I2C_BOARD_NAME "ov5642"

/* Sensor I2C Bus Number (Master I2C Controller: 0) */
#define OV5642_I2C_BUS_ID  (0)

/* Sensor I2C Slave Address */
#define OV5642_SLAVE_WR_ADDR 0x78 /* replaced by "msm_i2c_devices.addr" */
#define OV5642_SLAVE_RD_ADDR 0x79 /* replaced by "msm_i2c_devices.addr" */

/* Sensor I2C Device ID */
#define REG_OV5642_MODEL_ID    0x300A
#define OV5642_MODEL_ID        0x5642

/* SOC Registers */
#define REG_OV5642_SENSOR_RESET     0x001A

/* CAMIO Input MCLK (MHz) */
#define OV5642_CAMIO_MCLK  24000000 // from 12Mhz to 24Mhz

/* GPIO For Lowest-Power mode (SHUTDOWN mode) */
#define OV5642_GPIO_SHUTDOWN_CTL   32

/* GPIO For Sensor Clock Switch */

#define OV5642_GPIO_SWITCH_CTL     39

/* 
 * modify for mclk switch for msm7627_joe
 */
#if defined(CONFIG_MACH_RAISE)
#define OV5642_GPIO_SWITCH_VAL     1
#elif defined(CONFIG_MACH_R750) || defined(CONFIG_MACH_JOE)
#define OV5642_GPIO_SWITCH_VAL     0
#else
#define OV5642_GPIO_SWITCH_VAL     1
#endif

#define CAPTURE_FRAMERATE 375
#define PREVIEW_FRAMERATE 1500

/* ZTE_YGL_CAM_20101214
 * add the interface of touch AF for froyo
 */
#define OV5642_AF_WINDOW_FULL_WIDTH  64
#define OV5642_AF_WINDOW_FULL_HEIGHT 48
/* 
 * Auto Exposure Gain Factor (disused)
 * #define AE_GAIN_FACTOR  50    // 2.2: underexposure, 200: overexposure, 50: normal exposure
 */

/*-----------------------------------------------------------------------------------------
 *
 * TYPE DECLARATION
 *
 *----------------------------------------------------------------------------------------*/
struct ov5642_work_t {
    struct work_struct work;
};

static struct ov5642_work_t *ov5642_sensorw;
static struct i2c_client *ov5642_client;

struct ov5642_ctrl_t {
    const struct msm_camera_sensor_info *sensordata;
};

/*-----------------------------------------------------------------------------------------
 *
 * GLOBAL VARIABLE DEFINITION
 *
 *----------------------------------------------------------------------------------------*/
static struct ov5642_ctrl_t *ov5642_ctrl;

static uint32_t g_preview_exposure;
static uint32_t g_preview_line_width;
static uint16_t g_preview_gain;
static uint32_t g_preview_frame_rate = 0;

#if 0 //ZTE_ZT_CAM_20101203

static uint16_t temp_l,temp_m,temp_h;
#endif

DECLARE_MUTEX(ov5642_sem);

/*-----------------------------------------------------------------------------------------
 *
 * FUNCTION DECLARATION
 *
 *----------------------------------------------------------------------------------------*/
static int ov5642_sensor_init(const struct msm_camera_sensor_info *data);
static int ov5642_sensor_config(void __user *argp);
static int ov5642_sensor_release(void);

static int32_t ov5642_i2c_add_driver(void);
static void ov5642_i2c_del_driver(void);

extern int32_t msm_camera_power_backend(enum msm_camera_pwr_mode_t pwr_mode);
extern int msm_camera_clk_switch(const struct msm_camera_sensor_info *data,
                                         uint32_t gpio_switch,
                                         uint32_t switch_val);

/*
 * Get FTM flag to adjust 
 * the initialize process 
 * of camera
 */
#ifdef CONFIG_ZTE_PLATFORM
#ifdef CONFIG_ZTE_FTM_FLAG_SUPPORT
extern int zte_get_ftm_flag(void);
#endif
#endif

/*-----------------------------------------------------------------------------------------
 *
 * FUNCTION DEFINITION
 *
 *----------------------------------------------------------------------------------------*/
/*
 * Hard standby of sensor
 * on: =1, enter into hard standby
 * on: =0, exit from hard standby
 *
 * Hard standby mode is set by register of REG_OV5642_STANDBY_CONTROL.
 */
static int ov5642_hard_standby(const struct msm_camera_sensor_info *dev, uint32_t on)
{
    int rc;

    CDBG("%s: entry\n", __func__);

    rc = gpio_request(dev->sensor_pwd, "ov5642");
    if (0 == rc)
    {
        /* ignore "rc" */
        rc = gpio_direction_output(dev->sensor_pwd, on);

        /* time delay for the entry into standby */
        mdelay(10);
    }

    gpio_free(dev->sensor_pwd);

    return rc;
}

/*
 * Hard reset: RESET_BAR pin (active LOW)
 * Hard reset has the same effect as the soft reset.
 */
static int __attribute__((unused))ov5642_hard_reset(const struct msm_camera_sensor_info *dev)
{
    int rc = 0;

    CDBG("%s: entry\n", __func__);

    rc = gpio_request(dev->sensor_reset, "ov5642");
    if (0 == rc)
    {
        /* ignore "rc" */
        rc = gpio_direction_output(dev->sensor_reset, 1);

        /* time delay for asserting RESET */
        mdelay(10);

        /* ignore "rc" */
        rc = gpio_direction_output(dev->sensor_reset, 0);

        /*
          * RESET_BAR pulse width: Min 70 EXTCLKs
          * EXTCLKs: = MCLK (i.e., OV5642_CAMIO_MCLK)
          */
        mdelay(10);

        /* ignore "rc" */
        rc = gpio_direction_output(dev->sensor_reset, 1);

        /*
          * Time delay before first serial write: Min 100 EXTCLKs
          * EXTCLKs: = MCLK (i.e., OV5642_CAMIO_MCLK)
          */
        mdelay(10);
    }

    gpio_free(dev->sensor_reset);

    return rc;
}

static int32_t ov5642_i2c_txdata(unsigned short saddr,
                                       unsigned char *txdata,
                                       int length)
{
    struct i2c_msg msg[] = {
        {
            .addr  = saddr,
            .flags = 0,
            .len   = length,
            .buf   = txdata,
        },
    };

    if (i2c_transfer(ov5642_client->adapter, msg, 1) < 0)
    {
        CCRT("%s: failed!\n", __func__);
        return -EIO;
    }

    return 0;
}

static int32_t ov5642_i2c_write(unsigned short saddr,
                                    unsigned short waddr,
                                    unsigned short wdata,
                                    enum ov5642_width_t width)
{
    int32_t rc = -EFAULT;
    unsigned char buf[3];

    memset(buf, 0, sizeof(buf));

    switch (width)
    {
        case WORD_LEN:
        {
            buf[0] = (waddr & 0xFF00) >> 8;
            buf[1] = (waddr & 0x00FF);
#if 0 // 16-bit data width 
            buf[2] = (wdata & 0xFF00) >> 8;
            buf[3] = (wdata & 0x00FF);
            rc = ov5642_i2c_txdata(saddr, buf, 4);
#else // 8-bit data width for OV sensor  
            buf[2] = (wdata & 0x00FF);
            rc = ov5642_i2c_txdata(saddr, buf, 3);
#endif
        }
        break;

        case BYTE_LEN:
        {
            buf[0] = (waddr & 0xFF00) >> 8;
            buf[1] = (waddr & 0x00FF);
            buf[2] = wdata;

            rc = ov5642_i2c_txdata(saddr, buf, 3);
        }
        break;

        default:
        {
        }
        break;
    }

    if (rc < 0)
    {
        CCRT("%s: waddr = 0x%x, wdata = 0x%x, failed!\n", __func__, waddr, wdata);
    }

    return rc;
}

static int32_t ov5642_i2c_write_table(struct ov5642_i2c_reg_conf const *reg_conf_tbl,
                                             int len)
{
    uint32_t i;
    int32_t rc = 0;

    for (i = 0; i < len; i++)
    {
        rc = ov5642_i2c_write(ov5642_client->addr,
                               reg_conf_tbl[i].waddr,
                               reg_conf_tbl[i].wdata,
                               reg_conf_tbl[i].width);
        if (rc < 0)
        {
        
            break;
        }

        if (reg_conf_tbl[i].mdelay_time != 0)
        {
            /* time delay for writing I2C data */
            mdelay(reg_conf_tbl[i].mdelay_time);
        }
    }

    return rc;
}

static int ov5642_i2c_rxdata(unsigned short saddr,
                                   unsigned char *rxdata,
                                   int length)
{
    struct i2c_msg msgs[] = {
        {
            .addr  = saddr,
            .flags = 0,
            .len   = 2,
            .buf   = rxdata,
        },
        {
            .addr  = saddr,
            .flags = I2C_M_RD,
            .len   = length,
            .buf   = rxdata,
        },
    };

    if (i2c_transfer(ov5642_client->adapter, msgs, 2) < 0)
    {
        CCRT("%s: failed!\n", __func__);
        return -EIO;
    }

    return 0;
}

static int32_t ov5642_i2c_read(unsigned short saddr,
                                     unsigned short raddr,
                                     unsigned short *rdata,
                                     enum ov5642_width_t width)
{
    int32_t rc = 0;
    unsigned char buf[4];

    if (!rdata)
    {
        CCRT("%s: rdata points to NULL!\n", __func__);
        return -EIO;
    }

    memset(buf, 0, sizeof(buf));

    switch (width)
    {
        case WORD_LEN:
        {
            buf[0] = (raddr & 0xFF00) >> 8;
            buf[1] = (raddr & 0x00FF);

            rc = ov5642_i2c_rxdata(saddr, buf, 2);
            if (rc < 0)
            {
                return rc;
            }

            *rdata = buf[0] << 8 | buf[1];
        }
        break;

        case BYTE_LEN:
        {
            buf[0] = (raddr & 0xFF00) >> 8;
            buf[1] = (raddr & 0x00FF);

            rc = ov5642_i2c_rxdata(saddr, buf, 2);
            if (rc < 0)
            {
                return rc;
            }

            *rdata = buf[0]| 0x0000;
        }
        break;

        default:
        {
        }
        break;
    }

    if (rc < 0)
    {
        CCRT("%s: failed!\n", __func__);
    }

    return rc;
}

static int32_t ov5642_set_lens_roll_off(void)
{
    int32_t rc = 0;

    CDBG("%s: entry\n", __func__);

    rc = ov5642_i2c_write_table(ov5642_regs.rftbl, ov5642_regs.rftbl_size);

    return rc;
}

static long ov5642_set_brightness(int8_t brightness)
{
    long rc = 0;
    uint16_t tmp_reg = 0;

    CDBG("%s: entry: brightness=%d\n", __func__, brightness);

    switch(brightness)
    {
        case CAMERA_BRIGHTNESS_0:
        {
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5001, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5589, 0x0030, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x558a, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0008;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x558a, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }   
        }
        break;

        case CAMERA_BRIGHTNESS_1:
        {
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5001, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5589, 0x0020, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x558a, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0008;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x558a, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }   
        }
        break;

        case CAMERA_BRIGHTNESS_2:
        {
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5001, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5589, 0x0010, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x558a, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0008;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x558a, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }   
        }
        break;

        case CAMERA_BRIGHTNESS_3:
        {
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5001, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5589, 0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x558a, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00F7;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x558a, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }   
        }
        break;

        case CAMERA_BRIGHTNESS_4:
        {
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5001, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5589, 0x0010, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x558a, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00F7;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x558a, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }   
        }
        break;

        case CAMERA_BRIGHTNESS_5:
        {
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5001, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5589, 0x0020, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x558a, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00F7;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x558a, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case CAMERA_BRIGHTNESS_6:
        {
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5001, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5589, 0x0030, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x558a, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00F7;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x558a, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }   
        }
        break;

        default:
        {
            CCRT("%s: parameter error!\n", __func__);
            return -EFAULT;
        }            
    }

    return rc;
}

static long ov5642_set_contrast(int8_t contrast_val)
{
    long rc = 0;
    uint16_t tmp_reg = 0;

    CINF("%s: entry: contrast_val=%d\n", __func__, contrast_val);

    switch(contrast_val)
    {
        case CAMERA_CONTRAST_0:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, 0x00ff, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5580, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0004;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5580, tmp_reg, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5587, 0x0010, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5588, 0x0010, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x558a, 0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
        }
        break;

        case CAMERA_CONTRAST_1:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, 0x00ff, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5580, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0004;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5580, tmp_reg, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5587, 0x0018, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5588, 0x0018, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x558a, 0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
        }
        break;

        case CAMERA_CONTRAST_2:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, 0x00ff, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5580, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0004;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5580, tmp_reg, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5587, 0x0020, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5588, 0x0020, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x558a, 0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
        }
        break;

        case CAMERA_CONTRAST_3:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, 0x00ff, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5580, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0004;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5580, tmp_reg, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5587, 0x0028, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5588, 0x0028, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x558a, 0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
        }
        break;

        case CAMERA_CONTRAST_4:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, 0x00ff, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5580, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0004;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5580, tmp_reg, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5587, 0x0030, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5588, 0x0030, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x558a, 0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
        }        
        break;

        default:
        {
            CCRT("%s: parameter error!\n", __func__);
            return -EFAULT;
        }            
    }
    return rc;
}

static long ov5642_set_saturation(int8_t saturation_val)
{
    long rc = 0;
    uint16_t tmp_reg = 0;

    CDBG("%s: entry: saturation_val=%d\n", __func__, saturation_val);

    switch(saturation_val)
    {
        case CAMERA_SATURATION_0:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, 0x00ff, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5583, 0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5584, 0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5580, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0002;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5580, tmp_reg, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
        }
        break;

        case CAMERA_SATURATION_1:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, 0x00ff, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5583, 0x0018, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5584, 0x0018, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5580, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0002;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5580, tmp_reg, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }  
        }
        break;
        
        case CAMERA_SATURATION_2:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, 0x00ff, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5583, 0x003b, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5584, 0x003b, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5580, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0002;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5580, tmp_reg, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }  
        }
        break;
               
        case CAMERA_SATURATION_3:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, 0x00ff, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5583, 0x0050, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5584, 0x0050, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5580, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0002;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5580, tmp_reg, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }  
        }
        break;
        
        case CAMERA_SATURATION_4:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, 0x00ff, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5583, 0x0060, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5584, 0x0060, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5580, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0002;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5580, tmp_reg, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }  
        }        
        break;
        
        default:
        {
            CCRT("%s: parameter error!\n", __func__);
            return -EFAULT;
        }            
    }

    return rc;
}

/*
 * Auto Focus Trigger
 */
static int32_t ov5642_af_trigger(void)
{
    int32_t rc;
    uint16_t af_status;
    uint32_t i;

    CDBG("%s: entry\n", __func__);

    rc = ov5642_i2c_write(ov5642_client->addr, 0x3024, 0x0001, WORD_LEN);
    if (rc < 0)
    {
        CCRT("%s: failed, rc=%d!\n", __func__, rc);
        return rc;
    }

    rc = ov5642_i2c_write(ov5642_client->addr, 0x3024, 0x0003, WORD_LEN);
    if (rc < 0)
    {
        CCRT("%s: failed, rc=%d!\n",__func__, rc);        
        return rc;
    }

    /*
      * af_status: = 0x02,  AF is done successfully
      *            != 0x02, AF is being done
      *
      * i: < 100, time delay for tuning AF
      *    >= 100, time out for tuning AF
      */
    for (i = 0; (i < 1000) && (0x0002 != af_status); ++i)
    {
        af_status = 0x0000;
        rc = ov5642_i2c_read(ov5642_client->addr, 0x3027, &af_status, BYTE_LEN);
        if (rc < 0)
        {
           return rc;
        }        

        CDBG("%s: AF Statu_x=%x\n", __func__, af_status);
        mdelay(15);
    }
    
    return 0;
}

static long ov5642_set_wb(int8_t wb_mode)
{
    long rc = 0;

    CDBG("%s: entry: wb_mode=%d\n", __func__, wb_mode);

    switch(wb_mode)
    {
        case CAMERA_WB_MODE_AWB:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3406, 0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case CAMERA_WB_MODE_SUNLIGHT:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3406 ,0x0001, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3400 ,0x0007, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3401 ,0x0032, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3402 ,0x0004, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3403 ,0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3404 ,0x0005, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3405 ,0x0036, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case CAMERA_WB_MODE_INCANDESCENT:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3406 ,0x0001, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3400 ,0x0004, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3401 ,0x0088, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3402 ,0x0004, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3403 ,0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3404 ,0x0008, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3405 ,0x00b6, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;
        
        case CAMERA_WB_MODE_FLUORESCENT:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3406 ,0x0001, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3400 ,0x0006, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3401 ,0x0013, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3402 ,0x0004, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3403 ,0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3404 ,0x0007, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3405 ,0x00e2, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
        }
        break; 

        case CAMERA_WB_MODE_CLOUDY:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3406 ,0x0001, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3400 ,0x0007, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3401 ,0x0088, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3402 ,0x0004, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3403 ,0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3404 ,0x0005, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3405 ,0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case CAMERA_WB_MODE_NIGHT:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3406 ,0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }

            rc = ov5642_i2c_write(ov5642_client->addr, 0x3010 ,0x0070, WORD_LEN);//0x30->0x70 modified the wb of the night mode ZT_CAM_20100813
            if (rc < 0)
            {
               return rc;
            }

            /*
               * Change preview FPS from 1500 to 375
               */
            g_preview_frame_rate  = 375;
        }
        break;

        default:
        {
            CCRT("%s: parameter error!\n", __func__);
            return -EFAULT;
        }     
    }

    if(wb_mode != CAMERA_WB_MODE_NIGHT)
    {
        rc = ov5642_i2c_write(ov5642_client->addr, 0x3010, 0x0010, WORD_LEN);
        if (rc < 0)
        {
           return rc;
        }
    }

    return rc;
}

 /* ZTE_YGL_CAM_20101214
  * add the interface of touch AF for froyo
  */
static int32_t ov5642_set_aec_rio(aec_rio_cfg position)
{
    int32_t rc = 0;
    uint16_t af_status;
    uint32_t i;
    uint16_t window_full_width;
    uint16_t window_full_height;
    uint16_t x_ratio = 0;
    uint16_t y_ratio = 0;
  
    /*
     * window_full_width/window_full_height: 0~63
     */
    window_full_width = OV5642_AF_WINDOW_FULL_WIDTH;
    window_full_height = OV5642_AF_WINDOW_FULL_HEIGHT;
    x_ratio = position.preview_width/window_full_width;
    y_ratio = position.preview_height/window_full_height;
	
    if (x_ratio == 0 || y_ratio == 0)
    {
       return rc;
    }
	
    /* 
    * 1. set as idle mode
    */
    rc = ov5642_i2c_write(ov5642_client->addr, 0x3024, 0x08, BYTE_LEN);
    if (rc < 0)
    {
       return rc;
    }
	
    /* 
    * 2. read reg 0x3027, if =0, it means idle state
    */
    af_status = 0x0000;
    rc = ov5642_i2c_read(ov5642_client->addr, 0x3027, &af_status, BYTE_LEN);
    if (rc < 0)
    {
       return rc;
    }
    for (i = 0; (i < 100) && (0x0000 != af_status); ++i)
    {
        af_status = 0x0000;
        rc = ov5642_i2c_read(ov5642_client->addr, 0x3027, &af_status, BYTE_LEN);
        if (rc < 0)
        {
           return rc;
        }        

        mdelay(10);
    }
	
    /* 
    * 3. set windows position
    */
    rc = ov5642_i2c_write(ov5642_client->addr, 0x3025, 0x11, BYTE_LEN);
    if (rc < 0)
    {
       return rc;
    }
    rc = ov5642_i2c_write(ov5642_client->addr, 0x5084, (int32_t)(position.x / x_ratio), WORD_LEN);
    if (rc < 0)
    {
       return rc;
    }
    rc = ov5642_i2c_write(ov5642_client->addr, 0x5085, (int32_t)(position.y / y_ratio), WORD_LEN);
    if (rc < 0)
    {
       return rc;
    }
    rc = ov5642_i2c_write(ov5642_client->addr, 0x3024, 0x10, BYTE_LEN);
    if (rc < 0)
    {
       return rc;
    }
	
    /* 
    * 4. single foucs trigger
    */
    rc = ov5642_i2c_write(ov5642_client->addr, 0x3024, 0x03, BYTE_LEN);
    if (rc < 0)
    {
       return rc;
    }

    af_status = 0x0000;
    for (i = 0; (i < 1000) && (0x0002 != af_status); ++i)
    {
        af_status = 0x0000;
        rc = ov5642_i2c_read(ov5642_client->addr, 0x3027, &af_status, BYTE_LEN);
        if (rc < 0)
        {
           return rc;
        }        

        mdelay(15);
    }

    return rc;
}

/*
 * ISO Setting
 */
static int32_t ov5642_set_iso(int8_t iso_val)
{
    int32_t rc = 0;

    CDBG("%s: entry: iso_val=%d\n", __func__, iso_val);

    switch (iso_val)
    {
        case CAMERA_ISO_SET_AUTO:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a18 ,0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a19 ,0x007f, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case CAMERA_ISO_SET_HJR:
        {
            //add code here     
        }
        break;

        case CAMERA_ISO_SET_100:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a18 ,0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a19 ,0x003e, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case CAMERA_ISO_SET_200:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a18 ,0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a19 ,0x007c, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case CAMERA_ISO_SET_400:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a18 ,0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a19 ,0x00f2, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case CAMERA_ISO_SET_800:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a18 ,0x0001, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a19 ,0x00f2, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        default:
        {
            CCRT("%s: parameter error!\n", __func__);
            rc = -EFAULT;
        }     
    }

	return rc;
} 

/*
 * Antibanding Setting
 */
static int32_t  ov5642_set_antibanding(int8_t antibanding)
{
    int32_t rc = 0;

    CDBG("%s: entry: antibanding=%d\n", __func__, antibanding);

    switch (antibanding)
    {
        case CAMERA_ANTIBANDING_SET_OFF:
        {
            CCRT("%s: CAMERA_ANTIBANDING_SET_OFF NOT supported!\n", __func__);
        }
        break;

        case CAMERA_ANTIBANDING_SET_60HZ:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3C01, 0x0080, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3C00, 0x0000, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3A0A, 0x0007, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3A0B, 0x00D0, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3A0D, 0x0008, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
        }            
        break;

        case CAMERA_ANTIBANDING_SET_50HZ:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3C01, 0x0080, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3C00, 0x0004, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3A08, 0x0009, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3A09, 0x0060, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3A0E, 0x0006, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
        }
        break;

        case CAMERA_ANTIBANDING_SET_AUTO:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3623, 0x0001, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3630, 0x0024, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3633, 0x0000, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3C00, 0x0000, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3C01, 0x0034, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3C04, 0x0028, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3C05, 0x0098, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3C06, 0x0000, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3C07, 0x0007, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3C08, 0x0001, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3C09, 0x00C2, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x300D, 0x0002, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3104, 0x0001, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3C0A, 0x004E, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3C0B, 0x001F, WORD_LEN);
            if (rc < 0)
            {
                return rc;
            }
        }
        break;

        default:
        {
            CCRT("%s: parameter error!\n", __func__);
            rc = -EFAULT;
        }     
    }

	return rc;
} 

/*
 * Sharpness Setting
 */
static int32_t ov5642_set_sharpness(int8_t sharpness)
{
    int32_t rc = 0;

    CDBG("%s: entry: sharpness=%d\n", __func__, sharpness);
       
    switch (sharpness)
    {
        case CAMERA_SHARPNESS_0:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x530A ,0x0008, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x531e ,0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x531f ,0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case CAMERA_SHARPNESS_1:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x530A ,0x0008, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x531e ,0x0004, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x531f ,0x0004, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case CAMERA_SHARPNESS_2:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x530A ,0x0008, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x531e ,0x0008, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x531f ,0x0008, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;
        
        case CAMERA_SHARPNESS_3:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x530A ,0x0008, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x531e ,0x000f, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x531f ,0x000f, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
        }
        break; 

        case CAMERA_SHARPNESS_4:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x530A ,0x0008, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x531e ,0x001f, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x531f ,0x001f, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;        
        
        default:
        {
            CCRT("%s: parameter error!\n", __func__);
            rc = -EFAULT;
        }     
    }

	return rc;
} 

/* ZTE_ZT_CAM_20101026_04
 * add the interface of exposure compensation for foryo
 */
static long ov5642_set_exposure_compensation(int8_t exposure)
{
    long rc = 0;

    CDBG("%s: entry: exposure=%d\n", __func__, exposure);

    switch(exposure)
    {
        case CAMERA_EXPOSURE_0:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a0f, 0x0038, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a10, 0x0030, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a11, 0x0061, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a1b, 0x0038, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a1e, 0x0030, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a1f, 0x0010, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }        
        }
        break;
        
        case CAMERA_EXPOSURE_1:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a0f, 0x0040, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a10, 0x0038, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a11, 0x0071, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a1b, 0x0040, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a1e, 0x0038, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a1f, 0x0010, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }               
        }
        break;

        case CAMERA_EXPOSURE_2:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a0f, 0x0048, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a10, 0x0040, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a11, 0x0080, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a1b, 0x0048, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a1e, 0x0040, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a1f, 0x0020, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
        }
        break;

        case CAMERA_EXPOSURE_3:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a0f, 0x0050, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a10, 0x0048, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a11, 0x0090, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a1b, 0x0050, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a1e, 0x0048, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a1f, 0x0020, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
        }
        break;

        case CAMERA_EXPOSURE_4:
        {
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a0f, 0x0058, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a10, 0x0050, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a11, 0x0091, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a1b, 0x0058, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a1e, 0x0050, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3a1f, 0x0020, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }       
        }
        break;
        
        default:
        {
            CCRT("%s: parameter error!\n", __func__);
            return -EFAULT;
        }        
    }

    return rc;
}

static long ov5642_reg_init(void)
{
    long rc;

    CDBG("%s: entry\n", __func__);

    /* PLL Setup */
    rc = ov5642_i2c_write_table(ov5642_regs.plltbl, ov5642_regs.plltbl_size);
    if (rc < 0)
    {
        return rc;
    }

    /* Configure sensor for Preview mode and Snapshot mode */
    rc = ov5642_i2c_write_table(ov5642_regs.prev_snap_reg_settings, ov5642_regs.prev_snap_reg_settings_size);
    if (rc < 0)
    {
        return rc;
    }

    /* Configure for Noise Reduction */
    rc = ov5642_i2c_write_table(ov5642_regs.noise_reduction_reg_settings, ov5642_regs.noise_reduction_reg_settings_size);
    if (rc < 0)
    {
        return rc;
    }

    /* Configure for Refresh Sequencer */
    rc = ov5642_i2c_write_table(ov5642_regs.stbl, ov5642_regs.stbl_size);
    if (rc < 0)
    {
        return rc;
    }

    /* Configuration for AF */
    rc = ov5642_i2c_write_table(ov5642_regs.autofocus_reg_settings, ov5642_regs.autofocus_reg_settings_size);
    if (rc < 0)
    {
        return rc;
    }

    /* Configuration for Lens Roll */
    rc = ov5642_set_lens_roll_off();
    if (rc < 0)
    {
        return rc;
    }

    return 0;
}

static long ov5642_hw_ae_parameter_record(void)
{
    uint16_t ret_l, ret_m, ret_h;
    uint16_t temp_l, temp_m, temp_h;
    int32_t rc = 0;

    rc = ov5642_i2c_write(ov5642_client->addr, 0x3503, 0x0007, WORD_LEN);
    if (rc < 0)
    {
        return rc;
    }

    rc = ov5642_i2c_read(ov5642_client->addr, 0x3500, &ret_h, BYTE_LEN);
    if (rc < 0)
    {
        return rc;
    }
   
    rc = ov5642_i2c_read(ov5642_client->addr, 0x3501, &ret_m, BYTE_LEN);
    if (rc < 0)
    {
        return rc;
    }

    rc = ov5642_i2c_read(ov5642_client->addr, 0x3502, &ret_l, BYTE_LEN);
    if (rc < 0)
    {
        return rc;
    }

    //CDBG("ov5642_parameter_record:ret_h=%x,ret_m=%x,ret_l=%x\n",ret_h,ret_m,ret_l);

    temp_l = ret_l & 0x00FF;
    temp_m = ret_m & 0x00FF;
    temp_h = ret_h & 0x00FF;
    g_preview_exposure = (temp_h << 12) + (temp_m << 4) + (temp_l >> 4);
    //CDBG("ov5642_parameter_record:g_preview_exposure=%x\n",g_preview_exposure);

    /*
      * Set as global metering mode
      */
    rc = ov5642_i2c_read(ov5642_client->addr, 0x380e, &ret_h, BYTE_LEN);
    if (rc < 0)
    {
        return rc;
    }
    rc = ov5642_i2c_read(ov5642_client->addr, 0x380f, &ret_l, BYTE_LEN);
    if (rc < 0)
    {
        return rc;
    }

    //CDBG("ov5642_parameter_record:0x350c=%x, 0x350d=%x\n",ret_h,ret_l);

    g_preview_line_width = ret_h & 0xff;
    g_preview_line_width = g_preview_line_width * 256 + ret_l;
    //CDBG("ov5642_parameter_record:g_preview_line_width=%x\n",g_preview_line_width);

    //Read back AGC gain for preview
    rc = ov5642_i2c_read(ov5642_client->addr, 0x350b, &g_preview_gain, BYTE_LEN);
    if (rc < 0)
    {
        return rc;
    }
    //CDBG("ov5642_parameter_record:g_preview_gain=%x\n",g_preview_gain);

    return 0;
}

static long ov5642_hw_ae_transfer(void)
{
    uint8_t exposure_low;
    uint8_t exposure_mid;
    uint8_t exposure_high;
    uint32_t capture_exposure;
    uint32_t capture_exposure_gain;
    uint32_t capture_gain;
    uint8_t lines_10ms;
    uint32_t m_60Hz = 0;
    uint16_t reg_l = 0, reg_h = 0;
    uint16_t preview_lines_max = g_preview_line_width;
    uint16_t gain = g_preview_gain;
    uint32_t capture_lines_max;

    int32_t rc = 0;


#if 0

    uint8_t temp = 0;
#endif

    //CDBG("ov5642_hw_ae_transfer:gain=%x\n",gain);

    if (0 == g_preview_frame_rate)
    {
        g_preview_frame_rate = PREVIEW_FRAMERATE;
    }
    
    rc = ov5642_i2c_write_table(ov5642_regs.preview2snapshot_tbl, ov5642_regs.preview2snapshot_size);
    if (rc < 0)
    {
       return rc;
    }        

    /*
      * Set as global metering mode
      */
    rc = ov5642_i2c_read(ov5642_client->addr, 0x380e, &reg_h, BYTE_LEN);
    if (rc < 0)
    {
        return rc;
    }   
    rc = ov5642_i2c_read(ov5642_client->addr, 0x380f, &reg_l, BYTE_LEN);
    if (rc < 0)
    {
        return rc;
    }
    
    capture_lines_max = reg_h & 0xff;
    capture_lines_max = (capture_lines_max << 8) + reg_l;

    //CDBG("ov5642_hw_ae_transfer:0x350c=%x  0x350d=%x\n",reg_h,reg_l);
    //CDBG("ov5642_hw_ae_transfer:capture_lines_max=%x\n",capture_lines_max);
    if(m_60Hz== 1)
    {
        lines_10ms = CAPTURE_FRAMERATE * capture_lines_max / 12000;
    }
    else
    {
        lines_10ms = CAPTURE_FRAMERATE * capture_lines_max / 10000;
    }


    if(preview_lines_max == 0)
    {
        preview_lines_max = 1;
        //CDBG("can not divide zero\n");
    }

    /* ZTE_ZT_CAM_20101203
     * fix exception for capturing under backlight
     *
     * set gain no mater value of g_preview_exposure under normal light or backlight
     */
    /*
     * g_preview_exposure > 4: for normal light
     * g_preview_exposure <= 4: under backlight
     */
#if 0 //ZTE_ZT_CAM_20101203
    if (g_preview_exposure > 4)
#endif
    {    
        capture_exposure = (g_preview_exposure * (CAPTURE_FRAMERATE) * (capture_lines_max)) 
                            / (((preview_lines_max) * (g_preview_frame_rate )));

        capture_gain = (gain & 0x0f) + 16;
        //CDBG("ov5642_hw_ae_transfer:capture_gain=%x\n",capture_gain);

        if (gain & 0x10)
        {
            capture_gain = capture_gain << 1;
        }

        if (gain & 0x20)
        {
            capture_gain = capture_gain << 1;
        }

        if (gain & 0x40)
        {
            capture_gain = capture_gain << 1;
        }

        if (gain & 0x80)
        {
            capture_gain = capture_gain << 1;
        }

        /* 
          * Commented by li.jing
          * modify the brighness of snapshot
        */
        capture_exposure_gain = 20 * capture_exposure * capture_gain / 10; //24
        if(capture_exposure_gain < (capture_lines_max * 16))
        {
            capture_exposure = capture_exposure_gain / 16;

            if (capture_exposure > lines_10ms)
            {
                //capture_exposure *= 1.7;
                capture_exposure /= lines_10ms;
                capture_exposure *= lines_10ms;
            }
        }
        else
        {
            capture_exposure = capture_lines_max;
            //capture_exposure_gain *= 1.5;
        }

        if(capture_exposure == 0)
        {
            capture_exposure = 1;
        }

        capture_gain = (capture_exposure_gain * 2 / capture_exposure + 1) / 2;

        exposure_low = ((unsigned char)capture_exposure) << 4;
        exposure_mid = (unsigned char)(capture_exposure >> 4) & 0xff;
        exposure_high = (unsigned char)(capture_exposure >> 12);
        //CDBG("ov5642_hw_ae_transfer:capture_exposure =%x \n",capture_exposure);
        //CDBG("ov5642_hw_ae_transfer:exposure_low =%x \n",exposure_low);
        //CDBG("ov5642_hw_ae_transfer:exposure_mid =%x \n",exposure_mid);
        //CDBG("ov5642_hw_ae_transfer:exposure_high =%x \n",exposure_high);

        //ulCapture_Exposure_end=capture_exposure;
        //iCapture_Gain_end=capture_gain;

        gain = 0;
        if (capture_gain > 31)
        {
            gain |= 0x10;
            capture_gain = capture_gain >> 1;
        }

        if (capture_gain > 31)
        {
            gain |= 0x20;
            capture_gain = capture_gain >> 1;
        }

        if (capture_gain > 31)
        {
            gain |= 0x40;
            capture_gain = capture_gain >> 1;
        }

        if (capture_gain > 31)
        {
            gain |= 0x80;
            capture_gain = capture_gain >> 1;
        }

        if (capture_gain > 16)
        {
            gain |= ((capture_gain - 16) & 0x0f);
        }

        if(gain == 0x10)
        {
            gain = 0x11;
        }

        // write the gain and exposure to 0x350* registers

        //m_iWrite0x350b=gain;
        rc = ov5642_i2c_write(ov5642_client->addr, 0x350b, gain, WORD_LEN);
        if (rc < 0)
        {
            return rc;
        }    

        //m_iWrite0x3502=exposure_low;
        rc = ov5642_i2c_write(ov5642_client->addr, 0x3502, exposure_low, WORD_LEN);
        if (rc < 0)
        {
            return rc;
        }    

        //m_iWrite0x3501=exposure_mid;
        rc = ov5642_i2c_write(ov5642_client->addr, 0x3501, exposure_mid, WORD_LEN);
        if (rc < 0)
        {
            return rc;
        }    

        //m_iWrite0x3500=exposure_high;
        rc = ov5642_i2c_write(ov5642_client->addr, 0x3500, exposure_high, WORD_LEN);
        if (rc < 0)
        {
            return rc;
        }
    }
#if 0 //ZTE_ZT_CAM_20101203
    else
    {
        if (g_preview_exposure > 1)
        {
            temp = (temp_l * 3/2) & 0x00FF;            
        }
        else
        {
            if (g_preview_exposure == 1)
            {
                temp = (temp_l * 13/10) & 0x00FF;            
            }
            else
            {
                temp = 1;
            }
        }

        CDBG("ov5642_hw_ae_transfer: g_preview_exposure < 4 : temp_h =%x \n",temp_h);
        CDBG("ov5642_hw_ae_transfer: g_preview_exposure < 4 : temp_m =%x \n",temp_m);
        CDBG("ov5642_hw_ae_transfer: g_preview_exposure < 4 : temp_l =%x \n",temp);

        rc = ov5642_i2c_write(ov5642_client->addr, 0x3500, temp_h, WORD_LEN);
        if (rc < 0)
        {
            return rc;
        }

        rc = ov5642_i2c_write(ov5642_client->addr, 0x3501, temp_m, WORD_LEN);
        if (rc < 0)
        {
            return rc;
        }

        rc = ov5642_i2c_write(ov5642_client->addr, 0x3502, temp, WORD_LEN);    
        if (rc < 0)
        {
            return rc;
        }

    }
#endif

    /* time delay for auto exposure */
    mdelay(400);

    /* Exit from AF mode */
    rc = ov5642_i2c_write(ov5642_client->addr, 0x3024, 0x0002, WORD_LEN);
    if (rc < 0)
    {
        CCRT("%s: failed, rc=%d!\n", __func__, rc);
        return rc;
    }
  
    return 0;
}

static long ov5642_set_effect(int32_t mode, int32_t effect)
{
    uint16_t __attribute__((unused)) reg_addr;
    uint16_t __attribute__((unused)) reg_val;
    uint16_t tmp_reg = 0;
    long rc = 0;

    CDBG("%s: entry: mode=%d, effect=%d\n", __func__, mode, effect);

    switch (mode)
    {
        case SENSOR_PREVIEW_MODE:
        {
            /* Context A Special Effects */
            /* add code here
                 e.g.
                 reg_addr = 0xXXXX;
               */
        }
        break;

        case SENSOR_SNAPSHOT_MODE:
        {
            /* Context B Special Effects */
            /* add code here
                 e.g.
                 reg_addr = 0xXXXX;
               */
        }
        break;

        default:
        {
            /* add code here
                 e.g.
                 reg_addr = 0xXXXX;
               */
        }
        break;
    }

    switch (effect)
    {
        case CAMERA_EFFECT_OFF:
        {
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5001, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }

            /*
             * ZTE_LJ_CAM_20101026
             * fix bug of no preview image after changing effect mode repeatedly
             */
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5580, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg &= 0x0006;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5580, tmp_reg, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }   
        }
        break;

        case CAMERA_EFFECT_MONO:
        {
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5001, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5585, 0x0080, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5586, 0x0080, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5580, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x0087;
            tmp_reg |= 0x0018;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5580, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }	
        }
        break;
        
        case CAMERA_EFFECT_NEGATIVE:
        {
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5001, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5580, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x0087;
            tmp_reg |= 0x0040;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5580, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }	
        }
        break;          
        
        case CAMERA_EFFECT_SEPIA:
        {
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5001, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x00FF;
            tmp_reg |= 0x0080;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5001, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5585, 0x0040, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5586, 0x00a0, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg = 0;
            rc = ov5642_i2c_read(ov5642_client->addr, 0x5580, &tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }
            tmp_reg &= 0x0087;
            tmp_reg |= 0x0018;
            rc = ov5642_i2c_write(ov5642_client->addr, 0x5580, tmp_reg, BYTE_LEN);
            if (rc < 0)
            {
               return rc;
            }	
        }
        break;
        
        case CAMERA_EFFECT_BULISH:    
        case CAMERA_EFFECT_GREENISH:
        case CAMERA_EFFECT_REDDISH:
        {
            CCRT("%s: not supported!\n", __func__);
            rc = 0;
        }
        break;      
        
        default:
        {
            CCRT("%s: parameter error!\n", __func__);
            return -EFAULT;
        }
    }

    return rc;
}

static long ov5642_set_sensor_mode(int32_t mode)
{
    long rc = 0;

    /*
      * In order to avoid reentry of SENSOR_PREVIEW_MODE by
      * Qualcomm Camera HAL, e.g. vfe_set_dimension/vfe_preview_config,
      * 
      * enter into SENSOR_PREVIEW_MODE only when ov5642_previewmode_entry_cnt=0
      */
    static uint32_t ov5642_previewmode_entry_cnt = 0;

    CDBG("%s: entry: mode=%d\n", __func__, mode);

    switch (mode)
    {
        case SENSOR_PREVIEW_MODE:
        {
            /*
               * Enter Into SENSOR_PREVIEW_MODE
               * Only When ov5642_previewmode_entry_cnt=0
               */
            if (0 != ov5642_previewmode_entry_cnt)
            {
                CDBG("%s: reentry of SENSOR_PREVIEW_MODE: entry count=%d\n", __func__,
                     ov5642_previewmode_entry_cnt);
                break;
            }
            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3024, 0x0008, WORD_LEN);    
            if (rc < 0)
            {
               return rc;
            }        

            rc = ov5642_i2c_write_table(ov5642_regs.snapshot2preview_tbl, ov5642_regs.snapshot2preview_size);
            if (rc < 0)
            {
                return rc;
            }  

            /* Restore AF registers values */
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3000, 0x0020, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }            
            mdelay(50);
            
            rc = ov5642_i2c_write(ov5642_client->addr, 0x3000, 0x0000, WORD_LEN);
            if (rc < 0)
            {
               return rc;
            }

            /*
               * Enter Into SENSOR_PREVIEW_MODE
               * Only When ov5642_previewmode_entry_cnt=0
               */
            ov5642_previewmode_entry_cnt++;
        }
        break;

        case SENSOR_SNAPSHOT_MODE:
        {

            /*
               * Enter Into SENSOR_PREVIEW_MODE
               * Only When ov5642_previewmode_entry_cnt=0
               */
            ov5642_previewmode_entry_cnt = 0;

#if 1 //enable exposure algorithm
            rc = ov5642_hw_ae_parameter_record();
            if (rc < 0)
            {
                return rc;
            }

            rc = ov5642_hw_ae_transfer();
            if (rc < 0)
            {
                return rc;
            }            
#else //disable exposure algorithm
            rc = ov5642_i2c_write_table(ov5642_regs.preview2snapshot_tbl, ov5642_regs.preview2snapshot_size);
            if (rc < 0)
            {
                return rc;
            }        
            CDBG("5MP snapshot!!!\n");
            mdelay(3000);         
#endif            
        }
        break;

        default:
        {
            CCRT("%s: parameter error!\n", __func__);
            return -EFAULT;
        }
    }

    return 0;
}

/*
 * Power-up Process
 */
static long ov5642_power_up(void)
{
    CDBG("%s: not supported!\n", __func__);
    return 0;
}

/*
 * Power-down Process
 */
static long ov5642_power_down(void)
{
    CDBG("%s: not supported!\n", __func__);
    return 0;
}

/*
 * Set lowest-power mode (SHUTDOWN mode)
 *
 * OV5642_GPIO_SHUTDOWN_CTL: 0, to quit lowest-power mode, or
 *                            1, to enter lowest-power mode
 */
#if defined(CONFIG_MACH_RAISE)
static int __attribute__((unused))ov5642_power_shutdown(uint32_t on)
#elif defined(CONFIG_MACH_MOONCAKE)
static int __attribute__((unused)) ov5642_power_shutdown(uint32_t on)
#elif defined(CONFIG_MACH_JOE)
static int __attribute__((unused)) ov5642_power_shutdown(uint32_t on)
#else
static int __attribute__((unused)) ov5642_power_shutdown(uint32_t on)
#endif /* defined(CONFIG_MACH_RAISE) */
{
    int rc;

    CDBG("%s: entry\n", __func__);

    rc = gpio_request(OV5642_GPIO_SHUTDOWN_CTL, "ov5642");
    if (0 == rc)
    {
        /* ignore "rc" */
        rc = gpio_direction_output(OV5642_GPIO_SHUTDOWN_CTL, on);

        /* time delay for shutting sensor down */
        mdelay(1);
    }

    gpio_free(OV5642_GPIO_SHUTDOWN_CTL);

    return rc;
}

#if !defined(CONFIG_SENSOR_ADAPTER)
static int ov5642_sensor_init_probe(const struct msm_camera_sensor_info *data)
{
    uint16_t model_id = 0;    
    int rc = 0;

    CDBG("%s: entry\n", __func__);

    rc = ov5642_i2c_read(ov5642_client->addr, REG_OV5642_MODEL_ID, &model_id, WORD_LEN);
    if (rc < 0)
    {
        goto init_probe_fail;
    }

    CDBG("%s: model_id = 0x%x\n", __func__, model_id);

    /*
      * set sensor id for EM (Engineering Mode)
      */
#ifdef CONFIG_SENSOR_INFO
    msm_sensorinfo_set_sensor_id(model_id);
#else
    // do nothing here
#endif  

    /* Check if it matches it with the value in Datasheet */
    if (model_id != OV5642_MODEL_ID)
    {
        rc = -EFAULT;
        goto init_probe_fail;
    }

    rc = ov5642_reg_init();
    if (rc < 0)
    {
        goto init_probe_fail;
    }

    return rc;

init_probe_fail:
    CCRT("%s: rc = %d, failed!\n", __func__, rc);
    return rc;
}
#else
static int ov5642_sensor_i2c_probe_on(void)
{
    int rc;
    struct i2c_board_info info;
    struct i2c_adapter *adapter;
    struct i2c_client *client;

    rc = ov5642_i2c_add_driver();
    if (rc < 0)
    {
        CCRT("%s: add i2c driver failed!\n", __func__);
        return rc;
    }

    memset(&info, 0, sizeof(struct i2c_board_info));
    info.addr = OV5642_SLAVE_WR_ADDR >> 1;
    strlcpy(info.type, OV5642_I2C_BOARD_NAME, I2C_NAME_SIZE);

    adapter = i2c_get_adapter(OV5642_I2C_BUS_ID);
    if (!adapter)
    {
        CCRT("%s: get i2c adapter failed!\n", __func__);
        goto i2c_probe_failed;
    }

    client = i2c_new_device(adapter, &info);
    i2c_put_adapter(adapter);
    if (!client)
    {
        CCRT("%s: add i2c device failed!\n", __func__);
        goto i2c_probe_failed;
    }

    ov5642_client = client;

    return 0;

i2c_probe_failed:
    ov5642_i2c_del_driver();
    return -ENODEV;
}

static void ov5642_sensor_i2c_probe_off(void)
{
    i2c_unregister_device(ov5642_client);
    ov5642_i2c_del_driver();
}

static int ov5642_sensor_dev_probe(const struct msm_camera_sensor_info *pinfo)
{
    uint16_t model_id;
    uint32_t switch_on;
    int rc;

    CDBG("%s: entry\n", __func__);

    /*
      * Deassert Sensor Reset
      * Ignore "rc"
      */
    rc = gpio_request(pinfo->sensor_reset, "ov5642");
    rc = gpio_direction_output(pinfo->sensor_reset, 0);
    gpio_free(pinfo->sensor_reset);

    /* time delay for deasserting RESET */
    mdelay(1);

    /*
      * Enter Into Hard Standby
      */
    switch_on = 1;
    rc = ov5642_hard_standby(pinfo, switch_on);
    if (rc < 0)
    {
        goto dev_probe_fail;
    }

    /*
      * Power VREG on
      */
    rc = msm_camera_power_backend(MSM_CAMERA_PWRUP_MODE);
    if (rc < 0)
    {
        goto dev_probe_fail;
    }

    /*
     * Camera clock switch for both frontend and backend sensors, i.e., MT9V113 and OV5642
     *
     * For MT9V113: 0.3Mp, 1/11-Inch System-On-A-Chip (SOC) CMOS Digital Image Sensor
     * For OV5642: 5.0Mp, 1/4-Inch System-On-A-Chip (SOC) CMOS Digital Image Sensor
     *
     * switch_val: 0, to switch clock to frontend sensor, i.e., MT9V113, or
     *             1, to switch clock to backend sensor, i.e., OV5642
     */
#if defined(CONFIG_MACH_RAISE)
    rc = msm_camera_clk_switch(pinfo, OV5642_GPIO_SWITCH_CTL, OV5642_GPIO_SWITCH_VAL);
    if (rc < 0)
    {
        goto dev_probe_fail;
    }
#elif defined(CONFIG_MACH_MOONCAKE)
    /* Do nothing */
#elif defined(CONFIG_MACH_R750) || defined(CONFIG_MACH_JOE)
    rc = msm_camera_clk_switch(pinfo, OV5642_GPIO_SWITCH_CTL, OV5642_GPIO_SWITCH_VAL);
    if (rc < 0)
    {
        goto dev_probe_fail;
    }
#else
    /* Do nothing */
#endif /* defined(CONFIG_MACH_RAISE) */

    /* Input MCLK */
    msm_camio_clk_rate_set(OV5642_CAMIO_MCLK);

    /* time delay for enabling MCLK */
    mdelay(10);

    /*
      * Exit From Hard Standby
      */
    switch_on = 0;
    rc = ov5642_hard_standby(pinfo, switch_on);
    if (rc < 0)
    {
        goto dev_probe_fail;
    }

    /*
      * Assert Sensor Reset
      * Ignore "rc"
      */
    rc = gpio_request(pinfo->sensor_reset, "ov5642");
    rc = gpio_direction_output(pinfo->sensor_reset, 1);
    gpio_free(pinfo->sensor_reset);

    /* time delay for asserting RESET */
    mdelay(2);

    model_id = 0;
    rc = ov5642_i2c_read(ov5642_client->addr, REG_OV5642_MODEL_ID, &model_id, WORD_LEN);
    if (rc < 0)
    {
        goto dev_probe_fail;
    }

    CDBG("%s: model_id = 0x%x\n", __func__, model_id);

    /*
      * set sensor id for EM (Engineering Mode)
      */
#ifdef CONFIG_SENSOR_INFO
    msm_sensorinfo_set_sensor_id(model_id);
#else
    // do nothing here
#endif

    /* Check if it matches it with the value in Datasheet */
    if (model_id != OV5642_MODEL_ID)
    {
        rc = -EFAULT;
        goto dev_probe_fail;
    }

    return 0;

dev_probe_fail:
    CCRT("%s: rc = %d, failed!\n", __func__, rc);
    return rc;
}
#endif

static int ov5642_sensor_probe_init(const struct msm_camera_sensor_info *data)
{
#if !defined(CONFIG_SENSOR_ADAPTER)
    uint32_t switch_on; 
#endif
    int rc = 0;

    CDBG("%s: entry\n", __func__);

    if (!data || strcmp(data->sensor_name, "ov5642"))
    {
        CCRT("%s: invalid parameters!\n", __func__);
        rc = -ENODEV;
        goto probe_init_fail;
    }

    ov5642_ctrl = kzalloc(sizeof(struct ov5642_ctrl_t), GFP_KERNEL);
    if (!ov5642_ctrl)
    {
        CCRT("%s: kzalloc failed!\n", __func__);
        rc = -ENOMEM;
        goto probe_init_fail;
    }

    ov5642_ctrl->sensordata = data;

#if !defined(CONFIG_SENSOR_ADAPTER)
    /*
      * Deassert Sensor Reset
      * Ignore "rc"
      */
    rc = gpio_request(ov5642_ctrl->sensordata->sensor_reset, "ov5642");
    rc = gpio_direction_output(ov5642_ctrl->sensordata->sensor_reset, 0);
    gpio_free(ov5642_ctrl->sensordata->sensor_reset);

    /* time delay for deasserting RESET */
    mdelay(1);

    /* Enter Into Hard Standby */
    switch_on = 1;
    rc = ov5642_hard_standby(ov5642_ctrl->sensordata, switch_on);
    if (rc < 0)
    {
        CCRT("set standby failed!\n");
        goto probe_init_fail;
    }

    /*
      * Power VREG on
      */
    rc = msm_camera_power_backend(MSM_CAMERA_PWRUP_MODE);
    if (rc < 0)
    {
        CCRT("%s: camera_power_backend failed!\n", __func__);
        goto probe_init_fail;
    }

    /*
     * Camera clock switch for both frontend and backend sensors, i.e., MT9V113 and OV5642
     *
     * For MT9V113: 0.3Mp, 1/11-Inch System-On-A-Chip (SOC) CMOS Digital Image Sensor
     * For OV5642: 5.0Mp, 1/4-Inch System-On-A-Chip (SOC) CMOS Digital Image Sensor
     *
     * switch_val: 0, to switch clock to frontend sensor, i.e., MT9V113, or
     *             1, to switch clock to backend sensor, i.e., OV5642
     */
#if defined(CONFIG_MACH_RAISE)
    rc = msm_camera_clk_switch(ov5642_ctrl->sensordata, OV5642_GPIO_SWITCH_CTL, OV5642_GPIO_SWITCH_VAL);
    if (rc < 0)
    {
        CCRT("%s: camera_clk_switch failed!\n", __func__);
        goto probe_init_fail;
    }
#elif defined(CONFIG_MACH_MOONCAKE)
    /* Do nothing */
#elif defined(CONFIG_MACH_JOE)
    /* 
    * add for mclk switch for msm7627_joe
    */
    rc = msm_camera_clk_switch(ov5642_ctrl->sensordata, OV5642_GPIO_SWITCH_CTL, OV5642_GPIO_SWITCH_VAL);
    if (rc < 0)
    {
        CCRT("%s: camera_clk_switch failed!\n", __func__);
        goto probe_init_fail;
    }
#else
    /* Do nothing */
#endif /* defined(CONFIG_MACH_RAISE) */

    /* Input MCLK */
    msm_camio_clk_rate_set(OV5642_CAMIO_MCLK);

    /* time delay for enabling MCLK */
    mdelay(10);

    /* Exit From Hard Standby */
    switch_on = 0;
    rc = ov5642_hard_standby(ov5642_ctrl->sensordata, switch_on);
    if (rc < 0)
    {
        CCRT("set standby failed!\n");
        goto probe_init_fail;
    }

    /*
      * Assert Sensor Reset
      * Ignore "rc"
      */
    rc = gpio_request(ov5642_ctrl->sensordata->sensor_reset, "ov5642");
    rc = gpio_direction_output(ov5642_ctrl->sensordata->sensor_reset, 1);
    gpio_free(ov5642_ctrl->sensordata->sensor_reset);

    /* time delay for asserting RESET */
    mdelay(2);

    /* Sensor Register Setting */
    rc = ov5642_sensor_init_probe(ov5642_ctrl->sensordata);
    if (rc < 0)
    {
        CCRT("%s: sensor_init_probe failed!\n", __func__);
        goto probe_init_fail;
    }
#else
    rc = ov5642_sensor_dev_probe(ov5642_ctrl->sensordata);
    if (rc < 0)
    {
        CCRT("%s: ov5642_sensor_dev_probe failed!\n", __func__);
        goto probe_init_fail;
    }

    /* Sensor Register Setting */
    rc = ov5642_reg_init();
    if (rc < 0)
    {
        CCRT("%s: ov5642_reg_init failed!\n", __func__);
        goto probe_init_fail;
    }
#endif

    /* Enter Into Hard Standby */
    rc = ov5642_sensor_release();
    if (rc < 0)
    {
        CCRT("%s: sensor_release failed!\n", __func__);
        goto probe_init_fail;
    }

    return 0;

probe_init_fail:
    /*
      * To power sensor down
      * Ignore "rc"
      */
    msm_camera_power_backend(MSM_CAMERA_PWRDWN_MODE);
    if(ov5642_ctrl)
    {
        kfree(ov5642_ctrl);
    }
    return rc;
}

static int ov5642_sensor_init(const struct msm_camera_sensor_info *data)
{
    uint32_t switch_on; 
    int rc;

    CDBG("%s: entry\n", __func__);

    if (!data || strcmp(data->sensor_name, "ov5642"))
    {
        CCRT("%s: invalid parameters!\n", __func__);
        rc = -ENODEV;
        goto sensor_init_fail;
    }

    msm_camio_camif_pad_reg_reset();

    /* time delay for resetting CAMIF's PAD */
    mdelay(10);

    /* Input MCLK */
    msm_camio_clk_rate_set(OV5642_CAMIO_MCLK);

    /* time delay for enabling MCLK */
    mdelay(10);

    /* Exit From Hard Standby */   
    switch_on = 0;
    rc = ov5642_hard_standby(ov5642_ctrl->sensordata, switch_on);
    if (rc < 0)
    {
        CCRT("set standby failed!\n");
        rc = -EFAULT;
        goto sensor_init_fail;
    }

    /*
      * To avoid green display when preview mode is resumed
      * after snapshot or review.
      * The action of pulling STANDBY GPIO down is the key which
      * generates the problem mentioned above,
      * So time delay is added to avoid this problem.
      *
      * ZTE_YGL_CAM_20101018, ZTE_YGL_CAM_20101123
      * Decrease time delay from 700ms to 400ms in order to pass CTS
      */
    mdelay(400);

    return 0;

sensor_init_fail:
    return rc; 
}

static int ov5642_sensor_config(void __user *argp)
{
    struct sensor_cfg_data cfg_data;
    long rc = 0;

    CDBG("%s: entry\n", __func__);

    if (copy_from_user(&cfg_data, (void *)argp, sizeof(struct sensor_cfg_data)))
    {
        CCRT("%s: copy_from_user failed!\n", __func__);
        return -EFAULT;
    }

    /* down(&ov5642_sem); */

    CDBG("%s: cfgtype = %d, mode = %d\n", __func__, cfg_data.cfgtype, cfg_data.mode);

    switch (cfg_data.cfgtype)
    {
        case CFG_SET_MODE:
        {
            rc = ov5642_set_sensor_mode(cfg_data.mode);
        }
        break;

        case CFG_SET_EFFECT:
        {
            rc = ov5642_set_effect(cfg_data.mode, cfg_data.cfg.effect);
        }
        break;

        case CFG_PWR_UP:
        {
            rc = ov5642_power_up();
        }
        break;

        case CFG_PWR_DOWN:
        {
            rc = ov5642_power_down();
        }
        break;

        case CFG_SET_WB:
        {
            rc = ov5642_set_wb(cfg_data.cfg.wb_mode);
        }
        break;

        case CFG_SET_BRIGHTNESS:
        {
            rc = ov5642_set_brightness(cfg_data.cfg.brightness);
        }
        break;
        
        case CFG_SET_CONTRAST:
        {
            rc = ov5642_set_contrast(cfg_data.cfg.contrast);
        }
        break;
        
        case CFG_SET_SATURATION:
        {
            rc = ov5642_set_saturation(cfg_data.cfg.saturation);
        }
        break;

        case CFG_SET_AF:
        {
            rc = ov5642_af_trigger();
        }
        break;

        case CFG_SET_ISO:
        {
            rc = ov5642_set_iso(cfg_data.cfg.iso_val);
        }
        break;

        case CFG_SET_ANTIBANDING:
        {
            rc = ov5642_set_antibanding(cfg_data.cfg.antibanding);
        }
        break;

        case CFG_SET_SHARPNESS:
        {
            rc = ov5642_set_sharpness(cfg_data.cfg.sharpness);
        }
        break;

        case CFG_SET_EXPOSURE_COMPENSATION://ZTE_ZT_CAM_20101026_04
        {
            rc = ov5642_set_exposure_compensation(cfg_data.cfg.exposure);
        }
        break;

        /* ZTE_YGL_CAM_20101214
         * add the interface of touch AF for froyo
         */
        case CFG_SET_AEC_RIO:
        {
            rc = ov5642_set_aec_rio(cfg_data.cfg.aec_rio);
        }
        break;
        
        default:
        {
            CCRT("%s: parameter error!\n", __func__);
            rc = -EFAULT;
        }
        break;
    }

    /* up(&ov5642_sem); */

    return rc;
}

static int ov5642_sensor_release(void)
{
    uint32_t switch_on;
    int rc = 0;

    CDBG("%s: entry\n", __func__);

    /* down(&ov5642_sem); */

    /*
      * MCLK is disabled by 
      * msm_camio_clk_disable(CAMIO_VFE_CLK)
      * in msm_camio_disable
      */

    /* Enter Into Hard Standby */
    /* ignore rc */
    switch_on = 1;
    rc = ov5642_hard_standby(ov5642_ctrl->sensordata, switch_on);

    /* up(&ov5642_sem); */

    return rc;
}

static int ov5642_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    int rc = 0;

    CDBG("%s: entry\n", __func__);

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        rc = -ENOTSUPP;
        goto probe_failure;
    }

    ov5642_sensorw = kzalloc(sizeof(struct ov5642_work_t), GFP_KERNEL);
    if (!ov5642_sensorw)
    {
        rc = -ENOMEM;
        goto probe_failure;
    }

    i2c_set_clientdata(client, ov5642_sensorw);

    ov5642_client = client;

    return 0;

probe_failure:
    kfree(ov5642_sensorw);
    ov5642_sensorw = NULL;
    CCRT("%s: rc = %d, failed!\n", __func__, rc);
    return rc;
}

static int __exit ov5642_i2c_remove(struct i2c_client *client)
{
    struct ov5642_work_t *sensorw = i2c_get_clientdata(client);

    CDBG("%s: entry\n", __func__);

    free_irq(client->irq, sensorw);   
    kfree(sensorw);

    ov5642_client = NULL;
    ov5642_sensorw = NULL;

    return 0;
}

static const struct i2c_device_id ov5642_id[] = {
    { "ov5642", 0},
    { },
};

static struct i2c_driver ov5642_driver = {
    .id_table = ov5642_id,
    .probe  = ov5642_i2c_probe,
    .remove = __exit_p(ov5642_i2c_remove),
    .driver = {
        .name = "ov5642",
    },
};

static int32_t ov5642_i2c_add_driver(void)
{
    int32_t rc = 0;

    rc = i2c_add_driver(&ov5642_driver);
    if (IS_ERR_VALUE(rc))
    {
        goto init_failure;
    }

    return rc;

init_failure:
    CCRT("%s: rc = %d, failed!\n", __func__, rc);
    return rc;
}

static void ov5642_i2c_del_driver(void)
{
    i2c_del_driver(&ov5642_driver);
}

void ov5642_exit(void)
{
    CDBG("%s: entry\n", __func__);
    ov5642_i2c_del_driver();
}

int ov5642_sensor_probe(const struct msm_camera_sensor_info *info,
                               struct msm_sensor_ctrl *s)
{
    int rc = 0;

    CDBG("%s: entry\n", __func__);

#if !defined(CONFIG_SENSOR_ADAPTER)
    rc = ov5642_i2c_add_driver();
    if (rc < 0)
    {
        goto probe_failed;
    }
#endif

    rc = ov5642_sensor_probe_init(info);
    if (rc < 0)
    {
        CCRT("%s: ov5642_sensor_probe_init failed!\n", __func__);
        goto probe_failed;
    }
	
	/*
	 * add sensor configuration
	 * ZTE_CAM_LJ_20110413
	 */ 
    s->s_mount_angle = 0;
    s->s_camera_type = BACK_CAMERA_2D;

    s->s_init       = ov5642_sensor_init;
    s->s_config     = ov5642_sensor_config;
    s->s_release    = ov5642_sensor_release;

    return 0;

probe_failed:
    CCRT("%s: rc = %d, failed!\n", __func__, rc);

#if !defined(CONFIG_SENSOR_ADAPTER)
    ov5642_i2c_del_driver();
#else
    // Do nothing
#endif

    return rc;
}

static int __ov5642_probe_internal(struct platform_device *pdev)
{
#if defined(CONFIG_SENSOR_ADAPTER)
 	int rc;
#else
    // Do nothing
#endif

/*
 * Get FTM flag to adjust 
 * the initialize process 
 * of camera
 */
#ifdef CONFIG_ZTE_PLATFORM
#ifdef CONFIG_ZTE_FTM_FLAG_SUPPORT
    if(zte_get_ftm_flag())
    {
        return 0;
    }
#endif
#endif

#if !defined(CONFIG_SENSOR_ADAPTER)
    return msm_camera_drv_start(pdev, ov5642_sensor_probe);
#else
    rc = msm_camera_dev_start(pdev,
                              ov5642_sensor_i2c_probe_on,
                              ov5642_sensor_i2c_probe_off,
                              ov5642_sensor_dev_probe);
    if (rc < 0)
    {
        CCRT("%s: msm_camera_dev_start failed!\n", __func__);
        goto probe_failed;
    }

    rc = msm_camera_drv_start(pdev, ov5642_sensor_probe);
    if (rc < 0)
    {
        goto probe_failed;
    }

    return 0;

probe_failed:
    CCRT("%s: rc = %d, failed!\n", __func__, rc);
    /* 
      * ZTE_JIA_CAM_20101209
      * to avoid standby current exception problem
      *
      * ignore "rc"
      */
    if(rc != -ENOINIT){
	 	pr_err("%s: rc != -ENOINIT\n", __func__);
    	msm_camera_power_backend(MSM_CAMERA_PWRDWN_MODE);
    }
    //msm_camera_power_backend(MSM_CAMERA_PWRDWN_MODE);
    return rc;
#endif
}

#if defined(OV5642_PROBE_WORKQUEUE)
/* To implement the parallel init process */
static void ov5642_workqueue(struct work_struct *work)
{
    int32_t rc;

    /*
     * ignore "rc"
     */
    rc = __ov5642_probe_internal(pdev_wq);
}

static int32_t ov5642_probe_workqueue(void)
{
    int32_t rc;

    ov5642_wq = create_singlethread_workqueue("ov5642_wq");

    if (!ov5642_wq)
    {
        CCRT("%s: ov5642_wq is NULL!\n", __func__);
        return -EFAULT;
    }

    /*
      * Ignore "rc"
      * "queue_work"'s rc:
      * 0: already in work queue
      * 1: added into work queue
      */   
    rc = queue_work(ov5642_wq, &ov5642_cb_work);

    return 0;
}

static int __ov5642_probe(struct platform_device *pdev)
{
    int32_t rc;

    pdev_wq = pdev;

    rc = ov5642_probe_workqueue();

    return rc;
}
#else
static int __ov5642_probe(struct platform_device *pdev)
{
    return __ov5642_probe_internal(pdev);
}
#endif /* OV5642_PROBE_WORKQUEUE */

static struct platform_driver msm_camera_driver = {
    .probe = __ov5642_probe,
    .driver = {
        .name = "msm_camera_ov5642",
        .owner = THIS_MODULE,
    },
};

static int __init ov5642_init(void)
{
    return platform_driver_register(&msm_camera_driver);
}

module_init(ov5642_init);

