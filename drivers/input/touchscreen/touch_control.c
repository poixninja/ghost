/*
 * drivers/input/touchscreen/touch_control.c
 *
 *
 * Copyright (c) 2013, Dennis Rassmann <showp1984@gmail.com>
 *               2014, Zhao Wei Liew <zhaoweiliew@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/input.h>
#include <linux/hrtimer.h>
#include <mach/mmi_panel_notifier.h>
#include <linux/input/touch_control.h>

/* Version, author, desc, etc */
#define DRIVER_AUTHOR_GENERIC "Dennis Rassmann <showp1984@gmail.com>"
#define DRIVER_AUTHOR_GHOST "Zhao Wei Liew <zhaoweiliew@gmail.com>"
#define DRIVER_DESCRIPTION "Touch Control for Moto X"
#define LOGTAG "[touch_control]: "

MODULE_LICENSE("GPLv2");
MODULE_AUTHOR(DRIVER_AUTHOR_GENERIC);
MODULE_AUTHOR(DRIVER_AUTHOR_GHOST);
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);

/* Defaults */
#define TC_Y_MAX				1280
#define TC_X_MAX				720
#define TC_Y_LIMIT				(TC_Y_MAX - 100)
#define TC_X_FINAL				160

/* Right -> Left */
#define TC_X_B0				430
#define TC_X_B1				(TC_X_B0 - 300)
#define TC_X_B2				(TC_X_B0 - 70)

/* Left -> Right */
#define TC_X_B3				(TC_X_B0 + 60)
#define TC_X_B4				(TC_X_MAX - 145)
#define TC_X_B5				(TC_X_MAX - TC_X_B0)

/* Resources */
struct touch_control_stats {
	int touch_x;
	int touch_y;
	bool touch_x_called;
	bool touch_y_called;
	bool suspended;
	bool exec_count;
	bool screen_touched;
	bool barrier[2];
	bool r_barrier[2];
} stats = {
	.touch_x = 0,
	.touch_y = 0,
	.touch_x_called = false,
	.touch_y_called = false,
	.suspended = false,
	.exec_count = true,
	.screen_touched = false,
	.barrier[0] = false,
	.barrier[1] = false,
	.r_barrier[0] = false,
	.r_barrier[1] = false,
};

struct touch_control_tunables {
	bool debug;
	unsigned int pwrkey_duration;
} tunables = {
	.debug = false,
	.pwrkey_duration = 60,
};

bool s2w_enabled = false;
bool s2s_enabled = true;

static struct notifier_block panel_nb;
static struct input_dev *touch_control_pwrdev;
static DEFINE_MUTEX(pwrkeyworklock);
static struct workqueue_struct *tc_input_wq;
static struct work_struct input_work, suspend_work, resume_work;

/* Power key work function */
static void presspwr(struct work_struct *presspwr_work)
{
	if (!mutex_trylock(&pwrkeyworklock))
		return;

	input_event(touch_control_pwrdev, EV_KEY, KEY_POWER, 1);
	input_event(touch_control_pwrdev, EV_SYN, 0, 0);

	msleep(tunables.pwrkey_duration);

	input_event(touch_control_pwrdev, EV_KEY, KEY_POWER, 0);
	input_event(touch_control_pwrdev, EV_SYN, 0, 0);

	msleep(tunables.pwrkey_duration);

	mutex_unlock(&pwrkeyworklock);
}
static DECLARE_WORK(presspwr_work, presspwr);

/* Power key trigger */
static void pwrtrigger(void)
{
	schedule_work(&presspwr_work);
}

/* Reset on finger release */
static void reset(void)
{
	stats.exec_count = true;
	stats.barrier[0] = false;
	stats.barrier[1] = false;
	stats.r_barrier[0] = false;
	stats.r_barrier[1] = false;
	stats.screen_touched = false;
}

