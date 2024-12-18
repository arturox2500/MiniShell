#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include "parser.h"
#include <sys/stat.h>
#include <sys/types.h>

int reestablecer(int original_stdin, int original_stdout, int original_stderr);
void comprobarHijos();
void ejecutar(tline *line);
void liberarMemoria(int ** pipes, pid_t * hijosActual, int nc);
int execute_bg(int N);
int execute_fg(int N);
void execute_cd(char *path);
void print_dir();
int redirect_stdin(char *input_file);
int redirect_stdout(char *output_file);
int redirect_stderr(char *error_file);
void manejador_sigint(int sig);
int check(pid_t p);
void manejador_sigtstp(int sig);
void execute_umask(char *mask);
int execute_exit();
int proceso_en_fg();
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
	int nred,k,N,original_stdin,original_stdout,original_stderr, empty, run;
	tline * line;
	lineasbg[0] = NULL;
    	signal(SIGINT, manejador_sigint);
    	signal(SIGTSTP, manejador_sigtstp);
	while (1) {
		nred = 0;
		empty = 0;
		print_dir();
		printf(" msh> ");
		if (fgets(buf, 1024, stdin) == NULL){
			if (feof(stdin)){
				printf("\n");
				execute_exit();
				return 1;
			}
			if (errno == EINTR) {// Si el error es una interrupción por señal, se ignora
                		continue;
            		}
			fprintf(stderr, "Error: Fallo al leer la entrada: %s\n", strerror(errno));
			continue;
		}
		line = tokenize(buf);
		comprobarHijos();
		if (orden >= 21){//Para evitar sobrepasar el limite
			printf("No hay espacio para más procesos, espere que termine un proceso en background\n");
			continue;
		}
		if (line == NULL || line->commands == NULL) {
			printf("Error al entender la linea\n");
			continue;
		}
		buf[strcspn(buf, "\n")] = '\0';
		if(line->commands[0].filename != NULL){
			N = 0;
			for (k = 0; k < line->ncommands; k++){
				if (line->commands[k].filename == NULL){
					printf("El comando número %d no se encontró\n",k + 1);
					N = 1;
					break;
				}
			}
			if (N == 1){
				continue;
			}
			lineasbg[orden] = strdup(buf);
			ejecutar(line);
		} else {
			aux = strtok(buf, " ");
			if (aux != NULL) {
				k = 0;
				original_stdin = dup(STDIN_FILENO);
				original_stdout = dup(STDOUT_FILENO);
				original_stderr = dup(STDERR_FILENO);
				if (original_stdin == -1 || original_stdout == -1 || original_stderr == -1){
					perror("Error al guardar los descriptores originales");
					continue;
				}
				if (line->redirect_input != NULL){
					nred++;
					k = redirect_stdin(line->redirect_input);
				}
				if (line->redirect_output != NULL){
					nred++;
					k = redirect_stdout(line->redirect_output);
				}
				if (line->redirect_error != NULL){
					nred++;
					k = redirect_stderr(line->redirect_error);
				}
				if (k != 0){//Si falla la lectura o escritura de ficheros
					if (reestablecer(original_stdin,original_stdout,original_stderr) == 1){
						execute_exit();
						return 1;
					}
					continue;//En la función lo notifica al usuario
				}
				if (strcmp(aux, "cd") == 0){
			    		path = strtok(NULL, " ");
			    		while (nred > 0){
			    			jobs = strtok(NULL, " ");
			    			if (strcmp(jobs, ">") == 0 || strcmp(jobs, "<") == 0 || strcmp(jobs, ">&") == 0){
			    				nred++; 
			    			}
			    			nred--;
			    		}
			    		jobs = strtok(NULL, " ");
			    		if (jobs != NULL){//Demasiados parámetros
			    			nred = -1;
			    		} else{		    		
			    			execute_cd(path);
			    		}
			    	} else if(strcmp(aux, "exit") == 0){//Da igual que parámetros tenga
			    		execute_exit();
				    	break;//Se cierra
			    	} else if (strcmp(aux, "jobs") == 0){
			    		while (nred > 0){
			    			path = strtok(NULL, " ");
			    			if (strcmp(path, ">") == 0 || strcmp(aux, "<") == 0 || strcmp(aux, ">&") == 0){
			    				nred++;
			    			}
			    			nred--;
			    		}
			    		path = strtok(NULL, " ");
			    		if (path != NULL){//Demasiados parámetros
			    			nred = -1;
			    		} else {
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
					}
			    	} else if (strcmp(aux, "umask") == 0){
			    		path = strtok(NULL, " ");
			    		while (nred > 0){
			    			jobs = strtok(NULL, " ");
			    			if (strcmp(jobs, ">") == 0 || strcmp(jobs, "<") == 0 || strcmp(jobs, ">&") == 0){
			    				nred++;
			    			}
			    			nred--;
			    		}
			    		jobs = strtok(NULL, " ");
			    		if (jobs != NULL){//Demasiados parámetros
			    			nred = -1;
			    		} else {
			    			execute_umask(path);
			    		}
			    	}else if (strcmp(aux, "bg") == 0){			    		
			    		path = strtok(NULL, " ");//Siguiente valor
			    		if (path == NULL){
			    			N=0;
			    			for (k = 0; k < 21; k++){
			    				if (lineasbg[k] != NULL){
			    					N = k+1;
			    				}
			    			}
			    			
			    			while (N > 0){
			    				k = N - 1;
			    				if (lineasbg[k] != NULL && est[k] == 1){
			    					N--;
			    					break;
			    				}
			    				N--;
			    			}			    			
			    			N++;
			    			empty = 1;
			    			
			    			run = 1;
			    			for (k = 0; k < 21; k++){
			    				if (lineasbg[k] != NULL){
			    					if (est[k]==1){
			    						run = 0;
			    					}
			    				}
			    			}
			    			
			    		} else {
			    			N = atoi(path);
			    		}
					if (N <= 0 || N > 21){
						nred = -2;
					}
					while (nred > 0){
			    			jobs = strtok(NULL, " ");
			    			if (strcmp(jobs, ">") == 0 || strcmp(jobs, "<") == 0 || strcmp(jobs, ">&") == 0){
			    				nred++;
			    			}
			    			nred--;
			    		}
					jobs = strtok(NULL, " ");
					if (jobs != NULL){//Demasiados parámetro
			    			nred = -1;
			    		} else if (N > 0 && N <= 21){
			    			N--;
			    			if (lineasbg[N] == NULL){
			    				nred = -2;
			    			} else {
			    				if (run == 0 || path != NULL){
			    					execute_bg(N);
			    				}
			    				else {
			    					printf("bg: trabajos actualmente en background\n");
			    				}
			    			}
			    		}
			    	} else if (strcmp(aux, "fg") == 0){
			    		path = strtok(NULL, " ");//Siguiente valor
			    		if (path == NULL){
			    			N=0;
			    			for (k = 0; k < 21; k++){
			    				if (lineasbg[k] != NULL){
			    					N=k+1;
			    				}
			    			}
			    			empty = 1;
			    		} else {
			    			N = atoi(path);
			    		}
					if (N <= 0 || N > 21){
						nred = -2;
					}
					while (nred > 0){
			    			jobs = strtok(NULL, " ");
			    			if (strcmp(jobs, ">") == 0 || strcmp(jobs, "<") == 0 || strcmp(jobs, ">&") == 0){
			    				nred++;
			    			}
			    			nred--;
			    		}
					jobs = strtok(NULL, " ");
					if (jobs != NULL){//Demasiados parámetros
			    			nred = -1;
			    		} else if (N > 0 && N <= 21){
			    			N--;
			    			if (lineasbg[N] == NULL){
			    				nred = -2;
			    			} else {
			    				execute_fg(N);
			    			}
			    		}
			    	} else {//no se encontro el mandato
			    		printf("%s: No se encuentra el mandato\n",aux);
			    	}
				if (reestablecer(original_stdin,original_stdout,original_stderr) == 1){
					execute_exit();
					return 1;
				}
				if (nred == -1){//Se escribe después para tener los descriptores originales
					printf("%s: Demasiados argumentos\n",aux);
				} else if (nred == -2 && empty != 1){
					printf("%s: %s: no existe ese trabajo\n",aux,path);
				} else if (empty ==1 && nred == -2) {
					printf("%s: current: no existe ese trabajo\n",aux);
				}
			}
		}
	}
	return 0;
}

