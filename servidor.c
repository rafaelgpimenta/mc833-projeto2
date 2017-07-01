#define _POSIX_C_SOURCE 199309L
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <time.h>
#include "params.h"
#include<pthread.h>

#define LISTEN_PORT 12345
#define MAX_PENDING 400
#define MAX_LINE 32

typedef enum direcao {N,S,L,O} direcao;
typedef enum { false, true } bool;
typedef enum { SEGURANCA, ENTRETERIMENTO, CONFORTO } tipoServico;

typedef struct pairTime {
  double inicio;
  double fim;
  int ID;
  int velocidade;
} pairTime;

typedef struct pacote{
  struct timespec timestamp;
  double pos; // -30 a 31m (cruzamento nas posições 0 e 1)
  int velocidade; // de 0 a 33m/s
  unsigned short int tam; //de 1 a 30m
  direcao d; //norte, sul, leste, oeste
  tipoServico tipo;
  char msg[MAX_LINE];
} pacote;

typedef struct cliente{
  pacote p;
  int descritor;
  bool recebeu;
} cliente;

typedef struct cruzamento{
  int descritor;
  direcao d;
} cruzamento;

int infoPeer(int new_s){
  struct sockaddr_storage addr;
  socklen_t lenPeer = sizeof(addr);
  if (getpeername(new_s, (struct sockaddr *) &addr, &lenPeer) == -1) {
    printf("Erro ao gettar o peer name\n");
    return -1;
  }
  struct sockaddr_in *soc = (struct sockaddr_in *)&addr;
  char ipstr[INET_ADDRSTRLEN];
  int port = ntohs(soc->sin_port);
  inet_ntop(AF_INET, &soc->sin_addr, ipstr, sizeof ipstr);
  printf("IP:porta = %s:%d\n", ipstr, port);
  return 0;
}

//ordenado pelo inicio
void insereNS(pairTime (*vetor)[2], int size, pairTime verifica, int seleciona){
  int i;
  for (i = 0; i < size; i++)
    if (vetor[i][seleciona].inicio > verifica.inicio)
      break;

  for (int j = size; j > i; j--)
    vetor[j][seleciona] = vetor[j - 1][seleciona];

  vetor[i][seleciona] = verifica;
}

//carros LO inseridos na ultima posicao
void insereLO(pairTime (*vetor)[2], int size, pairTime verifica, int seleciona){
    vetor[size][seleciona] = verifica;
}

int removeCarro(pairTime (*vetor)[2], int size, int sockfd){
  int i, removeu = 0;
  for (i = 0; i < size; i++)
    if (vetor[i][0].ID == sockfd){
      removeu = 1;
      break;
    }
  for (; i < size - 1; i++)
    vetor[i][0] = vetor[i + 1][0];

  for (i = 0; i < size; i++)
    if (vetor[i][1].ID == sockfd)
      break;

  for (; i < size - 1; i++)
    vetor[i][1] = vetor[i + 1][1];

  return removeu;
}

bool buscarAlteracao (pairTime (*vetor)[2], int* size, pairTime verifica[2]) {
  bool alteracao = true;

  for (int i = 0; i < *size; i++) {
    if (vetor[i][0].ID == verifica[0].ID && vetor[i][0].inicio == verifica[0].inicio) {
        alteracao = false;
    }
  }

  if (alteracao)
    *size -= removeCarro(vetor, *size, verifica[0].ID);

  return alteracao;
}

