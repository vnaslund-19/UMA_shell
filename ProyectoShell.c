/*------------------------------------------------------------------------------
Proyecto Shell de UNIX. Sistemas Operativos
Grados I. Informatica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

Algunas secciones estan inspiradas en ejercicios publicados en el libro
"Fundamentos de Sistemas Operativos", Silberschatz et al.

Para compilar este programa: gcc ProyectoShell.c ApoyoTareas.c -o MiShell
Para ejecutar este programa: ./MiShell
Para salir del programa en ejecucion, pulsar Control+D
------------------------------------------------------------------------------*/

#include "ApoyoTareas.h" // Cabecera del modulo de apoyo ApoyoTareas.c
 
#define MAX_LINE 256 // 256 caracteres por linea para cada comando es suficiente
#include <string.h>  // Para comparar cadenas de cars. (a partir de la tarea 2)
#include <errno.h>
#include <stdbool.h>

// FASE 4: Lista global de tareas
job *job_list = NULL;

// FASE 4: Manejador para SIGCHLD
// TODO: hacer más legible
void manejador(int sig)
{
  int status, info;
  pid_t pid;
  (void)sig; // Ya se sabe que es SIGCHLD

  // FASE 4: revisar procesos sin bloquear
  while ((pid = waitpid(-1, &status, WUNTRACED | WNOHANG | WCONTINUED)) > 0)
  {
    enum status st = analyze_status(status, &info);

    // FASE 4: buscar el comando en la lista
    job *j = get_item_bypid(job_list, pid);
    if (!j) continue;

    if (st == FINALIZADO)
    {
      printf("Comando %s ejecutado en segundo plano con PID %d ha concluido.\n",
              j->command, pid);
      block_SIGCHLD();          // FASE 4
      delete_job(job_list, j);  // FASE 4
      unblock_SIGCHLD();        // FASE 4
    }
    else if (st == SUSPENDIDO)
    {
      printf("Comando %s ejecutado en segundo plano con PID %d ha suspendido su ejecución.\n",
              j->command, pid);
      j->ground = DETENIDO;     // FASE 4
    }
    else if (st == SEÑALADO)
    {
      printf("Comando %s ejecutado en segundo plano con PID %d ha reanudado su ejecución.\n",
              j->command, pid);
      j->ground = SEGUNDOPLANO; // FASE 4
    }
    else if (st == CONTINUADO)
    {
      printf("Comando %s ejecutado en segundo plano con PID %d ha reanudado su ejecución.\n",
            j->command, pid);
      j->ground = SEGUNDOPLANO; // vuelve a segundo plano
    }
  }
}

bool is_builtin(char **args)
{
  if (strcmp(args[0], "logout") == 0)
  {
    printf("\nSaliendo del Shell\n");
    exit(0);
  }
  else if (strcmp(args[0], "cd") == 0) // Doesn't handle failures, nor status msgs yet
  {
    if (!args[1])
    {
      const char *home = getenv("HOME");
      if (home != NULL)
        chdir(home);
    }
    else
      chdir(args[1]);
    return true;
  }
  // FASE 5: comando "jobs"
  else if (strcmp(args[0], "jobs") == 0)
  {
    if (empty_list(job_list))
      printf("No hay tareas en segundo plano ni suspendidas.\n");
    else
      print_job_list(job_list);
    return true;
  }
  // FASE 5: comando "fg"
  else if (strcmp(args[0], "fg") == 0)
  {
    if (empty_list(job_list))
    {
      printf("fg: no hay tareas en segundo plano ni suspendidas.\n");
      return true;
    }

    int pos = 1; // por defecto, primera tarea (última que entró)
    if (args[1])
      pos = atoi(args[1]);

    job *j = get_item_bypos(job_list, pos);
    if (!j)
    {
      printf("fg: no existe la tarea %d\n", pos);
      return true;
    }

    pid_t pgid = j->pgid;
    enum ground prev_ground = j->ground;
    char *command = strdup(j->command); // copia local del nombre del comando

    int status, info;
    enum status status_res;

    // Pasar la tarea a primer plano: quitarla de la lista y gestionar en FG
    block_SIGCHLD();                // FASE 5: proteger la lista
    delete_job(job_list, j);        // ya no está en jobs (FG no se lista)
    set_terminal(pgid);             // ceder terminal al comando

    if (prev_ground == DETENIDO)    // si estaba detenido, reanudarlo
      killpg(pgid, SIGCONT);

    waitpid(pgid, &status, WUNTRACED);
    status_res = analyze_status(status, &info);

    set_terminal(getpid());         // recuperar el terminal
    if (status_res == SUSPENDIDO)
    {
      printf("Comando %s suspendido en primer plano con pid %d.\n",
             command, pgid);

      // Si se suspende otra vez, vuelve a la lista como DETENIDO
      add_job(job_list, new_job(pgid, command, DETENIDO));
    }
    else // aquí entran FINALIZADO, SEÑALADO y CONTINUADO
    {
      printf("Comando %s ejecutado en primer plano con pid %d. Estado %s. Info: %d.\n",
             command, pgid, status_strings[status_res], info);
    }

    unblock_SIGCHLD();              // FASE 5: ya no tocamos la lista
    free(command);                  // liberamos la copia local

    return true;
  }
  // FASE 5: comando "bg"
  else if (strcmp(args[0], "bg") == 0)
  {
    if (empty_list(job_list))
    {
      printf("bg: no hay tareas en segundo plano ni suspendidas.\n");
      return true;
    }

    int pos = 1; // por defecto, primera tarea de la lista
    if (args[1])
      pos = atoi(args[1]);

    job *j = get_item_bypos(job_list, pos);
    if (!j)
    {
      printf("bg: no existe la tarea %d\n", pos);
      return true;
    }

    block_SIGCHLD(); // FASE 5: proteger modificaciones de la lista

    if (j->ground == DETENIDO)
    {
      killpg(j->pgid, SIGCONT);           // reanudar proceso detenido
      j->ground = SEGUNDOPLANO;           // ahora está en segundo plano
      printf("Comando %s reanudado en segundo plano con pid %d.\n",
             j->command, j->pgid);
    }
    else
    {
      printf("bg: la tarea %d no está detenida.\n", pos);
    }

    unblock_SIGCHLD();
    return true;
  }
  else
    return false;
}

