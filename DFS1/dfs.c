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
int read_to_client(int sock, char *file_name, struct sockaddr_in remote) 
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
		sendto(sock, &nfaccess, sizeof(int)+1, 0, (struct sockaddr *) &remote, sizeof(remote));
		return 1;
	}
	/*tell client that file was able to be opened */
	faccess = 1;
	nfaccess = htonl(faccess);
	sendto(sock, &nfaccess, sizeof(int)+1, 0, (struct sockaddr *) &remote, sizeof(remote));

	/*get file size and send it to client */
	fseek(fp, 0, SEEK_END);
	int file_size=ftell(fp);
	int nfile_size = htonl(file_size);
	sendto(sock, &nfile_size, sizeof(int)+1, 0, (struct sockaddr *) &remote, sizeof(remote));

	/*go back to beginning of file, read it into the buffer, send it client*/
	fseek(fp, 0, SEEK_SET);
	char *fbuffer = malloc(file_size); //allocate buffer
	fread(fbuffer, file_size, 1, fp);
	sendto(sock, fbuffer, strlen(fbuffer)+1, 0, (struct sockaddr *) &remote, sizeof(remote));
	free(fbuffer);
	fclose(fp);
	return 0;
}

/*gets file from client, writes it into local dir*/
int write_from_client(int sock, char *file_name, struct sockaddr_in remote)
{
	int nfile_size;
	int faccess;
	int nfaccess;
	int bytes_recv = 0;
	unsigned int remote_length;
	FILE *fp;
			
	if(access(file_name, F_OK) != -1) //file exists
	{
		/*send client status that file already exists and return*/
		faccess = 0;
		nfaccess = htonl(faccess);
		sendto(sock, &nfaccess, sizeof(int)+1, 0, (struct sockaddr *) &remote, sizeof(remote));
		return 1;
	}
	else //file does not exist, ok to write
	{
		/*let client know that write is possible*/
		faccess = 1;
		nfaccess = htonl(faccess);
		sendto(sock, &nfaccess, sizeof(int)+1, 0, (struct sockaddr *) &remote, sizeof(remote));

		fp = fopen(file_name, "wb");//open file

		/*get file size*/
		recvfrom(sock, &nfile_size, sizeof(int), 0, (struct sockaddr *) &remote, &remote_length);
		int file_size = ntohl(nfile_size);

		char fbuffer[MAXBUFSIZE];
	
		/*get file from client in packets, write to file */
		while (bytes_recv + sizeof(fbuffer) < file_size)
		{
			bzero(fbuffer,sizeof(fbuffer));
			bytes_recv += recvfrom(sock, fbuffer, sizeof(fbuffer), 0, (struct sockaddr *) &remote, &remote_length);
			fwrite(fbuffer, sizeof(fbuffer), 1, fp);
			
		}
		/*write leftover bytes that were incommensurable with sizeof(fbuffer)*/
		bzero(fbuffer,sizeof(fbuffer));
		int t_remain = file_size - bytes_recv;
		recvfrom(sock, fbuffer, t_remain, 0, (struct sockaddr *) &remote, &remote_length);
		fwrite(fbuffer, t_remain, 1, fp);
		fflush(fp);
		fclose(fp);
		return 0;
	}
}

int main (int argc, char * argv[] )
{
	int sock;                           //This will be our socket
	struct sockaddr_in sin, remote;     //"Internet socket address structure"
	unsigned int remote_length;         //length of the sockaddr_in structure
	              	
	char buffer[MAXBUFSIZE];             //a buffer to store our received message
	

	if (argc != 2)
	{
		printf ("USAGE:  <port>\n");
		exit(1);
	}

	/******************
	  This code populates the sockaddr_in struct with
	  the information about our socket
	 ******************/
	bzero(&sin,sizeof(sin));                    //zero the struct
	sin.sin_family = AF_INET;                   //address family
	sin.sin_port = htons(atoi(argv[1]));        //htons() sets the port # to network byte order
	sin.sin_addr.s_addr = INADDR_ANY;           //supplies the IP address of the local machine


	//Causes the system to create a generic socket of type UDP (datagram)
	if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
	{
		printf("unable to create socket");
	}


	/******************
	  Once we've created a socket, we must bind that socket to the 
	  local address and port we've supplied in the sockaddr_in struct
	 ******************/
	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		printf("unable to bind socket\n");
	}
	

	remote_length = sizeof(remote);

	while(1){
		bzero(buffer,sizeof(buffer));
		/*get command from client, parse it*/
		recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *) &remote, &remote_length);
		char *bufdup = strndup(buffer, strlen(buffer)-1);    //remove new line
		char *token = strsep(&bufdup, " ");

		if(!strcmp(token, "put"))
		{
			if(write_from_client(sock, bufdup, remote))
			{
				printf("requested file exists\n");
				continue;
			}
		}

		else if(!strcmp(token, "get"))
		{
			if(read_to_client(sock, bufdup, remote))
			{
				printf("FILE DOES NOT EXIST\n");
				continue;
			}
		}

		else if(!strncmp(buffer, "ls", 2))
		{
			system("ls > ls_tmp.txt"); //write ls results to temp file
			char file_name[] = "ls_tmp.txt";
			if(read_to_client(sock, file_name, remote))
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
			sendto(sock, comm, strlen(comm)+1, 0, (struct sockaddr *) &remote, sizeof(remote));
			continue; 
		}
	}	
	
	close(sock);
}