int evitarColisao(pairTime (*vetor)[2], pairTime (*vetorN)[2], int sizeN, pairTime (*vetorS)[2], int sizeS, int cl, int flag, int sockfd) {
  // carro indo para sul
  for(int i = 0; i < sizeS; i++) {
    // ou o carro indo para o L chega primeiro e depois o vindo do S ou ao contrario em 00
    // se o carro do sul chegar e o carro do leste ja estava
    // se o do leste chegar e o carro do sul ja estava
    if ((vetor[cl][0].inicio <= vetorS[i][flag].inicio && vetor[cl][0].fim >= vetorS[i][flag].fim) ||
        (vetorS[i][flag].inicio <= vetor[cl][0].inicio && vetorS[i][flag].fim >= vetor[cl][0].fim) ||
        (vetor[cl][0].inicio <= vetorS[i][flag].inicio && vetor[cl][0].fim <= vetorS[i][flag].fim) ||
        (vetor[cl][0].inicio >= vetorS[i][flag].inicio && vetor[cl][0].fim >= vetorS[i][flag].fim)) {
          removeCarro(vetorS,sizeS,vetorS[i][flag].ID);
          return 1;
    }

  }

  //verificar se nao vai bater com quem esta indo para o norte 10
  for(int i = 0; i < sizeN; i++) {
    // ou o carro indo para o L chega primeiro e depois o vindo do N ou ao contrario em 10
    // ou o carro do sul chegar e o carro do leste ja estava
    // ou o do leste chegar e o carro do sul ja estava
    if ((vetor[cl][1].inicio <= vetorN[i][flag].inicio && vetor[cl][1].fim >= vetorN[i][flag].fim) ||
        (vetorN[i][flag].inicio <= vetor[cl][1].inicio && vetorN[i][flag].fim >= vetor[cl][1].fim) ||
        (vetor[cl][1].inicio <= vetorN[i][flag].inicio && vetor[cl][1].fim <= vetorN[i][flag].fim) ||
        (vetor[cl][1].inicio >= vetorN[i][flag].inicio && vetor[cl][1].fim >= vetorN[i][flag].fim)) {
          // strcpy(mensagem, "freie");
          // send(sockfd, mensagem, sizeof(mensagem), 0);
          // printf("PARAAAAAAAAA!\n");
          removeCarro(vetorN,sizeN,vetorN[i][flag].ID);
          return 2;
    }
  }
  return 0;
}

void* divirta_se(void* sockfd) {
  char msg[MAX_LINE];
  strcpy(msg, "Sinta-se feliz.");
  send(*(int *)sockfd, msg, sizeof(msg), 0);
  usleep(100000);
  pthread_exit(NULL);
}

void* conforte_se(void* sockfd) {
  char msg[MAX_LINE];
  strcpy(msg, "Sinta-se confortavel.");
  send(*(int *)sockfd, msg, sizeof(msg), 0);
  usleep(100000);
  pthread_exit(NULL);
}

void bateu(int sockfd) {
  char msg[MAX_LINE];
  strcpy(msg, "bateu");
  send(sockfd, msg, sizeof(msg), 0);
}

