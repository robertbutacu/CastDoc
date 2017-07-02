
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <arpa/inet.h>
/* portul folosit */
#define PORT 2081

/* codul de eroare returnat de anumite apeluri */
extern int errno;

typedef struct thData{
	int idThread; //id-ul thread-ului tinut in evidenta de acest program
	int cl; //descriptorul intors de accept
}thData;

static void *treat(void * arg);
char* receiveExtensions(void *, char *, char *);
char* receiveFile(void *, char*,char *, char *);
int sendMyFile(void *, char *);

int sendMyFile(void *arg, char *filename)
{
	struct thData tdl;
	tdl = *((struct thData*)arg);
	int bytesToSend = 0;


	FILE *fileToSend;

	fileToSend = fopen(filename,"ab+");

	fseeko(fileToSend,0,SEEK_END);
	bytesToSend = ftello(fileToSend);
	fseeko(fileToSend,0,SEEK_SET);

	//scriem cati bytes vom trimite
	if(send(tdl.cl,&bytesToSend,sizeof(bytesToSend),0) <= 0)
		{perror("Eroare \n");}

	int bytesRead;

	char *buffer;
	buffer = malloc(1024 * sizeof(char));
	while(1)
	{

		memset(buffer,0,1024);
		
		//citim din fisier
		bytesRead = fread(buffer,sizeof(char),1024,fileToSend);

		//daca nu mai am citit nimic, am termiant
		if(bytesRead == 0) break;

		if(bytesRead < 0) perror("Eroare la citire\n");

		//aflam cati bytes au ramas de citit
		bytesToSend -= bytesRead;

		while(1)
		{
			//trimitem , pe rand, cati bytes putem
			int bytesSent;
			bytesSent = send(tdl.cl,buffer,bytesRead,0);
			if(bytesSent < 0)
				perror("Eroare la trimitere fisier\n");
			//aflam cati bytes au ramas de trimis "runda" aceastsa

			bytesRead -= bytesSent;
			if(bytesRead == 0)
				break;

			//repozitionam caractere pe care le vom trimite in buff
			buffer +=bytesSent;

		}
		//daca nu mai avem nimic de trimis, terminam
		if(bytesToSend == 0)
			break;
		
	}

	fclose(fileToSend);
	free(filename);
}

char* receiveFile(void *arg, char *command, char *initialExtension, char *finalExtension)
{
	struct thData tdl;
	tdl = *((struct thData*)arg);

	char *filename = malloc(100 * sizeof(char));
	memset(filename,0, 100);

	//construim numele fisierului
	strcpy(filename,"temp");

	sprintf(filename,"%s.%s",filename,initialExtension);

	//cream fisierul
	FILE *initialFile;

	initialFile = fopen(filename,"ab+");
	fseek(initialFile,0,SEEK_SET);

	//incepem primirea fisierului

	int bytesLeft;

	//lungimea fisierului
	if(recv(tdl.cl,&bytesLeft,sizeof(int),0) < 0)
	{
		perror("Eroare la citire marime fisier\n");
	}

	//printf("%d", bytesLeft);

	char *buffer = malloc( 1024 * sizeof(char));
	memset(buffer,0,1024);
	fflush(NULL);
	while(1)
	{
		int bytesReceived;
		bytesReceived = recv(tdl.cl, buffer, 1024,0);
		//am citit cati bytes am putut

		if(bytesReceived < 0)
			perror("Eroare la citire fisier\n");

		int writing;
		//scriere in fisier
		if((writing = fwrite(buffer,sizeof(char),bytesReceived,initialFile)) < 0)
		{
			perror("Eroare la scriere in fisier\n");
		}
		memset(buffer,0,1024);

		//bytes ramasi
		bytesLeft -=bytesReceived;
		//printf("Am primit %d bytes, au ramas %d bytes.\n",bytesReceived,bytesLeft);

		//daca au ramas 0 bytes, terminam
		if(bytesLeft == 0) break;
		


	}
	memset(buffer,0,1024);
	strcpy(buffer,"temp");

	char *finalFileName ;
	finalFileName = malloc(100* sizeof(char));
	memset(finalFileName,0,100);



	//cream numele fisierului final
	sprintf(finalFileName,"%s.%s",buffer, finalExtension);
	fclose(initialFile);

	pid_t pid;
	pid = fork();
	if(pid == 0)
	{
		if(strcmp(command,"pdf2txt") == 0)
		{
			execl("/usr/bin/pdf2txt","pdf2txt","-o",finalFileName,filename,NULL);
		}
		else
		{
			char *commandPath = malloc( 100 * sizeof(char));
			memset(commandPath,0,100);

			sprintf(commandPath,"/usr/bin/%s",command);

			execl(commandPath, command, filename, finalFileName,NULL);
		}
	}
	else
		{	
			int status;
			wait(&status);

			return finalFileName;
		}
}