/* Touch Control main function */
static void detect(int x, int y)
{
	int prevx = 0, nextx = 0;
	int r_prevx = 0, r_nextx = 0;

	if (tunables.debug)
		pr_info(LOGTAG"x: %d, y: %d\n", x, y);

	if (stats.suspended &&
		s2w_enabled) {
		prevx = 0;
		nextx = TC_X_B1;

		if (stats.barrier[0] ||
			(x > prevx &&
			x < nextx &&
			y > 0)) {
			prevx = nextx;
			nextx = TC_X_B2;
			stats.barrier[0] = true;

			if (stats.barrier[1] ||
				(x > prevx &&
				x < nextx &&
				y > 0)) {
				prevx = nextx;
				stats.barrier[1] = true;

				if (x > prevx &&
					y > 0 &&
					x > TC_X_MAX - TC_X_FINAL &&
					stats.exec_count) {
					pr_info(LOGTAG "ON\n");

					pwrtrigger();
					stats.exec_count = false;
				}
			}
		}

		r_prevx = (TC_X_MAX - TC_X_FINAL);
		r_nextx = TC_X_B2;

		if (stats.r_barrier[0] ||
			(x < r_prevx &&
			x > r_nextx &&
			y < TC_Y_MAX)) {
			r_prevx = r_nextx;
			r_nextx = TC_X_B1;
			stats.r_barrier[0] = true;

			if (stats.r_barrier[1] ||
				(x < r_prevx &&
				x > r_nextx &&
				y < TC_Y_MAX)) {
				r_prevx = r_nextx;
				stats.r_barrier[1] = true;

				if (x < r_prevx &&
					x < TC_X_FINAL &&
					y < TC_Y_MAX &&
					stats.exec_count) {
					pr_info(LOGTAG "ON\n");

					pwrtrigger();
					stats.exec_count = false;
				}
			}
		}
	} else if (!stats.suspended &&
		s2s_enabled) {
		stats.screen_touched = true;
		prevx = (TC_X_MAX - TC_X_FINAL);
		nextx = TC_X_B2;

		if (stats.barrier[0] ||
			(x < prevx &&
			x > nextx &&
			y > TC_Y_LIMIT)) {
			prevx = nextx;
			nextx = TC_X_B1;
			stats.barrier[0] = true;

			if (stats.barrier[1] ||
				(x < prevx &&
				x > nextx &&
				y > TC_Y_LIMIT)) {
				prevx = nextx;
				stats.barrier[1] = true;

				if (x < prevx &&
					y > TC_Y_LIMIT &&
					x < TC_X_FINAL &&
					stats.exec_count) {
					pr_info(LOGTAG "OFF\n");

					pwrtrigger();
					stats.exec_count = false;
				}
			}
		}

		r_prevx = TC_X_B0;
		r_nextx = TC_X_B3;

		if (stats.r_barrier[0] ||
			(x > r_prevx &&
			x < r_nextx &&
			y > TC_Y_LIMIT)) {
			r_prevx = r_nextx;
			r_nextx = TC_X_B4;
			stats.r_barrier[0] = true;

			if (stats.r_barrier[1] ||
				(x > r_prevx &&
				x < r_nextx &&
				y > TC_Y_LIMIT)) {
				r_prevx = r_nextx;
				stats.r_barrier[1] = true;

				if (x > r_prevx &&
					y > TC_Y_LIMIT &&
					x > TC_X_B5 &&
					stats.exec_count) {
					pr_info(LOGTAG "OFF\n");

					pwrtrigger();
					stats.exec_count = false;
				}
			}
		}
	}
}

static void input_callback(struct work_struct *unused)
{
	detect(stats.touch_x, stats.touch_y);
}

static void tc_input_event(struct input_handle *handle, unsigned int type,
		unsigned int code, int value)
{
	if ((!s2s_enabled && !s2w_enabled) ||
		(!s2w_enabled && stats.suspended) ||
		(!s2s_enabled && !stats.suspended))
		return;

	if (code == ABS_MT_SLOT ||
		(code == ABS_MT_TRACKING_ID &&
		value == -1)) {
		reset();
		return;
	}

	if (code == ABS_MT_POSITION_X) {
		stats.touch_x = value;
		stats.touch_x_called = true;
	}

	if (code == ABS_MT_POSITION_Y) {
		stats.touch_y = value;
		stats.touch_y_called = true;
	}

	if (stats.touch_x_called &&
		stats.touch_y_called) {
		stats.touch_x_called = false;
		stats.touch_y_called = false;
		queue_work_on(0, tc_input_wq, &input_work);
	}
}
static int tc_input_connect(struct input_handler *handler,
		struct input_dev *dev, const struct input_device_id *id)
{
	struct input_handle *handle;
	int error;

	handle = kzalloc(sizeof(struct input_handle), GFP_KERNEL);
	if (!handle)
		return -ENOMEM;

	handle->dev = dev;
	handle->handler = handler;
	handle->name = "touch_control";

	error = input_register_handle(handle);
	if (error)
		goto err2;

	error = input_open_device(handle);
	if (error)
		goto err1;

	return 0;
err1:
	input_unregister_handle(handle);
err2:
	kfree(handle);
	return error;
}

static void tc_input_disconnect(struct input_handle *handle)
{
	input_close_device(handle);
	input_unregister_handle(handle);
	kfree(handle);
}

static const struct input_device_id tc_ids[] = {
	/* Multi-touch touchscreen */
	{
		.flags = INPUT_DEVICE_ID_MATCH_EVBIT |
			INPUT_DEVICE_ID_MATCH_ABSBIT,
		.evbit = { BIT_MASK(EV_ABS) },
		.absbit = { [BIT_WORD(ABS_MT_POSITION_X)] =
			BIT_MASK(ABS_MT_POSITION_X) |
			BIT_MASK(ABS_MT_POSITION_Y) },
	},
	{ },
};

