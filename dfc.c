#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <openssl/md5.h>
#include <errno.h>
#include <unistd.h>


#define MAXBUFSIZE 100

/*struct to store all relevant configuration information */
struct config_struct 
{
	char dfs1_addr[32];
	char dfs2_addr[32];
	char dfs3_addr[32];
	char dfs4_addr[32];
	char dfs1_port[32];
	char dfs2_port[32];
	char dfs3_port[32];
	char dfs4_port[32];
	char username[32];
	char passwd[32];
};

/*prints simple menu to stout */
void display_user_menu(char *command)
{
	printf("Please provide one of the following commands: \n");
	printf("1. get [file_name]\n2. put [file_name]\n3. list\n4. exit\n");
	fgets(command, MAXBUFSIZE, stdin); //get user input
	return;
}

int configure_client(char *config_file, struct config_struct *c)
{
	FILE *fp;
	int read;
	ssize_t len = 0;
	char *line = NULL;
	
	puts("in configure client");

	fp = fopen(config_file, "r");
	if(fp == NULL) //cannot not find server config file
	{	
		perror("error opening config file");
		exit(1);
	}
	while((read = getline(&line, &len, fp)) != -1)
	{
		char *linedup = strndup(line, strlen(line)); //parse file
		char *token = strsep(&linedup, " ");
		strtok(linedup, "\n"); 
		if(!strcmp(token, "Server"))
		{
			char *token2 = strsep(&linedup, " ");
			char *token3 = strsep(&linedup, ":");
		 	if(!strcmp(token2, "DFS1"))
		 	{
		 		strcpy(c->dfs1_addr, token3);
		 		strcpy(c->dfs1_port, linedup);
		 	}
			else if(!strcmp(token2, "DFS2"))
		 	{
		 		strcpy(c->dfs2_addr, token3);
		 		strcpy(c->dfs2_port, linedup);
		 	}
		 	else if(!strcmp(token2, "DFS3"))
		 	{
		 		strcpy(c->dfs3_addr, token3);
		 		strcpy(c->dfs3_port, linedup);
		 	}
		 	else if(!strcmp(token2, "DFS4"))
		 	{
		 		strcpy(c->dfs4_addr, token3);
		 		strcpy(c->dfs4_port, linedup);
		 	}
		}
		else if(!strcmp(token, "Username:"))
		{
			strcpy(c->username, linedup);
		}
		else if(!strcmp(token, "Password:"))
		{
			strcpy(c->passwd, linedup);
		}
	}

	fclose(fp);
	return 0;
}

int get_from_server(int sock[4], char *command, struct sockaddr_in remote)
{
	return 0;
}

int put()
{
	return 0;
}

int get()
{
	return 0;
}

int list()
{
	return 0;
}

