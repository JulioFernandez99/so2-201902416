 #include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/mm.h>
#include <linux/uaccess.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Julio_Fernandez");
MODULE_DESCRIPTION("Archivo en /proc con información de procesos");
MODULE_VERSION("1.0");

#define PROC_FILENAME "proc_info"
#define MAX_INPUT_SIZE 16

static char user_pid[MAX_INPUT_SIZE] = "";

static void show_process_info(struct seq_file *s, struct task_struct *task) {
    if (task->mm) {
        unsigned long vm_size = (task->mm->total_vm << (PAGE_SHIFT - 10)) / 1024; // Convertir a MB
        unsigned long vm_rss = (get_mm_rss(task->mm) << (PAGE_SHIFT - 10)) / 1024; // Convertir a MB
        seq_printf(s, "%-8d %-20s %-10lu %-10lu %-10d\n",
                   task->pid, task->comm, vm_size, vm_rss, task->signal->oom_score_adj);
    } else {
        seq_printf(s, "%-8d %-20s %-10s %-10s %-10s\n",
                   task->pid, task->comm, "N/A", "N/A", "N/A");
    }
}

static int seq_show(struct seq_file *s, void *v) {
    struct task_struct *task = (struct task_struct *)v;

    if (user_pid[0] != '\0') { // Si se especificó un PID
        int pid = simple_strtol(user_pid, NULL, 10);
        if (task->pid == pid) {
            seq_printf(s, "PID     Nombre               VmSize (MB) VmRSS (MB)  OOMScore\n");
            seq_printf(s, "----------------------------------------------------------\n");
            show_process_info(s, task);
            return 0; // Mostrar solo un proceso
        }
        return 0; // Saltar a la siguiente iteración
    }

    // Mostrar todos los procesos si no se especificó PID
    if (s->count == 0) { // Encabezado solo al inicio
        seq_printf(s, "PID     Nombre               VmSize (MB) VmRSS (MB)  OOMScore\n");
        seq_printf(s, "----------------------------------------------------------\n");
    }
    show_process_info(s, task);

    return 0;
}

static void *seq_start(struct seq_file *s, loff_t *pos) {
    struct task_struct *task;
    loff_t i = 0;

    for_each_process(task) {
        if (i++ == *pos) {
            return task;
        }
    }
    return NULL;
}

static void *seq_next(struct seq_file *s, void *v, loff_t *pos) {
    struct task_struct *task = (struct task_struct *)v;
    ++*pos;

    task = next_task(task);
    if (task == &init_task) {
        return NULL; // Terminamos la iteración
    }
    return task;
}

static void seq_stop(struct seq_file *s, void *v) {
    // No se necesita liberar recursos aquí
}

static const struct seq_operations proc_seq_ops = {
    .start = seq_start,
    .next = seq_next,
    .stop = seq_stop,
    .show = seq_show,
};

static ssize_t proc_write(struct file *file, const char __user *buffer, size_t count, loff_t *f_pos) {
    if (count > MAX_INPUT_SIZE - 1) {
        return -EINVAL; // Entrada demasiado larga
    }

    if (copy_from_user(user_pid, buffer, count)) {
        return -EFAULT;
    }

    user_pid[count] = '\0'; // Asegurarse de que sea una cadena terminada en '\0'

    // Eliminar saltos de línea o espacios en blanco
    strim(user_pid);

    if (user_pid[0] == '\0') {
        printk(KERN_INFO "Mostrando todos los procesos\n");
    } else {
        printk(KERN_INFO "PID recibido: %s\n", user_pid);
    }

    return count;
}

static int proc_open(struct inode *inode, struct file *file) {
    return seq_open(file, &proc_seq_ops);
}

static const struct proc_ops proc_file_ops = {
    .proc_open = proc_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_write = proc_write,
    .proc_release = seq_release,
};

static int __init proc_info_init(void) {
    struct proc_dir_entry *entry;

    entry = proc_create(PROC_FILENAME, 0666, NULL, &proc_file_ops);
    if (!entry) {
        return -ENOMEM;
    }

    printk(KERN_INFO "/proc/%s creado.\n", PROC_FILENAME);
    return 0;
}

static void __exit proc_info_exit(void) {
    remove_proc_entry(PROC_FILENAME, NULL);
    printk(KERN_INFO "/proc/%s eliminado.\n", PROC_FILENAME);
}

module_init(proc_info_init);
module_exit(proc_info_exit);
