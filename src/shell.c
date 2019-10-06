#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>  

#define MAX_LINE 80

//parse buffer command (str)
void parse_command(char command[], char *args[], int *argc, int *waitFor) {
	char *arg;//argument elements storage
	int i=0;//argument element count
	char saveState[MAX_LINE];//save command state

	strcpy(saveState, command);

	//parse command --> args
	arg=strtok(command, " ");
	while (arg!=NULL) {
		args[i]=malloc(sizeof(char)*strlen(arg));
		strcpy(args[i], arg);
		arg=strtok(NULL, " ");
		i++;
	}

	//remove "&" from command and set wait=false
	if (strcmp(args[i-1], "&")==0) {
		free(args[i-1]);
		i--;
		*waitFor=0;
	}

	//add NULL terminate to args
	args[i]=NULL;
	i++;

	//restore buffer state
	strcpy(command, saveState);
	*argc=i;
}

//parent process action
void parent_process(int waitFor) {

	//wait for child process to exec
	if (waitFor==1) {
		wait(NULL);
		printf("--------------------------------\n");
		printf("Child process terminated!\n");
	}
}

//child process action
void child_process(char *args[], int *argc, char *pipeCmd) {
	pid_t pid;
	char *s_args[MAX_LINE/2+1]; //command argument
	int s_argc; //argument element count
	int waitFor;
	int pipefd[2];

	if (pipeCmd!=NULL) {
		if (pipe(pipefd) == -1) {
			printf("Pipe Error!\n");
			return;
		}
		switch (pid=fork()) {
			case -1:
				printf("Error creating child process!\n");
				break;
			case 0:
				close(pipefd[1]);
				dup2(pipefd[0], 0);
				parse_command(pipeCmd, s_args, &s_argc, &waitFor);
				child_process(s_args, &s_argc, NULL);
				close(pipefd[0]);
				break;
			default:
				close(pipefd[0]);
				dup2(pipefd[1], 1);
				child_process(args, argc, NULL);
				parent_process(waitFor);
				close(pipefd[1]);
		}
		printf("WAIT: %i",waitFor);
	}
	else
		execvp(args[0],args);
}

//get file name and reformat command
void file_info(char *args[], int *argc, char **fileName) {
	//get filename
	*fileName=malloc(sizeof(char)*strlen(args[*argc-2]));
	strcpy(*fileName, args[*argc-2]);

	//free allocated mem
	free(args[*argc-1]);
	free(args[*argc-2]);
	free(args[*argc-3]);
	*argc-=2;
	args[*argc-1]=NULL;
}

//execute parsed command
void command_exec(char *args[], int *argc, int waitFor) {
	pid_t pid; //process ID
	int fd;
	char *fileName;
	int file=0;//redirecting flag (0:no redirecting)

	//create child process using fork()
	switch (pid=fork()) { 
		case -1: //unsuccessful
			printf("Error creating child process!\n");
			break;
		case 0: //return to child process
			if (*argc>=4) {
				//redirect output case
				if (strcmp(args[*argc-3], ">")==0) {
					//turn flag on
					file=1;
					//get file name and reformat command
					file_info(args, argc, &fileName);
					//open/create file
					fd=open(fileName, O_WRONLY|O_TRUNC|O_CREAT, S_IRUSR|S_IRGRP|S_IWGRP|S_IWUSR);
					if (fd<0) {
						printf("ERROR open file!\n");
						return;
					}
					//make copy terminal to file
					dup2(fd, 1);
				}
				else
				//redirect input case
				if (strcmp(args[*argc-3], "<")==0) {
					//turn flag on
					file=1;
					//get file name and reformat command
					file_info(args, argc, &fileName);
					//open/create file
					fd=open(fileName, O_RDONLY);
					if (fd<0) {
						printf("ERROR open file!\n");
						return;
					}
					//make copy terminal to file
					dup2(fd, 0);
				}
			}
			child_process(args, argc, NULL);
			break;
		default: //return to parent/caller process
			parent_process(waitFor);
	}
	if (file==1) {
		close(fd);
	}
}

//free allocated memory
void clear_args(char *args[]) {
	int i=0;

	while (args[i]!=NULL) {
			free(args[i]);
			i++;
	}
}

int main() {
	int run=1; //process running control
	char *args[MAX_LINE/2+1]; //command argument
	int argc; //argument element count
	char history_buff[MAX_LINE]="";//history command buffer
	char command[MAX_LINE];//input commnad buffer
	int waitFor;//control parent to wait for child's termination (0: no waiting)
	pid_t pid; //process ID

	while (run==1) {
		printf("\nosh> ");
		scanf("%[^\n]s", command);//read input buffer
		char *subCmd=strchr(command, '|');//check pipe case
		waitFor=1;//set default: parent always wait for termination
		//exit case
		if (strcmp(command, "exit")==0) {
			printf("<=-----------exit triggered!-----------=>\n");
			run=0;
			kill(0,SIGKILL);
		}

		//history case
		else {
			if (strcmp(command, "!!")==0) {
				if (strcmp(history_buff, "")==0)
				printf("No commands in history.\n");
				else {
					parse_command(history_buff, args, &argc, &waitFor);
					printf("Previous command: %s\n", history_buff);
					command_exec(args, &argc, waitFor);
				}
			}
			else {
				//pipe case
				strcpy(history_buff, command);
				if (subCmd!=NULL) {
					char *cmd;
					cmd=strtok(command, "|");
					subCmd+=2;
					parse_command(cmd, args, &argc, &waitFor);
					switch (pid=fork()) {
						case -1:
							printf("Error creating child process!\n");
							break;
						case 0:
							child_process(args, &argc, subCmd);
							break;
						default:
							parent_process(waitFor);
					}
				}
				//execute command case
				else {
					strcpy(history_buff, command);
					parse_command(command, args, &argc, &waitFor);
					command_exec(args, &argc, waitFor);
				}
			}
		}
		clear_args(args);
		getchar();
	}
	clear_args(args);
	return 0;
}