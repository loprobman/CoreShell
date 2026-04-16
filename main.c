#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include "help.h"

#define BUFFER_SIZE 1024
#define MAX_ARGS 64

/* Global flag for signal handling */
volatile int interrupted = 0;

/* Signal handler for SIGINT (Ctrl+C) */
void signal_handler(int sig) 
{
    (void)sig; // Unused parameter
    printf("CTRL+C pressed\n");
    exit(0);
}

/* Parse command line into arguments */
int parse_command(char *input, char *args[]) 
{
    int i = 0;
    /*When strtok is called for the first time with the input string, 
    it searches for the first occurrence of any delimiter character (" " and "\t" and "\n") 
    and replaces it with a null terminator ('\0'). It then returns a pointer to the first token */
    char *token = strtok(input, " \t\n");
    
    while (token != NULL && i < MAX_ARGS - 1) 
    {
        args[i++] = token;
        /* Subsequent calls to strtok with NULL as the first argument will return the next 
           token in the string, continuing from where it left off.*/
        token = strtok(NULL, " \t\n");
    }
    /*By assigning NULL to args[i], the code ensures that the argument list is properly terminated. 
    This is important because functions like execvp() rely on this NULL pointer to know where the 
    list of arguments ends.*/
    args[i] = NULL;
    return i;
}

/* Execute a command */
void execute_command(char *args[]) 
{
    if (args[0] == NULL) 
    {
        return;
    }
    
    /* Built-in help command */
    if (strcmp(args[0], "help") == 0) 
    {
        display_help(args[1]);
        return;
    }

    /* Built-in commands */
    if (strcmp(args[0], "exit") == 0) 
    {
        if (args[1] != NULL && (strcmp(args[1], "--help") == 0 || strcmp(args[1], "-h") == 0))
        { 
            display_help("exit"); 
            return; 
        }
        exit(0);
    }

    /* Built-in cd command */
    if (strcmp(args[0], "cd") == 0) 
    {
        if (args[1] != NULL && (strcmp(args[1], "--help") == 0 || strcmp(args[1], "-h") == 0))
        { 
            display_help("cd"); 
            return; 
        }
        if (args[1] == NULL) 
        {
            /* No additional argument change to users home directory */
            /* getenv("HOME") function retrieves the path to the home 
               directory from the environment variables.*/
            chdir(getenv("HOME"));
        } 
        else 
        {   /* Change the specified directory */
            if (chdir(args[1]) != 0) 
            {   /* Print error message if chdir fails */
                perror("cd");
            }
        }
        return;
    }
    
    /* Built-in pwd command: print working directory */
    if (strcmp(args[0], "pwd") == 0) 
    {
        if (args[1] != NULL && (strcmp(args[1], "--help") == 0 || strcmp(args[1], "-h") == 0))
        { 
            display_help("pwd"); 
            return; 
        }
        char cwd[BUFFER_SIZE]; 
        /* getcwd() fills cwd with the absolute pathname of the current working directory */ 
        if (getcwd(cwd, sizeof(cwd)) != NULL)
        {
            printf("%s\n", cwd);
        } 
        else 
        {
            /* If getcwd fails, print an error message */
            perror("pwd");
        }
        return;
    }
    
    /* Built-in echo command: print arguments to the terminal */
    if (strcmp(args[0], "echo") == 0) 
    {
        if (args[1] != NULL && (strcmp(args[1], "--help") == 0 || strcmp(args[1], "-h") == 0))
        { 
            display_help("echo"); 
            return; 
        }
        for (int i = 1; args[i] != NULL; i++) 
        {
            /* Print each argument followed by a space */
            printf("%s ", args[i]);
        }
        printf("\n");
        return;
    }
    
    /* The shell needs to create a separate process to run external commands 
    so that the shell itself can continue running after the command finishes.*/
    pid_t pid = fork();
    if (pid == 0) 
    {
        /* Child process */
        /* Replace its memory with the external program specified by the user 
           (e.g., ls, cat, etc.).*/
        execvp(args[0], args);
        /* If execvp returns, it means an error occurred */
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid > 0) 
    {
        /* Parent process (The shell) */
        /* The parent waits for the child process to finish*/
        waitpid(pid, NULL, 0);
    } 
    else 
    {   /* Fork failed */
        perror("fork");
    }
}

char *read_input(void)
{
    /* Allocate memory for user input */
    char *input = malloc(BUFFER_SIZE * sizeof(char));
    /* Handle malloc fail */
    if(input == NULL)
    {
        fprintf(stderr, "Allocation error\n");
        exit(EXIT_FAILURE);
    }
    /* read a line from stdin (the terminal) into the input buffer */
    if(fgets(input, BUFFER_SIZE, stdin) == NULL)
    {
        free(input); /*free input alloctaed in the heap */
        exit(EXIT_FAILURE);
    }
    /* Remove newline character captured when user presses ENTER 
        and add string terminator character '\0'*/
    input[strcspn(input, "\n")] = '\0'; 
    return input;
}

/* Main REPL loop */
int main() 
{
    char *input;
    char *args[MAX_ARGS];
    
    /* Setup signal handler for Ctrl+C */
    signal(SIGINT, signal_handler);
    
    printf("SShell v1.0 - Simple Linux Shell\n");
    printf("Type 'exit' to quit\n\n");
    
    while(1) 
    {
        interrupted = 0;

        /* Print USER */
        printf("%s@SShell> ",getenv("USER"));
        /* flush stream to ensure prompt is printed before input */
        fflush(stdout); 
        input = read_input();

        if (input == NULL) 
        {
            continue;
        }
        parse_command(input, args);
        execute_command(args);
        free(input);
    }
    
    printf("Exiting SShell\n");
    return 0;
}
