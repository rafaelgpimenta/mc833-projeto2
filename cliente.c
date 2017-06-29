#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#define SERVER_PORT 12345
#define MAX_LINE 32

typedef enum direcao {N,S,L,O} direcao;
typedef enum { seguranca, entreterimento, conforto } tipoServico;
typedef struct pacote{
    time_t timestamp;
    double pos; // -30 a 31m (cruzamento nas posições 0 e 1)
    int velocidade; // de 0 a 33m/s
    unsigned short int tam; //de 1 a 30m
    direcao d; //norte, sul, leste, oeste
    tipoServico tipo;
    char msg[MAX_LINE];
} pacote;

pacote gerarCarro(){
  short int pos;
  int velocidade, tam;
  direcao d;
  pacote p;
  unsigned int randval;
  FILE *f;

  f = fopen("/dev/random", "r");
  fread(&randval, sizeof(randval), 1, f);
  fclose(f);

  srand( randval );
  d = rand() % 4;

  if (d == N || d == L)
    pos = -300;
  else
    pos = 301;

  velocidade = 5 + (rand() % 29);
  printf("VELOCIDADE: %d\n", velocidade);
  tam = 1 + (rand() % 30);

  p.pos = pos;
  p.velocidade = velocidade;
  p.tam = tam;
  p.d = d;
  p.tipo = seguranca;
  memset(p.msg, '\0', sizeof(p.msg));
  time ( &(p.timestamp) );
  return p;
}
int main(int argc, char * argv[])
{
    struct hostent *host_address;
    struct sockaddr_in socket_address;
    unsigned int i = 0;
    char *host;
    pacote buf;
    int s;
    int len;

    int pid;
    //so envia pacote colisao se nao passou pelo cruzamento
    for (int i = 0; i < 50; i++) {
      pid = fork();
      if (pid == 0)
        break;
    }
	 /* verificação de argumentos */
	 if (argc != 2) {
        printf("Numero incorreto de argumentos\n");
        return -1;
    }
    host = argv[1];
    /* tradução de nome para endereço IP */
    host_address = gethostbyname(host);
    if(host_address == NULL) {
        printf("Nao foi possivel resolver o nome\n");
        return -1;
    }

    printf("%s = ", host_address->h_name);
    while(host_address->h_addr_list[i] != NULL) {
        printf("%s ", inet_ntoa( *( struct in_addr*)( host_address->h_addr_list[i])));
        i++;
    }
    printf("\n");
    /* criação da estrutura de dados de endereço */
    bzero((char *)&socket_address, sizeof(socket_address));
    socket_address.sin_family = AF_INET;
    socket_address.sin_port = htons(SERVER_PORT);
    bcopy(host_address->h_addr_list[0], (char *) &socket_address.sin_addr.s_addr, host_address->h_length);


    /* criação de socket ativo*/
    s = socket(AF_INET, SOCK_STREAM, 0);
    if(s == -1) {
        printf("Erro na alocacao do socket\n");
        return -1;
    }
    /* estabelecimento da conexão */
    len = sizeof(socket_address);
    if(connect(s, (struct sockaddr *) &socket_address, len) == -1) {
        printf("Erro no estabelecimento da conexao\n");
        return -1;
    }

    buf = gerarCarro();
    while (1) {
      time_t antes,depois;
      time ( &(antes) );
      send(s, (char*)&buf, sizeof(buf), 0);
      recv(s, (char*)&buf, sizeof(buf), 0);
      time ( &(depois) );
      printf("Eco: %s\n", buf.msg);

      if (buf.msg == "freie") {
        buf.velocidade = 0;
      }

      if (buf.d == N || buf.d == L) {

        buf.pos += buf.velocidade*(0.01);
        // printf("Direcao: %d\n", buf.d);
        // printf("pos traseira: %f\n", buf.pos - (double)buf.tam);
        // printf("velocidade %d\n", buf.velocidade);
        if (buf.pos - (double)buf.tam > 0) {
          break;
        }

      }
      else{
        buf.pos -= buf.velocidade*(0.01);
        // printf("Direcao: %d\n", buf.d);
        // printf("pos traseira: %f\n", buf.pos + (double)buf.tam);
        // printf("velocidade %d\n", buf.velocidade);
        if (buf.pos + (double)buf.tam < 0) {
          break;
        }
      }
    }

    close(s);
    return 0;

}