int reestablecer(int original_stdin, int original_stdout, int original_stderr){
	if (dup2(original_stdout, STDOUT_FILENO) == -1){
   		fprintf(stderr, "Error al redirigir stdout al original: %s\n",strerror(errno));
		return 1;
	}
    	if (dup2(original_stdin, STDIN_FILENO) == -1){
    		fprintf(stderr, "Error al redirigir stdin al original: %s\n", strerror(errno));
		return 1;
	}
    	if (dup2(original_stderr, STDERR_FILENO) == -1){
    		fprintf(stderr, "Error al redirigir stderr al original: %s\n", strerror(errno));
    		return 1;
	}
	close(original_stdout);
	close(original_stdin);
	close(original_stderr);
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

void ejecutar(tline *line){
	int i,j,k,status;
	pid_t * hijosActual = (pid_t *)malloc(line->ncommands * sizeof(pid_t));
	if (hijosActual == NULL){
		perror("Error al reservar memoria");
		return;
	}
	hijosFG = (pid_t *)malloc((line->ncommands + 1) * sizeof(pid_t));
	if (hijosFG == NULL){
		perror("Error al reservar memoria");
		free(hijosActual);
		return;
	}
	hijosFG[line->ncommands] = 0;
	int ** pipes = (int **)malloc((line->ncommands - 1) * sizeof(int *));
	if (pipes == NULL){
		perror("Error al reservar memoria");
		free(hijosActual);
		free(hijosFG);
		return;
	}
	for(j = 0; j < line->ncommands - 1; j++){
		pipes[j] = (int *)malloc(2 * sizeof(int));
		if (pipes[j] == NULL){
			perror("Error al reservar memoria");
			liberarMemoria(pipes, hijosActual, j);
			return;
		}
		if (pipe(pipes[j]) != 0){
			perror("Error al crear el pipe");
			liberarMemoria(pipes, hijosActual, j);
			return;
		}
	}
	pid_t pid, result;
	for (i = 0; i < line->ncommands; i++){
		pid = fork();
		if (pid < 0){ //Error en el fork
			fprintf(stderr, "Falló el fort()\n%s\n", strerror(errno));
			exit(-1);
		} else if (pid == 0){ //Código hijo i
			if (i == line->ncommands - 1 && line->redirect_output != NULL){
				if (redirect_stdout(line->redirect_output) == 1){
					exit(-1);
				}
			}
			if (i == line->ncommands - 1 && line->redirect_error != NULL){
				if (redirect_stderr(line->redirect_error) == 1){
					exit(-1);
				}
			}
			if (i == 0){ //1º Hijo a ejecutar
				if (setpgid(0, 0) != 0){
					exit(-1);
				}
				if (line->redirect_input != NULL){
					if (redirect_stdin(line->redirect_input) == 1){
						exit(-1);
					}
				}
				if (line->ncommands > 1){
					if (dup2(pipes[i][1],STDOUT_FILENO) == -1){
						fprintf(stderr,"Error al modificar el descriptor de fichero del hijo %d\n", i);
						exit(-1);
					}
				}
			} else if (i == line->ncommands - 1){//Último Hijo a ejecutar
				if (setpgid(0, pid) != 0){
					exit(-1);
				}
				if (dup2(pipes[i - 1][0],STDIN_FILENO) == -1){
					fprintf(stderr,"Error al modificar el descriptor de fichero del hijo %d\n", i);
					exit(-1);
				}
			} else { //Los hijos del medio
				if (setpgid(0, pid) != 0){
					exit(-1);
				}
				if (dup2(pipes[i][1],STDOUT_FILENO) == -1){
					fprintf(stderr,"Error al modificar el descriptor de fichero del hijo %d\n", i);
					exit(-1);
				}
				if (dup2(pipes[i - 1][0],STDIN_FILENO) == -1){
					fprintf(stderr,"Error al modificar el descriptor de fichero del hijo %d\n", i);
					exit(-1);
				}
			}
			for (j = i - 1; j < line->ncommands - 1;j++){
				if (j >= 0){
					close(pipes[j][0]);
					close(pipes[j][1]);
				}
			}
			execvp(line->commands[i].filename,line->commands[i].argv);
			perror("Error en execvp");
    			exit(-1);
		} else {
			if (i == 0) {
				if (setpgid(pid, pid) != 0){
					return;
				}
			} else {
				if (setpgid(pid, hijosActual[0]) != 0){
					return;
				}
			}
			hijosActual[i] = pid;
			if (i > 0){
				close(pipes[i - 1][0]);
				close(pipes[i - 1][1]);
			}
		}
	}
	if (line->background == 0){//no es background
		for (j = 0; j < line->ncommands; j++){
			hijosFG[j] = hijosActual[j];
		}	
		for (j = 0; j < line->ncommands; j++){
			waitpid(hijosActual[j], &status, WUNTRACED);
			if (status == -1){
				perror("Error al terminar el hijo");
				liberarMemoria(pipes, hijosActual, line->ncommands);
				lineasbg[orden] = NULL;
				return;
			}
		}
		lineasbg[orden] = NULL;
	} else{//background
		ncom[orden] = line->ncommands;
		k = 0;
		rel[orden] = (int *)malloc(line->ncommands * sizeof(int));
		if (rel[orden] == NULL){
			perror("Error al reservar memoria");
			liberarMemoria(pipes, hijosActual, line->ncommands);
			return;
		}
		for (j = 0; j < line->ncommands; j++){
			result = waitpid(hijosActual[j], &status, WNOHANG);
			if (result == 0){//No ha terminado
				while (hijosST[k] != 0){
					if (k + 1 >= 20){//Si se pasa del espacio máximo
						printf("No hay espacio para que meter más procesos en background, espere a que terminen los actuales\n");
						for (j = 0; j<line->ncommands; j++){
							if(kill(hijosActual[j], SIGKILL) != 0){
								perror("Error al enviar SIGKILL al procesos");
		    						return;
							}
						}
						for (k = 0; k<line->ncommands; k++){
							if (rel[orden][k] != 0){
								hijosST[rel[orden][k]] = 0;
							}
						}
						liberarMemoria(pipes, hijosActual, line->ncommands);
						free(rel[orden]);
						lineasbg[orden] = NULL;
						est[orden] = 0;
						ncom[orden] = 0;
						return;
					}
					k++;
				}
				hijosST[k] = (pid_t)hijosActual[j];
				rel[orden][j] = k;
				if (j == line->ncommands - 1){//Si es el último comando
					est[orden] = 0;//Estado = Running
				}
			} else if (result == hijosActual[j]){//Ha terminado el hijo j
				if (j == line->ncommands - 1){//Han terminado todos
					liberarMemoria(pipes, hijosActual, line->ncommands);
					free(rel[orden]);
					lineasbg[orden] = NULL;
					est[orden] = 0;
					return;
				} else {//Aun quedan hijos por acabar
					rel[orden][j] = -1;
				}
			} else if (status != 0){//Error al terminar el hijo
				perror("Error al terminar el hijo");
				free(rel[orden]);
				lineasbg[orden] = NULL;
				est[orden] = 0;
				liberarMemoria(pipes, hijosActual, line->ncommands);
				return;
			}
		}
		printf("[%d] %d\n",orden + 1,hijosActual[line->ncommands - 1]);//Se pone después por si no hay espacio
		orden++;
		while(lineasbg[orden] != NULL){
			orden++;//Guarda el siguiente valor al q acceder
		}
	}
	liberarMemoria(pipes, hijosActual, line->ncommands);
	return;
}

void liberarMemoria(int ** pipes, pid_t * hijosActual, int nc){
	int j;
	for (j = 0; j < nc - 1;j++){
		free(pipes[j]);
	}
	free(pipes);
	free(hijosActual);
	if (hijosFG != NULL){
		free(hijosFG);
		hijosFG = NULL;
	}
}

int execute_bg(int N){
	printf("[%d]- %s\n",N + 1,lineasbg[N]);
	int j,status;
	pid_t result;
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
			result = waitpid(hijosST[rel[N][j]], &status, WNOHANG);//No espera a q termine ese hijo
			if (status != 0 && result != hijosST[rel[N][j]]){//Si se produce algún error
				perror("Error al terminar el hijo");
				return 1;
			}
			if (result == hijosST[rel[N][j]] && j != ncom[N] - 1){//Cuando ha terminado y no es el último comando
				hijosST[rel[N][j]] = 0;
				rel[N][j] = -1;
			}
		}
	}
	if (kill(hijosST[rel[N][ncom[N] - 1]], 0) != 0){//Si ha muerto el último proceso
		free(rel[N]);
		lineasbg[N] = NULL;
		ncom[N] = 0;
		est[N] = 0;
		if (orden > N){
			orden = N;
		}
	}
	return 0;
}

