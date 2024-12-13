
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


### Personalización del nombre del sistema

How to change Linux kernel name -> https://stackoverflow.com/questions/35623662/how-to-change-linux-kernel-name

En este foro proporcionaron la direccion donde esta la variable UTS_SYSNAME donde se define el nombre del sistema.

include/linux/uts.h



### Mensajes de inicio personalizados

Para esta parte el auxiliar nos oriento que debiamos hacer printk en algun lugar, y que hiba a ver el mensaje cuando hiciera grep en dmesg, entonces analizando esto, llegue a la conclusion que dicho mensaje debe escribirse en los logs del kernel, entonces investigando encontre que la funcion que inicializa el nucleo de linux es start_kernel() y que aca podia insertar mi mensaje.
https://www.oreilly.com/library/view/understanding-the-linux/0596002130/apas05.html

Ahora bien, lo siguiente fue buscar donde se encontraba esta funcion, para ello lei esto https://sopa.dis.ulpgc.es/ii-dso/leclinux/init_main/LEC5_INIT_MAIN.pdf

Una vez leido eso defini qu era en la ruta linux-6.8/init/main.c

Para saber la estructura del mensaje,utilice esta fuente
https://docs.kernel.org/core-api/printk-basics.html


### Capture_snapshot
Para esta syscall utilice la estructura sysinfo llenandola con si_meminfo() solo que le agregue active_memory para agregar las paginas activas. Ahora bien para la memoria swap utilice la libreria <linux/vmstat.h>. 

Ahora para la memoria activa del sistema inicialize active_memory con cero de mi structura extendida, ahora para iterar los nodos utilice pglist_data que obtiene la informacion de ese nodo, la cual lo devuelve como puntero y seguido a ello solo sume la memoria activa utilizando node_page_state que lee el estado de la pagina.

NR_ACTIVE_ANON -> paginas anonimas 
NR_ACTIVE_FILE -> paginas activas

Como utilimo converti a KB donde utilice desplazamiento a la izquierda de los bits donde PAGE_SHIFT - 10 va indicar cuantos bits desplazar a la izquierda.

Por ultimo copio e_info al espacio de usuario.

**Codigo en syscall_64.tbl**
   
```plaintext
    549    common    capture_memory_snapshot    sys_capture_memory_snapshot
```
    


**Codigo en sycalls.h

```c
    asmlinkage long sys_capture_memory_snapshot(struct extended_sysinfo __user *ext_info);
    ```



**Codigo en sys.c**

 ```c
    #include <linux/kernel.h>
    #include <linux/sysinfo.h>
    #include <linux/syscalls.h>
    #include <linux/mm.h>
    #include <linux/vmstat.h> // para node_page_state y macros NR_ACTIVE_*
    
    struct extended_sysinfo {
        struct sysinfo si;
        unsigned long active_memory; 
    };
    
    SYSCALL_DEFINE1(capture_memory_snapshot, struct extended_sysinfo __user *, ext_info)
    {
        struct extended_sysinfo e_info;
        int nid; // ID del nodo
    
        // Llenar sysinfo
        si_meminfo(&e_info.si);
    
        // info de swap
        e_info.si.totalswap = get_nr_swap_pages();
        e_info.si.freeswap = atomic_long_read(&nr_swap_pages); 
    
        // inicio active_memory en 0
        e_info.active_memory = 0;
    
        // itero sobre los notos
        for_each_online_node(nid) {
            struct pglist_data *pgdat = NODE_DATA(nid);
    
            // sumo las paginas activas y anonimas
            e_info.active_memory += node_page_state(pgdat, NR_ACTIVE_ANON);
            e_info.active_memory += node_page_state(pgdat, NR_ACTIVE_FILE);
        }
    
        // a kb
        e_info.active_memory <<= (PAGE_SHIFT - 10);
        e_info.si.totalswap <<= (PAGE_SHIFT - 10);  
        e_info.si.freeswap <<= (PAGE_SHIFT - 10);   
    
        // copo al espacio de usuario e_info
        if (copy_to_user(ext_info, &e_info, sizeof(e_info)))
            return -EFAULT;
    
        return 0;
    }
    ```
https://docs.rs/sysinfo/latest/sysinfo/ 
https://stackoverflow.com/questions/49930181/what-is-the-print-format-for-atomic-long-t-for-printk-in-c

---
### track_syscall_usage

En esta syscall utilice syscall_stats para guardar el numero de invocaciones y el timestamp de cada syscall que quiero rastrear y utilizo `track_syscall_usage` para registrar su uso.

**Codigo en syscalls_64.tbl**
```tbl
550 common track_syscall_usage sys_track_syscall_usage
```

**Codigo en syscalls.h**

```c
long sys_track_syscall_usage(struct syscall_usage __user *info);
```

**Codigo en sys.c**

```c
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/timekeeping.h>

// max syscalls
#define MAX_SYSCALLS 1024

/*
slab.h -> kzalloc
timekeeping.h -> maneja el tiempo
*/

// estructura para almacenar las estadisticas
struct syscall_usage {
    unsigned long count;             // llamadas
    struct timespec64 last_called;   // timp utima llamada
};

// aca defini la tabla
static struct syscall_usage *syscall_stats;

// inicializo la tabla
static int init_syscall_stats(void) {
    syscall_stats = kzalloc(sizeof(struct syscall_usage) * MAX_SYSCALLS, GFP_KERNEL);
    return syscall_stats ? 0 : -ENOMEM;
}

// defino el macro de la sys
SYSCALL_DEFINE1(track_syscall_usage, struct syscall_usage __user *, info) {
    if (!syscall_stats) {
        if (init_syscall_stats() != 0)
            return -ENOMEM;
    }

    // copio al espacio de usuario
    if (copy_to_user(info, syscall_stats, sizeof(struct syscall_usage) * MAX_SYSCALLS))
        return -EFAULT;

    return 0;
}

// wrapper para inicializar rastreo desde otra lado del kernel
void track_syscall(int syscall_id) {
    struct timespec64 now;

    if (!syscall_stats || syscall_id >= MAX_SYSCALLS)
        return;

    // incremento el  contador y registro el tiempo
    syscall_stats[syscall_id].count++;
    ktime_get_real_ts64(&now);
    syscall_stats[syscall_id].last_called = now;
}
```