void child(char **args, int background)
{
  new_process_group(getpid()); // FASE 3: el hijo crea su grupo
  if (background == 0)
    set_terminal(getpid());    // FASE 3: primer plano toma la terminal

  restore_terminal_signals();  // FASE 3: reactiva señales
  if (execvp(args[0], args) == -1)
  {
    printf("Error. Comando %s no encontrado\n", args[0]);
    exit(errno);
  }
  exit(0);
}

// --------------------------------------------
//                     MAIN          
// --------------------------------------------

int main(void)
{
  char inputBuffer[MAX_LINE]; // Bufer que alberga el comando introducido
  int background;             // Vale 1 si el comando introducido finaliza con '&'
  char *args[MAX_LINE/2];     // La linea de comandos (de 256 cars.) tiene 128 argumentos como m�x
      
  // Variables de utilidad:
  int pid_fork;     // pid para el proceso creado y esperado
  int status;                 // Estado que devuelve la funcion wait
  enum status status_res;     // Estado procesado por analyze_status()
  int info;		                // Informacion procesada por analyze_status()

  ignore_terminal_signals();  // FASE 3

  // FASE 4: Crear lista global de tareas
  job_list = new_list("Lista de tareas");

  // FASE 4: Asociar manejador SIGCHLD
  signal(SIGCHLD, manejador);

  while (1) // El programa termina cuando se pulsa Control+D dentro de get_command()
  {   		
    printf("COMANDO->");
    fflush(stdout);
    get_command(inputBuffer, MAX_LINE, args, &background); // Obtener el proximo comando
    if (args[0]==NULL)
      continue; // Si se introduce un comando vacio, no hacemos nada

    if (is_builtin(args))
      continue ;

    pid_fork = fork();
    if (pid_fork == 0)          // Hijo
      child(args, background);
    else if (pid_fork > 0)      // Padre
    {
      new_process_group(pid_fork);

      if (background == 0)      // Primer plano
      {
        set_terminal(pid_fork);                           // FASE 3: cede terminal al hijo
        waitpid(pid_fork, &status, WUNTRACED);            // FASE 3: wait con WUNTRACED
        status_res = analyze_status(status, &info);       // FASE 3: analiza estado

        set_terminal(getpid());   // FASE 3: recupera la terminal

        if (status_res == SUSPENDIDO)
        {
          printf("Comando %s suspendido en primer plano con pid %d.\n",
                 args[0], pid_fork);

          // FASE 4: Insertar tarea detenida en lista
          block_SIGCHLD();
          add_job(job_list, new_job(pid_fork, args[0], DETENIDO));
          unblock_SIGCHLD();
        }
        else  // Finalizado o Reanudado (TODO: double check) (Que hacer con reanudado)
        {
          printf("Comando %s ejecutado en primer plano con pid %d. Estado %s. Info: %d.\n",
                 args[0], pid_fork, status_strings[status_res], info);
        }
      }
      else                      // Segundo plano
      {
        printf("Comando %s ejecutado en segundo plano con pid %d.\n", args[0], pid_fork);

        // FASE 4: insertar tarea BG en lista
        block_SIGCHLD();
        add_job(job_list, new_job(pid_fork, args[0], SEGUNDOPLANO));
        unblock_SIGCHLD();

        continue;
      }
    }
    else
      printf("Error de fork\n");
  }
}


/* Los pasos a seguir a partir de aqui, son:
       (1) Genera un proceso hijo con fork()
       (2) El proceso hijo invocara a execvp()
       (3) El proceso padre esperara si background es 0; de lo contrario, "continue" 
       (4) El Shell muestra el mensaje de estado del comando procesado 
       (5) El bucle regresa a la funcion get_command()
*/