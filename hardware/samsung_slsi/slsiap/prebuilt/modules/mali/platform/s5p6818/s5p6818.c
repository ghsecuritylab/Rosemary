/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2010, 2012-2015 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file mali_platform.c
 * Platform specific Mali driver functions for:
 * - Realview Versatile platforms with ARM11 Mpcore and virtex 5.
 * - Versatile Express platforms with ARM Cortex-A9 and virtex 6.
 */
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/pm.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include <asm/io.h>
#include <linux/mali/mali_utgard.h>
#include "mali_kernel_common.h"
#include <linux/dma-mapping.h>
#include <linux/moduleparam.h>

#include "s5p6818_core_scaling.h"
#include "mali_executor.h"
#include "../../mali_kernel_linux.h"

static int mali_core_scaling_enable = 0;

void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data);

static struct mali_gpu_device_data mali_gpu_data = {
#if 0 //ndef CONFIG_MALI_DT
	.pmu_switch_delay = 0xFF, /* do not have to be this high on FPGA, but it is good for testing to have a delay */
	.max_job_runtime = 60000, /* 60 seconds */
#if 0 //defined(CONFIG_ARCH_VEXPRESS)
	.shared_mem_size = 256 * 1024 * 1024, /* 256MB */
#endif
#endif

#if defined( CONFIG_ARCH_ZYNQ )
	.pmu_switch_delay = 0xFF, /* do not have to be this high on FPGA, but it is good for testing to have a delay */
	.max_job_runtime = 60000, /* 60 seconds */
#endif
	.control_interval = 1000, /* 1000ms */
	.utilization_callback = mali_gpu_utilization_callback,
	.get_clock_info = NULL,
	.get_freq = NULL,
	.set_freq = NULL,
};

int mali_platform_device_init(struct platform_device *device)
{
	int num_pp_cores;
	int err = -1;

	/* Detect present Mali GPU and connect the correct resources to the device */
	num_pp_cores = 4;

	 if (device && !device->dev.dma_mask) {
		device->dev.dma_mask = &device->dev.coherent_dma_mask;		
		device->dev.coherent_dma_mask = DMA_BIT_MASK(32);	
#if 0		
#if defined(CONFIG_ARM64)
		device->dev.archdata.dma_ops = &noncoherent_swiotlb_dma_ops,
#endif
#endif		
	}

	err = platform_device_add_data(device, &mali_gpu_data, sizeof(mali_gpu_data));

	if (0 == err) {
#ifdef CONFIG_PM_RUNTIME
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 37))
		pm_runtime_set_autosuspend_delay(&(device->dev), 1000);
		pm_runtime_use_autosuspend(&(device->dev));
#endif
		pm_runtime_enable(&(device->dev));
#endif
		MALI_DEBUG_ASSERT(0 < num_pp_cores);
		mali_core_scaling_init(num_pp_cores);
	}

	return err;
}

int mali_platform_device_deinit(struct platform_device *device)
{
	MALI_IGNORE(device);

	MALI_DEBUG_PRINT(4, ("mali_platform_device_deinit() called\n"));

	mali_core_scaling_term();

	return 0;
}

static int param_set_core_scaling(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val, kp);

	if (1 == mali_core_scaling_enable) {
		mali_core_scaling_sync(mali_executor_get_num_cores_enabled());
	}
	return ret;
}

static struct kernel_param_ops param_ops_core_scaling = {
	.set = param_set_core_scaling,
	.get = param_get_int,
};

module_param_cb(mali_core_scaling_enable, &param_ops_core_scaling, &mali_core_scaling_enable, 0644);
MODULE_PARM_DESC(mali_core_scaling_enable, "1 means to enable core scaling policy, 0 means to disable core scaling policy");

void mali_gpu_utilization_callback(struct mali_gpu_utilization_data *data)
{
	if (1 == mali_core_scaling_enable) {
		mali_core_scaling_update(data);
	}
}