Ahora puedo llamar desde cualquier funcion de la siguiente manera
```c
	track_syscall(__NR_read);
```

`__NR_read`: Es el identificador numérico de la syscall `read`.

---
#### sys throttle

Aca utilice la estructura task_struct, que devuelve informacion de los procesos

**Codigo en syscalls_64.tbl**
```tbl
551 common get_io_throttle sys_get_io_throttle
```

**Codigo en syscalls.h**

```c
asmlinkage long sys_get_io_throttle(pid_t pid, struct io_stats __user *stats);
```

**Codigo en sys.c**
```c
    #include <linux/kernel.h>
    #include <linux/syscalls.h>
    #include <linux/sched.h>
    #include <linux/pid.h>
    
    struct io_stats {
        u64 rchar;
        u64 wchar;
        u64 syscr;
        u64 syscw;
        u64 read_bytes;
        u64 write_bytes;
    };
    
    SYSCALL_DEFINE2(get_io_throttle, pid_t, pid, struct io_stats __user *, stats) {
        struct task_struct *task;
        struct io_stats kstats;
    
        rcu_read_lock();
        task = find_task_by_vpid(pid);
        if (!task) {
            rcu_read_unlock();
            return -ESRCH; // proceso no encontrado
        }
    
        memset(&kstats, 0, sizeof(kstats));
        kstats.rchar = task->ioac.rchar;
        kstats.wchar = task->ioac.wchar;
        kstats.syscr = task->ioac.syscr;
        kstats.syscw = task->ioac.syscw;
        kstats.read_bytes = task->ioac.read_bytes;
        kstats.write_bytes = task->ioac.write_bytes;
    
        rcu_read_unlock();
    
        if (copy_to_user(stats, &kstats, sizeof(kstats)))
            return -EFAULT;
    
        return 0;
    }
```

https://stackoverflow.com/questions/56531880/how-does-the-kernel-use-task-struct

rcu_read_lock -> puedo aceder al archivo de io del proceso
find_task_by_vpid -> busca el pid ->https://stackoverflow.com/questions/8547332/efficient-way-to-find-task-struct-by-pid
https://stackoverflow.com/questions/57988667/what-does-rcu-read-lock-actually-do-linux-kernel -> rcu_read_unlock()

https://medium.com/@agadallh5/esrch-a-comprehensive-guide-acd50b133165

---
| Día  | Actividad           |Links de apoyo           |
|------|---------------------|---------------------|
| Día 0|Configure mi entorno,descargue virual box, descargue y compile el kernel                     |	||
| Día 1|Este dia empece con buscando informacion de como cambiar el nomre del sistema y el saludo                    | https://www.oreilly.com/library/view/understanding-the-linux/0596002130/apas05.html - https://sopa.dis.ulpgc.es/ii-dso/leclinux/init_main/LEC5_INIT_MAIN.pdf -  https://docs.kernel.org/core-api/printk-basics.html ||
| Día 2|  En el dia dos por poco no sigo el curso. Probe de muchas formas hacer la syscall1 y no lograba nada funcional, ya que no habia terminado de comprender bien la estructura del kernel                   |||
| Día 3|  Este dia el auxiliar menciono la estructura sysinfo y fue donde comprendi todo, lei mas documentacion y logre hacer la primera syscall                   |||
| Día 4| Hice la syscall2 y syscal3, cabe mencionar que para la syscall2, recibi un poco de ayuda de parte de un ingeniero, donde me explico teoria y el enfoque que debia de uitlizar para realizar la syscall|  https://stackoverflow.com/questions/56531880/how-does-the-kernel-use-task-struct - rcu_read_lock -> puedo aceder al archivo de io del proceso find_task_by_vpid -> busca el pid ->https://stackoverflow.com/questions/8547332/efficient-way-to-find-task-struct-by-pid https://stackoverflow.com/questions/57988667/what-does-rcu-read-lock-actually-do-linux-kernel -> rcu_read_unlock() - https://medium.com/@agadallh5/esrch-a-comprehensive-guide-acd50b133165  ||
| Día 5| Este dia hice mis test a mis syscalls y algunas modificacion que eran necesarias para que entendiera mas la informacion, hice la documentacion                    | ||

---
# Comentario personal
EL proyecto fue un poco complicado al inicio, ya que no tenia nada de conocimiento de la estructura del kernel, con la mayor dificultad que me tope al principio fue que no sabia en que archivos definir mis syscalls y en donde implementarlas, en lo personal aprendi esta estructura buscando documentacion y buscando videos. 

Cuando logre comprender la estructura del kernel, mi siguiente reto fue comprender la sintaxis, porque era DEFINE1 para algunas macros y para otras era DEFINE0 y algunas cosas mas, tambien me causaba duda en que parte de los archivos colocar el codigo. La verdad el proyecto me sirvio mucho para desarrollar mis habilidades de programador y demostrarme a mi mismo que soy capaz de lograr cosas que a veces pienso que no puedo lograr.

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