static struct input_handler tc_input_handler = {
	.event		= tc_input_event,
	.connect	= tc_input_connect,
	.disconnect	= tc_input_disconnect,
	.name		= "tc_input_req",
	.id_table	= tc_ids,
};

static void touch_control_suspend(struct work_struct *work)
{
	stats.suspended = true;
}

static void touch_control_resume(struct work_struct *work)
{
	stats.suspended = false;
}

static int touch_control_panel_cb(struct notifier_block *this,
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

/*
 * SYSFS interface start
 */
static ssize_t show_s2s_enabled(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, sizeof(*buf), "%u\n", s2s_enabled);
}

static ssize_t store_s2s_enabled(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	s2s_enabled = val;

	return count;
}

static struct kobj_attribute s2s_enabled_attribute =
	__ATTR(s2s_enabled, S_IWUSR | S_IRUGO,
	show_s2s_enabled, store_s2s_enabled);

static ssize_t show_s2w_enabled(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, sizeof(*buf), "%u\n", s2w_enabled);
}

static ssize_t store_s2w_enabled(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	s2w_enabled = val;

	return count;
}

static struct kobj_attribute s2w_enabled_attribute =
	__ATTR(s2w_enabled, S_IWUSR | S_IRUGO,
	show_s2w_enabled, store_s2w_enabled);

static ssize_t show_debug(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, sizeof(*buf), "%u\n", tunables.debug);
}

static ssize_t store_debug(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	tunables.debug = val;

	return count;
}

static struct kobj_attribute debug_attribute =
	__ATTR(debug, S_IWUSR | S_IRUGO,
	show_debug, store_debug);

static ssize_t show_pwrkey_duration(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, sizeof(*buf), "%u\n", tunables.pwrkey_duration);
}

static ssize_t store_pwrkey_duration(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int ret;
	unsigned long val;

	ret = kstrtoul(buf, 0, &val);
	if (ret < 0)
		return ret;

	tunables.pwrkey_duration = val;
	return count;
}

static struct kobj_attribute pwrkey_duration_attribute =
	__ATTR(pwrkey_duration, S_IWUSR | S_IRUGO,
	show_pwrkey_duration, store_pwrkey_duration);

static struct attribute *touch_control_attrs[] = {
	&s2s_enabled_attribute.attr,
	&s2w_enabled_attribute.attr,
	&debug_attribute.attr,
	&pwrkey_duration_attribute.attr,
	NULL,
};

static struct attribute_group touch_control_attr_group = {
	.attrs = touch_control_attrs,
	.name = "touch_control",
};

/*
 * SYSFS interface end
 */

/*
 * INIT / EXIT stuff below here
 */

static int __init touch_control_init(void)
{
	int rc = 0;

	touch_control_pwrdev = input_allocate_device();
	if (!touch_control_pwrdev) {
		pr_err("Can't allocate suspend autotest power button\n");
		goto err_alloc_dev;
	}

	input_set_capability(touch_control_pwrdev, EV_KEY, KEY_POWER);
	touch_control_pwrdev->name = "tc_pwrkey";
	touch_control_pwrdev->phys = "tc_pwrkey/input0";

	rc = input_register_device(touch_control_pwrdev);
	if (rc) {
		pr_err("%s: input_register_device err=%d\n", __func__, rc);
		goto err_input_dev;
	}

	tc_input_wq = create_workqueue("tc_input_wq");
	if (!tc_input_wq) {
		pr_err("%s: Failed to create tc_input_wq workqueue\n",
			__func__);
		return -EFAULT;
	}

	INIT_WORK(&input_work, input_callback);
	INIT_WORK(&resume_work, touch_control_resume);
	INIT_WORK(&suspend_work, touch_control_suspend);

	rc = input_register_handler(&tc_input_handler);
	if (rc)
		pr_err("%s: Failed to register tc_input_handler\n", __func__);

	panel_nb.notifier_call = touch_control_panel_cb;
	if (mmi_panel_register_notifier(&panel_nb) != 0)
		pr_err("%s: Failed to register panel notifier\n", __func__);

	rc = sysfs_create_group(kernel_kobj, &touch_control_attr_group);
	if (rc)
		pr_warn("%s: sysfs_create_file failed\n", __func__);

err_input_dev:
	input_free_device(touch_control_pwrdev);
err_alloc_dev:
	pr_info(LOGTAG"%s done\n", __func__);

	return 0;
}

static void __exit touch_control_exit(void)
{
	mmi_panel_unregister_notifier(&panel_nb);
	input_unregister_handler(&tc_input_handler);
	destroy_workqueue(tc_input_wq);
	input_unregister_device(touch_control_pwrdev);
	input_free_device(touch_control_pwrdev);
}

late_initcall(touch_control_init);
module_exit(touch_control_exit);
