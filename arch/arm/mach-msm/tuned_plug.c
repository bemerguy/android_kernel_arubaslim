/*
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

#include <linux/workqueue.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/rq_stats.h>
#include <linux/cpufreq.h>
#include <linux/delay.h>

static struct delayed_work tuned_plug_work;
static struct workqueue_struct *tunedplug_wq;

static unsigned int tuned_plug_active = 1;
module_param(tuned_plug_active, uint, 0644);

static unsigned int sampling_time;

static unsigned int downcpu[NR_CPUS] = {0};

static void inline down_one(void){
	unsigned int i = NR_CPUS-1;
	while (i > 0) {
		if (cpu_online(i)) {
			if (downcpu[i] > 30) {
				cpu_down(i);
				downcpu[i]=0;
				pr_info("tunedplug: DOWN cpu %d.", i);
			}
			else downcpu[i]++;
			return;
		}
		i--;
	}
}
static void inline up_one(void){
        unsigned int i;
        for (i = 1; i < NR_CPUS; i++) {
                if (!cpu_online(i)) {
			cpu_up(i);
			downcpu[i]=0;
			pr_info("tunedplug: UP cpu %d", i);
                        return;
                }
        }
}
static void __cpuinit tuned_plug_work_fn(struct work_struct *work)
{
	unsigned int nr_cpus, i;
	struct cpufreq_policy policy;

        queue_delayed_work_on(0, tunedplug_wq, &tuned_plug_work, sampling_time);

        if (!tuned_plug_active)
                return;

        nr_cpus = num_online_cpus();

	if (nr_cpus < NR_CPUS) {
		for_each_online_cpu(i) {
			if (cpufreq_get_policy(&policy, i) != 0)
				continue;
			if ((i < NR_CPUS-1) && (policy.cur && policy.max) && (policy.cur >= policy.max)) {
				up_one();
			}
		}
	}

	if (nr_cpus > 1) {
                for_each_online_cpu(i) {
                        if (i == 0 || (cpufreq_get_policy(&policy, i) != 0))
                                continue;
                        if ((policy.cur && policy.min) && (policy.cur <= policy.min)) {
				down_one();
				msleep(100);
			}
                }
	}
}

int __init tuned_plug_init(void)
{

	tunedplug_wq = alloc_workqueue("tunedplug", WQ_HIGHPRI, 0);

	INIT_DELAYED_WORK(&tuned_plug_work, tuned_plug_work_fn);

	sampling_time = msecs_to_jiffies(50);

	queue_delayed_work_on(0, tunedplug_wq, &tuned_plug_work, msecs_to_jiffies(40000));

	return 0;
}

MODULE_AUTHOR("Heiler Bemerguy <heiler.bemerguy@gmail.com>");
MODULE_DESCRIPTION("'tuned_plug' - A simple cpu hotplug driver");
MODULE_LICENSE("GPL");

late_initcall(tuned_plug_init);
