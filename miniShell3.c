#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "parser.h"

int ejecutar(tline *line);//,char * in, char * out, char * err, int bg);
void execute_cd(char *path);
void print_dir();
void redirect_stdin(char *input_file);
void redirect_stdout(char *output_file);
void redirect_stderr(char *error_file);

int main(int argc, char * argv[]) {
	if (argc > 1){
		printf("El ejecutable %s no necesita argumentos\n",argv[0]);
		return 1;
	}
	char buf[1024];
	char *path;
	char *aux;
	tline * line;	
	while (1) {
		print_dir();
		printf(" msh> ");
		if (fgets(buf, 1024, stdin) == NULL){
			if (feof(stdin)){
				printf("\n");
				break;
			} 
			fprintf(stderr, "Error: Fallo al leer la entrada: %s\n", strerror(errno));
			continue;
		}
		line = tokenize(buf);
		if (line==NULL) {
			printf("Error al entender la linea\n");
			continue;
		}
		
		if(line->commands[0].filename!=NULL){
			ejecutar(line);// ,line->redirect_input,line->redirect_output,line->redirect_error,line->background);	
		} else {
			buf[strcspn(buf, "\n")] = '\0';
			aux = strtok(buf, " ");
			if (aux != NULL && strcmp(aux, "cd") == 0) {
		    		path = strtok(NULL, " ");		    		
		    		execute_cd(path);
			}	
		}
		
	}
	return 0;
}

int ejecutar(tline *line){//,char * in, char * out, char * err, int bg){
	int i,j;
	int nc = line->ncommands;
	tcommand * coms = line->commands;
	FILE *file;
	int ** pipes = (int **)malloc((nc - 1) * sizeof(int *));
	for(j = 0; j < nc - 1; j++){
		pipes[j] = (int *)malloc(2 * sizeof(int));
		pipe(pipes[j]);
	}
	pid_t pid;
	for (i = 0; i < nc; i++){
		pid = fork();
		if (pid < 0){ //Error en el fork
			fprintf(stderr, "Falló el fort()\n%s\n", strerror(errno));
			exit(-1);
		} else if (pid == 0){ //Código hijo i
			if (i == nc - 1 && line->redirect_output != NULL){
				redirect_stdout(line->redirect_output);
			}
			if (i == nc - 1 && line->redirect_error != NULL){
				redirect_stderr(line->redirect_error);
			}
			if (i == 0){ //1º Hijo a ejecutar
				if (line->redirect_input != NULL){
					redirect_stdin(line->redirect_input);
				}
				if (nc > 1){
					dup2(pipes[i][1],STDOUT_FILENO);
				}
			} else if (i == nc - 1){//Último Hijo a ejecutar
				dup2(pipes[i - 1][0],STDIN_FILENO);	
			} else { //Los hijos del medio
				dup2(pipes[i][1],STDOUT_FILENO);
				dup2(pipes[i - 1][0],STDIN_FILENO);
			}
			for (j = i - 1; j < nc - 1;j++){//
				if (j >= 0){
					close(pipes[j][0]);
					close(pipes[j][1]);
				}
			}
			execvp(coms[i].filename,coms[i].argv);
			perror("Error en execvp");
    			exit(1);
		} else {
			if (i > 0){
				close(pipes[i - 1][0]);
				close(pipes[i - 1][1]);
			}
		}
	}
	for (j = 0; j < nc - 1;j++){
		free(pipes[j]);
	}
	free(pipes);
	for (j = 0; j < nc; j++){
		wait(NULL);
	}
	return 0;
}

void execute_cd(char *path){
	char *home = getenv("HOME");
	if (path == NULL){
		path = home;
	}	
	if (chdir(path) != 0) {
        	fprintf(stderr, "Fallo al cambiar la ruta\n%s\n", strerror(errno));
    	}    	
}

void print_dir() {
    char cwd[1024];
    char *home = getenv("HOME");
    
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        if (home != NULL && strncmp(cwd, home, strlen(home)) == 0) {
            printf("~%s", cwd + strlen(home)); //reemplaza /home/user con ~ 
        } else {
            printf("%s\n", cwd);  // imprime directorio completo si no empieza en $HOME (/home/user)
        }
    } else {
        fprintf(stderr, "Fallo al encontrar la ruta actual\n%s\n", strerror(errno));
    }
}

void redirect_stdin(char *input_file) {
	
    	FILE *file = fopen(input_file, "r");  
    	if (!file) {
        	fprintf(stderr, "Falló lectura de fichero: %s\n", strerror(errno));
        	return;
    	}
    // Redirigir STDIN a este fichero
    	if (dup2(fileno(file), STDIN_FILENO) == -1) {
        	fprintf(stderr, "Error al redirigir stdin: %s\n", strerror(errno));
        	fclose(file);
        	return;
    	}
    	fclose(file);  
}

void redirect_stdout(char *output_file) {
    	FILE *file = fopen(output_file, "w"); 
    	if (!file) {
		fprintf(stderr, "Falló escritura a fichero %s: %s\n", output_file, strerror(errno));
		return;
    	}
    // Redirigir STDOUT a este fichero
    	if (dup2(fileno(file), STDOUT_FILENO) == -1) {
		fprintf(stderr, "Error al redirigir stdout a %s: %s\n", output_file, strerror(errno));
		fclose(file);
		return;
    	}
    	fclose(file);  
}

void redirect_stderr(char *error_file) {
    	FILE *file = fopen(error_file, "w");  
    	if (!file) {
        	fprintf(stderr, "Falló escritura a fichero: %s\n", strerror(errno));
        	return;
    	}
    // Redirigir STDERR a este archivo
    	if (dup2(fileno(file), STDERR_FILENO) == -1) {
        	fprintf(stderr, "Error al redirigir stderr a %s: %s\n", error_file, strerror(errno));
        	fclose(file);
        	return;
    }
    	fclose(file);  
}
