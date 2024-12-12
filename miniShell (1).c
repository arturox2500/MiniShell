#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "parser.h"
#include <sys/stat.h>
#include <sys/types.h>

int proceso_en_fg();
void manejador_sigint(int sig);
void manejador_sigtstp(int sig);
void comprobarHijos();
int ejecutar(tline *line);
int execute_bg(int N);
void execute_cd(char *path);
void execute_umask(char *mask);
int execute_exit();
void print_dir();
void redirect_stdin(char *input_file);
void redirect_stdout(char *output_file);
void redirect_stderr(char *error_file);
int check(pid_t p);
pid_t hijosST[20] = {0};
pid_t * hijosFG;
char * lineasbg[21] = {" "};
int ncom[21];//guarda el numero de comandos de cada uno
int * rel[21];//Para la posición dentro de hijosST
int est[21] = {0};//0 running, 1 stopped
int orden = 0;//Guarda la primera posición para lineasbg y rel

int main(int argc, char * argv[]) {
	if (argc > 1){
		printf("El ejecutable %s no necesita argumentos\n",argv[0]);
		return 1;
	}
	char buf[1024];
	char *path, *aux, *jobs;
	int j,k,N;
	tline * line;
	lineasbg[0] = NULL;
    	signal(SIGINT, manejador_sigint);
    	signal(SIGTSTP, manejador_sigtstp);
	while (1) {
		if (ncom[0] != 0){
			//printf("rel[0][0] = %d\n",rel[0][0]);
		}
		print_dir();
		printf(" msh> ");
		if (fgets(buf, 1024, stdin) == NULL){
			if (feof(stdin)){
				printf("\n");
				break;
			}
			if (errno == EINTR) {
                	// Si el error es una interrupción por señal, se ignora
                	continue;
            		}
			fprintf(stderr, "Error: Fallo al leer la entrada: %s\n", strerror(errno));
			continue;
		}
		line = tokenize(buf);
		comprobarHijos();
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
			    		execute_exit();
			    		break;//Se cierra
			    	} else if (strcmp(aux, "jobs") == 0){
			    		for(k = 0; k < 21; k++){
						if (lineasbg[k] != NULL){
							if (est[k] == 0){
								jobs = "+  Running";
							} else if (est[k] == 1){	
								jobs = "-  Stopped";
							}
							printf("[%d]%s             %s\n",k + 1,jobs,lineasbg[k]);
						}
					}
			    	} else if (strcmp(aux, "umask") == 0){
			    		path = strtok(NULL, " ");
			    		execute_umask(path);
			    		
			    	}else if (strcmp(aux, "bg") == 0){
			    		path = strtok(NULL, " ");//Siguiente valor
			    		N = atoi(path);
					if (N <= 0){
						printf("El segundo parámetro debe ser numérico y mayor que 0\n");
						continue;
					}
					if (N > 21){
						printf("No existe ese comando en background\n");
						continue;
					}
					N--;
			    		execute_bg(N);
			    	} else {//no se encontro el mandato
			    		printf("No se encontro el comando\n");
			    		continue;
			    	}
			}
		}
	}
	//Falta enviar kill a todos los procesos q falten por terminar
	return 0;
}

void comprobarHijos(){
	int i, j, k, l, sum;
	for(i = 0; i < 20; i++){
		if (hijosST[i] != 0){
			pid_t result = waitpid(hijosST[i], NULL, WNOHANG);
			if (result == hijosST[i]){
				hijosST[i] = 0;
				for(j = 0; j < 21; j++){
					if (ncom[j] != 0){//Si existe algo en esta posición
						k = 0;
						while (k < ncom[j] && rel[j][k] != i){
							k++;
						}
						if (rel[j][k] == i){//Si ha encontrado el proceso
							break;
						}
					}
				}
				sum = 0;
				for(l = 0; l < ncom[j]; l++){
					if (rel[j][l] == -1){
						sum++;
					}			
				}
				rel[j][k] = -1;
				if(sum == ncom[j] - 1){//Si ya no quedan más posiciones
					printf("[%d]+  Done             %s\n",j + 1,lineasbg[j]);
					lineasbg[j] = NULL;
					ncom[j] = 0;
					est[j] = 0;
					if (orden > j){
						orden = j;
					}
					free(rel[j]);
				}
			}
		}
	}
}

