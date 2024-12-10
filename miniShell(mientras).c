#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "parser.h"
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

void manejador_sigint(int sig);
void manejador_sigtstp(int sig);
void configurar_senales();
void comprobarHijos();
int ejecutar(tline *line);
void execute_bg(int N);
void execute_cd(char *path);
void execute_umask(char *mask);
void execute_exit();
void print_dir();
void redirect_stdin(char *input_file);
void redirect_stdout(char *output_file);
void redirect_stderr(char *error_file);
pid_t hijosST[20] = {0};
pid_t hijosFG[20] = {0};
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
	time_t start = time(NULL);//hora inicial
	char buf[1024];
	char *path, *aux, *jobs;
	int j,k,N;
	tline * line;
	lineasbg[0] = NULL;
    	configurar_senales();
    	signal(SIGINT, manejador_sigint);
	while (1) {
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
		if (line == NULL || line->commands == NULL) {
			printf("Error al entender la linea\n");
			continue;
		}
		buf[strcspn(buf, "\n")] = '\0';
		if(line->commands[0].filename != NULL){
			ncom[orden] = line->ncommands;
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
		if (time(NULL) - start > 5){//Si han pasado más de 5 segundos desde la última comprobación
			comprobarHijos();
			start = time(NULL);
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
			hijosFG[j] = (pid_t)hijosActual[j];
		}
		for (j = 0; j < nc; j++){
			wait(NULL);
		}
		
		for (j = 0; j < nc; j++) {
    			hijosFG[j] = 0;  // Restablecer los valores de hijosFG
		}
		
		lineasbg[orden] = NULL;
		ncom[orden] = 0;
		
	} else{//background
		pid_t result;
		k = 0;
		rel[orden] = (int *)malloc(nc * sizeof(int));
		for (j = 0; j < nc; j++){
			result = waitpid(hijosActual[j], NULL, WNOHANG);
			if (result == 0){
				while (hijosST[k] != 0){
					k++;
				}
				hijosST[k] = (pid_t)hijosActual[j];
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
	free(hijosActual);
	return 0;
}

void execute_bg(int N){
	if (lineasbg[N] == NULL){
		printf("No existe ese comando en background\n");
		return;
	}
	int j;
	for(j = 0; j < ncom[N]; j++){
		if (rel[N][j] != -1){
			waitpid(hijosST[rel[N][j]],NULL,0);//Espera a q termine ese hijo
			rel[N][j] = -1;
		}
	}
	free(rel[N]);
	lineasbg[N] = NULL;
	ncom[N] = 0;
	est[N] = 0;
	if (orden > N){
		orden = N;
	}
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

void configurar_senales() {
    struct sigaction sa;
    sa.sa_handler = manejador_sigtstp;
    sa.sa_flags = 0; // No hay flags adicionales
    sigemptyset(&sa.sa_mask); // No bloquear otras señales mientras manejamos SIGTSTP

    if (sigaction(SIGTSTP, &sa, NULL) == -1) {
        perror("Error al configurar SIGTSTP");
        exit(1);
    }
}

void manejador_sigtstp(int sig) {
    for (int i = 0; i < 20; i++) {
        if (hijosST[i] != 0 && est[i] == 0) { // Si el proceso está activo
            if (kill(hijosST[i], SIGTSTP) == 0) {
                printf("Proceso con PID %d detenido.\n", hijosST[i]);
                est[i] = 1; // Cambiar estado a detenido
            } else {
                perror("Error al detener proceso");
            }
        }
    }
    printf("\n");
}

void manejador_sigint(int sig){
	int c = 0;
	for (int i = 0; i < 20; i++) {
    		if (hijosFG[i] != 0) {
        		if (kill(hijosFG[i], 0) == 0) { // Verifica si el proceso está activo
            			if (kill(hijosFG[i], SIGKILL) == -1) {
               				fprintf(stderr, "Error al intentar matar el proceso: %s\n", strerror(errno));
            			}
        		} 
        		hijosFG[i] = 0;
        		c = 1;
    		}
	}
	
	printf("\n");
	if (c == 0){
		print_dir();
		printf(" msh> ");
	}
	fflush(stdout);
}

void execute_umask(char *mask){
	if(mask == NULL){
		mode_t mascara_act = umask(0); // nos da la máscara actual
		umask(mascara_act); 
		printf("%04o\n", mascara_act); // imprime la mascara en formato octal si no se pasan argumentos
	} else {
		char *endptr; //puntero para verificar las conversiones no validas
		mode_t nueva_mask; 
		nueva_mask = strtol(mask, &endptr, 8); // convierte la mascara en octal
		if (*endptr != '\0') {
            		fprintf(stderr, "Error: Umask no valida '%s'. Debe ser un octal.\n", mask);
            		return;
        	}
        	umask(nueva_mask); // si la mascara es valida se establece la nueva umask
	}
}

void execute_exit() {
    for (int i = 0; i < 21; i++) {
        if (ncom[i] != 0) {
            free(rel[i]);
        }
    }
}
