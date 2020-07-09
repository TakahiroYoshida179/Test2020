#include <stdio.h>
#include <string.h>
#include <math.h>


int heatrun = 0;
int ipver = 0;
int time_out_sec = 5;

int main ()
{
        int rt;

        if (argc != 3) {
                fprintf(stderr, "Usage : %s <s or c> <nh or h>\n", argv[0]);
                fprintf(stderr, "ex :\n %s s h %s c nh\n", argv[0], argv[0]);
                exit(1);
        }
        
        if (!(((strcmp(argv[1], 's') == 0) || (strcmp(argv[1], 'c') == 0)) &&
		   ((strcmp(argv[2], "h") == 0) || (strcmp(argv[2], "nh") == 0))) ) {
		fprintf(stderr, "Usage : %s <s or c> <nh or h>\n", argv[0]);
		fprintf(stderr, "ex:\n %s s h\n %s c nh\n", argv[0], argv[0]);
		exit(1);
	}
	
	if (strcmp(argv[2], 'h') == 0)
	        heatrun = 1;
	        
	        
	if (strcmp(tdata.ip, 'v4') == 0) ipver = 0;
	else if (strcmp(tdata.ip, 'v6') == 0) ipver = 1;
	else {
		printf("iptype Errrrorrr\n");
		exit(1);
	}
	
	if (strcmp(argv[1], 's') == 0)
		do_server_side();
	else if (strcmp(argv[1], 'c') == 0)
		do_client_side(info.cnfqdn);
        
        return 0;
}

/*
 * ~do_sever_side~
 * ソケット部周辺をやる関数、送受信ループをする関数を呼び出す関数
 */
void 
do_server_side(void) {
	int sock;

	if ((sock = socket_svr_udp()) < 0) {
			fprintf(stderr, "socket_svr_udp():error\n");
			exit(1);
	}

	printf("main sock: %d\n\n", sock);
	if (heatrun == 0) {
		if (forwardloop_svr_udp(sock) == -1) {
			fprintf(stderr, "forwardloop_svr_udp(): error\n");
			close(sock);
			exit(1);
		}
	} else {
		if (forwardloop_svr_udp_hrun(sock) == -1) {
			fprintf(stderr, "forwardloop_svr_udp_hrun(): error\n");
			close(sock);
			exit(1);
		}
	}
}
/*
 * ~~ tcp_server_socket ~~
 * ソケット部周辺のあれこれをしてくれる
 *
 * 返り値:
 *   ntmfw_socketで取得したファイルディスクリプタの値
 */
uint32_t 
socket_svr_udp(void)
{
  int sock;
  struct sockaddr_in addr;
  struct addrinfo hints, *res;
  int result;

  memset(&hints, 0, sizeof(hints));
  if (ipver == 0) {
    hints.ai_family = AF_INET;
    printf("Set \"AF_INET\" to hints.ai_family\n");
  } else {
    hints.ai_family = AF_INET6;
    printf("Set \"AF_INET6\" to hints.ai_family\n"); 
  }
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_protocol = 0;

  printf("Go socket_svr_udp\n");

  printf("server ntmfw_getaddrinfo()\n");
  result = ntmfw_getaddrinfo(NULL, PORT_NUM, &hints, &res);
  if (result != 0) {
      fprintf(stderr, "ntmfw_getaddrinfo(): %s\n", gai_strerror(result));
      return -1;
  }
  if (res == NULL) {
      fprintf(stderr, "warning??: res == NULL\n");
		return -1;
  }

  printf("server sock()\n");
  sock = ntmfw_socket(res->ai_family, res->ai_socktype, 0);
  printf("inss sock: %d\n\n\n", sock);
  if (sock < 0) {
    perror("ntmfw_socket()");
    ntmfw_freeaddrinfo(res);
    ntmfw_close(sock);
    return -1;
  }

  printf("server ntmfw_bind()\n");
  if (ntmfw_bind(sock, res->ai_addr, res->ai_addrlen) < 0) {
    perror("ntmfw_bind");
    ntmfw_freeaddrinfo(res);
    ntmfw_close(sock);
    return -1;
  } else {
    printf("ntmfw_bind --> success\n");
  }
  printf("after bind sock: %d\n\n", sock);

  return sock;
}

/*
 * forwardloop_svr_udp
 * "ntmfw_recvfrom -> ntmfw_sendto" のループをする
 * 返り値:
 *   0 -> 正常終了
 *  -1 -> 何かしらのエラー
 * 引数:
 *   sock -> socket file descriptor
 */
