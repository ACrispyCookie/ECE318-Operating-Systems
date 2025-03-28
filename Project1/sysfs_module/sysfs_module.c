#include <linux/module.h>
#include <linux/printk.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/sched.h>

static struct kobject *team_kobject;

static ssize_t on_file_read(struct kobject *kobj, struct kobj_attribute *attr, char *buf) {
    printk("find_roots system call called by process %d\n", current->pid);
    
    struct task_struct *process;
    for (process = current; ; process = process->parent) {
        printk("id: %d, name: %s\n", process->pid, process->comm);
        
        if (process->pid == 1)
            break;
    }
    
    return sprintf(buf, "%d\n", current->pid);
}

struct kobj_attribute roots_attribute = __ATTR(find_roots, 0660, on_file_read, NULL);

static int __init sysfs_module_init(void) {
    int error = 0;

    team_kobject = kobject_create_and_add("team367237713796", kernel_kobj);

    if (!team_kobject)
        return -ENOMEM;

    error = sysfs_create_file(team_kobject, &roots_attribute.attr);
    if (error)
        printk("Failed to create the foo file in /sys/kernel/team367237713796\n");

    return error;
}

static void __exit sysfs_module_exit(void) {
    printk("Module uninitialized successfully\n");
    kobject_put(team_kobject);
}

module_init(sysfs_module_init);
module_exit(sysfs_module_exit);
MODULE_LICENSE("GPL");