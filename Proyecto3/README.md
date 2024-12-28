
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


Para la syscall 1 sabia que tenia que hacer uso de una lista, asi que lo primero que hice fue hacer solo el codigo de mi macro que reciba el pid y el maximo_memoria, y lo almacene en forma de lista simplemente enlazada, para hacer esto la verdad es que no busque mucho ya que tenia conocimiento de como hacerlo, sin embargo para estar seguro revide una implementacion en python (mas facil de comprender) y lo compare con mi codigo (use este enlace https://www.datacamp.com/es/tutorial/python-linked-lists ).

Teniendo mi lista simplemente enlazada bien definida solo tenia que econtrar la manera de limitar la memoria a mi proceso, buscando en google encontre un un struct llamado rlimit con atributos rlim_cur y rlim_max y entendi que tenia que usarlos para definir el limite de memoria (lo que mencione esta en la parte de DESCRIPCION https://manpages.ubuntu.com/manpages/focal/es/man2/getrlimit.2.html )

Sabiendo que tengo que utilizar rlim y sus atributos para definir los limites de memoria busque como limitar la memoria en google y encontre RLIMIT_AS que es el tamaño máximo de la memoria virtual del proceso (espacio de direcciones) en bytes (lo encontre aca: https://www.rdocumentation.org/packages/unix/versions/1.5.9/topics/rlimit ), teniendo esto claro y que rlim es parte de mm_struct busque informacion para saber como limitar la memoria y original mente habia pensado algo como task.mm.rlim.rlim_max= memoria_maxima  * 1024 * 1024 pero tenia duda como usar RLIMIT_AS asi que le pregunte a chatGPT y la verdad no estaba tan perdido solo que aca comprendi donde usar RLIMIT_AS y como hacerlo.

---
# Explicacion del codigo

Antes de empezar con todas las macros de las  syscalls defini un struct que hace la funcion de mi nodo

```c
// struct del nodo de la lista

struct nodo_proceso {

pid_t pid;

unsigned int memoria_maxima;

struct nodo_proceso *siguiente;

};
```

Y tambien defini dos variables que guardan la cabeza y cola (ultimo de mi lista)

```c
// guardo la cabeza y cola de la lista

static struct nodo_proceso *cabeza = NULL;

static struct nodo_proceso *ultimo = NULL;
```
### Agregar proceso

Lo primero que hice fue definir algunos punteros que voy a usar durante la syscall. Uno de ellos es task, que me sirve para buscar el proceso con el PID que me pasen. También definí nuevo_nodo, que será el nodo que agregaré a mi lista enlazada, y mm, que uso para manejar la memoria del proceso. Además, tengo una variable llamada memoria_usada para calcular cuánta memoria está ocupando el proceso.

Lo primero que hago dentro de la syscall es asegurarme de que quien la está ejecutando sea root. Esto es porque limitar la memoria de un proceso es algo sensible, y solo alguien con permisos de administrador debería poder hacerlo. Si no es root, simplemente devuelvo un error y listo.

Después verifico que los parámetros que recibo, el PID y memoria máxima, sean válidos. Por ejemplo, si me pasan valores negativos, devuelvo un error porque no tiene sentido trabajar con esos datos.

Ya con parámetros válidos, me pongo a buscar el proceso usando su PID. Si no lo encuentro, retorno otro error, porque no hay mucho más que hacer si el proceso no existe. También verifico si el proceso tiene una estructura mm_struct asociada. Si no tiene, significa que no puedo controlar su memoria, así que devuelvo otro error.

Cuando ya tengo el proceso, incremento el contador de referencias para que no lo liberen mientras trabajo con él. Luego desbloqueo la lectura de datos porque ya terminé esa parte. Después obtengo la memoria del proceso. Si esto falla, libero el proceso y retorno otro error.

Aquí reviso cuánta memoria está usando el proceso. Uso una función para obtener el tamaño en páginas y luego lo convierto a megabytes. Si el proceso ya está usando más memoria que el límite que me pasaron, libero todo y devuelvo un error personalizado, porque no tiene sentido limitarlo si ya superó ese valor.

Si todo está bien, procedo a establecer los límites de memoria en el proceso. Esto lo hago configurando unos valores en su estructura interna. Básicamente, convierto los megabytes a bytes multiplicando por un factor y esos valores los guardo como límites. Para asegurarme de que esto funcionó, imprimo un mensaje que dice algo como "Se asignó la memoria exitosamente".

Ahora, creo un nuevo nodo para registrar este proceso y el límite de memoria que le acabo de poner. Uso una función para pedir memoria en el espacio del kernel. Si no puedo crear el nodo, devuelvo un error.

Con el nodo ya creado, le asigno los datos del proceso, como el PID y la memoria máxima. Después lo agrego a mi lista enlazada. Si la lista está vacía, hago que tanto cabeza como último apunten al nuevo nodo. Si no, lo agrego al final actualizando el puntero siguiente del último nodo.

Al final, si  todo esta bien  retorno cero, indicando que la syscall se ejecutó correctamente.


```c
SYSCALL_DEFINE2(jufer_agregar_proceso, pid_t, pid, unsigned int, memoria_maxima) {

  

struct task_struct *task; // uso task_struct para buscar el proceso

struct nodo_proceso *nuevo_nodo; // nuevo nodo a agregar

struct mm_struct *mm;

unsigned long memoria_usada = 0;

  

// verifico si el usuario es root

if (current_uid().val != 0) {

return -EPERM;

}

  

// veo si pid y memoria_maxima son validos

if (pid < 0 || memoria_maxima < 0) {

return -EINVAL;

}

  

// busco el proceso con el pid

task = find_task_by_vpid(pid);

if (task == NULL) {

return -ESRCH;

}

  

// verificar si tiene mm_struct, si no tiene no se puede limitar la memoria

if (task->mm == NULL) {

return -EINVAL;

}

  
  

get_task_struct(task); // aumento el contador de referencias

rcu_read_unlock();

  

// verifico el uso actual de memoria del proceso

mm = get_task_mm(task);

if (!mm) {

put_task_struct(task);

return -EINVAL;

}

  

// verifico el uso actual de memoria del proceso

memoria_usada = get_mm_rss(mm) >> (20 - PAGE_SHIFT); // Conversión a MB

if (memoria_usada > memoria_maxima) {

mmput(mm);

put_task_struct(task);

return -100; // el proceso ya excede el límite de memoria

}

  

// limito la memoria en el mm_struct con rmlim del proceso

task->signal->rlim[RLIMIT_AS].rlim_cur = memoria_maxima*1024*1024;

task->signal->rlim[RLIMIT_AS].rlim_max = memoria_maxima*1024*1024;

  

printk("Se asigno la memoria exitosamente");

// creo el nuevo nodo

nuevo_nodo = kmalloc(sizeof(*nuevo_nodo), GFP_KERNEL);

// GFP_KERNEL -> es para espacio de memoria del kernel

// verifico si se pudo crear el nodo

if (nuevo_nodo == NULL) {

return -ENOMEM;

}

  

// guardo los datos en el nodo

nuevo_nodo->pid = pid;

nuevo_nodo->memoria_maxima = memoria_maxima;

  

if (cabeza == NULL) {

cabeza = nuevo_nodo;

ultimo = nuevo_nodo;

} else {

ultimo->siguiente = nuevo_nodo;

ultimo = nuevo_nodo;

}

  

return 0;

}
```

---
### Listar procesos

Primero, defino algunas variables importantes. La principal es actual, que me sirve para recorrer la lista enlazada de procesos. También defini una variable llamada count que me ayuda a llevar la cuenta de cuantos procesos voy copiando al buffer.

Lo primero que hago dentro de la syscall es asegurarme de que quien la está ejecutando sea root. Esto es importante porque estamos manipulando datos sensibles del kernel y no cualquiera debería tener acceso. Si no es root, devuelvo un error y no sigo adelante.

Luego verifico que los parametros que recibo sean válidos. Por ejemplo, reviso si el buffer donde voy a copiar los datos, el puntero para devolver la cantidad de procesos, y el número máximo de entradas son correctos. Si alguno de estos es nulo o si max_entries es menor o igual a cero, devuelvo un error porque no tiene sentido continuar con datos inválidos.

También hago una validación adicional para asegurarme de que los punteros pasados desde el espacio de usuario sean accesibles. Esto lo hago con una función que verifica si el usuario tiene permiso para acceder a esas direcciones de memoria. Si alguno de los punteros es inválido, devuelvo otro error.

Con todas las validaciones hechas, empiezo a recorrer la lista enlazada de procesos. Mientras el nodo actual no sea nulo y no haya alcanzado el número máximo de procesos que me indicaron, voy copiando los datos del proceso actual a una estructura temporal. Esta estructura se llama info, y ahí guardo cosas como el PID y la memoria máxima del proceso.

Una vez que lleno la estructura info, la copio al buffer del espacio de usuario. Para esto uso una función que se asegura de que la copia sea segura. Si algo falla al intentar copiar los datos, devuelvo un error y detengo la ejecución.

A medida que voy copiando datos, incremento el contador count para saber cuántos procesos ya transferí. Luego paso al siguiente nodo en la lista enlazada.

Cuando termino de recorrer la lista o alcanzo el límite máximo de entradas, escribo en el puntero processes_returned la cantidad de procesos que logré copiar al buffer. Si no puedo escribir ese valor correctamente, devuelvo otro error.

Por ultimo si todo salio bien retorno cero para indicar que la syscall se ejecuto sin problemas.
```c
SYSCALL_DEFINE3(jufer_obtener_lista_procesos,struct proceso_info __user *, buffer,size_t, max_entries,int __user *, processes_returned) {

struct nodo_proceso *actual = cabeza; 

size_t count = 0;

  



if (current_uid().val != 0) {

return -EPERM; 

}

  



if (!buffer || !processes_returned || max_entries <= 0) {

return -EINVAL; 

}

  

if (!access_ok(buffer, max_entries * sizeof(struct proceso_info)) ||

!access_ok(processes_returned, sizeof(int))) {

return -EFAULT; 

}

  



while (actual != NULL && count < max_entries) {

struct proceso_info info;

  

info.pid = actual->pid;

info.memoria_maxima = actual->memoria_maxima;



if (copy_to_user(&buffer[count], &info, sizeof(info))) {

return -EFAULT; 

}

  

count++;

actual = actual->siguiente;

}

  



if (put_user(count, processes_returned)) {

return -EFAULT; 

}

  

return 0; 

}
```

---

### Actualizar proceso

Lo primero que hago en esta syscall es definir la variable actual, que apunta al primer nodo de la lista enlazada de procesos. La idea es recorrer esta lista para encontrar el proceso que quiero actualizar.

Antes de hacer cualquier cosa, verifico que quien esté ejecutando esta syscall tenga privilegios de root. Esto es muy importante porque no cualquiera puede andar cambiando los límites de memoria de los procesos. Si el usuario no es root, devuelvo un error de permiso denegado y no hago nada más.

Después me aseguro de que los parámetros que recibo sean válidos. Reviso que el PID del proceso y el nuevo límite de memoria no sean valores negativos. Si algo está mal, devuelvo un error porque no tiene sentido continuar con datos inválidos.

Con todo validado, empiezo a recorrer la lista enlazada. En cada iteracion, reviso si el nodo actual corresponde al proceso que estoy buscando, comparando el PID. Si no coincide, paso al siguiente nodo en la lista.

Cuando encuentro el nodo que corresponde al proceso, me aseguro de que el proceso realmente exista en el sistema. Uso una función para buscarlo por su PID, y si no lo encuentro, devuelvo un error diciendo que el proceso no existe.

También verifico si el proceso tiene una estructura de memoria asociada. Esto es importante porque, sin esta estructura, no tiene sentido hablar de límites de memoria. Si no la tiene, devuelvo un error.

Luego calculo cuánta memoria está usando actualmente el proceso. Para esto convierto el tamaño de la memoria virtual total a megabytes. Si el proceso ya está usando más memoria de la que quiero asignarle como nuevo límite, devuelvo un error específico para indicar que no puedo reducir el límite porque ya lo excede.

Si todo está bien, actualizo los límites de memoria en el proceso. Ajusto tanto el límite actual como el máximo a la nueva memoria máxima que me recibi como argumento, pero en bytes porque así lo espera el sistema.

Finalmente, también actualizo la información en el nodo de la lista enlazada para reflejar el nuevo límite de memoria del proceso. Así mantengo sincronizados los datos de la lista con los del sistema.

Si no encuentro el proceso en la lista despuess de recorrerla toda, significa que no estaba registrado, y devuelvo un error para indicarlo.


```c
SYSCALL_DEFINE2(jufer_actualizar_proceso, pid_t, pid, unsigned int, nueva_memoria_maxima) {

struct nodo_proceso *actual = cabeza;

  

// que use sudo

if (current_uid().val != 0) {

return -EPERM; 

}

  

// valido parametros

if (pid < 0 || nueva_memoria_maxima < 0) {

return -EINVAL; 

}

  

// busco el nodo en la lista

while (actual != NULL) {

if (actual->pid == pid) {

// nodo encontrado -> verifica el proceso

struct task_struct *task = find_task_by_vpid(pid);

if (task == NULL) {

return -ESRCH; //no encontrado

}

  

if (task->mm == NULL) {

return -EINVAL; // proceso sin estructura de memoria

}

  

// verifico si el proceso ya excede memoria_maxima

unsigned long memoria_usada = task->mm->total_vm * PAGE_SIZE / (1024 * 1024); 

if (memoria_usada > nueva_memoria_maxima) {

return -100; 

}

  

// actualizo los límites de memoria del proceso

task->signal->rlim[RLIMIT_AS].rlim_cur = nueva_memoria_maxima * 1024 * 1024;

task->signal->rlim[RLIMIT_AS].rlim_max = nueva_memoria_maxima * 1024 * 1024;

  

// actualizo el nodo en la lista

actual->memoria_maxima = nueva_memoria_maxima;

  

return 0; 

}

  

actual = actual->siguiente;

}

  

// sino encuentro el nodo

return -102; 

}
```

---
### Eliminar proceso

Lo primero que hago en esta syscall es definir las variables actual y anterior. La variable actual apunta al primer nodo de la lista enlazada de procesos, mientras que anterior me ayuda a mantener el enlace con el nodo previo mientras recorro la lista. La idea es encontrar el nodo que contiene el proceso que quiero eliminar.

Antes de hacer cualquier operación, verifico que quien esté ejecutando esta syscall tenga privilegios de root. Si el usuario no es root, devuelvo un error de permiso denegado y no realizo ninguna acción más.

Luego, valido el PID recibido. Si el PID es negativo, devuelvo un error indicando que el argumento es inválido despues verifico si el proceso con el PID proporcionado existe en el sistema. Uso la función find_task_by_vpid para buscar el proceso por su PID. Si no encuentro el proceso, devuelvo un error indicando que no se ha encontrado el proceso.

Luego comienzo a recorrer la lista enlazada de procesos. En cada iteración, comparo el PID del nodo actual con el PID que quiero eliminar. Si encuentro el nodo correspondiente, realizo la eliminación. Si el nodo a eliminar es el primero (la cabeza de la lista), actualizo la cabeza de la lista. Si el nodo está en medio o al final, simplemente actualizo los enlaces del nodo anterior para que apunte al siguiente nodo del que quiero eliminar. Si el nodo que elimino es el último de la lista, actualizo la variable ultimo para que apunte al nodo anterior.

Antes de eliminar el nodo de la lista, restablezco los límites de memoria del proceso, en caso de que sea necesario, para que el sistema mantenga la consistencia de los límites de memoria asociados al proceso. Esto es opcional y depende del contexto, pero me asegura que los límites de memoria del proceso se ajusten al valor que tenían en el nodo de la lista.

Finalmente, libero la memoria ocupada por el nodo utilizando la función kfree para evitar fallos de memoria.

Si no encuentro el nodo con el PID proporcionado en toda la lista, devuelvo un error indicando que el proceso no esta en la lista.

```c
SYSCALL_DEFINE1(jufer_eliminar_proceso, pid_t, pid) {

struct nodo_proceso *actual = cabeza;

struct nodo_proceso *anterior = NULL;

  

// verifico privilegios root

if (current_uid().val != 0) {

return -EPERM; 

}

  



if (pid < 0) {

return -EINVAL; 

}

  

// verifico si el proceso existe

struct task_struct *task = find_task_by_vpid(pid);

if (!task) {

return -ESRCH; 

}

  

// busco el nodo con el PID especificado en la lista

while (actual != NULL) {

if (actual->pid == pid) {



if (anterior == NULL) {

cabeza = actual->siguiente;

if (cabeza == NULL) {

ultimo = NULL; // La lista queda vacía

}

} else {

anterior->siguiente = actual->siguiente;

if (actual == ultimo) {

ultimo = anterior; 

}

}

  

// re-defino los límites del proceso

if (task->mm) {

task->signal->rlim[RLIMIT_AS].rlim_cur = actual->memoria_maxima;

task->signal->rlim[RLIMIT_AS].rlim_max = actual->memoria_maxima;

}

  

// libero la memoria

kfree(actual);

return 0; 

}

  

anterior = actual;

actual = actual->siguiente;

}

  

// Nodo no encontrado en la lista

return -102; // Proceso no está en la lista

}
```


---
# Planificaciones

| Día | Actividad                                                                  |
| --- | -------------------------------------------------------------------------- |
| 1   | Hice la estructura de mi lista simplemente enlazada.                       |
| 2   | Lei documentacion y descubri `rlim` para limitar la memoria de un proceso. |
| 3   | Termine el proyecto e hice la documentacion.                               |


--- 
# Comentario

Para este proyecto la verdad no encontre mayor complicacion ya que tenia conocimientos previos de listas enlazadas, al inicio queria trabajar con una lista circular doblemente enlazada pero se me estaba complicando un poco la estructura, asi que decidi ustilizar una simplemente enlazada, de ahi mi mayor dificultad fue como definir el limite, que pense que hiba a ser mas facil pero si me costo un poco. En lo demas solo eran las operaciones de una lista enlazada asi que no tuve complicaciones.


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
