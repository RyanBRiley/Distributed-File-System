#include <sys/types.h>
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


#define MAXBUFSIZE 100

/* gets file, reads it into a buffer, sends it to client */
int read_to_client(int sock, char *file_name, struct sockaddr_in cli_addr) 
{
	int faccess;
	int nfaccess;
	FILE *fp;
	fp = fopen(file_name, "r");
	if(fp == NULL) //file does not exist
	{
		faccess = 0; //status code to inform client that there was a problem opening file
		nfaccess = htonl(faccess); //switch to network order

		/*send status to client and return*/
		send(sock, &nfaccess, sizeof(int)+1, 0);
		return 1;
	}
	/*tell client that file was able to be opened */
	faccess = 1;
	nfaccess = htonl(faccess);
	send(sock, &nfaccess, sizeof(int)+1, 0);

	/*get file size and send it to client */
	fseek(fp, 0, SEEK_END);
	int file_size=ftell(fp);
	int nfile_size = htonl(file_size);
	send(sock, &nfile_size, sizeof(int)+1, 0);

	/*go back to beginning of file, read it into the buffer, send it client*/
	fseek(fp, 0, SEEK_SET);
	char *fbuffer = malloc(file_size); //allocate buffer
	fread(fbuffer, file_size, 1, fp);
	send(sock, fbuffer, strlen(fbuffer)+1, 0);
	free(fbuffer);
	fclose(fp);
	return 0;
}

/*gets file from client, writes it into local dir*/
int write_from_client(int sock, char *file_name, struct sockaddr_in cli_addr)
{
	int nfile_size;
	int faccess;
	int nfaccess;
	int bytes_recv = 0;
	unsigned int cli_addr_length;
	FILE *fp;
			
	if(access(file_name, F_OK) != -1) //file exists
	{
		/*send client status that file already exists and return*/
		faccess = 0;
		nfaccess = htonl(faccess);
		send(sock, &nfaccess, sizeof(int)+1, 0);
		return 1;
	}
	else //file does not exist, ok to write
	{
		/*let client know that write is possible*/
		faccess = 1;
		nfaccess = htonl(faccess);
		send(sock, &nfaccess, sizeof(int)+1, 0);

		fp = fopen(file_name, "wb");//open file

		/*get file size*/
		recv(sock, &nfile_size, sizeof(int), 0);
		int file_size = ntohl(nfile_size);

		char fbuffer[MAXBUFSIZE];
	
		/*get file from client in packets, write to file */
		while (bytes_recv + sizeof(fbuffer) < file_size)
		{
			bzero(fbuffer,sizeof(fbuffer));
			bytes_recv += recv(sock, fbuffer, sizeof(fbuffer), 0);
			fwrite(fbuffer, sizeof(fbuffer), 1, fp);
			
		}
		/*write leftover bytes that were incommensurable with sizeof(fbuffer)*/
		bzero(fbuffer,sizeof(fbuffer));
		int t_remain = file_size - bytes_recv;
		recv(sock, fbuffer, t_remain, 0);
		fwrite(fbuffer, t_remain, 1, fp);
		fflush(fp);
		fclose(fp);
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
	sock_accepted = accept(sock, (struct sockaddr *)&cli_addr,&cli_addr_length);

	while(1){
		bzero(buffer,sizeof(buffer));
		/*get command from client, parse it*/
		recv(sock_accepted, buffer, sizeof(buffer), 0);
		printf("received from client: \n%s\nend\n", buffer);
		char *bufdup = strndup(buffer, strlen(buffer)-1);    //remove new line
		char *token = strsep(&bufdup, " ");


		if(!strcmp(token, "put"))
		{
			if(write_from_client(sock, bufdup, cli_addr))
			{
				printf("requested file exists\n");
				continue;
			}
		}

		else if(!strcmp(token, "get"))
		{
			if(read_to_client(sock, bufdup, cli_addr))
			{
				printf("FILE DOES NOT EXIST\n");
				continue;
			}
		}

		else if(!strncmp(buffer, "list", 2))
		{
			system("ls > ls_tmp.txt"); //write ls results to temp file
			char file_name[] = "ls_tmp.txt";
			if(read_to_client(sock, file_name, cli_addr))
			{
				printf("ERROR executing ls command\n");
				continue;
			}
		}

		else if(!strncmp(buffer, "exit", 4))
		{
			printf("exiting....\n");
			break;
		}

		else 
		{
			/* command was not recognized, repeat it back to client*/
			printf("unrecognized command\n");
			char *comm = strndup(buffer, strlen(buffer)-1);
			strcat(comm, " -- THE PREVIOUS COMMAND IS NOT UNDERSTOOD\n");
			send(sock, comm, strlen(comm)+1, 0);
			continue; 
		}
	}	
	
	close(sock);
}