char* receiveExtensions( void *arg, char *initialExtension, char *finalExtension)
{
	struct thData tdl;
	tdl = *((struct thData*)arg);

	char *message = malloc(100 * sizeof(char));
	memset(message,0,100);

	FILE *extensionsName;

	extensionsName = fopen("conversions.txt","r");

	char *argument1 = malloc(100 * sizeof(char));
	memset(argument1,0,100);

	char *argument2 = malloc( 100 * sizeof(char));
	memset(argument2,0,100);

	char *command = malloc(100 * sizeof(char));
	memset(command,0,100);

	while(1)
	{
		int match = fscanf(extensionsName,"%s %s %s", argument1,argument2, command);

		if (match == EOF)
			break;
		if(match < 0)
			perror("Eroare match:");

		if(send(tdl.cl, argument1,100,0) < 0)
			perror("Eroare arg");

		if(send(tdl.cl, argument2,100, 0) < 0)
			perror("Eroare arg");
	}

	strcpy(argument1,"End.");

	if(send(tdl.cl, argument1,100,0) < 0)
		perror("Eroare trimitere final:");

	int commandNr;

	fseek(extensionsName,0,SEEK_SET);

	if( recv(tdl.cl, &commandNr, sizeof(int),0) < 0)
		perror("Eroare primire nr comanda");
	int found = 0;

	int line = 1;
	while(1)
	{
		//vom verifica daca citim exact 3 argumente
		int match;

		match = fscanf(extensionsName, "%s %s %s",initialExtension,finalExtension,command);

		//am ajuns la finalul fisierului
		if(match == EOF)
			break;

		
		//nu au fost matchuite 3 argumente
		if(match != 3)
			perror("Eroare la citire din fisier\n");

		//verificam daca extensiile au fost gasite
		if(line == commandNr)
			{
				found = 1;
				if ( send(tdl.cl, &found, sizeof(int),0) < 0)
				{
					perror("Eroare la trimitere\n");
				}
				return command;
			}
		line++;
	}


	fclose(extensionsName);
	found = 0;
	//scriem ca nu a fost gasit
	if( send(tdl.cl, &found, sizeof(int),0) < 0)
	{
		perror("Eroare la trimitere\n");
	}
	free(argument1);free(argument2);
	strcpy(command,"Fileextensionunavailable");
	return command;


}

int main ()
{
	struct sockaddr_in server;	// structura folosita de server
	struct sockaddr_in from;	
	int sd;		//descriptorul de socket 
	int pid;
	pthread_t th[100];    //Identificatorii thread-urilor care se vor crea
	int i=0;
	

	/* crearea unui socket */
	if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
			{
				perror ("[server]Eroare la socket().\n");
				return errno;
			}
	/* utilizarea optiunii SO_REUSEADDR */
	int on=1;
	setsockopt(sd,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on));
	
	/* pregatirea structurilor de date */
	bzero (&server, sizeof (server));
	bzero (&from, sizeof (from));
	
	/* umplem structura folosita de server */
	/* stabilirea familiei de socket-uri */
		server.sin_family = AF_INET;	
	/* acceptam orice adresa */
		server.sin_addr.s_addr = htonl (INADDR_ANY);
	/* utilizam un port utilizator */
		server.sin_port = htons (PORT);
	
	/* atasam socketul */
	if (bind (sd, (struct sockaddr *) &server, sizeof (struct sockaddr)) == -1)
			{
				perror ("[server]Eroare la bind().\n");
				return errno;
			}

	/* punem serverul sa asculte daca vin clienti sa se conecteze */
	if (listen (sd, 2) == -1)
			{
				perror ("[server]Eroare la listen().\n");
				return errno;
			}
	/* servim in mod concurent clientii...folosind thread-uri */
	while (1)
			{
				int client;
				thData * td; //parametru functia executata de thread     
				int length = sizeof (from);

				printf ("[server]Asteptam la portul %d...\n",PORT);
				fflush (stdout);

				/* acceptam un client (stare blocanta pina la realizarea conexiunii) */
				if ( (client = accept (sd, (struct sockaddr *) &from, &length)) < 0)
					 {
						 perror ("[server]Eroare la accept().\n");
						 continue;
					 }
	
				/* s-a realizat conexiunea, se astepta mesajul */
		
				/* int idThread; //id-ul threadului
				 int cl; //descriptorul intors de accept
*/
				 td=(struct thData*)malloc(sizeof(struct thData));	
				 td->idThread=i++;
				 td->cl=client;

				 pthread_create(&th[i], NULL, &treat, td);	      
				
			}//while    
};				
static void *treat(void * arg)
{		
		struct thData tdL; 
		tdL= *((struct thData*)arg);	
		fflush (stdout);		 

		char *command;
		command = malloc(100 * sizeof(char));
		memset(command,0,100);

		char *initialExtension = malloc(100 * sizeof(char));
		memset(initialExtension,0,100);

		char *finalExtension = malloc( 100 * sizeof(char));
		memset(finalExtension,0, 100);

		char *fileName;
		fileName = malloc(100 * sizeof(char));
		memset(fileName, 0 ,100);

		pthread_detach(pthread_self());		
		//pastram commanda, si aflam daca exista extensiile
		strcpy(command,receiveExtensions((struct thData*)arg, initialExtension, finalExtension));


		//incheiem comunicatia daca nu comanda nu exista, altfel primim fisierul
		if(strcmp(command,"Fileextensionunavailable") != 0)
			{
				int result;
				strcpy(fileName,receiveFile((struct thData*)arg, command, initialExtension, finalExtension));
				result = sendMyFile((struct thData*)arg, fileName);
			}
		else
		{
			free(command);
			free(fileName);
			close((intptr_t)arg);
			return (NULL);

		}
		/* am terminat cu acest client, inchidem conexiunea */
		free(command);
		free(initialExtension);
		free(finalExtension);
		free(fileName);
		close ((intptr_t)arg);
		return(NULL);	
			
};
