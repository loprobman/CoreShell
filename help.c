#include <stdio.h>
#include <string.h>
#include "help.h"

void display_help(const char *command)
{
    if (command == NULL || strcmp(command, "help") == 0) 
    {
        printf("\nSShell built-in commands:\n");
        printf("  help [command]      Show general or command-specific help\n");
        printf("  exit                Exit the shell\n");
        printf("  cd [dir]            Change directory (default: $HOME)\n");
        printf("  pwd                 Print working directory\n");
        printf("  echo [args...]      Print arguments\n\n");
        return;
    }
    if (strcmp(command, "exit") == 0)  
    { 
        printf("\nexit - Exit the shell\nUsage: exit\n\n"); 
        return; 
    }
    if (strcmp(command, "cd") == 0)    
    { 
        printf("\ncd - Change directory\nUsage: cd [directory]\n\n"); 
        return; 
    }
    if (strcmp(command, "pwd") == 0)   
    { 
        printf("\npwd - Print working directory\nUsage: pwd\n\n"); 
        return; 
    }
    if (strcmp(command, "echo") == 0)  
    { 
        printf("\necho - Print arguments\nUsage: echo [args...]\n\n"); 
        return; 
    }
    printf("No help available for '%s'\n", command);
}