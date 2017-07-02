#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <string.h>
#include <time.h>


/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;

void sendExtensions( int);
void sendFile (int);
void receiveFile(int);


void receiveFile ( int sd)
{

  FILE *file;
  char *extension;
  extension = malloc( 100 * sizeof(char));
  memset(extension, 0,100);

  char *result ;
  result = malloc( 100* sizeof(char));
  memset(result,0,100);
  strcpy(result,"result");

  char *final = malloc(100* sizeof(char));
  memset(final,0,100);

  printf("Mai introduceti o data extensia fisierului final...\n");
  scanf("%s",extension);

  //cream numele fisierului final

  sprintf(final,"%s.%s",result,extension);

  file = fopen(final,"ab+");

  int filesize = 0;
  //citim lungimea fisierului
  if(recv ( sd, &filesize, sizeof(int),0) <= 0)
  {
    perror("Eroare la primire lungime mesaj\n");
  }
  

  char *buffer ;
  buffer = malloc(1025 * sizeof(char));

  while(1)
  {
    memset(buffer,0,1024);

    int reading;
    //citim cat putem
    reading = recv(sd,buffer,1024,0);

    //nu am citit nimic => am terminat de citit
    if(reading == 0)
      break;
    if(reading < 0)
      perror("Eroare la primire mesaj\n");
    
    //scriem in fisier
    if(fwrite(buffer,sizeof(char), reading,file) <= 0)
        perror("Eroare la scriere in fisier\n");
    //cat mai avem de citit
    filesize -= reading;
    //nu mai avem de citit => terminam
    if(filesize == 0)
      break;
  }

  fclose(file);
}


void sendFile(int sd)
{
  char *filename;
  filename = malloc(100*sizeof(char));
  memset(filename,0,100);

  printf("Va rog introduceti numele fisierului din directorul curent...\n");
  scanf("%s", filename);

  FILE *file;
  file = fopen(filename,"rb+");

  //aflam numarul de bytes
  fseek(file,0,SEEK_END);
  int nrOfBytes;
  nrOfBytes = ftell(file);
  fseek(file,0,SEEK_SET);

  int bytesSent;

  //scriem numarul de bytes
  if(send(sd,&nrOfBytes, sizeof(int),0) < 0)
    perror("Eroare \n");

  fflush(NULL);
  while(1)
  {
    char *buffer = malloc( 1024 * sizeof(char));
    memset(buffer,0,1024);

    int reading;

    //citim in chunk de 1024 bytes
    reading = fread(buffer,sizeof(char), 1024, file);

    if (reading < 0)
      perror("Eroare la trimitere");
    nrOfBytes -= reading;
 
    while(1)
    {
      //trimitem cat putem de fiecare data pana cand am terminat de trimis chunkul curent
      int sending;
      sending = send(sd,buffer,reading,0);
      //printf("Am scris %d bytes, au ramas %d\n", sending,nrOfBytes);

      if(sending < 0)
        perror("Eroare la scriere\n");
      
      reading -= sending;
      if(reading == 0)
        break;
      buffer = buffer + sending;
    }

    //am terminat de citit tot?
    if(nrOfBytes == 0) break;

  }
  fclose(file);free(filename);
}

void sendExtensions(int sd)
{
  char *initial = malloc(100 * sizeof(char));
  memset(initial,0,100);

  char *final = malloc( 100 * sizeof(char));
  memset(final,0,100);

  int line = 1;

  while(1)
  {
    if(recv(sd,initial,100,0) < 0)
      perror("Eroare primire arg 1: ");

    if(strcmp(initial,"End.") == 0)
      {
        printf("Numele extensiilor au fost primite. Alegeti un numar.\n");
        break;

      }

    if(recv(sd,final,100,0) < 0)
      perror("Eroare primire arg 2:");

    printf("%d. %s %s\n", line, initial, final);

    line++;

  }

  scanf("%d", &line);

  if(send(sd, &line, sizeof(int),0) < 0)
    perror("Eroare trimitere numar linie:");

  int answer ;

  //primim raspunsul daca extensiile au fost gasite
  if(recv ( sd, &answer, sizeof(int),0) < 0)
  {
    perror("Eroare la primire raspuns\n");
  }

  if(answer == 0)
  {
    //extensiile nu au fost gasite -> intrerupem comunicarea
    printf("Extensiile nu au fost gasite...Programul acum se va termina...");
    free(initial);
    free(final);
    close(sd);
    exit(0);
  }
  free(initial);
  free(final);
}

int main (int argc, char *argv[])
{
  int sd;			// descriptorul de socket
  struct sockaddr_in server;	// structura folosita pentru conectare 
  		// mesajul trimis
  int nr=0;
  char buf[10];

  /* exista toate argumentele in linia de comanda? */
  if (argc != 3)
    {
      printf ("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
      return -1;
    }

  /* stabilim portul */
  port = atoi (argv[2]);

  /* cream socketul */
  if ((sd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("Eroare la socket().\n");
      return errno;
    }

  /* umplem structura folosita pentru realizarea conexiunii cu serverul */
  /* familia socket-ului */
  server.sin_family = AF_INET;
  /* adresa IP a serverului */
  server.sin_addr.s_addr = inet_addr(argv[1]);
  /* portul de conectare */
  server.sin_port = htons (port);
  
  /* ne conectam la server */
  if (connect (sd, (struct sockaddr *) &server,sizeof (struct sockaddr)) == -1)
      {
        perror ("[client]Eroare la connect().\n");
        return errno;
      }

  sendExtensions(sd);
  sendFile(sd);
  receiveFile(sd);
  /* inchidem conexiunea, am terminat */
  close (sd);
}
