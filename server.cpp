#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
//Libreria de hilos
#include <pthread.h>

#include <vector>
#include <sstream>
#include <fstream>

using namespace std;



void error(const char *msg)
{
  perror(msg);
  exit(1);
}

//Variable globales
int sockfd, newsockfd, portno;
socklen_t clilen;
char buffer[1024];
int n;

//GENERIC RESPONSE FORMAT
const char HEADERS_ERROR[] = "HTTP/1.1 404 OK\r\nContent-Type: ";
const char HEADERS_OK[] = "HTTP/1.1 200 OK\r\nContent-Type: ";
const char HEADERS_LENGTH[] = "Content-Length: ";
const char HEADERS_END[]= "\r\n\r\n";
const char PATH[] = "mi_web";

//Funciones hilo
void* entradaMensaje(void*);
void* salidaMensaje(void*);

//Funciones personalizadas
vector<string> getAllTokens(string);

//HTTP functions
int fileStreamSize(fstream&);
void sendFile(fstream&);
void sendHeaders(const char* ,fstream&);

int main(int argc, char *argv[])
{
  //Creacion de la estructura de hilo
  pthread_t entrada;
  pthread_t salida;

  struct sockaddr_in serv_addr, cli_addr;

  sockfd = socket(AF_INET, SOCK_STREAM, 0);

  if (sockfd < 0)
  error("ERROR NO se puede abrir el socket");

  bzero((char *) &serv_addr, sizeof(serv_addr));
  portno = 8080;
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;
  serv_addr.sin_port = htons(portno);
  bool err = false;
  int maxRetry = 100;
  while (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0){
    printf("retrying(%d) on port %d\n",(101-(maxRetry--)),portno);
    if(!maxRetry)
    break;
    sleep(1);
  }
  if(!maxRetry){
    error("ERROR en asociar datos a conexion");
  }
  cout << "Servidor iniciado. Esperando conexiones..." << endl;
  listen(sockfd,5);

  clilen = sizeof(cli_addr);
  newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
  if (newsockfd < 0)
  error("ERROR en aceptar");
  cout << "Cliente conectado" << endl;

  //Creación del hilo
  pthread_create( &entrada, NULL, entradaMensaje, (void*) NULL );
  pthread_create( &salida, NULL, salidaMensaje, (void*) NULL );
  while (1);
  /*
  close(newsockfd);
  close(sockfd);
  */
  return 0;
}

void* entradaMensaje(void*) {
  while (1) {
    bzero(buffer,1024);
    n = read(newsockfd,buffer,1023);
    if (n < 0) error("ERROR reading from socket");
    //PRINTS REQUEST FROM BROWSER
    cout << "MENSAJE RECIBIDO:\n" << buffer << endl;

    //Parse request
    vector<string> tokens = getAllTokens(buffer);

    //INITIALIZE FILE
    stringstream ss;
    if(tokens.size() >= 3){
      fstream file;

      if(tokens[0] == "GET"){
        ss << PATH << tokens[1];
        file.open (ss.str().c_str(), fstream::in);
      }else if(tokens[0] == "POST"){
        ss << PATH << tokens[1];
        ofstream archivo_salida(ss.str().c_str());
        archivo_salida << tokens[3] << endl;
        archivo_salida.close();
        cout << "response post";
      }else if(tokens[0] == "PUT"){
        ss << PATH << tokens[1];
        ofstream file1;
        file1.open(ss.str().c_str(),std::ios_base::app);
        for(int i =3; i<= tokens.size();i++){
          if( tokens[i].compare("Content-Type")==0 ){
            break;
          }else{
            file1 << tokens[i];
          }
        }
        file1.close();
      }

      if(file.is_open()){
        sendHeaders(tokens[1].c_str(),file);
        sendFile(file);
        cout << "response sent";
      }else{
        cout << "404 NOT FOUND COULDN'T OPEN FILE" << endl;

        if(tokens[1] == "/"){
          ss << PATH << "index.html";
          file.open(ss.str().c_str(),fstream::in);
          sendHeaders(ss.str().c_str(),file);
          sendFile(file);
        }else{
          ss << PATH << "/error.html";
          file.open(ss.str().c_str(), fstream::in);
          sendHeaders("/error.html",file);
          sendFile(file);
        }
      }
      file.close();
    }
  }
}

void* salidaMensaje(void*) {
  while (1) {
    while(1){
      sleep(30);
    }
    cout << "Mensaje salida: ";
    bzero(buffer,1024);
    fgets(buffer,1023,stdin);
    n = write(newsockfd,buffer,strlen(buffer));
    if (n < 0)
    error("ERROR writing to socket");
  }
}

vector<string> getAllTokens(string toTokenize){
  vector<string> v;
  string::iterator stringIT = toTokenize.begin();
  for(string::iterator i = toTokenize.begin(); i != toTokenize.end(); i++){
    if(*i == ' '){
      v.push_back(string(stringIT,i));
      stringIT=i+1;
    }
  }
  return v;
}

int fileStreamSize(fstream& file){
  if(file){
    file.seekg(0,file.end);
    int fileSize = file.tellg();
    file.seekg(0, file.beg);
    cout << fileSize << endl;
    return fileSize;

  }
  return -1;
}

void sendHeaders(const char* contentType, fstream& file){
  stringstream response;
  int OUTPUT_LENGTH = fileStreamSize(file);
  if(OUTPUT_LENGTH <= 0){
    response << HEADERS_ERROR;
    fstream errorFile ("mi_web/error.html",fstream::in);
    OUTPUT_LENGTH = fileStreamSize(errorFile);
    errorFile.close();
  }else{
    response << HEADERS_OK;
  }

  //FORMATO MIME
  if(strstr(contentType,".htm")){
    response << "text/html";
  }else if (strstr(contentType,".css")){
    response << "text/css";
  }else if (strstr(contentType,".ico")){
    response << "image/x-icon";
  }else if(strstr(contentType,".pdf")){
    response << "application/pdf";
  }else if(strstr(contentType,".js")){
    response << "application/js";
  }else{
    response << "text/plain";
  }

  response << "\r\n" << HEADERS_LENGTH << OUTPUT_LENGTH << HEADERS_END;
  n= write(newsockfd, response.str().c_str(),response.str().length());
  if (n < 0)
  error("ERROR writing HEADERS to socket");
}
void sendFile(fstream& file){
  char* bufferData;
  int bufferSize = fileStreamSize(file);
  if(bufferSize > 0){
    bufferData = new char[bufferSize];
    file.read(bufferData, bufferSize);
  }else{
    //SEND ERROR HTML FILE
    stringstream ss;
    ss << PATH << "/error.html";
    file.close();
    file.open(ss.str().c_str(), fstream::in);
    bufferSize = fileStreamSize(file);
    bufferData = new char[bufferSize];
    file.read(bufferData, bufferSize);
  }
  n = write(newsockfd, bufferData, bufferSize);
  delete[] bufferData;
  if(n < 0)
  error("ERROR writing FILE to socket");
}
