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
#include <linux/module.h>
#include <mach/cpufreq.h>
#include <mach/mmi_panel_notifier.h>

#define CPUFREQ_LIMIT "cpufreq_limit"

static uint32_t suspend_max_freq __read_mostly = 1026000;
module_param(suspend_max_freq, uint, 0644);

static uint32_t previous_max_freq = MSM_CPUFREQ_NO_LIMIT;
static struct work_struct suspend_work, resume_work;
static struct notifier_block panel_nb;

static void cpufreq_limit_suspend(struct work_struct *work)
{
	unsigned int cpu = 0;

	previous_max_freq = cpufreq_quick_get_max(cpu);

	if (unlikely(previous_max_freq <= suspend_max_freq))
		return;

	for_each_possible_cpu(cpu) {
		msm_cpufreq_set_freq_limits(cpu, MSM_CPUFREQ_NO_LIMIT,
			suspend_max_freq ? suspend_max_freq : MSM_CPUFREQ_NO_LIMIT);

		pr_info("%s: CPU%d max freq limited to: %d Hz\n",
			CPUFREQ_LIMIT, cpu, suspend_max_freq);
	}
}

static void cpufreq_limit_resume(struct work_struct *work)
{
	unsigned int cpu = 0;

	if (unlikely(previous_max_freq <= suspend_max_freq))
		return;

	for_each_possible_cpu(cpu) {
		msm_cpufreq_set_freq_limits(cpu, MSM_CPUFREQ_NO_LIMIT,
			previous_max_freq);

		pr_info("%s: CPU%d max frequency restored to %d Hz\n",
			CPUFREQ_LIMIT, cpu, previous_max_freq);
	}
}

static int cpufreq_limit_panel_cb(struct notifier_block *this,
		unsigned long event, void *data)
{
	switch (event) {
	case MMI_PANEL_EVENT_PWR_ON:
		schedule_work(&resume_work);
		break;
	case MMI_PANEL_EVENT_PRE_DEINIT:
		schedule_work(&suspend_work);
		break;
	default:
		break;
	}

	return 0;
}

static int __init cpufreq_limit_init(void)
{
	pr_info("%s: Initialized", CPUFREQ_LIMIT);

	INIT_WORK(&suspend_work, cpufreq_limit_suspend);
	INIT_WORK(&resume_work, cpufreq_limit_resume);

	panel_nb.notifier_call = cpufreq_limit_panel_cb;
	if (mmi_panel_register_notifier(&panel_nb) != 0)
		pr_err("%s: Failed to register panel notifier\n", __func__);

	return 0;
}

static void __exit cpufreq_limit_exit(void)
{
	mmi_panel_unregister_notifier(&panel_nb);
}

late_initcall(cpufreq_limit_init);
module_exit(cpufreq_limit_exit);

MODULE_AUTHOR("Zhao Wei Liew <zhaoweiliew@gmail.com>");
MODULE_DESCRIPTION(CPUFREQ_LIMIT);
MODULE_LICENSE("GPLv2");
