//Imports
#include <stdlib.h>
#include <stdio.h>
#include <zconf.h>
#include <memory.h>
#include <sys/wait.h>

//Macros
#define TRUE 1
#define FALSE 0
#define NUM_COMMANDS 1
#define MAX_BUFFER 4096
#ifndef _REENTRANT
#define _REENTRANT
#endif

//Structs
typedef struct command
{
    char* command;
    char** arguments;
} command;

//Method declarations
int read_command_line(char**); //took method from website https://brennan.io/2015/01/16/write-a-shell-in-c/
int parse_command_line(char*** parsed_words, char* line);
void parse_command(command* parsed_command, char** words);
int execute_command(command cmd);
int execute_built_in_command(command* command);
int command_equals(command command1, command command2);
int is_built_in(command cmd);
void initialize_built_in_commands();
void free_memory(char** cmd_line, char*** cmd_words, command** cmd);
void error_message();

//Global variables and arrays
command built_in_commands[NUM_COMMANDS];

/*
 * Method main()
 * Referenced pseudocode layout on https://www.cs.cornell.edu/courses/cs414/2004su/homework/shell/shell.html
 */
int main(int argc, char** argv) {
    //Local variables
    pid_t pid;
    char *cmd_line = NULL;
    int status;
    int read_command_status;
    int parse_command_status;
    command *cmd = NULL;
    char **cmd_words = NULL;

    //Initialize and setup system commands
    initialize_built_in_commands();

    while (TRUE) {
        //Print prompt and read the command line
        printf("Shells by the Seashore$ ");
        read_command_status = read_command_line(&cmd_line);
        if (!read_command_status) {
            //Free memory and exit on failure
            if (cmd_line != NULL) {
                free(cmd_line);
                cmd_line = NULL;
            }
            if (cmd_words != NULL) {
                free(cmd_words);
                cmd_words = NULL;
            }

            if (cmd != NULL) {
                free(cmd);
                cmd = NULL;
            }
            exit(EXIT_FAILURE);
        }

        //Parse command line into command and arguments
        parse_command_status = parse_command_line(&cmd_words, cmd_line);
        if (!parse_command_status) {
            //Free memory and exit on failure
            if (cmd_line != NULL) {
                free(cmd_line);
                cmd_line = NULL;
            }
            if (cmd_words != NULL) {
                free(cmd_words);
                cmd_words = NULL;
            }

            if (cmd != NULL) {
                free(cmd);
                cmd = NULL;
            }
            exit(EXIT_FAILURE);
        }
        cmd = malloc(sizeof(command));
        if (cmd == NULL) {
            error_message();
            //Free memory and exit on failure
            if (cmd_line != NULL) {
                free(cmd_line);
                cmd_line = NULL;
            }
            if (cmd_words != NULL) {
                free(cmd_words);
                cmd_words = NULL;
            }

            if (cmd != NULL) {
                free(cmd);
                cmd = NULL;
            }
            exit(EXIT_FAILURE);
        }

        parse_command(cmd, cmd_words);

        //Fork from parent as long as command exists
        if (cmd->command != NULL) {
            if (!is_built_in(*cmd)) {
                pid = fork();

                if (pid == 0) {
                    if(!execute_command(*cmd)) {
                        if (cmd_line != NULL) {
                            free(cmd_line);
                            cmd_line = NULL;
                        }

                        if (cmd_words != NULL) {
                            free(cmd_words);
                            cmd_words = NULL;
                        }

                        if (cmd != NULL) {
                            free(cmd);
                            cmd = NULL;
                        }
                        exit(EXIT_FAILURE);
                    }
                } else if (pid > 0) {
                    waitpid(pid, &status, 0); //wait on the forked child...
                } else {    //something went wrong with forking
                    error_message();
                    if (cmd_line != NULL) {
                        free(cmd_line);
                        cmd_line = NULL;
                    }

                    if (cmd_words != NULL) {
                        free(cmd_words);
                        cmd_words = NULL;
                    }

                    if (cmd != NULL) {
                        free(cmd);
                        cmd = NULL;
                    }
                    exit(EXIT_FAILURE);
                }

                if (cmd_line != NULL) {
                    free(cmd_line);
                    cmd_line = NULL;
                }

                if (cmd_words != NULL) {
                    free(cmd_words);
                    cmd_words = NULL;
                }

                if (cmd != NULL) {
                    free(cmd);
                    cmd = NULL;
                }
            } else {
                int return_val = execute_built_in_command(cmd);

                if (cmd_line != NULL) {
                    free(cmd_line);
                    cmd_line = NULL;
                }

                if (cmd_words != NULL) {
                    free(cmd_words);
                    cmd_words = NULL;
                }

                if (cmd != NULL) {
                    free(cmd);
                    cmd = NULL;
                }

                //want to exit ONLY after freeing all memory
                if (return_val == TRUE) {
                    break;
                }
            }
        }

        if (cmd_line != NULL) {
            free(cmd_line);
            cmd_line = NULL;
        }

        if (cmd_words != NULL) {
            free(cmd_words);
            cmd_words = NULL;
        }

        if (cmd != NULL) {
            free(cmd);
            cmd = NULL;
        }
    }
    return EXIT_SUCCESS;
}