int 
forwardloop_svr_udp(int sock)
{
  int32_t rlen = 0, slen = 0;
  struct sockaddr_storage peer_addr;
  socklen_t peer_addr_len;
  char recv_buf[BUFF_SIZE];
  char msg_buf[BUFF_SIZE];
  int num_of_sended;
  int loop_escape_flag = 0;
  int c;

  for (;;) {
    memset(recv_buf, 0, sizeof(recv_buf));
    memset(msg_buf, 0, sizeof(msg_buf));
    num_of_sended = 0;
    
    printf("============================================\n");
    printf("Wait for packet..\n");
    socklen_t peer_addr_len = sizeof(struct sockaddr_storage);
    rlen = ntmfw_recvfrom(sock, recv_buf, BUFF_SIZE, 0, 
                              (struct sockaddr *)&peer_addr,
                              &peer_addr_len);
    if (rlen == -1) {
          perror("Error: ntmfw_recvfrom()");
          return -1;
    }
    printf("\tReceived the following message: %s\n", recv_buf);
    // 終了フラグが来てるかどうかの確認
    sscanf(recv_buf, "%s %d", msg_buf, &num_of_sended);
    if (num_of_sended == -1) {
      printf("received forward finish flag!\nBye!\n");
      break;
    }

    //sprintf(recv_buf, "%s %d", msg_buf, num_of_sended);
    printf("\nSend this packet: %s..\n", recv_buf);
    usleep(100000);
    if (ipver == 0) 
      peer_addr.ss_family = AF_INET;
    else if (ipver == 0) 
      peer_addr.ss_family = AF_INET6;
    else {
      fprintf(stderr, "This ipver is crazy!!\n");
      return -1;
    }
    slen = ntmfw_sendto(sock, recv_buf, rlen, 0,
                        (struct sockaddr *) &peer_addr,
                        peer_addr_len);
    if (slen != rlen) {
      perror("ntmfw_sendto()");
      return -1;
    }
    printf("\tntmfw_sendto() complete\n");
    if (loop_escape_flag == 1) {
      fprintf(stderr, "\nEscape forward loop, Bye!!\n");
      break;
    }
    printf("============================================\n");
  }
}

/*
 * forwardloop_svr_udp_hrun
 * "ntmfw_recvfrom -> ntmfw_sendto" のループをする.HeatRun Ver.
 * 絵が出ます
 * 返り値:
 *   0 -> 正常終了
 *  -1 -> 何かしらのエラー
 * 引数:
 *   sock -> socket file descriptor
 */
int 
forwardloop_svr_udp_hrun(int sock)
{
  int32_t rlen = 0, slen = 0;
  struct sockaddr_storage peer_addr;
  socklen_t peer_addr_len;
  char send_buf[BUFF_SIZE];
  char recv_buf[BUFF_SIZE];
  int i;

  for (i = 0; ;) {
    memset(recv_buf, 0, sizeof(recv_buf));
    memset(send_buf, 0, sizeof(send_buf));

    memcpy(&fds, &rfds, sizeof(fd_set));

    if (i == 80) i = 0;
    peer_addr_len = sizeof(struct sockaddr_storage);
    rlen = ntmfw_recvfrom(sock, recv_buf, BUFF_SIZE, 0, 
                          (struct sockaddr *)&peer_addr,
                          &peer_addr_len);
    if (rlen == -1) {
      perror("Error: ntmfw_recvfrom()");
      return -1;
    }
    // データを受信したら絵を表示
    draw_wata(i);
    ++i;    // ヒート欄の時しか使わないのでここでincrement
    usleep(100000);
    if (ipver == 0) 
      peer_addr.ss_family = AF_INET;
    else if (ipver == 0) 
      peer_addr.ss_family = AF_INET6;
    else {
      fprintf(stderr, "ipver is crazy!!\n");
      return -1;
    }
    slen = ntmfw_sendto(sock, recv_buf, rlen, 0,
                        (struct sockaddr *)&peer_addr,
                        peer_addr_len);
    if (slen == -1) {
      perror("ntmfw_sendto()");
      return -1;
    }
  }
}

/*
 * do_client_side
 * ソケット部の関数やら、recvのループやらの関数を呼び出す関数
 * 引数:
 *   cnfqdn: 通信相手さんのfqdn
 */
void 
do_client_side(const uint8_t *cnfqdn) 
{
	int sock;
	struct addrinfo *res;
	
	if ((sock = socket_clt_udp(cnfqdn, &res)) == -1) {
		fprintf(stderr, "socket_svr_udp():error\n");
		exit(1);
	}

	if (heatrun == 0) {
		if (forwardloop_clt_udp(sock, cnfqdn, &res) == -1) {
			fprintf(stderr, "forwardloop_clt_udp(): error\n");
			close(sock);
			exit(1);
		}
	} else {
		if (forwardloop_clt_udp_hrun(sock, cnfqdn, &res) == -1) {
			fprintf(stderr, "forwardloop_clt_udp_hrun(): error\n");
			close(sock);
			exit(1);
		}
	}
}

/*
 * ~~ socket_clt_udp ~~
 * ソケット部周辺のあれこれをしてくれる
 *
 * 返り値:
 *   ntmfw_socketで取得したファイルディスクリプタの値
 * 引数:
 *   fqdn: 通信相手さんのfqdn
 *   res0: getaddrinfoによって
 *         通信相手の情報が入る構造体(実体はdo_client_sideで定義)
 */
uint32_t 
socket_clt_udp(const uint8_t *fqdn, struct addrinfo **res0)
{
	int sock;
  struct addrinfo hints, *res;
  int result;
  uint8_t addr_buff[INET6_ADDRSTRLEN+1];
	
	printf("Go socket_clt_udp\n");
  memset(&hints, 0, sizeof(hints));
  if (ipver == 0) {
    hints.ai_family = AF_INET;
    printf("Set \"AF_INET\" to hints.ai_family\n");
  } else {
    hints.ai_family = AF_INET6;
    printf("Set \"AF_INET6\" to hints.ai_family\n"); 
  }
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_protocol = 0;

  printf("client ntmfw_getaddrinfo()\n");
  result = ntmfw_getaddrinfo(fqdn, PORT_NUM, &hints, &(*res0));
  if (result == -1) {
      fprintf(stderr, "ntmfw_getaddrinfo(): %s\n", gai_strerror(result));
      return -1;
  }
  if (*res0 == NULL) {
      fprintf(stderr, "warning??: res == NULL\n");
		return -1;
  }
	
	printf("client sock()\n");
	sock = ntmfw_socket((*res0)->ai_family, (*res0)->ai_socktype, 0);
  if (sock == -1) {
    perror("ntmfw_socket()");
    ntmfw_freeaddrinfo(*res0);
    ntmfw_close(sock);
    return -1;
  }

  return sock;
}