int execute_fg(int N){
	printf("%s\n",lineasbg[N]);
	int j,status;
	hijosFG = (pid_t *)malloc((ncom[N] + 1) * sizeof(pid_t));
	if (hijosFG == NULL){
		perror("Error al reservar memoria");
		return 1;
	}
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
					if (hijosFG != NULL){
						free(hijosFG);
						hijosFG = NULL;
					}
					return 1;
				}
			}
		}
		est[N] = 0;
	}
	for(j = 0; j < ncom[N]; j++){
		if (rel[N][j] != -1){
			waitpid(hijosST[rel[N][j]], &status, WUNTRACED);//Espera a q termine ese hijo
			if (status == -1){
				perror("Error al terminar el hijo");
				if (hijosFG != NULL){
					free(hijosFG);
					hijosFG = NULL;
				}
				return 1;
			}
		}
	}
	if (kill(hijosST[rel[N][ncom[N] - 1]], 0) != 0){//Si ha muerto el último proceso
		if (rel[N] != NULL){
			free(rel[N]);
		}
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

int redirect_stdin(char *input_file) {
    	FILE *file = fopen(input_file, "r");  
    	if (!file) {
        	fprintf(stderr, "%s: Error. %s\n", input_file, strerror(errno));
        	return 1;
    	}
    	if (dup2(fileno(file), STDIN_FILENO) == -1) {// Redirigir STDIN a este fichero
        	fprintf(stderr, "%s: Error. %s\n", input_file, strerror(errno));
        	fclose(file);
        	return 1;
    	}
    	fclose(file);
    	return 0;  
}

int redirect_stdout(char *output_file) {
    	FILE *file = fopen(output_file, "w"); 
    	if (!file) {
		fprintf(stderr, "%s: Error. %s\n", output_file, strerror(errno));
		return 1;
    	}
    	if (dup2(fileno(file), STDOUT_FILENO) == -1) { // Redirigir STDOUT a este fichero
		fprintf(stderr, "%s: Error. %s\n", output_file, strerror(errno));
		fclose(file);
		return 1;
    	}
    	fclose(file); 
    	return 0; 
}

int redirect_stderr(char *error_file) {
    	FILE *file = fopen(error_file, "w");  
    	if (!file) {
        	fprintf(stderr, "%s: Error. %s\n", error_file, strerror(errno));
        	return 1;
    	}
    	if (dup2(fileno(file), STDERR_FILENO) == -1) {// Redirigir STDERR a este archivo
        	fprintf(stderr, "%s: Error. %s\n", error_file, strerror(errno));
        	fclose(file);
        	return 1;
	}
    	fclose(file);  
    	return 0;
}

void manejador_sigtstp(int sig) {
	if (hijosFG == NULL){
		printf("\n");
		print_dir();
		printf(" msh> ");
		fflush(stdout);
		return;
		printf("%d", sig);
	}
	int procs = proceso_en_fg(),j,k,pos;
	pid_t pgid;
	if (procs != 0){ //si hay procesos en ejecución
		pgid = getpgid(hijosFG[0]);
		if (pgid == -1) {
			perror("Error al obtener el grupo de procesos");
			return;
		}
		
		for (j = 0; j<procs; j++){
			if(kill(hijosFG[j], SIGTSTP) != 0){
				perror("Error al enviar SIGTSTP al procesos");
		    		return;
			}
		}
		
		pos = check(hijosFG[0]);
		if (pos == -1) { // Si no está en las variables, se añade
			rel[orden] = (int *)malloc(procs * sizeof(int));
			if (rel[orden] == NULL){
				perror("Error al reservar memoria");
				return;
			}
			for (j = 0; j < procs; j++){
				k = 0;//ahora pasar a los otros arrays		
				while (hijosST[k] != 0){
					if (k + 1 >= 20){
						printf("\nNo hay espacio para que meter más procesos en background, espere a que terminen los actuales\n");
						for (j = 0; j<procs; j++){
							if(kill(hijosFG[j], SIGKILL) != 0){
								perror("Error al enviar SIGKILL al procesos");
		    						return;
							}
						}
						
						for (k = 0; k<j; k++){
							if (rel[orden][k] != 0){
								hijosST[rel[orden][k]] = 0;
							}
						}
						if (hijosFG != NULL){
							free(hijosFG);
							hijosFG = NULL;
						}
						free(rel[orden]);
						lineasbg[orden] = NULL;
						est[orden] = 0;
						return;
					}
					k++;
				}
				hijosST[k] = (pid_t)hijosFG[j];
				rel[orden][j] = k;
			}
			ncom[orden] = procs;
			est[orden] = 1;//Estado = Stopped
			printf("\n[%d]+  Stopped             %s\n", orden + 1, lineasbg[orden]);
			orden++;
			while(lineasbg[orden] != NULL){
				orden++;//Guarda el siguiente valor al q acceder
			}
		} else {
			est[pos] = 1; // Estado detenido
			printf("\n[%d]+  Stopped             %s\n", pos + 1, lineasbg[pos]);
		}
		if (hijosFG != NULL){
			free(hijosFG);
        		hijosFG = NULL;
		}
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

void manejador_sigint(int sig) {
	int i, procs, pos;
        if (hijosFG == NULL) {// Verificar si hijosFG es NULL antes del bucle
        	printf("\n");
        	print_dir();
		printf(" msh> ");
		fflush(stdout);
        	return;  
        	printf("%d", sig);
        }
 	procs = proceso_en_fg();
 	pos = check(hijosFG[procs - 1]);//Compruebas el último proceso de la linea
	if (pos != -1){//Si se encuentra se quita
		for (i = 0; i<ncom[pos]; i++){
			if (rel[pos][i] != -1){
				hijosST[rel[pos][i]] = 0;
			}
		}
		free(rel[pos]);
		lineasbg[pos] = NULL;
		est[pos] = 0;
		ncom[pos] = 0;
		if (orden > pos){
			orden = pos;
		}
	}
 	for (i = 0; i<procs; i++){
		if (hijosFG[i] != 0){
			if (kill(hijosFG[i], 0) == 0){
				if (kill(hijosFG[i], SIGKILL) != 0){
					fprintf(stderr, "Error al intentar matar el proceso: %s\n", strerror(errno));
				}
			}
		}
	}
        printf("\n");
        free(hijosFG);
        hijosFG = NULL;
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
		if (*endptr != '\0' || strlen(mask) > 4) {
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