int ejecutar(tline *line){
	int i,j,k,status;
	int nc = line->ncommands;
	tcommand * coms = line->commands;
	FILE *file;
	hijosFG = (pid_t *)malloc((nc + 1) * sizeof(pid_t));
	hijosFG[nc] = 0;
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
			hijosFG[i] = pid;
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
			waitpid(hijosFG[j], &status, WUNTRACED);
			if (WIFEXITED(status)) { // si se acabo el proceso
		    		hijosFG[j] = 0;
			}	
		}
		lineasbg[orden] = NULL;
	} else{//background
		printf("[%d] %d\n",orden + 1,hijosFG[nc - 1]);
		ncom[orden] = nc;
		pid_t result;
		k = 0;
		rel[orden] = (int *)malloc(nc * sizeof(int));
		for (j = 0; j < nc; j++){
			result = waitpid(hijosFG[j], NULL, WNOHANG);
			if (result == 0){
				while (hijosST[k] != 0){
					k++;
				}
				hijosST[k] = (pid_t)hijosFG[j];
				rel[orden][j] = k;
				if (j == nc - 1){//Si es el último comando
					est[orden] = 0;//Estado = Running
				}
			}
		}
		orden++;
		while(lineasbg[orden] != NULL){
			orden++;//Guarda el siguiente valor al q acceder
		}
	}
	if (hijosFG != NULL){
		free(hijosFG);
		hijosFG = NULL;
	}
	return 0;
}