/*
 * forwardloop_clt_udp
 * "ntmfw_sendto -> ntmfw_recvfrom" のループをする
 * 返り値:
 *   0 -> 正常終了
 *  -1 -> 何かしらのエラー
 * 引数:
 *   sock -> socket file descriptor
 *   fqdn -> 相手のFQDN
 *   res  -> getaddrinfoによって
 *          通信相手の情報が入る構造体(実体はdo_client_sideで定義)
 */
int 
forwardloop_clt_udp(int sock, const uint8_t *fqdn, 
                    struct addrinfo **res)
{
  int32_t rlen = 0, slen = 0;
  struct sockaddr_storage peer_addr;
  socklen_t peer_addr_len = sizeof(struct sockaddr_storage);
  uint8_t addr_buf[INET6_ADDRSTRLEN+1];
  char send_buf[BUFF_SIZE];
  char recv_buf[BUFF_SIZE];
  char msg_buf[BUFF_SIZE] = "Onimotsudesu: ";
  int num_of_send = 1;
  int num_of_recv = 0;
  int loop_escape_flag = 0;
  double max_value;
  int c;

  fd_set fds, rfds;
  struct timeval tv;
  int retval;
 
  max_value = pow(2, sizeof(num_of_send)*8) / 2 - 1;

  if (ipver == 0) {
    ((struct sockaddr_in *)(*res)->ai_addr)->sin_port = htons(12345);
  } else {
    ((struct sockaddr_in6 *)(*res)->ai_addr)->sin6_port = htons(12345);
  }

  FD_ZERO(&rfds);
  FD_SET(sock, &rfds);

  for (;;) {
    if (num_of_send == max_value)
      num_of_send = 0;

    memset(recv_buf, 0, sizeof(recv_buf));
    memset(send_buf, 0, sizeof(send_buf));

    printf("============================================\n");
    printf("If you want to \n");
    printf("1. send this packet:%s\n", recv_buf);
    printf("\t-> press the EnterKey\n");
    printf("2. exit this program\n");
    printf("\t-> enter \'q\' \n");
    printf("command: ");

    while ((c = getchar()) != '\n') {
      if (c == 'q') {
        num_of_send = -1;
        loop_escape_flag = 1;
      }
    }

    sprintf(send_buf, "%s %d", msg_buf, num_of_send);
    printf("Sending this packet: %s..\n", send_buf);
    slen = ntmfw_sendto(sock, send_buf, strlen(send_buf), 0, (*res)->ai_addr, (*res)->ai_addrlen);
    if (slen == -1) {
      perror("ntmfw_sendto()");
      return -1;
    }
    // 送信回数を更新
    ++num_of_send;
    printf("\tntmfw_sendto() complete\n");

    if (loop_escape_flag == 1) {
      fprintf(stderr, "\nEscape forward loop, Bye!!\n");
      break;
    }

    printf("\nWait for packet..\n");

    memcpy(&fds, &rfds, sizeof(fd_set));
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    retval = ntmfw_select(sock+1, &fds, NULL, NULL, &tv);
    if (retval == -1) {
      perror("ntmfw_select()");
      return -1;
    } else if (FD_ISSET(sock, &fds)) {  
      rlen = ntmfw_recvfrom(sock, recv_buf, BUFF_SIZE, 0, 
                                (struct sockaddr *)&peer_addr,
                                &peer_addr_len);
      if (rlen == -1) {
        perror("Error: ntmfw_recvfrom()");
        return -1;
      }
      printf("\tReceived the following message: %s\n", recv_buf);

      if (num_of_recv == -1) {
        printf("received Forward Finish Flag!\nBye!\n");
        break;
      }
    } else {
      printf("TIMEOUT!!\n");
      printf("\n!!!!!!!!!!!! No data within %d seconds !!!!!!!!!!!!\n", timeout_sec);
    }
    printf("============================================\n");
  }
}

/*
 * forwardloop_clt_udp
 * "ntmfw_sendto -> ntmfw_recvfrom" のループをする.HeatRun Ver.i
 * 絵が出ます
 * 返り値:
 *   0 -> 正常終了
 *  -1 -> 何かしらのエラー
 * 引数:
 *   sock -> socket file descriptor
 *   fqdn -> 相手のFQDN
 *   res  -> getaddrinfoによって
 *          通信相手の情報が入る構造体(実体はdo_client_sideで定義)
 */
int 
forwardloop_clt_udp_hrun(int sock, const uint8_t *fqdn, 
                    struct addrinfo **res)
{
  int32_t rlen = 0, slen = 0;
  struct sockaddr_storage peer_addr;
  socklen_t peer_addr_len = sizeof(struct sockaddr_storage);
  uint8_t addr_buf[INET6_ADDRSTRLEN+1];
  char send_buf[BUFF_SIZE] = "Onimotsudesu: 1";
  char recv_buf[BUFF_SIZE];
  int i;

  fd_set fds, rfds;
  struct timeval tv;
  int retval;

  if (ipver == 0) {
    ((struct sockaddr_in *)(*res)->ai_addr)->sin_port = htons(12345);
  } else {
    ((struct sockaddr_in6 *)(*res)->ai_addr)->sin6_port = htons(12345);
  }

