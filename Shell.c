#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 100
#define HISTORY_SIZE 100

// Structure for storing command history
typedef struct {
    char *commands[HISTORY_SIZE];
    int count;
} History;

History history;

// Function to add a command to the history
void add_history(const char *cmd) {
    if (history.count < HISTORY_SIZE) {
        history.commands[history.count] = strdup(cmd);
        history.count++;
    } else {
        free(history.commands[0]);
        for (int i = 1; i < HISTORY_SIZE; i++) {
            history.commands[i - 1] = history.commands[i];
        }
        history.commands[HISTORY_SIZE - 1] = strdup(cmd);
    }
}

// Function to display history
void show_history() {
    for (int i = 0; i < history.count; i++) {
        printf("%d %s\n", i + 1, history.commands[i]);
    }
}

// Function to execute a single command (with background process support)
void launch(char **args, int is_background) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("Failed to fork");
    } else if (pid == 0) {
        // In child process, execute the command
        if (execvp(args[0], args) == -1) {
            perror("Error executing command");
        }
        exit(EXIT_FAILURE);
    } else {
        if (!is_background) {
            // In parent process, wait for the child to complete
            int status;
            waitpid(pid, &status, 0);
        } else {
            // In background, do not wait for the child process
            printf("Process running in background with PID: %d\n", pid);
        }
    }
}

// Function to pass the command input into arguments and detect background processing
int pass_command(char *cmd, char **args) {
    char *token;
    int index = 0;
    int is_background = 0;

    // Remove trailing newline
    cmd[strcspn(cmd, "\n")] = 0;

    // Check if command ends with '&' (background processing)
    if (cmd[strlen(cmd) - 1] == '&') {
        is_background = 1;
        cmd[strlen(cmd) - 1] = '\0'; // Remove '&' from command string
    }

    // Tokenize the command into arguments
    token = strtok(cmd, " ");
    while (token != NULL && index < MAX_ARGS) {
        args[index++] = token;
        token = strtok(NULL, " ");
    }
    args[index] = NULL; // Null-terminate the array

    return is_background;
}

// Function to handle "cd" command as a built-in
int handle_cd(char **args) {
    if (args[1] == NULL) {
        fprintf(stderr, "cd: expected argument to \"cd\"\n");
        return 1;
    } else {
        if (chdir(args[1]) != 0) {
            perror("cd");
        }
    }
    return 1;
}

// Function to handle commands with pipes
void execute_piped_commands(char *cmd) {
    char *commands[10];
    char *token = strtok(cmd, "|");
    int num_commands = 0;

    // Split the input into individual commands by pipe symbol '|'
    while (token != NULL) {
        commands[num_commands++] = token;
        token = strtok(NULL, "|");
    }

    int pipefds[2 * (num_commands - 1)];  // File descriptors for pipes

    // Create pipes
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipefds + i * 2) == -1) {
            perror("Pipe failed");
            exit(EXIT_FAILURE);
        }
    }

    int cmd_index = 0;
    while (cmd_index < num_commands) {
        char *args[MAX_ARGS];
        int is_background = pass_command(commands[cmd_index], args);  // Pass the command into args

        pid_t pid = fork();
        if (pid == -1) {
            perror("Failed to fork");
            exit(EXIT_FAILURE);
        } else if (pid == 0) {
            // If not the first command, get input from the previous pipe
            if (cmd_index > 0) {
                if (dup2(pipefds[(cmd_index - 1) * 2], 0) == -1) {
                    perror("dup2 failed");
                    exit(EXIT_FAILURE);
                }
            }
            // If not the last command, pipe output to the next pipe
            if (cmd_index < num_commands - 1) {
                if (dup2(pipefds[cmd_index * 2 + 1], 1) == -1) {
                    perror("dup2 failed");
                    exit(EXIT_FAILURE);
                }
            }

            for (int i = 0; i < 2 * (num_commands - 1); i++) {
                close(pipefds[i]);
            }

            // Execute the command
            if (execvp(args[0], args) == -1) {
                perror("Error executing command");
                exit(EXIT_FAILURE);
            }
        }
        cmd_index++;
    }

    // Close all pipe file descriptors in the parent process
    for (int i = 0; i < 2 * (num_commands - 1); i++) {
        close(pipefds[i]);
    }

    
    for (int i = 0; i < num_commands; i++) {
        wait(NULL);
    }
}

int main() {
    char cmd[MAX_CMD_LEN];
    char *args[MAX_ARGS];

    while (1) {
        // Display prompt
        printf("Kanav> ");
        fflush(stdout);

        // Read user input
        if (fgets(cmd, MAX_CMD_LEN, stdin) == NULL) {
            break;
        }

        // Add command to history
        add_history(cmd);

        //built-in commands
        if (strcmp(cmd, "history\n") == 0) {
            show_history();
            continue;
        }

        // Pass command
        int is_background = pass_command(cmd, args);

        // exit" command
        if (strcmp(args[0], "exit") == 0) {
            break;
        }

        // cd command
        if (strcmp(args[0], "cd") == 0) {
            handle_cd(args);
            continue;
        }

        // Check for pipes
        if (strchr(cmd, '|') != NULL) {
            execute_piped_commands(cmd);  
        } else {
            
            launch(args, is_background);
        }
    }

    // Free history memory
    for (int i = 0; i < history.count; i++) {
        free(history.commands[i]);
    }

    return 0;
}
