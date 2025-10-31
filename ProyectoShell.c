/*------------------------------------------------------------------------------
Proyecto Shell de UNIX. Sistemas Operativos
Grados I. Inform�tica, Computadores & Software
Dept. Arquitectura de Computadores - UMA

Algunas secciones est�n inspiradas en ejercicios publicados en el libro
"Fundamentos de Sistemas Operativos", Silberschatz et al.

Para compilar este programa: gcc ProyectoShell.c ApoyoTareas.c -o MiShell
Para ejecutar este programa: ./MiShell
Para salir del programa en ejecuci�n, pulsar Control+D
------------------------------------------------------------------------------*/

#include "ApoyoTareas.h" // Cabecera del m�dulo de apoyo ApoyoTareas.c
 
#define MAX_LINE 256 // 256 caracteres por l�nea para cada comando es suficiente
#include <string.h>  // Para comparar cadenas de cars. (a partir de la tarea 2)

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

  while (1) // El programa termina cuando se pulsa Control+D dentro de get_command()
  {   		
    printf("COMANDO->");
    fflush(stdout);
    get_command(inputBuffer, MAX_LINE, args, &background); // Obtener el proximo comando
    if (args[0]==NULL)
      continue; // Si se introduce un comando vacio, no hacemos nada

    if (strcmp(args[0], "logout") == 0)
    {
      
    }
    int pid = fork();
    if (pid == 0)      // hijo
    {
      execvp(args[0], args);
      exit(0);
    }
    else if (pid > 0)  // Padre
    {
      if (background == 0)
      {
        // Si NO es en segundo plano, esperar al hijo
        wait(NULL);
        printf("Comando %s ejecutado en primer plano con pid %d.\n", args[0], pid);
      }
      else
      {
        // Si es en segundo plano, no esperar
        printf("Comando %s ejecutado en segundo plano con pid %d.\n", args[0], pid);
        continue;
      }
    }
    else
      printf("Error. Comando %s no encontrado\n", args[0]);
  }
}


/* Los pasos a seguir a partir de aqui, son:
       (1) Genera un proceso hijo con fork()
       (2) El proceso hijo invocara a execvp()
       (3) El proceso padre esperara si background es 0; de lo contrario, "continue" 
       (4) El Shell muestra el mensaje de estado del comando procesado 
       (5) El bucle regresa a la funcion get_command()
*/