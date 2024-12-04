#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "parser.h"
#include <time.h>

int ejecutar(tline *line);
void execute_cd(char *path);
void print_dir();
void redirect_stdin(char *input_file);
void redirect_stdout(char *output_file);
void redirect_stderr(char *error_file);
pid_t hijosST[20] = {0};
char * lineasbg[21] = {" "};
int rel[21][2] = {0,0};//0 para la posición dentro de hijosST y 1 para el estado, 0 running, 1 stopped
int orden = 0;//Guarda la primera posición para lineasbg y rel

int main(int argc, char * argv[]) {
	if (argc > 1){
		printf("El ejecutable %s no necesita argumentos\n",argv[0]);
		return 1;
	}
	time_t start = time(NULL);//hora inicial
	char buf[1024];
	char *path, *aux, *jobs;
	int j,k;
	tline * line;
	lineasbg[0] = NULL;
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
		if (line == NULL || line->commands == NULL) {
			printf("Error al entender la linea\n");
			continue;
		}
		buf[strcspn(buf, "\n")] = '\0';
		if(line->commands[0].filename != NULL){
			lineasbg[orden] = strdup(buf);
			ejecutar(line);	
		} else {
			aux = strtok(buf, " ");
			if (aux != NULL) {
				if (strcmp(aux, "cd") == 0){
			    		path = strtok(NULL, " ");		    		
			    		execute_cd(path);
			    	} else if(strcmp(aux, "exit") == 0){
			    		break;//Se cierra
			    	} else if (strcmp(aux, "jobs") == 0){
			    		for(k = 0; k < 21; k++){
						if (lineasbg[k] != NULL){
							if (rel[k][1] == 0){
								jobs = "+  Running";
							} else if (rel[k][1] == 1){	
								jobs = "-  Stopped";
							}
							printf("[%d]%s             %s\n",rel[k][0],jobs,lineasbg[k]);
						}
					}
			    	}else {//no se encontro el mandato
			    		printf("No se encontro el comando\n");
			    		continue;
			    	}
			}
		}
		if (time(NULL) - start > 5){
			for(j = 0; j < 20; j++){
				if (hijosST[j] != 0){
					pid_t result = waitpid(hijosST[j], NULL, WNOHANG);
					if (result == hijosST[j]){
						hijosST[j] = 0;
						for(k = 0; k < 21; k++){
							if (rel[k][0] == j){
								lineasbg[k] = NULL;
								rel[k][0] = 0;
								rel[k][1] = 0;
								orden = k;
							}
						}
					}
				}
			}
			start = time(NULL);
		}
	}
	//Falta enviar kill a todos los procesos q falten por terminar
	return 0;
}

int ejecutar(tline *line){
	int i,j,k;
	int nc = line->ncommands;
	tcommand * coms = line->commands;
	FILE *file;
	pid_t * hijosActual = (pid_t *)malloc(nc * sizeof(pid_t));
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
			hijosActual[i] = pid;
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
	if (line->background == 0){//no es background
		for (j = 0; j < nc; j++){
			wait(NULL);
		}
	} else{//background
		pid_t result;
		k = 0;
		for (j = 0; j < nc; j++){
			result = waitpid(hijosActual[j], NULL, WNOHANG);
			if (result == 0){
				while (hijosST[k] != 0){
					k++;
				}
				hijosST[k] = (pid_t)hijosActual[j];
				if (j == nc - 1){//Si es el último comando
					rel[orden][0] = k;//Guarda q posición de hijosST corresponde al último comando de la linea número orden
					rel[orden][1] = 0;//Estado = Running
				}
			}
		}
		orden++;
		while(lineasbg[orden] != NULL){
			orden++;//Guarda el siguiente valor al q acceder
		}
	}
	free(hijosActual);
	return 0;
}

void execute_cd(char *path){
	char *home = getenv("HOME");
	if (path == NULL){
		path = home;
	}	
	if (chdir(path) != 0) {
        	fprintf(stderr, "%s\n", strerror(errno));
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
