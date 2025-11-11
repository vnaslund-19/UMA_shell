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
  int pid_fork, pid_wait;     // pid para el proceso creado y esperado
  int status;                 // Estado que devuelve la funcion wait
  enum status status_res;     // Estado procesado por analyze_status()
  int info;		                // Informacion procesada por analyze_status()

  ignore_terminal_signals();  // FASE 3

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
      if (background == 0)
      {
        set_terminal(pid_fork);                           // FASE 3: cede terminal al hijo
        pid_wait = waitpid(pid_fork, &status, WUNTRACED); // FASE 3: wait con WUNTRACED
        status_res = analyze_status(status, &info);       // FASE 3: analiza estado

        set_terminal(getpid());   // FASE 3: recupera la terminal
        printf("Comando %s ejecutado en primer plano con pid %d. Estado %s. Info: %d.\n",
                args[0], pid_fork, status_strings[status_res], info);
      }
      else
      {
        // Si es en segundo plano, no esperar
        printf("Comando %s ejecutado en segundo plano con pid %d.\n", args[0], pid_fork);
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