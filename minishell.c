#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "parser.h" 

void setUp(tline *line);
void execute(tcommand *cmd, int input_fd, int output_fd);
void execute_cd(char *path);

int main(void) {
	char input[1024]; // Para leer la entrada del usuario.
	char *aux;
	char *path;
	tline *line;    // Estructura para guardar los comandos procesados.	

	printf("msh> "); // Prompt inicial.
	while (fgets(input, sizeof(input), stdin)) {
		// Tokenizar la línea.
		line = tokenize(input);	
		setUp(line);

		if (line->commands[0].filename==NULL){
			input[strcspn(input, "\n")] = '\0';
			aux = strtok(input, " ");
			if (aux != NULL && strcmp(aux, "cd") == 0) {
		    		path = strtok(NULL, " ");
		    		
		    		execute_cd(path);
			
			}	
	
	
		}
		printf("msh> ");
	}
	return 0;
}
void setUp(tline *line) {
    int numCommands = line->ncommands;
    int pipes[2];
    int stdinp = STDIN_FILENO;
    int stdoutp = STDOUT_FILENO;
    FILE *file;

    // setup para restaurar
    int original_stdin = dup(STDIN_FILENO);
    int original_stdout = dup(STDOUT_FILENO);
    int original_stderr = dup(STDERR_FILENO);

    for (int i = 0; i < numCommands; i++) {
        if (i < numCommands - 1) {
            pipe(pipes);
            stdoutp = pipes[1];
        } else {
            stdoutp = STDOUT_FILENO;  //ultimo comando stdout
        }

        // redireccion primer comando
        if (i == 0 && line->redirect_input) {
            file = fopen(line->redirect_input, "r");
            if (!file) {
                perror("Failed to open input file");
                exit(EXIT_FAILURE);
            }
            dup2(fileno(file), STDIN_FILENO);
            fclose(file);
        }

        //redireccion del ultimo comando
        if (i == numCommands - 1 && line->redirect_output) {
            file = fopen(line->redirect_output, "w");
            if (!file) {
                perror("Failed to open output file");
                exit(EXIT_FAILURE);
            }
            dup2(fileno(file), STDOUT_FILENO);
            fclose(file);
        }

        
        execute(&line->commands[i], stdinp, stdoutp);

        
        if (i < numCommands - 1) {
            close(pipes[1]);
        }

        // Prepare siguiente comando
        stdinp = pipes[0]; 
    }

    
    for (int i = 0; i < numCommands; i++) {
        wait(NULL);
    }

    // Restore original file descriptors
    dup2(original_stdout, STDOUT_FILENO);
    dup2(original_stdin, STDIN_FILENO);
    dup2(original_stderr, STDERR_FILENO);

    // Close duplicates
    close(original_stdin);
    close(original_stdout);
    close(original_stderr);
}

void execute(tcommand *cmd, int stdinp, int stdoutp){
	pid_t pid;
	pid = fork();
	if (pid < 0){
		printf("issue");
	} else if (pid==0){ //proceso hijo
		if(stdinp != STDIN_FILENO){
			dup2(stdinp, STDIN_FILENO);
			close(stdinp);
		}
		if(stdoutp != STDOUT_FILENO){
			dup2(stdoutp, STDOUT_FILENO);
			close(stdoutp);
		}
		
		execvp(cmd->filename, cmd->argv);
		printf("lmao");
	} 
}

void execute_cd(char *path){
	char *home = getenv("HOME");
	char dir[1024];
	if (path == NULL){
		path = home;
	}
	
	if (chdir(path) != 0) {
        	perror("cd");
    	}
    	
    	if (getcwd(dir, sizeof(dir)) != NULL) {
        	printf("%s\n", dir);
    	} else {
        perror("getcwd");
    	}
}





