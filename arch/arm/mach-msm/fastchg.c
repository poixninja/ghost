/*
 * based on sysfs interface from:
 *	Chad Froebel <chadfroebel@gmail.com> &
 *	Jean-Pierre Rasquin <yank555.lu@gmail.com>
 * for backwards compatibility
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

/*
 * To disable fast charge: Assign 0 to fast_charge_level
*/

#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/fastchg.h>

int fast_charge_level;

static ssize_t charge_level_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", fast_charge_level);
}

static ssize_t charge_level_store(struct kobject *kobj,
			struct kobj_attribute *attr, const char *buf,
			size_t count)
{

	int new_charge_level;

	sscanf(buf, "%du", &new_charge_level);

	switch (new_charge_level) {
		case FAST_CHARGE_DISABLED:
		case FAST_CHARGE_500:
		case FAST_CHARGE_700:
		case FAST_CHARGE_900:
		case FAST_CHARGE_AC:
		case FAST_CHARGE_1300:
		case FAST_CHARGE_1500:
			fast_charge_level = new_charge_level;
			return count;
		default:
			return -EINVAL;
	}
	return -EINVAL;
}

/* sysfs interface for "fast_charge_levels" */
static ssize_t available_charge_levels_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", FAST_CHARGE_LEVELS);
}

static struct kobj_attribute available_charge_levels_attribute =
	__ATTR(available_charge_levels, 0444,
		available_charge_levels_show, NULL);

static struct kobj_attribute fast_charge_level_attribute =
	__ATTR(fast_charge_level, 0666,
		charge_level_show,
		charge_level_store);

static struct attribute *force_fast_charge_attrs[] = {
	&fast_charge_level_attribute.attr,
	&available_charge_levels_attribute.attr,
	NULL,
};

static struct attribute_group force_fast_charge_attr_group = {
	.attrs = force_fast_charge_attrs,
};

/* Initialize fast charge sysfs folder */
static struct kobject *force_fast_charge_kobj;

int force_fast_charge_init(void)
{
	int force_fast_charge_retval;

	/* Fast charge level set at AC by default */
	fast_charge_level = FAST_CHARGE_AC;

	force_fast_charge_kobj
		= kobject_create_and_add("fast_charge", kernel_kobj);

	if (!force_fast_charge_kobj) {
		return -ENOMEM;
	}

	force_fast_charge_retval
		= sysfs_create_group(force_fast_charge_kobj,
				&force_fast_charge_attr_group);

	if (force_fast_charge_retval)
		kobject_put(force_fast_charge_kobj);

	return force_fast_charge_retval;
}

void force_fast_charge_exit(void)
{
	kobject_put(force_fast_charge_kobj);
}

module_init(force_fast_charge_init);
module_exit(force_fast_charge_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jean-Pierre Rasquin <yank555.lu@gmail.com>");
MODULE_AUTHOR("Paul Reioux <reioux@gmail.com>");
MODULE_AUTHOR("Zhao Wei Liew <zhaoweiliew@gmail.com>");
MODULE_DESCRIPTION("Fast Charge Hack for Android");
