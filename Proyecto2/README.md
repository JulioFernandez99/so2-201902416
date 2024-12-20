

![](https://user-images.githubusercontent.com/73097560/115834477-dbab4500-a447-11eb-908a-139a6edaec5c.gif)

<div style="text-align: center;">
    <span style="font-size: 18px;">Universidad de San Carlos de Guatemala</span><br>
    <span style="font-size: 18px;">Facultad de Ingeniería</span><br>
    <span style="font-size: 18px;">Escuela de Ciencias y Sistemas</span><br>
    <span style="font-size: 18px;">Laboratorio de Sistemas Operativos 2<br>
    <span style="font-size: 18px;">Julio Alfredo Fernández Rodríguez 201902416</span>
</div>

<br>

![](https://user-images.githubusercontent.com/73097560/115834477-dbab4500-a447-11eb-908a-139a6edaec5c.gif)



# Tamalloc

Para implementar ``tamalloc`` fue muy complicado, ya que no comprendi del todo si necesitaba una syscall. Mi enfoque principa fue pensar que como era una variacion de calloc pues tenia que definirlo en el mismo archivo, y que tambien tenia que hacer una variacion de memset. Pero buscando documentacion de como reservar memoria sin comprometer inmediatamente recursos fisicos, asi que encontre ``MAP_NORESERVE`` que era una bandera que podria utilizar ya que esta permite asignar memoria fisica hasta que realmente se necesite.

https://man7.org/linux/man-pages/man2/mmap.2.html

Para aclarar mas mis dudas pregunte a chatGPT y me respondio: `MAP_NORESERVE` es una bandera usada en la función `mmap` en sistemas Unix/Linux. Indica que el sistema no debe reservar memoria física inmediatamente para el mapeo solicitado, sino que la asignará solo cuando se acceda. Esto ahorra recursos iniciales, pero puede causar errores si no hay suficiente memoria cuando se accede al área mapeada.

Lo siguiente fue que me di cuenta que necesitaba pasar por argumento el size de la memoria que que queria reservar y por la forma en que trabaja el kernel es por bloques osea por ``PAGE_SIZE``. Aca algo que me llamo la atencion como es que se determina el tamaño de una pagina en memoria virtual asi que lo busque y encontre esto ( https://unix.stackexchange.com/questions/128213/how-is-page-size-determined-in-virtual-address-space ), tambien me causo como lo hacian en el kernel y buscando encontre esto ( https://stackoverflow.com/questions/66693793/why-do-memory-managers-use-size-page-size-1-page-size-to-calculate-the-num ) asi tenia que redondear mis ``pages`` a un multiplo de ``PAGE_SIZE`` para mejorar la alineacion de memoria virtual.

Teniendo claro lo anterior, ahora como asigno la memoria? buscando encontre la funcion ``mmap`` que se utiliza para asignar memoria pero en espacio de usuario, asi que definitivamente no me servia, segui buscando y encontre ``vm_mmap`` que es practicamente lo mismo que ``mmap`` solo que en espacio de kernel ( https://web.mit.edu/darwin/src/modules/xnu/osfmk/man/vm_map.html )

Algo que me dio curiosida es que si podria utilizar las mismas flags como argumentos en mmap, como no encontre algo que me ayudara le pregunte a chatGPT y me respondio:

No, no puedes utilizar las mismas flags de **mmap** directamente en **vm_mmap**. Aunque ambas funciones están relacionadas con la asignación de memoria, **mmap** es una llamada del sistema en usuarios, mientras que **vm_mmap** es una función interna en el núcleo (kernel) de Linux, utilizada por el sistema operativo para gestionar la memoria virtual.

**vm_mmap** y **mmap** pueden tener conjuntos de banderas diferentes porque tienen propósitos y contextos diferentes. Las banderas que usas con **mmap** en el espacio de usuario, como **MAP_NORESERVE**, **MAP_SHARED**, y **MAP_PRIVATE**, no se traducen directamente a **vm_mmap**. En el espacio del kernel, la administración de la memoria es más compleja y tiene consideraciones adicionales que no están disponibles o son relevantes en el espacio de usuario.

Si estás trabajando en el espacio de usuario, debes usar **mmap** y sus banderas correspondientes. Si estás desarrollando en el espacio del kernel, necesitarás usar las funciones y banderas específicas de **vm_mmap**, que están diseñadas para el control de la memoria virtual a nivel del núcleo.

Teniendo todo esto claro implemente mi codigo



# syscalls.c
```c
asmlinkage long sys_tamalloc(size_t size);
```


# sys.c 

```c
#include <linux/mman.h>

/*
	TODO:
	-Redondear a un multiplo de PAGE_SIZE para mejorar la alineacion de memoria virtual.
	- MAP_PRIVATE -> asegura que los cambios no afecten a otros procesos.
	- MAP_ANONYMOUS -> desvincula la memoria de archivos fisicos.
	- MAP_NORESERVE -> reserva la memoria hasta el acceso.
	- Permitir que la memoria asignada sea accesible para escributra y lectura
	- 

*/

SYSCALL_DEFINE1(tamalloc, size_t, size) {

void __user *addr;

// pages -> cantidad de paginas
unsigned long pages, mem_size;

  
// verifico que la memoria solicitada no sea cero
if (size == 0) {

pr_err("cantidad de memoria solicitada no puede ser 0.\n");

return -EINVAL;

}

  

// redondeo 

pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;

mem_size = pages * PAGE_SIZE;

  

// devolver a espacio de usuario el puntero -> void __user *
// vm_mmap me devuelve una direccion de memoria

addr = (void __user *)vm_mmap(NULL, 0, mem_size,

PROT_READ | PROT_WRITE,

MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE,

0);

  

if (IS_ERR(addr)) {

pr_err("no se puede asignar memoria.\n");

return PTR_ERR(addr);

}

  

// No inicializo la memoria, simplemente devuelvo la dirección

pr_info("memoria asignada exitosamente.\n");

  

// Devuelvo la dirección de la memoria asignada

return (unsigned long)addr;

}
```
# tabla de simbolos .tbl
```tbl
549    common    tamalloc    sys_tamalloc
```

---
# Modulo para vmSize y vmRSS

Este codigo ya lo tenia implementado en su mayoria en el curso de sistemas operativos1 con la diferencia de que lo usaba para acceder a detalles como el nombre del proceso, el PID y otros atributos básicos. Esta funcionalidad  ya la tenia implementada en mi codigo inicialmente, y el propósito era mostrar información general de los procesos cuando el usuario consultaba el archivo `/proc/proc_info`.

Los cambios que realice fueron pocos y consistieron principalmente en agregar las líneas que calculan y muestran estos valores (`VmSize` y `VmRSS`). El resto del código, incluyendo la parte que recorre los procesos (`for_each_process`), la gestión del archivo `/proc/proc_info`, y la forma en que se muestran los datos (`seq_file`), se mantuvo igual, ya que funcionaba completamente en mi codigo original.

En el codigo ya estaba usando `task_struct` para acceder a la informacion de cada proceso. La estructura `task_struct` lo que hace es almacenar datos sobre los procesos del lado del kernel osea en espacio del kernel, como el PID, el nombre del proceso (`comm`), entre otros atributos. Esta estructura ya la estaba usando para recuperar información de los procesos, lo que me permitió agregar `VmSize` y `VmRSS` sin realizar tantos cambios en mi codigo.

Para agregar **`VmSize`** de un proceso utilicé el campo `task->mm->total_vm` de la estructura `task_struct`. Este campo ya estaba accesible en mi codigo, ya que anteriormente obtenia otros valores del proceso, como el nombre y el PID. Solo se añadi la conversión de este valor en KB a MB para que fuera mas legible.
    
Y para **`VmRSS`** fue de manera similar, la información sobre la memoria residente (`VmRSS`) ya estaba disponible a través de la funcion `get_mm_rss()`. Solo añadi el calculo necesario para convertir el valor de paginas a MB y mostrarlo de manera más accesible.

```c
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
```
Algo que no pude hacer es poner de colores el nombre del pid ya que como estoy escribiendo en un archivo me tiraba error de ANSI.

Forma de uso:
### Todos lo procesos
`` cho "" > /proc/proc_info  ``
`` watch cat /proc/proc_info
 ``
### Solo un proceso
`` cho "201902416" > /proc/proc_info  ``
`` watch cat /proc/proc_info

---
# Estadisticas de memoria

Esto si lo tenia completamente implementado de sistemas operativos1
```c
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

    // obtengo la información del sistema
    si_meminfo(&si);

    // convierto totalram a KB y luego a MB
    totalram = (si.totalram << (PAGE_SHIFT - 10)) / 1024;
    freeram = (si.freeram << (PAGE_SHIFT - 10)) / 1024;
    usedram = totalram - freeram; // memoria usada en MB
    activeram = usedram;          // en este contexto, la activa coincide con la usada

    // calcular porcentajes
    unsigned long free_percent = (freeram * 100) / totalram;
    unsigned long used_percent = (usedram * 100) / totalram;
    unsigned long active_percent = (activeram * 100) / totalram;

    // escribir en el archivo /proc/memory_stats
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
    pr_info("Modulo de estadisticas de memoria cargado. Archivo: /proc/%s\n", PROC_FILENAME);
    return 0;
}

static void __exit memory_stats_exit(void) {
    // eliminar el archivo de /proc
    remove_proc_entry(PROC_FILENAME, NULL);
    pr_info("Modulo de estadísticas de memoria descargado. Archivo eliminado: /proc/%s\n", PROC_FILENAME);
}

module_init(memory_stats_init);
module_exit(memory_stats_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Julio Fernandez");
MODULE_DESCRIPTION("Modulo para mostrar estadisticas de memoria en /proc/so1_memstats. ");

```

---
# Planificacion

Aquí tienes la tabla con la actividad dividida en dos filas para los días 2 y 3:

|**Día**|**Actividad**|
|---|---|
|**Día 1**|Este día estaba trabajando en un proyecto que no era el correcto, ya lo había terminado.|
|**Día 2**|Para implementar `tamalloc` fue complicado, ya que no comprendí completamente si necesitaba una syscall. Pensé que, como era una variación de `calloc`, debía definirla en el mismo archivo y también debía crear una variación de `memset`. Busqué documentación sobre cómo reservar memoria sin comprometer inmediatamente los recursos físicos, y encontré la bandera `MAP_NORESERVE`, que se utiliza para asignar memoria física solo cuando realmente se necesita. Para aclarar más mis dudas, pregunté a ChatGPT y me respondió: `MAP_NORESERVE` es una bandera usada en `mmap` en sistemas Unix/Linux, que indica que el sistema no debe reservar memoria física inmediatamente, sino solo cuando se accede. Esto ahorra recursos, pero puede causar errores si no hay suficiente memoria cuando se accede al área mapeada.|
|**Día 3**|Más tarde me di cuenta de que necesitaba pasar el tamaño de la memoria como argumento. Como el kernel trabaja por bloques, el tamaño de la página de memoria es determinado por `PAGE_SIZE`. Encontré información sobre cómo se determina el tamaño de una página en memoria virtual, y me di cuenta de que debía redondear mis páginas a un múltiplo de `PAGE_SIZE` para mejorar la alineación de la memoria virtual. Después, encontré la función `mmap` para asignar memoria en el espacio de usuario, pero me di cuenta de que necesitaba `vm_mmap`, que es similar a `mmap`, pero en el espacio de kernel. Al preguntar a ChatGPT, me respondió que no podía usar las mismas banderas que con `mmap` en `vm_mmap`, ya que son funciones diferentes en contextos diferentes. Finalmente, con toda esta información, implementé el código.|
|**Día 4**|Hice los módulos en base a unos que ya tenía implementados y realicé pruebas.|


---

# Comentario personal
Este proyecto la verdad estuvo algo sencillo, la parte que mas se me complico fue el comprender como definir tamalloc, ya que no estaba seguro si debia hacerlo en donde se definia las variacion alloc, y perdi mucho tiempo investigando en que archivo se definia esto, hasta que econtre que se hacia en utils.c pero habia que esportarla en tantos lados que termine arruinando mi kernel. Tambien tuve muchos problemas con violaciones de segmentos, en especifico con clear_user en mi macro de la syscall, ya que intentaba escribir en espacios donde ya habia informacion. Tambien llegue a la conclusion que la IA es muy util para consultas teoricas, yo la utilice mas que todo para comprender vm_mmap y sus flags para implementar tamalloc. De ahi en mas no tuve complicaciones ya que los modulos ya los tenia casi implementados del todo por el curso de sistemas operativos1. El aux matus es el mejor XD. (Pongame 100 aux).

<br>

 ![](https://user-images.githubusercontent.com/73097560/115834477-dbab4500-a447-11eb-908a-139a6edaec5c.gif)

#### <div align="center"> Desarrollado por
<div align="center">

<br>


Julio Alfredo Fernandez Rodriguez
201902416

<br>

<img src="https://user-images.githubusercontent.com/74038190/229223263-cf2e4b07-2615-4f87-9c38-e37600f8381a.gif" width="200px" />

<br>



<br>
<br>

 ![](https://user-images.githubusercontent.com/73097560/115834477-dbab4500-a447-11eb-908a-139a6edaec5c.gif)