/*
 * Method to execute built-in commands; the only supported command, currently, is exit. Please note that all memory is freed before exiting.
 * Talked with classmates in TA session
 */
int execute_built_in_command(command* command) {
    //If the command equals the "exit" command for the system
    if (command_equals(built_in_commands[0], *command)) {
        return TRUE; //want to exit and break the while loop
    }
    return FALSE;
}

/*
 * Method to check if a command is built-in or not
 */
int is_built_in(command cmd) {
    for(int i=0; i<NUM_COMMANDS; i++) {
        if(command_equals(cmd, built_in_commands[i])) {
            return TRUE;
        }
    }
    return FALSE;
}

/*
 * Method to execute a command using standard path search; returns if success or not
 */
int execute_command(command cmd) {
    if (execvp(cmd.command, cmd.arguments) == -1) {
        printf("-bash: %s: command not found\n", cmd.command);
        return FALSE;
    }
    return TRUE;
}

/*
 * Method for reading the command line input from the user prompt line. Return TRUE on success and FALSE on failure.
 * Took parts of method, below, from website: https://brennan.io/2015/01/16/write-a-shell-in-c/. I also modified it to use methods I had written.
 */
int read_command_line(char** return_val) {
    int bufsize = MAX_BUFFER;
    int position = 0;
    char *buffer = malloc(sizeof(char) * bufsize);
    int c;

    //Raise an error if the memory malloc did not go through (we know our size is >0, so NULL is invalid)
    if (buffer == NULL) {
        error_message();
        return FALSE;
    }

    while (1) {
        // Read a character from stdin
        c = getchar();

        // If we hit EOF, replace it with a null character and return as means of breaking the loop.
        if (c == EOF || c == '\n') {
            buffer[position] = '\0'; //Null terminate the string!
            *return_val = buffer;
            break;
        } else {
            buffer[position] = c;
        }
        position++;

        // If we have exceeded the buffer, reallocate.
        if (position >= bufsize) { //>=, since we also need room for the null terminator
            bufsize += MAX_BUFFER;
            buffer = realloc(buffer, bufsize);
            if (buffer == NULL) {
                error_message();
                return FALSE;
            }
        }
    }
    return TRUE;
}

/*
 * Method to parse input shell command. Returns TRUE upon success and FALSE upon error.
 * Used some copy and paste from method, below. (did look at https://brennan.io/2015/01/16/write-a-shell-in-c/)
 * Looked at https://www.tutorialspoint.com/c_standard_library/c_function_strtok.htm for help understanding the functionality of strtok()
 */
int parse_command_line(char*** parsed_words, char* line) {
    int bufsize = MAX_BUFFER;
    int position = 0;
    char **tokens = malloc(sizeof(char *) * bufsize); //2-d array
    const char s[2] = " "; //want to separate using spaces or other deliminators
    char *current_token;

    if (tokens == NULL) {
        error_message();
        return FALSE;
    }

    current_token = strtok(line, s); //TODO: figure out how to use strtok_rgit
    while (current_token != NULL) {
        tokens[position] = current_token;
        position++;

        // If we have exceeded the buffer, reallocate. (copied and pasted from, below...)
        if (position >= bufsize) {
            bufsize += MAX_BUFFER;
            tokens = realloc(tokens, bufsize);
            if (tokens == NULL) {
                error_message();
                return FALSE;
            }
        }

        current_token = strtok(NULL, s);
    }

    //NULL terminate the buffer to be used by execvp
    tokens[position] = NULL;

    //Return the words
    *parsed_words = tokens;
    return TRUE;
}

/*
 * Method to take words from command line and return TRUE/FALSE on success/failure
 */
void parse_command(command* parsed_command, char** words) {
    parsed_command->command = words[0];
    parsed_command->arguments = words;
}

 /*
  * Method to initialize built-in commands
  */
void initialize_built_in_commands() {
     built_in_commands[0].command = "exit\0";
     built_in_commands[0].arguments = NULL;
 }

/*
 * Method to compare the command portion of the struct and see if they are equal; mainly used for built-in commands to know what to do
 */
int command_equals(command command1, command command2) {
    int compare_value = strcmp(command1.command, command2.command);
    if (compare_value == 0) {
        return TRUE;
    } else {
        return FALSE;
    }
}

/*
 * Error method
 */
void error_message() {
    printf("I am sorry, but there has been an error. Exiting now.\n");
}