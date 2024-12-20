#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/sysinfo.h>

#define PROC_FILENAME "memory_stats"

static int memory_stats_show(struct seq_file *m, void *v) {
    struct sysinfo si;
    unsigned long totalram, freeram, usedram, activeram;

    // obtengo la info
    si_meminfo(&si);

    // convierto totalram a KB y luego a MB
    totalram = (si.totalram << (PAGE_SHIFT - 10)) / 1024;
    freeram = (si.freeram << (PAGE_SHIFT - 10)) / 1024;
    usedram = totalram - freeram; // Memoria usada en MB
    activeram = usedram;          // En este contexto, la activa coincide con la usada

    // calcular porcentajes
    unsigned long free_percent = (freeram * 100) / totalram;
    unsigned long used_percent = (usedram * 100) / totalram;
    unsigned long active_percent = (activeram * 100) / totalram;

    // escribo en el archivo /proc/memory_stats
    seq_printf(m, "------------------------------\n");
    seq_printf(m, "|  Metrica   |  %%  |   MB   |\n");
    seq_printf(m, "------------------------------\n");
    seq_printf(m, "| RAM Libre  | %3lu | %6lu |\n", free_percent, freeram);
    seq_printf(m, "| RAM Usada  | %3lu | %6lu |\n", used_percent, usedram);
    seq_printf(m, "| RAM Activa | %3lu | %6lu |\n", active_percent, activeram);
    seq_printf(m, "-----------------------------\n");

    return 0;
}

static int memory_stats_open(struct inode *inode, struct file *file) {
    return single_open(file, memory_stats_show, NULL);
}

static const struct proc_ops memory_stats_fops = {
    .proc_open = memory_stats_open,
    .proc_read = seq_read,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};

static int __init memory_stats_init(void) {
    // crear el archivo en /proc
    if (!proc_create(PROC_FILENAME, 0, NULL, &memory_stats_fops)) {
        pr_err("Error al crear /proc/%s\n", PROC_FILENAME);
        return -ENOMEM;
    }
    pr_info("Módulo de estadísticas de memoria cargado. Archivo: /proc/%s\n", PROC_FILENAME);
    return 0;
}

static void __exit memory_stats_exit(void) {
    // eliminar el archivo de /proc
    remove_proc_entry(PROC_FILENAME, NULL);
    pr_info("Módulo de estadísticas de memoria descargado. Archivo eliminado: /proc/%s\n", PROC_FILENAME);
}

module_init(memory_stats_init);
module_exit(memory_stats_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Julio_Fernandez");
MODULE_DESCRIPTION("Modulo para mostrar estadisticas de memoria en /proc.");

