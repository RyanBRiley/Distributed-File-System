#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>


#define MAXBUFSIZE 256
#define MAXUSERS 32

struct user_struct 
{
	char username[MAXUSERS][32];
	char passwd[MAXUSERS][32];
	int total_user_index;
};

struct config_struct 
{
	char username[32];
	char passwd[32];
	char base_dir[32];
};

/*loads stored user information into memory from server config file*/
int configure_server(struct user_struct *users)
{
	FILE *fp;
	int read;
	ssize_t len = 0;
	char *line = NULL;
	
	puts("in configure server");

	fp = fopen("dfs.conf", "r");
	if(fp == NULL) //cannot not find server config file
	{	
		perror("error opening config file");
		exit(1);
	}
	int i = 0;
	while(((read = getline(&line, &len, fp)) != -1) && (i < MAXUSERS))
	{
		char *linedup = strndup(line, strlen(line)); //parse file
		char *uname = strsep(&linedup, " ");
		strtok(linedup, "\n"); 
		strcpy(users->username[i], uname);
		strcpy(users->passwd[i], linedup);
		i++;
	}
	users->total_user_index = i;

	fclose(fp);
	return 0;
}

/*user authentication*/
int authenticate(int sock, struct config_struct *c, struct user_struct *users)
{
	

	int authenticated = 0;
	char auth_attempt[MAXBUFSIZE];
	char uname_attempt[32];
	char passwd_attempt[32];
	

	/*tell the client to send credentials*/
	int ack = 1;
	send(sock, &ack, sizeof(int), 0);

	
	/*get credentials*/
	recv(sock, auth_attempt, MAXBUFSIZE, 0);
	sscanf(auth_attempt, "%s %s", uname_attempt, passwd_attempt);
	
	int i = 0;
	while(i < users->total_user_index)
	{
		if(!strcmp(users->username[i], uname_attempt))
		{
			authenticated = !strcmp(users->passwd[i], passwd_attempt);
		}
		i++;
	}
	printf("authenticated: %d\n", authenticated);
	if(authenticated)
	{
		sscanf(auth_attempt, "%s %s", c->username, c->passwd);
	}
	ack = 0;
	send(sock, &authenticated, sizeof(int), 0);
	recv(sock, &ack, sizeof(int), 0);
	return authenticated;

}

/*contruct server directory based on user and particular server*/
int construct_directory(char *base_dir, struct config_struct *c)
{
	struct stat st = {0};

	sprintf(c->base_dir, "..%s/%s",base_dir,c->username);
	//printf("c->base_dir: %s\n", c->base_dir);
	if (stat(c->base_dir, &st) == -1)
	{
		mkdir(c->base_dir, 0700);
	}
	return 0;
}

/* gets file, reads it into a buffer, sends it to client */
int read_to_client(int sock, char *file_name) 
{
	struct stat file_stat_struct;
	int faccess;
	int nfaccess;
	FILE *fp;
	char fbuffer[MAXBUFSIZE];
	int fd;
	printf("file_name to fetch: %s\n",file_name);
	fd = open(file_name, O_RDONLY);
	if(fd == -1) //file does not exist
	{
	
		faccess = 0; //status code to inform client that there was a problem opening file
		nfaccess = htonl(faccess); //switch to network order

		/*send status to client and return*/
		send(sock, &nfaccess, sizeof(int), 0);
		return 1;
	}
	/*tell client that file was able to be opened */
	faccess = 1;
	nfaccess = htonl(faccess);
	send(sock, &nfaccess, sizeof(int), 0);
	fstat(fd, &file_stat_struct);
	/*get file size and send it to client */
	/*fseek(fp, 0, SEEK_END);
	int file_size=ftell(fp);*/
	int file_size = (int)file_stat_struct.st_size;
	int nfile_size = htonl(file_size);
	send(sock, &nfile_size, sizeof(int), 0);
	sendfile(sock, fd, NULL, file_stat_struct.st_size);
	/*go back to beginning of file, read it into the buffer, send it client*/
	//fseek(fp, 0, SEEK_SET);
	//char *fbuffer = malloc(file_size); //allocate buffer
	int bytes_remn = file_size;
	int bytes_sent = 0; 
	/*
	while( bytes_remn > 0)
	{
		fread(fbuffer, file_size, 1, fp);
		int sent = send(sock, fbuffer, strlen(fbuffer), 0);
		bytes_remn -= sent;
		bytes_sent += sent;
		printf("bytes_sent: %d\n", bytes_sent);
	}
	//free(fbuffer);
	fclose(fp);*/
	return 0;
}

/*gets file from client, writes it into local dir*/
int write_from_client(int sock, char *file_name)
{
	int file_size;
	int faccess;
	int nfaccess;
	int bytes_recv = 0;
	unsigned int cli_addr_length;
	FILE *fp;
	char fbuffer[MAXBUFSIZE];

	if(access(file_name, F_OK) != -1) //file exists
	{
		/*send client status that file already exists and return*/
		faccess = 0;
		nfaccess = htonl(faccess);
		send(sock, &nfaccess, sizeof(int), 0);
		return 1;
	}
	else //file does not exist, ok to write
	{
		/*let client know that write is possible*/
		faccess = 1;
		nfaccess = htonl(faccess);
		send(sock, &nfaccess, sizeof(int), 0);

		fp = fopen(file_name, "wb");//open file

		/*get file size*/
		recv(sock, &file_size, sizeof(int), 0);
		//int file_size = ntohl(nfile_size);
		printf("file size: %d\n", file_size);
		///printf("nfile size: %d\n", nfile_size);

		/*get file from client in packets, write to file */
		int bytes_remn = file_size;
		while(bytes_remn > 0) 
		{
			bytes_recv = recv(sock, fbuffer, MAXBUFSIZE, 0);
			fwrite(fbuffer, sizeof(char), bytes_recv, fp);
			bytes_remn -= bytes_recv;
			
		}
		fclose(fp);
		send(sock, &faccess, sizeof(int), 0);
		return 0;
	}
}