  FD_ZERO(&rfds);
  FD_SET(sock, &rfds);

  for (i = 0; ; ) {
    memset(recv_buf, 0, sizeof(recv_buf));
    memset(send_buf, 0, sizeof(send_buf));

    usleep(100000);
    slen = ntmfw_sendto(sock, send_buf, strlen(send_buf), 0, (*res)->ai_addr, (*res)->ai_addrlen);
    if (slen == -1) {
      perror("ntmfw_sendto()");
      return -1;
    }

    memcpy(&fds, &rfds, sizeof(fd_set));
    tv.tv_sec = timeout_sec;
    tv.tv_usec = 0;
    
    retval = ntmfw_select(sock+1, &fds, NULL, NULL, &tv);
    if (retval == -1) {
      perror("ntmfw_select()");
      return -1;
    } else if (FD_ISSET(sock, &fds)) {
      if (i == 80) i = 0;
      rlen = ntmfw_recvfrom(sock, recv_buf, BUFF_SIZE, 0, 
                            (struct sockaddr *)&peer_addr,
                            &peer_addr_len);
      if (rlen == -1) {
        perror("Error: ntmfw_recvfrom()");
        return -1;
      }
      // データを受信したら絵を表示
      draw_suzu(i);
      ++i;
    } else {
      fprintf(stderr, "TIMEOUT!!\n");
      fprintf(stderr, "Resend packet..\n");
    }
  }
}