int main() {
  struct sockaddr_in socket_address;
  pacote buf;
  unsigned int len;
  int s, new_s, i, sockfd, cliente_num, maxfd, nready;
  cliente c[FD_SETSIZE];
  fd_set todos_fds, novo_set;
  // char so_addr[INET_ADDRSTRLEN];

  /* criação da estrutura de dados de endereço */
  bzero((char *)&socket_address, sizeof(socket_address));
  socket_address.sin_family = AF_INET;
  socket_address.sin_port = htons(LISTEN_PORT);
  socket_address.sin_addr.s_addr = INADDR_ANY;

    /* criação de socket passivo */
  s = socket(AF_INET, SOCK_STREAM, 0);
  if(s == -1) {
    printf("Erro na alocacao do socket\n");
    return -1;
  }

	  /* Associar socket ao descritor */
  len = sizeof(socket_address);
  bind(s, (struct sockaddr *) &socket_address, len);

     /* Criar escuta do socket para aceitar conexões */
  if(listen(s, MAX_PENDING) == -1) {
    printf("Erro ao criar escuta para aceitar conexao\n");
    return -1;
  }

    /* aguardar/aceita conexão, receber e imprimir texto na tela, enviar eco */
  maxfd = s;
  cliente_num = -1;
  for (i = 0; i < FD_SETSIZE; i++)
    c[i].descritor = -1;

  FD_ZERO(&todos_fds);
  FD_SET(s, &todos_fds);


  int sizeN = 0, sizeS = 0, sizeL = 0, sizeO = 0;
  /* espera conexao, recebe, imprime texto e envia resposta */
  while(1) {
    novo_set = todos_fds;
    nready = select(maxfd+1, &novo_set, NULL, NULL, NULL);
    if(nready < 0) {
      perror("select" );
      exit(1);
    }
    if(FD_ISSET(s, &novo_set)) {
      len = sizeof(socket_address);
      if ((new_s = accept(s, (struct sockaddr *)&socket_address, &len)) < 0) {
        perror("simplex-talk: accept");
        exit(1);
      }
      for (i = 0; i < FD_SETSIZE; i++) {
        if (c[i].descritor < 0) {
          c[i].descritor = new_s; 	//guarda descritor
          break;
        }
      }

      if (i == FD_SETSIZE) {
        perror("Numero maximo de clientes atingido.");
   			exit(1);
      }
      FD_SET(new_s, &todos_fds);		// adiciona novo descritor ao conjunto
      // infoPeer(new_s);

      if (new_s > maxfd)
        maxfd = new_s;			// para o select
      if (i > cliente_num)
        cliente_num = i;		// i­ndice maximo no vetor clientes[]
      if (--nready <= 0)
        continue;			// nao existem mais descritores para serem lidos
    }

    pairTime vetorN[cliente_num][2], vetorS[cliente_num][2], vetorL[cliente_num][2],vetorO[cliente_num][2];

    cruzamento carrosnaposicao00[cliente_num], carrosnaposicao01[cliente_num], carrosnaposicao10[cliente_num], carrosnaposicao11[cliente_num];
    int sizeCar00 = 0, sizeCar01 = 0, sizeCar10 = 0, sizeCar11 = 0;
    for (i = 0; i <= cliente_num; i++) {	// verificar se há dados em todos os clientes
      c[i].recebeu = false;
      if ( (sockfd = c[i].descritor) < 0)
        continue;
      if (FD_ISSET(sockfd, &novo_set)) {
        if ( (len = recv(sockfd, &buf, sizeof(buf), 0)) == 0) {
          printf("removeu Carro\n");

          if (removeCarro(vetorN, sizeN, sockfd) != 0){
            printf("removeu Carro N\n");
            sizeN -= 1;
          }
          else if (removeCarro(vetorS, sizeS, sockfd))
            sizeS -= 1;
          else if (removeCarro(vetorL, sizeL, sockfd))
            sizeL -= 1;
          else
            sizeO -= removeCarro(vetorO, sizeO, sockfd);


          //conexao encerrada pelo cliente
          close(sockfd);
          FD_CLR(sockfd, &todos_fds);
          c[i].descritor = -1;
      } else {
        pthread_t my_thread;
        if (buf.tipo == ENTRETERIMENTO) {
          pthread_create(&my_thread, NULL, divirta_se, (void *)&sockfd);
        }
        else if (buf.tipo == CONFORTO) {
          pthread_create(&my_thread, NULL, conforte_se, (void *)&sockfd);
        }
        usleep(10000);
        pairTime verifica[2];
        double distancia, dv;
        c[i].recebeu = true;
        // infoPeer(sockfd);
        printf("velocidade do cliente: %d\n", buf.velocidade);

        switch (buf.d) {
          case N:
            if ((int)buf.pos >= 0 && (int)buf.pos - buf.tam <= 0) {
              /* posicao 1,0 ocupada*/
              carrosnaposicao10[sizeCar10].descritor = sockfd;
              carrosnaposicao10[sizeCar10].d = N;
              sizeCar10++;
            }
            if ((int)buf.pos >= 1 && (int)buf.pos - buf.tam <= 1) {
              /* posicao 1,1 ocupada */
              carrosnaposicao11[sizeCar11].descritor = sockfd;
              carrosnaposicao11[sizeCar11].d = N;
              sizeCar11++;
            }
            break;
          case S:
            if ((int)buf.pos <= 0  && (int)buf.pos + buf.tam >= 0) {
              /* posicao 0,0 ocupada */
              carrosnaposicao00[sizeCar00].descritor = sockfd;
              carrosnaposicao00[sizeCar00].d = S;
              sizeCar00++;
            }
            if ((int)buf.pos <= 1  && (int)buf.pos + buf.tam >= 1) {
              /* posicao 0,1 ocupada */
              carrosnaposicao01[sizeCar01].descritor = sockfd;
              carrosnaposicao01[sizeCar01].d = S;
              sizeCar01++;
            }
            break;
          case L:
            if ((int)buf.pos >= 0 && (int)buf.pos - buf.tam <= 0) {
              /* posicao 0,0 ocupada */
              carrosnaposicao00[sizeCar00].descritor = sockfd;
              carrosnaposicao00[sizeCar00].d = L;
              sizeCar00++;
            }
            if ((int)buf.pos >= 1 && (int)buf.pos - buf.tam <= 1) {
              /* posicao 1,0 ocupada */
              carrosnaposicao10[sizeCar10].descritor = sockfd;
              carrosnaposicao10[sizeCar10].d = L;
              sizeCar10++;
            }
            break;
          case O:
            if ((int)buf.pos <= 0  && (int)buf.pos + buf.tam >= 0) {
              /* posicao 0,1 ocupada */
              carrosnaposicao01[sizeCar01].descritor = sockfd;
              carrosnaposicao01[sizeCar01].d = O;
              sizeCar01++;
            }
            if ((int)buf.pos <= 1  && (int)buf.pos + buf.tam >= 1) {
              /* posicao 1,1 ocupada */
              carrosnaposicao11[sizeCar11].descritor = sockfd;
              carrosnaposicao11[sizeCar11].d = O;
              sizeCar11++;
            }
            break;
        }


        verifica[0].ID = verifica[1].ID = sockfd;
        verifica[0].velocidade = verifica[1].velocidade = buf.velocidade;
        distancia = (buf.d == N || buf.d == L) ? - buf.pos : buf.pos;
        dv = 1 / buf.velocidade;

        double timestampSEC = (double)((buf.timestamp).tv_sec) + (double)((buf.timestamp).tv_nsec) / 1.0e9;

        //carros em direcao ao norte e leste atigem ponto 0 primeiro (na visao de suas respectivas coordenadas)
        verifica[0].inicio = timestampSEC + ((buf.d == N || buf.d == L) ?  distancia / buf.velocidade
                                                      : (distancia + 1) / buf.velocidade);
        verifica[0].fim    = timestampSEC + ((buf.d == N || buf.d == L) ? (distancia + buf.tam) / buf.velocidade
                                                      : (distancia + buf.tam + 1) / buf.velocidade);
        //carros em direcao ao norte e leste atigem ponto 0 primeiro (na visao de suas respectivas coordenadas)
        verifica[1].inicio = timestampSEC + ((buf.d == N || buf.d == L) ? verifica[0].inicio + dv
                                                      : verifica[0].inicio - dv);
        verifica[1].fim    = timestampSEC + ((buf.d == N || buf.d == L) ? verifica[0].fim + dv
                                                      : verifica[0].fim - dv);

        // ja passou do cruzamento esquece esse carro
        if (verifica[0].fim < 0 && verifica[1].fim < 0) {
          switch (buf.d) {
            case N:
            printf("N passou do cruzamento\n");
              sizeN -= removeCarro(vetorN, sizeN, sockfd);
            break;
            case S:
            printf("S passou do cruzamento\n");
              sizeS -= removeCarro(vetorS, sizeS, sockfd);
            break;
            case L:
              sizeL -= removeCarro(vetorL, sizeL, sockfd);
            break;
            case O:
              sizeO -= removeCarro(vetorO, sizeO, sockfd);
            break;
          }
          continue;
        }

        switch (buf.d) {
          case N:
            // ver se alguem mudou o tempo de chegada ao cruzamento
            if(buscarAlteracao(vetorN, &sizeN, verifica)){
              printf("MUDOU N\n");
              insereNS(vetorN, sizeN, verifica[0], 0);
              insereNS(vetorN, sizeN, verifica[1], 1);
              sizeN += 1;
            }
            break;
          case S:
            if(buscarAlteracao(vetorS, &sizeS, verifica)){
              printf("MUDOU S\n");

              insereNS(vetorS, sizeS, verifica[0], 0);
              insereNS(vetorS, sizeS, verifica[1], 1);
              sizeS += 1;
            }
            break;
          case L:
            if(buscarAlteracao(vetorL, &sizeL, verifica)){
              printf("MUDOU L\n");

              insereLO(vetorL, sizeL, verifica[0], 0);
              insereLO(vetorL, sizeL, verifica[1], 1);
              sizeL += 1;
            }
            break;
          case O:
            if(buscarAlteracao(vetorO, &sizeO, verifica)){
              printf("MUDOU O\n");

              insereLO(vetorO, sizeO, verifica[0], 0);
              insereLO(vetorO, sizeO, verifica[1], 1);
              sizeO += 1;
            }
            break;
        }
        printf("sizeO: %d\n", sizeO);
        printf("sizeL: %d\n", sizeL);
        printf("sizeS: %d\n", sizeS);
        printf("sizeN: %d\n", sizeN);

        // strcpy(buf.msg, "parabains");
        // send(sockfd, buf.msg, sizeof(buf.msg), 0);
        direcao dir;
        int mandousend = 0;
        for (int i = 0; i < sizeCar00; i++) {
          if (i==0) {
            dir = carrosnaposicao00[i].d;
          }
          else{
            if (carrosnaposicao00[i].d != dir) {
              printf("chame a ambulancia\n");
              for (int j = 0; j < sizeCar00; j++) {
                if (carrosnaposicao00[j].descritor == sockfd) {
                  bateu(carrosnaposicao00[j].descritor);
                  mandousend = 1;
                  /* code */
                }
              }
              break;
            }
          }
        }

        for (int i = 0; i < sizeCar01; i++) {
          if (i==0) {
            dir = carrosnaposicao01[i].d;
          }
          else{
            if (carrosnaposicao01[i].d != dir) {
              printf("chame a ambulancia\n");
              for (int j = 0; j < sizeCar01; j++) {
                if (carrosnaposicao01[j].descritor == sockfd) {
                  bateu(carrosnaposicao01[j].descritor);
                  mandousend = 1;
                }
              }
              break;
            }
          }
        }

        for (int i = 0; i < sizeCar10; i++) {
          if (i==0) {
            dir = carrosnaposicao10[i].d;
          }
          else{
            if (carrosnaposicao10[i].d != dir) {
              printf("chame a ambulancia\n");
              for (int j = 0; j < sizeCar10; j++) {
                if (carrosnaposicao10[j].descritor == sockfd) {
                  bateu(carrosnaposicao10[j].descritor);
                  mandousend = 1;
                }
              }
              break;
            }
          }
        }

        for (int i = 0; i < sizeCar11; i++) {
          if (i==0) {
            dir = carrosnaposicao11[i].d;
          }
          else{
            if (carrosnaposicao11[i].d != dir) {
              printf("chame a ambulancia\n");
              for (int j = 0; j < sizeCar11; j++) {
                if (carrosnaposicao11[j].descritor == sockfd) {
                  bateu(carrosnaposicao11[j].descritor);
                  mandousend = 1;
                }
              }
              break;
            }
          }
        }


        char mensagem[MAX_LINE];
        int flag = 0;
        for (int i = 0; i < sizeL; i++) {
          if (vetorL[i][0].ID == sockfd && (flag = evitarColisao(vetorL, vetorN, sizeN, vetorS, sizeS, i, 0, vetorL[i][0].ID))){
            strcpy(mensagem, "freie");
            send(vetorL[i][0].ID, mensagem, sizeof(mensagem), 0);
            strcpy(mensagem, "acelera");
            mandousend = 1;
            printf("PARAAAAAAAAA!\n");
            printf("evitar Colisao\n");
            if (flag == 1) {
              sizeS--;
            }else if (flag == 2) {
              sizeN--;
            }
            sizeL -= removeCarro(vetorL, sizeL, vetorL[i][0].ID);
            i--;
          }
          flag = 0;
        }
        for (int i = 0; i < sizeO; i++) {
          if (vetorL[i][0].ID == sockfd && (flag = evitarColisao(vetorO, vetorN, sizeN, vetorS, sizeS, i, 1, vetorL[i][0].ID))){
            strcpy(mensagem, "freie");
            send(vetorL[i][0].ID, mensagem, sizeof(mensagem), 0);
            strcpy(mensagem, "acelera");
            mandousend = 1;
            printf("PARAAAAAAAAA!\n");
            printf("evitar Colisao\n");
            if (flag == 1) {
              sizeS--;
            }else if (flag == 2) {
              sizeN--;
            }
            sizeO -= removeCarro(vetorO, sizeO, vetorO[i][0].ID);
            i--;
          }
          flag = 0;
        }





        if (mandousend ==0) {
          char mensagem[MAX_LINE];
          strcpy(mensagem, "acelere");
          send(sockfd, mensagem, sizeof(mensagem), 0);
        }

      }




      if (--nready <= 0)
        break;				// nao existem mais descritores para serem lidos
      }

    }
    // se carrosnaposicao00 tem 2 carros entao temos uma batida, logo mande que eles chamem ambulancia

}
  return 0;
}