int execute_bg(int N){
	if (lineasbg[N] == NULL){
		printf("No existe ese comando en background\n");
		return 0;
	}
	printf("[%d]- %s\n",N + 1,lineasbg[N]);
	int j,status;
	pid_t pgid;
	hijosFG = (pid_t *)malloc((ncom[N] + 1) * sizeof(pid_t));
	hijosFG[ncom[N]] = 0;
	for(j = 0; j < ncom[N]; j++){
		if (rel[N][j] != -1){
			hijosFG[j] = hijosST[rel[N][j]];
		}
	}
	if (est[N] == 1){//Si el proceso estaba pausado
		for(j = 0; j < ncom[N]; j++){
			if (rel[N][j] != -1){
				if (kill(hijosST[rel[N][j]], SIGCONT) != 0) {//Si no es exitoso
					perror("Error al intentar reanudar el proceso");
					return 1;
				}
			}
		}
		est[N] = 0;
	}
	for(j = 0; j < ncom[N]; j++){
		if (rel[N][j] != -1){
			waitpid(hijosST[rel[N][j]], &status, WUNTRACED);//Espera a q termine ese hijo
			if (WIFEXITED(status)) { // si se acabo el proceso
		    		hijosFG[j] = 0;
			}
		}
	}
	if (kill(hijosST[rel[N][ncom[N] - 1]], 0) != 0){//Si ha muerto el proceso
		for(j = 0; j < ncom[N]; j++){
			rel[N][j] = -1;
		}
		free(rel[N]);
		lineasbg[N] = NULL;
		ncom[N] = 0;
		est[N] = 0;
		if (orden > N){
			orden = N;
		}
		if (hijosFG != NULL){
			free(hijosFG);
			hijosFG = NULL;
		}
	}
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
    	if (dup2(fileno(file), STDIN_FILENO) == -1) {// Redirigir STDIN a este fichero
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
    	if (dup2(fileno(file), STDOUT_FILENO) == -1) { // Redirigir STDOUT a este fichero
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
    	if (dup2(fileno(file), STDERR_FILENO) == -1) {// Redirigir STDERR a este archivo
        	fprintf(stderr, "Error al redirigir stderr a %s: %s\n", error_file, strerror(errno));
        	fclose(file);
        	return;
	}
    	fclose(file);  
}

void manejador_sigtstp(int sig) {
	printf("0\n");//BORRAR
	if (hijosFG == NULL){
		return;
	}
	int procs = proceso_en_fg(),j,k,pos;
	pid_t pgid;
	printf("1\n");//BORRAR
	if (procs != 0){ //si hay procesos en ejecución
		printf("hijosFG[0] = %d\n",hijosFG[0]);//BORRAR
		pgid = getpgid(hijosFG[0]); 
		printf("pgid = %d\n",pgid);//BORRAR
		if (pgid == -1) {
			perror("Error al obtener el grupo de procesos");
			return;
		}
		if(kill(-pgid, SIGTSTP) != 0){
			perror("Error al enviar SIGTSTP al grupo de procesos");
		    	return;
		}
		printf("2\n");//BORRAR
		pos = check(hijosFG[0]);
		printf("3\n");//BORRAR
		if (pos == -1) { // Si no está en las variables, se añade
			rel[orden] = (int *)malloc(procs * sizeof(int));
			for (j = 0; j < procs; j++){
				k = 0;//ahora pasar a los otros arrays		
				while (hijosST[k] != 0){
					k++;
				}
				hijosST[k] = (pid_t)hijosFG[j];
				rel[orden][j] = k;
			}
			printf("4.1\n");//BORRAR
			ncom[orden] = procs;
			est[orden] = 1;//Estado = Stopped
			printf("[%d]+  Stopped             %s\n", orden + 1, lineasbg[orden]);
			orden++;
			while(lineasbg[orden] != NULL){
				orden++;//Guarda el siguiente valor al q acceder
			}
		} else {
			printf("4.2\n");//BORRAR
			est[pos] = 1; // Estado detenido
			printf("[%d]+  Stopped             %s\n", pos + 1, lineasbg[pos]);
		}
		printf("5\n");//BORRAR
		if (hijosFG != NULL){
			free(hijosFG);
			hijosFG = NULL;
			printf("6\n");//BORRAR
		}
		printf("7\n");//BORRAR
	}
	fflush(stdout);	
}

int check(pid_t p){
	int i,j;
	for(i = 0; i<21; i++){
		if (ncom[i] != 0){
			for(j = 0; j < ncom[i]; j++){
				if(hijosST[rel[i][j]] == p){
					return i;
				}
			}
		}
	}
	return -1;
}

void manejador_sigint(int sig){
	int c = 0,i;
	for (i = 0; i < 20; i++) {
    		if (hijosFG[i] != 0) { // verifica si existe
        		if (kill(hijosFG[i], 0) == 0) { // Verifica si el proceso está activo
            			if (kill(hijosFG[i], SIGKILL) == -1) {
               				fprintf(stderr, "Error al intentar matar el proceso: %s\n", strerror(errno));
            			}
        		}
        		c = 1;
    		}
	}
	printf("\n");
	if (c == 0){
		print_dir();
		printf(" msh> ");
	} else {
		free(hijosFG);
	}
	fflush(stdout);
}

void execute_umask(char *mask){
	mode_t mascara_act = umask(0); // nos da la máscara actual
	char *endptr; //puntero para verificar las conversiones no validas
	mode_t nueva_mask; 
	if(mask == NULL){
		umask(mascara_act); 
		printf("%04o\n", mascara_act); // imprime la mascara en formato octal si no se pasan argumentos
	} else {
		nueva_mask = strtol(mask, &endptr, 8); // convierte la mascara en octal
		if (*endptr != '\0') {
            		fprintf(stderr, "Error: Umask no valida '%s'. Debe ser un octal.\n", mask);
            		return;
        	}
        	umask(nueva_mask); // si la mascara es valida se establece la nueva umask
	}
}

int execute_exit() {
	int i,j;
	for (i = 0; i < 21; i++) {
		if (ncom[i] != 0) {
			for(j = 0; j < ncom[i]; j++){
				if (kill(hijosST[rel[i][j]], SIGTERM) != 0) {//Si no es exitoso
					perror("Error al intentar matar el proceso");
					return 1;
				}
			}
			free(rel[i]);
		}
	}
	return 0;
}

int proceso_en_fg(){//Cuenta los procesos sin terminar
	int proc = 0;
	while (hijosFG[proc] != 0){
		proc++;
	}
	return proc;
}
