/*
 * drivers/cpufreq/cpufreq_limit.c
 *
 * Copyright (C) Zhao Wei Liew <zhaoweiliew@gmail.com>
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
 */

#include <linux/cpufreq.h>
#include <linux/earlysuspend.h>
#include <linux/module.h>
#include <mach/cpufreq.h>

#define CPUFREQ_LIMIT "cpufreq_limit"

static uint32_t suspend_max_freq = 1026000;
static uint32_t previous_max_freq = MSM_CPUFREQ_NO_LIMIT;

static void cpufreq_limit_early_suspend(struct early_suspend *h)
{
	int cpu = 0;

	previous_max_freq = cpufreq_quick_get_max(0);

	for_each_possible_cpu(cpu) {
		msm_cpufreq_set_freq_limits(cpu, MSM_CPUFREQ_NO_LIMIT,
                        suspend_max_freq ? suspend_max_freq : MSM_CPUFREQ_NO_LIMIT);

                pr_info("%s: Max freq limited to: %d\n", CPUFREQ_LIMIT,
                        suspend_max_freq);
	}
}

static void cpufreq_limit_late_resume(struct early_suspend *h)
{
	int cpu = 0;

	for_each_possible_cpu(cpu) {
		msm_cpufreq_set_freq_limits(cpu, MSM_CPUFREQ_NO_LIMIT,
                        previous_max_freq);

		pr_info("%s: Max frequency restored.\n", CPUFREQ_LIMIT);
	}
}

static struct early_suspend cpufreq_limit_suspend_handler = {
	.level = EARLY_SUSPEND_LEVEL_DISABLE_FB,
	.suspend = cpufreq_limit_early_suspend,
	.resume = cpufreq_limit_late_resume,
};

static int __init cpufreq_limit_init(void)
{
	pr_info("%s: Initialized", CPUFREQ_LIMIT);

	register_early_suspend(&cpufreq_limit_suspend_handler);

	return 0;
}
late_initcall(cpufreq_limit_init);

MODULE_AUTHOR("Zhao Wei Liew <zhaoweiliew@gmail.com>");
MODULE_DESCRIPTION(CPUFREQ_LIMIT);
MODULE_LICENSE("GPLv2");