int main (int argc, char * argv[] )
{
	int sock;                           //This will be our socket
	int sock_accepted;
	struct sockaddr_in serv_addr, cli_addr;     //"Internet socket address structure"
	unsigned int cli_addr_length;         //length of the sockaddr_in structure
	unsigned int serv_addr_length;         //length of the sockaddr_in structure
	char base_dir[32];
	              	
	char buffer[MAXBUFSIZE];             //a buffer to store our received message
	struct user_struct *users = malloc(sizeof(struct user_struct));
	

	if (argc != 3)
	{
		printf ("USAGE: ./dfs <server directory> <port>\n");
		
		exit(1);
	}

	if(strcmp(argv[1], "/DFS1") && strcmp(argv[1], "/DFS2") && strcmp(argv[1], "/DFS3") && strcmp(argv[1], "/DFS4"))
	{
		printf ("USAGE: ./dfs <server directory> <port>\n");
		exit(1);
	}
	else
	{
		strcpy(base_dir, argv[1]);
	}

	configure_server(users);
	//printf("users->username[1]: %s\nusers->passwd[1]: %s\nusers->total_user_index: %d\n",users->username[1],users->passwd[1],users->total_user_index);

	/******************
	  This code populates the sockaddr_in struct with
	  the information about our socket
	 ******************/
	bzero(&serv_addr,sizeof(serv_addr));                    //zero the struct
	serv_addr.sin_family = AF_INET;                   //address family
	serv_addr.sin_port = htons(atoi(argv[2]));        //htons() sets the port # to network byte order
	serv_addr.sin_addr.s_addr = INADDR_ANY;           //supplies the IP address of the local machine


	//Causes the system to create a generic socket of type TCP (datastream)
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("unable to create socket");
	}


	/******************
	  Once we've created a socket, we must bind that socket to the 
	  local address and port we've supplied in the sockaddr_in struct
	 ******************/
	if (bind(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		printf("unable to bind socket\n");
	}
	
	listen(sock, 5);
	printf("listening on port %s....\n", argv[2]);
	cli_addr_length = sizeof(cli_addr);
	serv_addr_length = sizeof(serv_addr);
	


	while(1){
		if((sock_accepted = accept(sock, (struct sockaddr *)&cli_addr,&cli_addr_length)) > 0)
		{
			printf("connection  formed\n");
			int pid = fork();
			if(pid == 0)
			{	
				
				while(1)
				{
					char sys_command[64];
					char file_name[128];
					bzero(buffer,sizeof(buffer));
					/*get command from client, parse it*/
					recv(sock_accepted, buffer, sizeof(buffer), 0);
					printf("command received: %s\n", buffer);
					char *bufdup = strndup(buffer, strlen(buffer)-1);    //remove new line
					char *token = strsep(&bufdup, " ");
					struct config_struct *c = malloc(sizeof(struct config_struct));
					
					
					if(!strcmp(token, "put"))
					{
						if(!authenticate(sock_accepted, c, users))
						{
							puts("Invalid Username/Password");
							continue;
						}
						if(construct_directory(argv[1], c))
						{
							continue;
						}
						sprintf(file_name,"%s/%s",c->base_dir,bufdup);
						if(write_from_client(sock_accepted, file_name))
						{
							printf("requested file exists\n");
							continue;
						}
					}

					else if(!strcmp(token, "get"))
					{
						if(!authenticate(sock_accepted, c, users))
						{
							puts("Invalid Username/Password");
							continue;
						}
						if(construct_directory(argv[1], c))
						{
							continue;
						}
						sprintf(file_name,"%s/%s",c->base_dir,bufdup);
						if(read_to_client(sock_accepted, file_name))
						{
							printf("FILE DOES NOT EXIST\n");
							continue;
						}
					}

					else if(!strcmp(token, "list"))
					{
						if(!authenticate(sock_accepted, c, users))
						{
							puts("Invalid Username/Password");
							continue;
						}
						if(construct_directory(argv[1], c))
						{
							continue;
						}
						if(access("ls_tmp", F_OK) != -1) //file exists need to remove old copy
						{
							system("rm ls_tmp.txt");
						}
						char file_name[MAXBUFSIZE];
						sprintf(file_name, "ls_temp_%s.txt", argv[2]);
						sprintf(sys_command, "ls -a %s > %s", c->base_dir, file_name);
						printf("sys_command: %s\n",sys_command );
						system(sys_command); //write ls results to temp file
						//char file_name[] = "ls_tmp.txt";
		
						if(read_to_client(sock_accepted, file_name))
						{
							printf("ERROR executing list command\n");
							continue;
						}

					}

					else if(!strcmp(token, "exit"))
					{
						printf("user exiting....\n");
						break;
					}

					else 
					{
						/* command was not recognized, repeat it back to client*/
						printf("unrecognized command\n");
						char *comm = " -- THE PREVIOUS COMMAND IS NOT UNDERSTOOD --\n";
						int msglen = strlen(comm);
						send(sock_accepted, &msglen, sizeof(int), 0);
						send(sock_accepted, comm, strlen(comm), 0);
						continue; 
					}
					free(c);
				}
				
				exit(0);
			}
		}
	}	
	free(users);
	
	close(sock);
}
