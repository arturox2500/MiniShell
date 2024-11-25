#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "parser.h"

int ejecutar(int nc, tcommand * coms);//,char * in, char * out, char * err, int bg);

int main(int argc, char * argv[]) {
	if (argc > 1){
		printf("El ejecutable %s no necesita argumentos\n",argv[0]);
		return 1;
	}
	char buf[1024];
	tline * line;	
	while (1) {
		printf("msh> ");
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
		ejecutar(line->ncommands,line->commands);// ,line->redirect_input,line->redirect_output,line->redirect_error,line->background);	
	}
	return 0;
}

int ejecutar(int nc, tcommand * coms){//,char * in, char * out, char * err, int bg){
	int i,j;
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
			if (i == 0){ //1º Hijo a ejecutar
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