void draw_suzu(int row) {
	switch (row) {
	case 0:
		printf( " , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , . . . , , , . . , , , . . . . . , , , , , ,\n");
		break;
	case 1:
		printf( " , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , . . . , , , . . , , , . . . . . , , , , , ,\n");
		break;
	case 2:
		printf( " , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , . . . , , , . . , , , . . . . . , , , , , ,\n");
		break;
	case 3:
		printf( " , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , . . . . . . . . . . . . . . . . . . . . . . . .\n");
		break;
	case 4:
		printf( " , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , . . . . . . . . . . . . . . . . . . . . . . . .\n");
		break;
	case 5:
		printf( " , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , . . . . . . . . . . . . . . . . . . . . . . . .\n");
		break;
	case 6:
		printf( " , , , , , , , , , , , , , , , , , , , , , , , , . . . , , , . . . . . . . . . . . . . . . . . . . . . . . .\n");
		break;
	case 7:
		printf( " , , , , , , , , , , , , , , , , , , , , , , , , . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .\n");
		break;
	case 8:
		printf( " , , , , , , , , , , , , , , , , , , , . . , , , , , . . , , . , . . . . . . . . . . . . . . . . . . . . . .\n");
		break;
	case 9:
		printf( " , , , , , , , , , , , , , , , , , , , . , , , . , , , , ' , , , , . . . . . . . . . . . . . . . . . . . . .\n");
		break;
	case 10:
		printf( " , , , , , , , , , , , , , , . . . . . , , , , , , : : ' # + + ' : . , . . . . . . . . . . . . . . . . . . .\n");
		break;
	case 11:
		printf( " , , , , , , , , , , , , , , , , . . . , , . : : ' + # # + + # : : , , , . . . . . . . . . . . . . . . . . .\n");
		break;
	case 12:
		printf( " , , , , , , , , , , , , , , , , , : : ' # # + # # + # # # # # # ' ' : , , . , . . . . . . . . . . . . . . .\n");
		break;
	case 13:
		printf( " , , , , , , , , , , , , , , , , : # # # # # # # # # # # # # # # # # + + ; , , . . . . . . . . . . . . . . .\n");
		break;
	case 14:
		printf( " , , , , , , , , . . . . . , , : ' ' + # # # # @ # # # # # # # # + # # # + ' . . . . . . . . . . . . . . . .\n");
		break;
	case 15:
		printf( " , , , , , , , , . . . . . , ; + + # # @ @ @ @ # @ # # # # # # # @ # # + # + ' , . . . . . . . . . . . . . .\n");
		break;
	case 16:
		printf( " , , , , , , . . . . . . . : # + # # @ @ @ @ # # @ @ # # # @ # # # # # # # + ' ; , . . . . . . . . . . . . .\n");
		break;
	case 17:
		printf( " , , , , , , . . . . . . : # # # # # # # @ # # # # # # # # # # @ # # # # # # + + ' . . . . . . . . . . . . .\n");
		break;
	case 18:
		printf( " , , , . . . . . . . . , ; # # # # # # # # @ @ @ @ @ @ # # @ # # # # @ # # # # + + : . . . . . . . . . . . .\n");
		break;
	case 19:
		printf( " , , , . . . . . . . . . + # # # # # # # @ + # # @ @ # # # # @ @ @ # # # # # # # ' + . . . . . . . . . . . .\n");
		break;
	case 20:
		printf( " , , , . . . . . . . . , @ # # # # # # # # # # # # # # # # # # # # # # # # # # # # ; , . . . . . . . . . . .\n");
		break;
	case 21:
		printf( " . . . . . . . . . . . + # # # # # # # # # # # @ # # # # # # # # # @ # # # # # # # ; . . . . . . . . . . . .\n");
		break;
	case 22:
		printf( " . . . . . . . . . . , ' @ # # # # @ # # @ # + + + # # # ' ; ' + # # # # # # # # # # . . . . . . . . . . . .\n");
		break;
	case 23:
		printf( " . . . . . . . . . . , @ # @ # @ @ # # # + + ' ' ' ; ' ' ; ; : ' ' ' + # # # # # # # : . . . . . . . . . . .\n");
		break;
	case 24:
		printf( " . . . . . . . . . . : # # @ @ @ # # # + ' ' ; ; ; ; : ; : : : : ; ; ; # + @ # # # # ' . . . . . . . . . . .\n");
		break;
	case 25:
		printf( " . . . . . . . . . , ; # # @ @ @ # + ' ' ; ; ; ; ; ; : : : : : , , : : ' ' # # # # # ; . . . . . . . . . . .\n");
		break;
	case 26:
		printf( " . . . . . . . . . . : @ # @ @ # # + ' ' ; ; ; ; : : : , , , , , , , : : ; + # # @ # : . . . . . . . . . . .\n");
		break;
	case 27:
		printf( " . . . . . . . . . . , @ @ # @ # # + ' ' ; ; ; : : : : : : : : , , , : : : + # # # @ : . . . . . . . . . . .\n");
		break;
	case 28:
		printf( " . . . . . . . . . . , @ @ @ @ # # + ' ' ; ; : : : : : : : : , , , , , : : + # # # # : . . . . . . . . . . .\n");
		break;
	case 29:
		printf( " . . . . . . . . . . , @ @ @ @ # + ' ' ' ' ; : : : : , , , , , , , , : , : ; # # # # , . . . . . . . . . . .\n");
		break;
	case 30:
		printf( " . . . . . . . . . . , ' @ @ # # ' ' ' ' ' + + ' ; : : , : : : ; ; : : : : : ' @ # # . . . . . . . . . . . .\n");
		break;
	case 31:
		printf( " . . . . . . . . . . . + # # # + # # + + + + + ' ' ; ; : ; ; ; ; : : , : : , : # # + . . . . . . . . . . . .\n");
		break;
	case 32:
		printf( " . . . . . . . . . . . ' # @ # ' # + ' ; ; ; ; : ; + + ' ' ; ; ; : : : : ; , ' @ @ + . . . . . . . . . . . .\n");
		break;
	case 33:
		printf( " . . . . . . . . . . . + ' ' @ + + ' ' # ; # + # ; + ' ; ' ; ' # + ; + ; ' ; + ' , : . . . . . . . . . . . .\n");
		break;
	case 34:
		printf( " . . . . . . . . . . : ' ; + @ ' ' ; ' ; ; ; ; ; ' + : : , : ; ; ; : , ; ; ; : # , : , . . . . . . . . . . .\n");
		break;
	case 35:
		printf( " . . . . . . . . . . ; ' ; + # ' ; ; ; ; : : : ; ' ' : , : , , : : , , , : , : # . , : . . . . . . . . . . .\n");
		break;
	case 36:
		printf( " . . . . . . . . . . , ' ' + @ ' ; ; ; ; : ; : ; ' ; : , , : , , , , , , , , : # , , : ` . . . . . . . . . .\n");
		break;
	case 37:
		printf( " . . . . . . . . . . . ' ' + # ' ; ; ; : : : ; ' ' ; : , , , , , , , , , , , : ' : , , ` ` . . . . . . . . .\n");
		break;
	case 38:
		printf( " . . . . . . . . . . . + ' ' # ' ' ; ; : : : ; ' ' ; : , , : , , , , , , , , : ; : , , ` ` ` ` ` ` . . . . .\n");
		break;
	case 39:
		printf( " . . . . . . . . . . . , ' ; ' ' ' ' ; ; : : : ' ; ; : , , , : , , , , , , : ; , , , . ` ` ` ` ` ` ` ` . . .\n");
		break;
	case 40:
		printf( " . . . . . . . . ` ` ` . : ; ' ' ' ' ; ; ; : ; + ' # ' ; ' ; : : , , , , , : ; , . , ` ` ` ` ` ` ` ` ` ` ` `\n");
		break;
	case 41:
		printf( " . . . . . . ` ` ` ` ` ` ` . ; + ' ' ; ; ; : ; ' ' ' ; ; , : , , , , , , , : : , . ` ` ` ` ` ` ` ` ` ` ` ` `\n");
		break;
	case 42:
		printf( " . . . . . ` ` ` ` ` ` ` ` ` . . ; ' ' ; ; ; ; ; ; ; , ; , , , , , , , , : : : ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");
		break;
	case 43:
		printf( " . . . ` ` ` ` ` ` ` ` ` ` ` ` . ; ; ' ' ; ; ; ; ; : , : , , , : : , : : : : : ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");
		break;
	case 44:
		printf( " . ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` # ' ' ' ; ; + ' ' ' ' ; ; ; : ; : : : : : : ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");
		break;
	case 45:
		printf( " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` : ; ' ' ' ; + + ' ; : : : : ; ; ; : : : : , ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");
		break;
	case 46:
		printf( " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` , ' ' ' ; ; ' ' ' ; ; ; : , , : : : : , ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");
		break;
	case 47:
		printf( " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ' ' ' ; ' ' ; ; ; : : , , , : : : ; ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");
		break;
	case 48:
		printf( " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ' ' ' ' ' ' ; ; : : : , : : : : ; : ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");
		break;
	case 49:
		printf( " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ' ' ' ' ' ' ; ; ; : : , : , : ; : : ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");
		break;
	case 50:
		printf( " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ' ' ' ' ' + ' ' ; ; : ; ; : ; : : : ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");
		break;
	case 51:
		printf( " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `   ' ' ' ' + + + + ' ' ' ' ' ; : : , :         ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");
		break;
	case 52:
		printf( " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` . ; ' ' ' ' + + + + + + ' ; : : , , , `                   ` ` ` ` ` ` ` `\n");
		break;
	case 53:
		printf( " ` ` ` ` ` ` ` ` ` ` ` ` ` ` `   . : ' ' ' ' ' ' ' ' ' ' ; : : : , , , : ` `                 ` ` ` ` ` ` ` `\n");
		break;
	case 54:
		printf( " ` ` ` ` ` ` ` ` ` ` `         ` @ : : ' ; ' ' ' ' ' ; : : : : : , , , . ` #                     ` ` ` ` ` `\n");
		break;
	case 55:
		printf( " ` ` ` ` ` ` ` ` ` `         , + @ : : : ' ; ' ' ' ' ; : : : : , , , : ` ` # #                         ` ` `\n");
		break;
	case 56:
		printf( " ` ` ` ` ` ` ` `     ` `   ' # # # : : : , ' ; ' ; ; ; ; : : : : , :   ` ` # + + `                          \n");
		break;
	case 57:
		printf( " ` ` ` ` ` `   `   `   . # # # # # : : : , . : ; ; ; ; : : , , , :   `   ` # + + + '                        \n");
		break;
	case 58:
		printf( " ` ` ` ` ` ` `   ` # + + + @ # # # ; : : , . . . : : : : : , : ,         ` + # + + + ' + `                  \n");
		break;
	case 59:
		printf( " ` ` `     ` . # + + # # # # # # # # : : , , . ` ` ; : : : ; .           . + + + + + ' ' ' + `              \n");
		break;
	case 60:
		printf( " ` ` `   , # + + + # # # # # # # # # ; : , , . ` ` ` ; : ; , `         ` ; + + + + + + + + + ' + .          \n");
		break;
	case 61:
		printf( "   . ' + + + + + # # # # # # # # # # + ; : , , . ' ; : : ; ' +         ` @ + + + + + + + + + + ' ' ' :      \n");
		break;
	case 62:
		printf( " ' + + + + + + + # # + # # # # # # # # : : , , ' # ; ' ; # # ' #     ` ` + + + + + + + + + + + + ' ' ' ' '  \n");
		break;
	case 63:
		printf( " + + + + + + + + # # # # # # # # # # # : , : , # # # + ; ' ' + ' ' ` ` . + + + + + + + + # # + + + + + + ' '\n");
		break;
	case 64:
		printf( " + + + + + + + # # # # # # # # # # # # ; , : , ; # # # + + ' + ; ; ` ` : + # + + + + + + + + + + + + + + + '\n");
		break;
	case 65:
		printf( " + + + + + + + # # # # # # # # # # # # ' . : , ; ; # + ' + # , ; ; : ` ' + + + + + + + + + + + + + + + + + +\n");
		break;
	case 66:
		printf( " + + + + + + + # # # # # # # # # # # # + : . , , ; ' + + ' , . , : ; ` + # # + + + + + + + + + + + + + + + +\n");
		break;
	case 67:
		printf( " + + + + + + # # # # # # # # # # # # # @ ; . . . ; : + + ' ` ` . . : ` # + # + + + + + + + + + + + + + + + +\n");
		break;
	case 68:
		printf( " + + + # # # # # # # # # # # # # # # # # ` , . . : ; ' # ' + ` ` ` . : # + + + + + # # + + + + + + + + + + +\n");
		break;
	case 69:
		printf( " + + + # # # + # # # # # # # # # # # # # . , . . : # + + ; # ` . . ` : # # + + + + + + # + + + + + + + + + +\n");
		break;
	case 70:
		printf( " # # # # # # # # # # # # # # # # # # # # + , , , ; # ' ; + + ` . ` ` ; # + + # + # # + + + + + + + + + + + +\n");
		break;
	default:
		printf( "************************************************************************************************************\n");
		break;
	}

}