int main (int argc, char * argv[])
{

	int sock[4];      // Array of sockets, one for each server
	int connectfd[4]; // Array of connection descriptors, one for each connection (server)
	char *command = malloc(sizeof(char)*MAXBUFSIZE);
	char msg[MAXBUFSIZE];	
	int nfile_size;
	int file_size;
	FILE *fp;
	struct config_struct *c = malloc(sizeof(struct config_struct));	

	struct sockaddr_in server1;              //"Internet socket address structure"
	struct sockaddr_in server2;      
	struct sockaddr_in server3;      
	struct sockaddr_in server4;

	int addr_length = sizeof(struct sockaddr);

	if (argc != 2)
	{
		printf("USAGE:  ./dfc <user>.conf\n");
		exit(1);
	}

	configure_client(argv[1] , c);

	//printf("%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",c->dfs1_addr,c->dfs1_port,c->dfs2_addr,c->dfs2_port,c->dfs3_addr,c->dfs3_port,c->username,c->passwd);
	/******************
	  Here we populate a sockaddr_in struct with
	  information regarding where we'd like to send our packet 
	  i.e the Server.
	 ******************/
	bzero(&server1,sizeof(struct sockaddr_in));               //zero the struct
	server1.sin_family = AF_INET;                 //address family
	server1.sin_port = htons(atoi(c->dfs1_port));      //sets port to network byte order
	server1.sin_addr.s_addr = inet_addr(c->dfs1_addr); //sets server1 IP address
	
	bzero(&server2,sizeof(server2));               //zero the struct
	server2.sin_family = AF_INET;                 //address family
	server2.sin_port = htons(atoi(c->dfs2_port));      //sets port to network byte order
	server2.sin_addr.s_addr = inet_addr(c->dfs2_addr); //sets server1 IP address

	bzero(&server3,sizeof(server3));               //zero the struct
	server3.sin_family = AF_INET;                 //address family
	server3.sin_port = htons(atoi(c->dfs3_port));      //sets port to network byte order
	server3.sin_addr.s_addr = inet_addr(c->dfs3_addr); //sets server1 IP address

	bzero(&server4,sizeof(server4));               //zero the struct
	server4.sin_family = AF_INET;                 //address family
	server4.sin_port = htons(atoi(c->dfs4_port));      //sets port to network byte order
	server4.sin_addr.s_addr = inet_addr(c->dfs4_addr); //sets server1 IP address

	//Causes the system to create a generic socket of type TCP (datastream)
	int i = 0;
	while (i < 4)
	{	
		if ((sock[i] = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		{
			printf("unable to create socket %d\n", i);
			//exit(1);
		}
		i++; 
	}
	connectfd[0] = connect(sock[0], (struct sockaddr *)&server1, sizeof(server1));
	connectfd[1] = connect(sock[1], (struct sockaddr *)&server2, sizeof(server2));
	connectfd[2] = connect(sock[2], (struct sockaddr *)&server3, sizeof(server3));
	connectfd[3] = connect(sock[3], (struct sockaddr *)&server4, sizeof(server4));
	
	i = 0;
	while (i < 4)
	{
		if(connectfd[i] == -1)
		{
			printf("unable to connect to server %d\n", i + 1);
		}
		i++;
	}

	while(1)
	{
		
		display_user_menu(command);
		printf("command: %s\n", command);	
		char *comdup = strndup(command, strlen(command)-1);    //remove new line
		char *token = strsep(&comdup, " "); //split into command and filename
		if(!strcmp(token, "put")){
			fp = fopen(comdup, "r"); //open file
			if(fp == NULL) //check if file exists
			{
				strcpy(msg, "FILE DOES NOT EXIST. CHOOSE ANOTHER FILE\n");
				puts(msg);
				continue; 
			} 
			int faccess;
			int nfaccess;
			//send command to server
			puts("ONE");
			send(sock[0], command, strlen(command)+1, 0);
			puts("TWO");
			//get back whether or not server can complete command (maybe file already exists)
			recv(sock[0], &nfaccess, sizeof(int), 0);
			faccess = ntohl(nfaccess);

			if(faccess) //server can complete
			{ /* this needs to where I cut the file up and check md5hash, etc */
				/* get file size send it to server */
				fseek(fp, 0, SEEK_END);
				file_size=ftell(fp);
				nfile_size = htonl(file_size);
				send(sock[0], &nfile_size, sizeof(int)+1, 0);
				fseek(fp, 0, SEEK_SET);
				char fbuffer[MAXBUFSIZE];
				/*read from file and send it buffered to server in packets */
				while(fread(fbuffer, 1, MAXBUFSIZE, fp) > 0)
				{
					send(sock[0], fbuffer, sizeof(fbuffer), 0);
				}
				
			}
			else //server returned 0 for faccess -- cannot complete request
			{
				printf("ERROR: FILE ALREADY EXISTS ON SERVER. CHOOSE ANOTHER FILE\n");
				continue;
			}
			fclose(fp);
		}
		else if(!strcmp(token, "get")) 
		{
			if(get_from_server(sock, command, server1)) //get file from server
			{
				printf("FILE DOES NOT EXIST ON SERVER. PLEASE CHOOSE A NEW FILE\n");
			}
		}
		else if(!strcmp(token, "list"))
		{
			if(get_from_server(sock, command, server1)) // get ls from server
			{
				printf("ERROR executing ls command\n");
			}
		}
		else if(!strcmp(token, "exit"))
		{
			/* send server exit command, exit */
			send(sock[0], command, strlen(command)+1, 0);	
			printf("exiting...\n");
			break;
		}
		else
		{
			/*unrecognized command.. let server handle this*/
			send(sock[0], command, strlen(command)+1, 0);
			char msg[MAXBUFSIZE];
			recv(sock[0], msg, sizeof(msg), 0);
			puts(msg);

		}

	}
	
	close(sock[0]);
	close(sock[1]);
	close(sock[2]);
	close(sock[3]);
	return 0;
}
	