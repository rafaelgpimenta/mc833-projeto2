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

#define LISTEN_PORT 12345
#define MAX_PENDING 5
#define MAX_LINE 32

typedef enum direcao {N,S,L,O} direcao;
typedef enum { false, true } bool;
typedef enum { seguranca, entreterimento, conforto } tipoServico;

typedef pairTime {
  double inicio;
  double fim;
  int ID;
  unsigned short int velocidade;
} pairTime;

typedef struct pacote{
  time_t timestamp;
  double pos; // -30 a 31m (cruzamento nas posições 0 e 1)
  unsigned short int velocidade; // de 0 a 33m/s
  unsigned short int tam; //de 1 a 30m
  direcao d; //norte, sul, leste, oeste
  tipoServico tipo;
  char msg[MAX_LINE];
} pacote;

typedef struct cliente{
  pacote p;
  int descritor;
  bool recebido;
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
void insereNS(pairTime& (*vetor)[2], int size, pairTime verifica, int seleciona){
  int i;
  for (i = 0; i < size; i++)
    if (vetor[i][seleciona].inicio > verifica.inicio)
      break;

  for (int j = size; j > i; j--)
    vetor[j][seleciona] = vetor[j - 1][seleciona];

  vetor[i][seleciona] = verifica;
}

//carros LO inseridos na ultima posicao
void insereLO(pairTime& (*vetor)[2], int size, pairTime verifica, int seleciona){
    vetor[size][seleciona] = verifica;
}

int removeCarro(pairTime& (*vetor)[2], int size, int sockfd){
  int i, removeu = 0;
  for (i = 0; i < size; i++)
    if (vetor[i][0].descritor == sockfd){
      removeu = 1;
      break;
    }
  for (; i < size - 1; i++)
    vetor[i][0] = vetor[i + 1][0];

  for (i = 0; i < size; i++)
    if (vetor[i][1].descritor == sockfd)
      break;

  for (; i < size - 1; i++)
    vetor[i][1] = vetor[i + 1][1];

  return removeu;
}

bool buscarAlteracao (pairTime& (*vetor)[2], int& size, pairTime verifica) {
  bool alteracao = true;

  for (int i = 0; i < size; i++) {
    if (vetor[i][0].descritor == verifica.descritor && vetor[i][0].inicio == verifica.inicio) {
        alteracao = false;
    }
  }

  if (alteracao)
    size -= removeCarro(vetor, size, verifica.ID)

  return alteracao;
}

int main() {
  struct sockaddr_in socket_address;
  pacote buf;
  unsigned int len;
  int s, new_s, i, sockfd, cliente_num, maxfd, nready;
  cliente c[FD_SETSIZE]
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
      infoPeer(new_s);

      if (new_s > maxfd)
        maxfd = new_s;			// para o select
      if (i > cliente_num)
        cliente_num = i;		// i­ndice maximo no vetor clientes[]
      if (--nready <= 0)
        continue;			// nao existem mais descritores para serem lidos
    }

    pairTime vetorN[cliente_num][2], vetorS[cliente_num][2], vetorL[cliente_num][2], vetorO[cliente_num][2];

    cruzamento carrosnaposicao00[cliente_num], carrosnaposicao01[cliente_num], carrosnaposicao10[cliente_num], carrosnaposicao11[cliente_num];
    int sizeCar00 = 0, sizeCar01 = 0, sizeCar10 = 0, sizeCar11 = 0;
    for (i = 0; i <= cliente_num; i++) {	// verificar se há dados em todos os clientes
      c[i].recebeu = false;
      if ( (sockfd = c[i].descritor) < 0)
        continue;
      if (FD_ISSET(sockfd, &novo_set)) {
        if ( (len = recv(sockfd, (char*)&buf, sizeof(buf), 0)) == 0) {
          if (removeCarro(vetorN, sizeN, sockfd))
            sizeNeS -= 1;
          else if (removeCarro(vetorS, sizeS, sockfd))
            sizeL -= 1;
          else if (removeCarro(vetorL, sizeL, sockfd))
            sizeL -= 1;
          else
            sizeO -= removeCarro(vetorO, sizeO, sockfd);


          //conexao encerrada pelo cliente
          close(sockfd);
          FD_CLR(sockfd, &todos_fds);
          c[i].descritor = -1;
      } else {
        pairTime verifica[2];
        double distancia, dv;
        struct tm * timeinfo;
        //recebeu pacote de i
        c[i].pacote = buf;
        c[i].recebeu = true;
        infoPeer(sockfd);
        printf("velocidade do cliente: %u\n", buf.velocidade);
        timeinfo = localtime ( &(buf.timestamp) );
        printf ("relogio do cliente: %s\n", asctime (timeinfo) );
        strcpy(buf.msg, "parabains");
        send(sockfd, (char*)&buf, sizeof(buf), 0);

        switch (buf.d) {
          case N:
            if (((int)buf.pos) == 0) {
              /* posicao 1,0 ocupada*/
              carrosnaposicao10[sizeCar10].descritor = sockfd;
              carrosnaposicao10[sizeCar10].d = N;
              sizeCar10++;
            } else if (((int)buf.pos) == 1) {
              /* posicao 1,1 ocupada */
              carrosnaposicao11[sizeCar11].descritor = sockfd;
              carrosnaposicao11[sizeCar11].d = N;
              sizeCar11++;
            }
            break;
          case S:
            if (((int)buf.pos) == 0) {
              /* posicao 0,0 ocupada */
              carrosnaposicao00[sizeCar00].descritor = sockfd;
              carrosnaposicao00[sizeCar00].d = S;
              sizeCar00++;
            }else if (((int)buf.pos) == 1) {
              /* posicao 0,1 ocupada */
              carrosnaposicao01[sizeCar01].descritor = sockfd;
              carrosnaposicao01[sizeCar01].d = S;
              sizeCar01++;
            }
            break;
          case L:
            if (((int)buf.pos) == 0) {
              /* posicao 0,0 ocupada */
              carrosnaposicao00[sizeCar00].descritor = sockfd;
              carrosnaposicao00[sizeCar00].d = L;
              sizeCar00++;
            }else if (((int)buf.pos) == 1) {
              /* posicao 1,0 ocupada */
              carrosnaposicao10[sizeCar10].descritor = sockfd;
              carrosnaposicao10[sizeCar10].d = L;
              sizeCar10++;
            }
            break;
          case O:
            if (((int)buf.pos) == 0) {
              /* posicao 0,1 ocupada */
              carrosnaposicao01[sizeCar01].descritor = sockfd;
              carrosnaposicao01[sizeCar01].d = O;
              sizeCar01++;
            }else if (((int)buf.pos) == 1) {
              /* posicao 1,1 ocupada */
              carrosnaposicao11[sizeCar11].descritor = sockfd;
              carrosnaposicao11[sizeCar11].d = O;
              sizeCar11++;
            }
            break;
        }






        verifica[0].ID = verifica[1].ID = sockfd;
        verifica[0].velocidade = verifica[1].velocidade = buf.velocidade;
        distancia = (buf.d == N || buf.d == L) ? - buf.posicao : buf.posicao;
        dv = 1 / buf.velocidade;
        //carros em direcao ao norte e leste atigem ponto 0 primeiro (na visao de suas respectivas coordenadas)
        verifica[0].inicio = buf.timestamp + ((buf.d == N || buf.d == L) ?  distancia / buf.velocidade
                                                      : (distancia + 1) / buf.velocidade);
        verifica[0].fim    = buf.timestamp + ((buf.d == N || buf.d == L) ? (distancia + buf.tam) / buf.velocidade
                                                      : (distancia + buf.tam + 1) / buf.velocidade);
        //carros em direcao ao norte e leste atigem ponto 0 primeiro (na visao de suas respectivas coordenadas)
        verifica[1].inicio = buf.timestamp + ((buf.d == N || buf.d == L) ? verifica.inicio + dv
                                                      : verifica.inicio - dv);
        verifica[1].fim    = buf.timestamp + ((buf.d == N || buf.d == L) ? verifica.fim + dv
                                                      : verifica.fim - dv);

        // ja passou do cruzamento esquece esse carro
        if (verifica[0].fim < 0 && verifica[1].fim < 0) {
          switch (buf.d) {
            case N:
              sizeN -= removeCarro(vetorN, sizeN, sockfd);
            break;
            case S:
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
            if(buscarAlteracao(vetorN, sizeN, verifica, N)){
              insereNS(vetorN, sizeN, verifica[0], 0);
              insereNS(vetorN, sizeN, verifica[1], 1);
              sizeN += 1;
            }
            break;
          case S:
            if(buscarAlteracao(vetorS, sizeS, verifica, S)){
              insereNS(vetorS, sizeS, verifica[0], 0);
              insereNS(vetorS, sizeS, verifica[1], 1);
              sizeS += 1;
            }
            break;
          case L:
            if(buscarAlteracao(vetorL, sizeL, verifica, L)){
              insereLO(vetorL, sizeL, verifica[0], 0);
              insereLO(vetorL, sizeL, verifica[1], 1);
              sizeL += 1;
            }
            break;
          case O:
            if(buscarAlteracao(vetorO, sizeO, verifica, O)){
              insereLO(vetorO, sizeO, verifica[0], 0);
              insereLO(vetorO, sizeO, verifica[1], 1);
              sizeO += 1;
            }
            break;
        }


      }

      if (--nready <= 0)
        break;				// nao existem mais descritores para serem lidos
      }
    }
    // se carrosnaposicao00 tem 2 carros entao temos uma batida, logo mande que eles chamem ambulancia



    // TRETA
    for (int i = 0; i < sizeL; i++) {
      evitarColisao(vetorL[i][0], vetorNeS, sizeNeS)
    }


  }

  return 0;
}