void draw_wata(int row){
	switch (row) {
	case 0:
	printf(  " ` ` ` ` ` ` ` ` ` ` `                                                   `                                      \n");
		break;
	case 1:
		printf(  " ` ` ` ` ` ` ` ` ` `   ` ` `                                           `                                                \n");
		break;
	case 2:
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `                       `                 ` `                     ` ` `  \n");  
		break;
	case 3:
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `   ` ` `   . : : : : ; ' ' : ` ` ` ` ` ` ` ` `       ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 4:
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `   ` ; : . , , : ' ' : ' ' + + , ` `   ` ` `       ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 5:
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `   : ' + . : ; @ . : . : # : + ' + ' '   ` ` ` `       ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 6:
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` `   ` ` : ' ; ; + ; ' , : : , ; ; + ; : ; ' ' ; ' : ; ` `   ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 7:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `   : ' ' # ; ' ; ; : ; ' ; : : ' ' : # # # ' + ; ; , :     ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 8:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` `   ` ; ' @ + + ; . ; ; : + # + ' : ; , ; ' + + ; ' ' ' ; ; '   ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 9:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` , ; ; # ' ; ; ' ' ; ' + : ; , . ; + ; ; ; + + ; + # : ; ' ' : ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 10:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` . ; ' + ' ; + # ; ' ; , , : : ; + + ; # # ' + ' ; + + , ; + + + ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 11:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ' ' ' ' + + + ' ' ' ; + # # + + @ # # + + + # + # ' ; : ; ; # + +     ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 12:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` + + ; # # + + + ; , ' + # # # @ # # # # # # + # + # + + , : + ' + # # ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 13:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ; + ; # # ' ; ' ' , + # # @ # # # # + + # + # + # + + # + ' ; ; + + + +   ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 14:	
		printf(  " ` ` ` ` ` ` ` ` ` `   ' # + + + + ; ; # + # + # # + + # + ' ; ; ' + ' + + + # ' ' ' + ' + + # # ` `   ` ` ` ` ` ` ` ` `\n");  
		break;
	case 15:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` + # + + ' ' # + + # + ' , , , , : : , , , : ; ' ' ' + # + ' + ; + + # # +   ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 16:	
		printf(  " ` ` ` ` ` ` ` ` `   ; + + # + + + # # # ' , , , . . . , , , , : : : ; ; ' ' ' + ' + + ' + + + + : `   ` ` ` ` ` ` ` ` `\n");  
		break;
	case 17:	
		printf(  " ` ` ` ` ` ` ` `   ` ' + # # # ' # # + ; , , , . . . . , , , , : : : : ; ; ; ' ' + ' + # + + + + @ ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 18:	
		printf(  " ` ` ` ` ` ` ` ` ` , + + @ # + # ' ' , , , . , . . , , , , , : : : : ; ; ; ; ; ' + + + ' + + + # # `   ` ` ` ` ` ` ` ` `\n");  
		break;
	case 19:	
		printf(  " ` ` ` ` ` ` ` ` : ' + # # # + ' ; , , , , , , . , . . , , , : : : ; ; ; ; ; ' ' + + + ' + + + # +   ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 20:	
		printf(  " ` ` ` ` ` ` ` ` ; + + # + + ' ' ; , , , , , , , . , , . , , : : : : ; ; ; ; ' ' ' + ' + : + + # + .   ` ` ` ` ` ` ` ` `\n");  
		break;
	case 21:	
		printf(  " ` ` ` ` ` ` `   ; ' ' + ' ' ' : : : , , , , . , . . . . . , : : : : ; : ; ; ' ' + + # ' + ' + + + ' ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 22:	
		printf(  " ` ` ` ` ` `   ` ; ' + ; + ' ; : : , , , , . ` ` ` ` . . . , , , , : : : : ; ; ' + + + ' ' ' + # + ' ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 23:	
		printf(  " ` ` ` ` ` ` ` ` ' + ; ' ' + : : : : ; , : , . . . . . . . . . , : : : ; : ; ; ' ' + + + + + ' + + + ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 24:	
		printf(  " ` ` ` ` ` ` ` ` + ; ' + # : : : ' ; ; ; ; : : , , . , . , , : ; ' + ' ' ; ' ' + + + # # # + + + ' +   ` ` ` ` ` ` ` ` `\n");  
		break;
	case 25:	
		printf(  " ` ` ` ` ` ` ` . ; ' ' # # : : ; ; : , , , , , , , , , , , , : ; ' ' ' ; ' ' ' ' + + + # # + + ' ' ' ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 26:	
		printf(  " ` ` ` ` ` ` ` ` ' ' # # # : ; : : : , , , , , , , , , , , : ; : , . . , : ; ; ' ' + + + # + ' : ' ' ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 27:	
		printf(  " ` ` ` ` ` ` ` ` ' ' + # # : ' ; : : : : ' ; : : : , , , : ' ; , ` ` , : : ; ; ' ' + + + # + + ; ' ' ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 28:	
		printf(  " ` ` ` ` ` ` ` ` ' + + # # ; + : : : + , ; + ' ; : , . . : ' : : : + ' # ' ; ; ' ' ' + # # + + + ; ; ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 29:	
		printf(  " ` ` ` ` ` ` ` ` , + + @ : : : : : : ; , # + : : , , : , ; ' : , . . # + : + ' ' ' ' ' + # # + + ' ' ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 30:	
		printf(  " ` ` ` ` ` ` ` ` ` # + ; ; : ; ; : , : , . : : : . . : , ; ; : , , , : : ; ; ; ; ; ' ' + # + + + ' ' ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 31:	
		printf(  " ` ` ` ` ` ` ` ` ` + ' ; ' ; : , : , . . . . , , , : , , ; ; : : , , . , : : : ; ; ' ' ' + # + ' ' + ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 32:	
		printf(  " ` ` ` ` ` ` ` ` ` . + ; + ; : : , , , . ` , , . . , : , ; ' ; ; , , , , : : ; ; ' ' ; ' # # + ' + + ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 33:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ; ' + ; : : , . , , , , . . , , : , ; ' ; : , , , , : ; ; ; ' ; ; ' # # ' ' + ' ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 34:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ' + ; : : , , . . . . . . , , , , ; ' ' ; , , . , , : ; ; ; ; ; ' # # ' + + ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 35:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ; # ; : , , . . . . . . , , : , , : ; ' ; : , , , : : ; ; ; ; ; ' + # + # + ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 36:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` : + ' : : , , . . . ` . , : , , , : ; + ' , : , , : : ; ; ; ; ; + + + + ' . ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 37:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` . ' + ; : , , . . . . , : : : . . : ; ' ' : , : , : : : ; ; ; ; + + + + + ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 38:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` : ; ; : : , , . . . , , , , , , : ; ' ' ' , , , : : ; ' ; ; ; + + ' ' ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 39:	
		printf(  " ` ` ` ` ` ` ` ` ` ` `   ` : ; : : , , , . , , , , ; , : : ' # + ' , , , : : ; ; ; ; ; ' ' ' ; ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 40:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ; : , , , , , , . . ; , , ' # # + ' ; , , , : : ; ; ; ; ; ' + ' . ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 41:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` + : , , , , , , . . , , . : : : ' ; ; ; , , : ; ; ; ; ; ; ' # + . ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 42:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` + ; : , , , , . , , , , , : , ; ; ; ; ; : , : : ; ; ; ; ; # @ : ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 43:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ' ; : , , , , , , : : : , , : ; ; ; ; ' ; : : ; ; ; ; ; ; # @ . ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 44:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` , + : : , , , , : : , : : : ; ; ' ' ' ' ' ; : ; ; ; ' ; ; # ; ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 45:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ; ; : : , , : : : ' ; ' ; ' # + + + ' ' ; : ; ; ' ' ; + # . ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 46:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` , : : , , : + : ; ; : : : ; ; ' ' ; ; : : ; ; ; ' ; + @ ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 47:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` : : , , , , , : : : ; ' ; ' ' ' ; ; : ; ; ' ' ; ' ; ; ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 48:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` : : : , , , , : : : : : : : ; ; ; ; : ; ; ; ' ; ; ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 49:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` : : : , , , , , : , : : : ; ; ; ; ; ; ; ' ; ' . ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 50:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ; : : : , , , , : , , : : ; ; ; : ; ; ' ' ; ' ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 51:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` + : : ; : , , , , , , , : : : ; ; ; ; ' ' ' ; '   ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 52:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` . ' : . , ' ; : , , , , : : : ; ; ; ; ; ' ' ' ; ' ' + ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 53:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` . # ` ` : . ; ' : , , : : : : : ; ; ' + + ' ' ' ' ' , @ ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 54:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` # @ ` ` ` . , ; ' ; ; ; ; ; ; ' ' + + ' ' ' ' ' ' + . # , ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 55:	
		printf(  " ` ` ` ` ` ` ` ` ` ` ` ` ` ` # # +       , , , ; ' + ' + + + + + + + ' ' ' ' ' ' ' , @ # ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 56:	
		printf(  " ` ` ` ` ` ` ` ` . . ` ` ` ' # # + `   `   , , : : , : ' + + + + + ' ' ' ' ' + # , . + # # . ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 57:	
		printf(  " ` ` ` ` ` ` ` ` ` ` . ` ' + # # @ `   `     : , , , . , : : ; ; ' ' ' ' ' + # ; , . + # # # ` ` ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 58:	
		printf(  " ` ` ` ` ` ` ` ` ` ` , # # # # # #   `       ` , , , . . . , , : ; ; ' ' + + ; , , , + # # # # . ` ` ` ` ` ` ` ` ` ` ` `\n");  
		break;
	case 59:	
		printf(  " . ` ` . . . ` . # + # + # # # # # `           ` : , , . . , , ; ; ; ' ' ' ; . , , , # # # # # # + , ` . . . ` ` ` ` ` `\n");  
		break;
	case 60:	
		printf(  " ` ` . ` . ' + + + + # # # # # # # `             ` , : , , , : ; ; ; ' ' ; ` . , , , # # # # # # + + + + ` . . ` ` ` ` `\n");  
		break;
	case 61:	
		printf(  " ` ` ; ' # + + + # # + # # # # # # + `             . , , : ; ; ; ; ' ' ' ` ` . , , , # # # # # # + + + # + + ; ` ` ` ` `\n");  
		break;
	case 62:	
		printf(  " ' ' + + + + + # # + + # # # # # # @                 . , , : : : ; ' +   ` ` . , , , # # # # # # # # # + + + + + + ' . `\n");  
		break;
	case 63:	
		printf(  " # + + + + # # # + + + # # # # # # #                 ` . : : : : ; '   ` ` ` . , , : # # # # # # # # # # + + + + + + + +\n");  
		break;
	case 64:	
		printf(  " + + + # # # # # # # # # # # # # # # ; `                 ` ; : : ;   `   ` . . , , ; # # # # # # # # # # + + + + + + + +\n");  
		break;
	case 65:	
		printf(  " + + # # # # # # # # # # # # # # # # @     `           ` ` ` ' :   ` ` ` ` . , , : ' # # # # # # # # # # # + + + + + + +\n");  
		break;
	case 66:	
		printf(  " # # # # # # # + # # # # # # # # # # # `   `                 # , + # ` ` ` . , : ; + # # # # # # # # # # # # # # # # + +\n");  
		break;
	case 67:	
		printf(  " # # # # # # # # # # # # # # # # # # # ,                   ' ; ' + + # . . . , : ; ' # # # # # # # # # # # # # # # # # #\n");  
		break;
	case 68:	
		printf(  " # # # # # # # # # # # # # # # # # # # ;                 # ; : ' ; # # + . . , : ; ; # # # # # # # # # # # # # # # # # #\n");  
		break;
	case 69:	
		printf(  " # # # # # # # # # # # # # # # # # # # #               ' ; : : ' + + # # : , , : ; ' # # # # # # # # # # # # # # # # # #\n");  
		break;
	case 70:	
		printf(  " # # # # # # # # # # # # # # # # # # # #             , # + ; : ' + + # # + , , : : # # # # # # # # # # # # # # # # # # #\n");  
		break;
	case 71:	
		printf(  " # # # # # # # # # # # # # # # # # # # # `         ` # @ + ' ; ' + # # + ; ` : : : # # # # # # # # # # # # # # # # # # #\n");  
		break;
	default :
		printf( "************************************************************************************************************************\n");
		break;

	}
}

