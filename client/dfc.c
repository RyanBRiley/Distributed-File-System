#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <openssl/md5.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#define MAXBUFSIZE 256

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


struct file_fragment
{
	char frag_command[4][64];
	off_t offset[4];
	size_t size[4];
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

int calc_MD5_sum(char *file_name)
{
	unsigned char hash[MD5_DIGEST_LENGTH];
	FILE *fp = fopen(file_name, "rb");
	MD5_CTX mdContext;
	int bytes_read;
	unsigned char buffer[1024];
	char num;
	int mod;
	int i;

	if(fp == NULL)
	{
		printf("MD5 cannot be calculated");
		return 0;
	}
	MD5_Init(&mdContext);
	while((bytes_read = fread(buffer, 1, 1024, fp)) != 0)
	{
		MD5_Update(&mdContext, buffer, bytes_read);
	}
	MD5_Final(hash, &mdContext);

	
	return hash[0] % 4;
}

int authenticate(int sock, struct config_struct *c)
{
	char *credentials = malloc(sizeof(char)*(strlen(c->username)+strlen(c->passwd)+1));
	int ack;
	int authenticated = 0;
	
	recv(sock, &ack, sizeof(int), 0);

	sprintf(credentials, "%s %s", c->username, c->passwd);

	send(sock, credentials, MAXBUFSIZE, 0);
	recv(sock, &authenticated, sizeof(int), 0);
	send(sock, &ack, sizeof(int), 0);
	free(credentials);

	if(!authenticated)
	{
		puts("Invalid Username/Password. Please try again.");
	}
	return authenticated;
	
}

int handle_file_transfer(int sock, int fd, int fragment, struct file_fragment *frag, struct config_struct *c)
{
	
	int faccess;
	int nfaccess;
	int ack = 0;
	int sz_file_frag;
	send(sock, frag->frag_command[fragment - 1], strlen(frag->frag_command[fragment - 1]), 0);
	if (!authenticate(sock, c))
	{
		return 0;
	}

	//get back whether or not server can complete command (maybe file already exists)
	recv(sock, &nfaccess, sizeof(int), 0);
	faccess = ntohl(nfaccess);
	
	if(faccess)
	{
		sz_file_frag = (int) frag->size[fragment-1];
		
		send(sock, &sz_file_frag, sizeof(int), 0);
		off_t offset = frag->offset[fragment-1];
		int bytes = sendfile(sock, fd, &offset, frag->size[fragment-1]);

		recv(sock, &ack, sizeof(int), 0);
		if(ack)
		{
			return 0;
		}

	}
	else //server returned 0 for faccess -- cannot complete request
	{
		printf("ERROR: FILE ALREADY EXISTS ON SERVER. CHOOSE ANOTHER FILE\n");
		return 1;
	}
}

int handle_list_request(int sock, char *buffer, struct config_struct *c)
{
	char *command = "list\n";
	int nfile_size;
	int faccess;
	int nfaccess;
	//send command to server
	memset(buffer, 0, sizeof(buffer));
	send(sock, command, strlen(command), 0);
	if (!authenticate(sock, c))
	{
		return 0;
	}
	/*let client know that write is possible*/
	recv(sock, &nfaccess, sizeof(int), 0);
	faccess = ntohl(nfaccess);
	if(!faccess)
	{
		puts("ERROR FETCHING FILE FROM SERVER");
		return 1;
	}

	/*get file size*/
	recv(sock, &nfile_size, sizeof(int), 0);
	int file_size = ntohl(nfile_size);
	
	
	recv(sock, buffer, file_size, 0);
	buffer[file_size-1] = '\0';
	
	return 0;
}

void aggregate_list_results(char *buffer_aggregrate, char *buffer)
{

	char *bufdup = strndup(buffer, strlen(buffer));    //remove new line

	char *token = "";
	
	int i = 0;
	while (token != NULL)
	{
		
		token = strsep(&bufdup, "\n");

		if (i < 2)
		{
			i++;
			continue;
		}
		if ((token != NULL) && (token[0] != ' '))
		{
			strcat(buffer_aggregrate, token);
			strcat(buffer_aggregrate, "\n");
			
		}
		
	}
}

int handle_get_request(int sock, char *command, char *file_name, struct config_struct *c)
{			
	
	int nfile_size;
	int faccess;
	int nfaccess;
	int bytes_recv = 0;
	FILE *fp;
	char fbuffer[MAXBUFSIZE];


	if(access(file_name, F_OK) != -1) //file exists
	{
		puts("FILE ALREADY EXISTS");
		return 0;
	}
	else //file does not exist, ok to write
	{
		//send command to server
		send(sock, command, strlen(command), 0);
		if (!authenticate(sock, c))
		{
			return 1;
		}
		/*let client know that write is possible*/
		recv(sock, &nfaccess, sizeof(int), 0);
		faccess = ntohl(nfaccess);
		if(!faccess)
		{
			puts("ERROR FETCHING FILE FROM SERVER");
			return 0;
		}

	

		fp = fopen(file_name, "wb");//open file
		if (fp == NULL)
		{
			printf("error opening file for writing");
			return 1;
		}

		/*get file size*/
		recv(sock, &nfile_size, sizeof(int), 0);
		int file_size = ntohl(nfile_size);
		printf("file size:%d\n",file_size);
	
		/*get file from client in packets, write to file */
		int bytes_remn = file_size;
		while(bytes_remn > 0) 
		{
			bytes_recv = recv(sock, fbuffer, MAXBUFSIZE, 0);
			fwrite(fbuffer, sizeof(char), bytes_recv, fp);
			bytes_remn -= bytes_recv;
			
		}
		fclose(fp);
		return 1;
	}
}

int get(int sock[4], char *command, char *file_name, struct config_struct *c)
{
	int s;
	int p;
	int request_complete = 0;
	char frag_name[4][64];
	char frag_command[64];
	char buffer;

	for (s = 0; s < 4; s++)
	{	
		printf("checking server: %d for file: %s\n", s+1, file_name);
		request_complete = handle_get_request(sock[s], command, file_name, c);
		if(request_complete)
		{
			return 0;
		}
	}

	if(access(file_name, F_OK) != -1) //file exists
	{
		puts("FILE ALREADY EXISTS");
		return 0;
	}

	for(p = 1; p < 5; p++)
	{
		sprintf(frag_name[p-1], ".%s.%d", file_name, p);
		printf("frag_name[%d]: %s\n", p-1, frag_name[p-1]);
	}

	for(p = 1; p < 5; p++)
	{
		sprintf(frag_command, "get %s\n", frag_name[p-1]);
		for (s = 0; s < 4; s++)
		{	
			printf("requesting: %s from server %d\n", frag_name[p-1], s+1);
			request_complete = handle_get_request(sock[s], frag_command, frag_name[p-1], c);
			if(request_complete)
			{
				break;
			}
		}
	}

	FILE *fp = fopen(file_name, "wb");//open file
	if (fp == NULL)
	{
		printf("error opening file for writing");
		return 1;
	}
	for(p = 1; p < 5; p++)
	{
		char sys_command[64];
		sprintf(sys_command, "rm %s", frag_name[p-1]);
		FILE *fragfp = fopen(frag_name[p-1], "a+");//open file
		if (fragfp == NULL)
		{
			printf("error opening file for reading");
			return 1;
		}
		while(!feof(fragfp))
		{
			fread(&buffer, sizeof(char), 1, fragfp);
			fwrite(&buffer, sizeof(char), 1 , fp);
		}
		fclose(fragfp);
		system(sys_command);
	}
	fclose(fp);		
	return 0;
}

int put(int sock[4], char *file_name, struct config_struct *c)
{
	struct stat file_stat_struct;
	int nfile_size;
	int file_size;
	FILE *fp;
	int fd;
	int bytes_read = 0;
	struct file_fragment *frag = malloc(sizeof(struct file_fragment));
	
	int i;
	off_t offset;

	int hash_mod = calc_MD5_sum(file_name);
	

	fd = open(file_name, O_RDONLY); //open file
	if(fd == -1) //check if file exists
	{
		puts("FILE DOES NOT EXIST. CHOOSE ANOTHER FILE");
		return 1; 
	} 
	

	fstat(fd, &file_stat_struct);
	
	
	for(i = 0; i < 4; i++)
	{
		sprintf(frag->frag_command[i], "put .%s.%d\n",file_name,i+1);
		frag->offset[i] = i * (file_stat_struct.st_size/4);
		if(i < 3)
		{
			frag->size[i] = file_stat_struct.st_size/4;
		}
		else
		{
			frag->size[i] = file_stat_struct.st_size - (3 * (file_stat_struct.st_size/4));
		}
		
	}
	
	switch(hash_mod)
	{
		case 0:
			handle_file_transfer(sock[0], fd, 1, frag, c);
			handle_file_transfer(sock[0], fd, 2, frag, c);
			handle_file_transfer(sock[1], fd, 2, frag, c);
			handle_file_transfer(sock[1], fd, 3, frag, c);
			handle_file_transfer(sock[2], fd, 3, frag, c);
			handle_file_transfer(sock[2], fd, 4, frag, c);
			handle_file_transfer(sock[3], fd, 4, frag, c);
			handle_file_transfer(sock[3], fd, 1, frag, c);
			break;
		
		case 1:
			handle_file_transfer(sock[0], fd, 4, frag, c);
			handle_file_transfer(sock[0], fd, 1, frag, c);
			handle_file_transfer(sock[1], fd, 1, frag, c);
			handle_file_transfer(sock[1], fd, 2, frag, c);
			handle_file_transfer(sock[2], fd, 2, frag, c);
			handle_file_transfer(sock[2], fd, 3, frag, c);
			handle_file_transfer(sock[3], fd, 3, frag, c);
			handle_file_transfer(sock[3], fd, 4, frag, c);
			break;
		
		case 2: 
			handle_file_transfer(sock[0], fd, 3, frag, c);
			handle_file_transfer(sock[0], fd, 4, frag, c);
			handle_file_transfer(sock[1], fd, 4, frag, c);
			handle_file_transfer(sock[1], fd, 1, frag, c);
			handle_file_transfer(sock[2], fd, 1, frag, c);
			handle_file_transfer(sock[2], fd, 2, frag, c);
			handle_file_transfer(sock[3], fd, 2, frag, c);
			handle_file_transfer(sock[3], fd, 3, frag, c);
			break;
		
		case 3:
			handle_file_transfer(sock[0], fd, 2, frag, c);
			handle_file_transfer(sock[0], fd, 3, frag, c);
			handle_file_transfer(sock[1], fd, 3, frag, c);
			handle_file_transfer(sock[1], fd, 4, frag, c);
			handle_file_transfer(sock[2], fd, 4, frag, c);
			handle_file_transfer(sock[2], fd, 1, frag, c);
			handle_file_transfer(sock[3], fd, 1, frag, c);
			handle_file_transfer(sock[3], fd, 2, frag, c);
			break;
	}

	close(fd);
	return 0;
}


int list(int sock[4], struct config_struct *c)
{


	char buffer_1[MAXBUFSIZE];
	char buffer_2[MAXBUFSIZE];
	char buffer_3[MAXBUFSIZE];
	char buffer_4[MAXBUFSIZE];

	char buffer_aggregrate[1024];
	char list_buffer[1024];

	char fragments[128][64];
	char uniq_frags[128][64];

	memset(buffer_1, 0, sizeof(buffer_1));
	memset(buffer_2, 0, sizeof(buffer_2));
	memset(buffer_3, 0, sizeof(buffer_3));
	memset(buffer_4, 0, sizeof(buffer_4));

	memset(buffer_aggregrate, 0, sizeof(buffer_aggregrate));
	memset(list_buffer, 0, sizeof(list_buffer));

	handle_list_request(sock[0], buffer_1, c);
	handle_list_request(sock[1], buffer_2, c);
	handle_list_request(sock[2], buffer_3, c);
	handle_list_request(sock[3], buffer_4, c);

	aggregate_list_results(buffer_aggregrate, buffer_1);
	aggregate_list_results(buffer_aggregrate, buffer_2);
	aggregate_list_results(buffer_aggregrate, buffer_3);
	aggregate_list_results(buffer_aggregrate, buffer_4);

	int i = 0;
	char *bufdup = strndup(buffer_aggregrate, strlen(buffer_aggregrate));    //remove new line

	char *token = "";

	while (token != NULL)
	{
	
		token = strsep(&bufdup, "\n");

		if ((token != NULL) && (token[0] != '.'))
		{
			strcat(list_buffer, token);
			strcat(list_buffer, "\n");
		}
		else if ((token != NULL) && (token[0] == '.'))
		{
			strcpy(fragments[i], token);
			i++;
		}
	}
	list_buffer[strlen(list_buffer) - 1] = '\0';


	int x = 0;
	
	int uniq_count = 0;
	while (x < i)
	{
		int u = 0;
		int uniq = 1;
		while(u < uniq_count)
		{
			if (!strncmp(fragments[x]+1, uniq_frags[u], strlen(fragments[x]+1)-2))
			{
				uniq = 0;
				break;
			}
			u++;
		}
		if(uniq)
		{
			strncpy(uniq_frags[u], fragments[x]+1, strlen(fragments[x]+1)-2);
			uniq_count++;
		}
		x++; 

	}

	x = 0;

	while(x < uniq_count)
	{
	
		int all_frags[4];
		int init;
		for(init = 0; init < 4; init++)
		{
			all_frags[init] = 0;
		}

		int y = 0;
		while(y < i)
		{
		
			if(!strncmp(uniq_frags[x], fragments[y] + 1, strlen(uniq_frags[x])))
			{
			
				all_frags[atoi(&strrchr(fragments[y], '.')[1]) -1 ] = 1;
				
			}
			y++;
		}
		
		if(all_frags[0] && all_frags[1] && all_frags[2] && all_frags[3])
		{
			
			char complete[64];
			sprintf(complete, "%s\n", uniq_frags[x]);
			strcat(list_buffer, complete);
		}
		else
		{
			
			char incomplete[64];
			sprintf(incomplete, "%s [incomplete]", uniq_frags[x]);
			strcat(list_buffer, incomplete);
		}
		x++;
	}

	printf("%s\n\n", list_buffer);

	return 0;
	
}

int main (int argc, char * argv[])
{

	int sock[4];      // Array of sockets, one for each server
	int connectfd[4]; // Array of connection descriptors, one for each connection (server)
	char *command = malloc(sizeof(char)*MAXBUFSIZE);
	char msg[MAXBUFSIZE];	
	
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
		if(!strcmp(token, "put"))
		{
			if(put(sock, comdup, c)) //get file from server
			{
				printf("ERROR executing put command\n");
			}	
		}
		else if(!strcmp(token, "get")) 
		{
			if(get(sock, command, comdup, c)) //get file from server
			{
				printf("FILE DOES NOT EXIST ON SERVER. PLEASE CHOOSE A NEW FILE\n");
			}
		}
		else if(!strcmp(token, "list"))
		{
			if(list(sock, c)) // get ls from server
			{
				printf("ERROR executing list command\n");
			}
		}
		else if(!strcmp(token, "exit"))
		{
			/* send server exit command, exit */
			send(sock[0], command, strlen(command), 0);
			send(sock[1], command, strlen(command), 0);	
			send(sock[2], command, strlen(command), 0);	
			send(sock[3], command, strlen(command), 0);		
			printf("exiting...\n");
			break;
		}
		else
		{
			/*unrecognized command.. let server handle this*/
			send(sock[0], command, strlen(command), 0);
			int msglen = 0;
			recv(sock[0], &msglen, sizeof(int), 0);
			char *msg = malloc(sizeof(char) * msglen);
			recv(sock[0], msg, msglen, 0);
			puts(msg);
			free(msg);
		}

	}
	
	close(sock[0]);
	close(sock[1]);
	close(sock[2]);
	close(sock[3]);
	free(c);
	return 0;
}
	