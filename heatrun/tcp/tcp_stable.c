/*************************************************************************
 *
 * ~~tcpの送受信のテストを行うためのプログラム~~
 *
 * テキストファイルからinitを行うためのデータなどを読み出し、
 * そのデータを元に、sendまたはrecvを複数回呼び出す。成功or失敗の判定がでます(スループットなんか出ないです)
 *
 * 使用方法：コマンドライン第1引数に、 s または c を
 *             第2引数に、 h または nh を入力してから実行する。
 *     s  : サーバ側として実行
 *          -> recvループをします
 *     c  : クライアント側として実行
 *          -> sendループをします
 *     h  : ヒートラン試験を行う(Heatrun)
 *      　　 -> 自動でsend または recv呼び出しループをやる
 *            ・・・絵が出ます
 *     nh : ヒートラン試験を行わない(NotHeatrun)
 *           -> エンターキーを押すたびにsendを呼び出す。
 *              (recvは受けですので、エンターキー押すとかの操作しないです)
 *              q を入力すると終了する
 *
 * *************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "math.h"

#include "ntma_debug.h"

#define INTERFACE "lo"
#define BUFF_SIZE 512
#define FILE_NAME "txtfile.txt"
#define PORT_NUM "12345"
#define NUM_LOOP 1000000

#define DC_FQDN "dc.ntm.jp"
#define APP_ID_STRING ""

/* テキストファイルから読み込む情報 */
typedef struct {
        uint8_t afqdn[64];   //AS FQDN
        uint8_t maddr[64];     //Mail Address
        uint8_t pw[128];     //AS PW
        uint8_t cfqdn[128];   //CN IPaddress or FQDN
        uint8_t ip[8];        //Internet Protocol(v4 or v6)
        uint8_t prot[8];      //tcp or udp
        uint8_t interface[16];  //for tcp connection
} txtdata_s;

//サーバ側の処理を行うための2つの関数を呼び出す関数
void do_tcp_svr(int ip_type);

uint32_t socket_svr_tcp(int ip_type);
int forwardloop_svr_tcp(uint32_t sock);

//クライアント側の処理を行うための2つの関数を呼び出す関数
void do_tcp_clt(char *cfqdn, int ip_ver);

uint32_t socket_clt_tcp(const uint8_t *hostnm, int ip_type);
int forwardloop_clt_tcp(uint32_t sock);

//絵が出ます
void draw_suzu(int row);
void draw_wata(int row);

int heatrun = 0;	//HeatrunTest Flag
int timeout_sec = 5;

int main(int argc, char *argv[])
{
	uint32_t sock;
	int ip_ver = 0;
	int result;
        //char *fqdn = "192.168.226.159";
        char *fqdn = "112a6cfa3ed398a5248c479ed6a061d397633a3fb4d5b9ecca274695.dc.ntm.jp";

	txtdata_s tdata;

	if (argc != 3) {
		LOG(stderr, "Usage : %s <s or c> <nh or h>", argv[0]);
		LOG(stderr, "ex:\n %s s h\n %s c nh", argv[0], argv[0]);
		exit(1);
	}
	
	if ( !(((strcmp(argv[1], "s") == 0) || (strcmp(argv[1], "c") == 0)) &&
		   ((strcmp(argv[2], "h") == 0) || (strcmp(argv[2], "nh") == 0))) ) {
		LOG(stderr, "Usage : %s <s or c> <nh or h>", argv[0]);
		LOG(stderr, "ex:\n %s s h\n %s c nh", argv[0], argv[0]);
		exit(1);
	}
	
	if (strcmp(argv[2], "h") == 0)
                heatrun = 1;

	if (strcmp(argv[1], "s") == 0)
		do_tcp_svr(ip_ver);
	else if (strcmp(argv[1], "c") == 0)
		do_tcp_clt(fqdn, ip_ver);
	
	return 0;
}

/*
 *  ~~read_info~~
 *  テキストファイルから情報を読み込み、
 *  引数のtxtdata_s *の実体に情報を入れる
 *
 *  引数:
 *    filename -> ファイル名
 *    tdata    -> テキストファイルから読み込んだ情報
 */
void read_info(char *filename, txtdata_s *tdata)
{
	FILE *f;

	f = fopen(filename, "r");
	if (f == NULL) {
		perror(filename);
		exit(1);
	}

	char tmp[BUFF_SIZE] = {'\0'};
	int i;
	
	for (i = 0; fgets(tmp, BUFF_SIZE, f) != NULL; i++) {
		int tmp_len = 0;
		tmp_len = strlen(tmp);
		tmp[tmp_len-1] = '\0';  //改行コードを書き換え
		#ifdef DEBUG
		LOG(stderr, "%d, fgetsString: %s", i, tmp);
		#endif
		switch (i) {
			case 0:
				strcpy(tdata->afqdn, tmp);
				break;
			case 1:
				strcpy(tdata->maddr, tmp);
				break;
			case 2:
				strcpy(tdata->pw, tmp);
				break;
                        case 3:
				strcpy(tdata->cfqdn, tmp);
				break; 
                        case 4:
				strcpy(tdata->ip, tmp);
				break; 
                        case 5:
				strcpy(tdata->prot, tmp);
				break;
                        case 6:
				strcpy(tdata->interface, tmp);
				break;
                        default:
				break;
		}
	}

	if (i != 7) {
		LOG(stderr, "Read File Format Error: Read File..%s", filename);
		exit(1);
	}

	#ifdef DEBUG
	LOG(stderr, "before return");
	LOG(stderr, "%s",tdata->afqdn);
	LOG(stderr, "%s",tdata->maddr);
	LOG(stderr, "%s",tdata->pw);
	LOG(stderr, "%s",tdata->cfqdn);
	LOG(stderr, "%s",tdata->ip);
	LOG(stderr, "%s",tdata->prot);
	LOG(stderr, "%s",tdata->interface);
	LOG(stderr, "read side fin");
	putchar('\n');
	#endif

	fclose(f);
}

/*
 * ~do_tcp_svr~
 * ソケット部のあれこれやら、recvのループやらをする関数を呼び出す関数
 * 引数:
 *   ip_ver: IPアドレスのバージョン
 *           0 -> IPv4
 *           1 -> IPv6
 */
void do_tcp_svr(int ip_ver)
{
	int sock = -1;
	if ((sock = socket_svr_tcp(ip_ver)) == -1 ) {
		LOG(stderr, "socket_svr_tcp():error");
		exit(1);
	}
	if (forwardloop_svr_tcp(sock) == -1) {
		LOG(stderr, "forwardloop_clt_tcp: error");
		close(sock);
		exit(1);
	}
}

/*
 * ~~ socket_svr_tcp ~~
 * ソケット部周辺のあれこれをしてくれる
 *
 * 返り値:
 *   socketで取得したファイルディスクリプタの値
 * 引数:
 *   ip_ver: IPアドレスのバージョン
 *           0 -> IPv4
 *           1 -> IPv6
 */
uint32_t 
socket_svr_tcp(int ip_type)
{
	struct addrinfo hints, *res;
	struct sockaddr_in client;
	int sock0;
	int sock;
	socklen_t len;
	int result;

	memset(&hints, 0, sizeof(hints));
	if (ip_type == 0) {
		hints.ai_family = AF_INET;
	} else {
		hints.ai_family = AF_INET6;
	}
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	hints.ai_protocol = IPPROTO_TCP;

	LOG(stderr, "getaddrinfo()");
	result = getaddrinfo(NULL, PORT_NUM, &hints, &res);
	if (result == -1) {
		LOG(stderr, "getaddrinfo: %s", gai_strerror(result));
		return -1;
	}
	if (res == NULL) {
		LOG(stderr, "Could not connect");
                return -1;
	}

	LOG(stderr, "socket()");
	sock0 = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sock0 == -1) {
		perror("socket()");
		freeaddrinfo(res);
		close(sock0);
		return -1;
	}
        
        LOG(stderr, "bind()");		//debug
	if (bind(sock0, res->ai_addr, res->ai_addrlen) < 0) {
		perror("bind");
	        freeaddrinfo(res);
		close(sock0);
		return -1;
	}
	
	LOG(stderr, "freeaddrinfo()"); //debug
	freeaddrinfo(res);
	
	LOG(stderr, "listen()");	//debug
	if (listen(sock0, 5) < 0) {
		perror("listen");
		return -1;
	}
	
	len = sizeof(client);
	LOG(stderr, "accept()");
	sock = accept(sock0, (struct sockaddr *)&client, &len);
	if (sock < 0) {
		perror("accept");
		freeaddrinfo(res);
		return -1;
	}
	
	LOG(stderr, "return sock");
	return sock;
}

/*
 * forwardloop_svr_tcp
 * "recv -> send" のループをする
 * 返り値:
 *   0 -> 正常終了
 *  -1 -> 何かしらのエラー
 * 引数:
 *   sock -> socket file descriptor
 */
int
forwardloop_svr_tcp(uint32_t sock)
{
	int slen, rlen;
	int i, c;
	char recv_buf[128];   // 通信相手からのデータ(メッセージ部と送信回数部に分かれてる)
	char msg_buf[128];    // recv_buf内の文字列部
	int recv_fin_flag = 0;     // クライアントから送られる終了フラグ(recv_buf内の数値部)

	LOG(stderr, "\nWait for the message...");
	for (i = 0;;) {
		memset(recv_buf, 0, sizeof(recv_buf));
		memset(msg_buf, 0, sizeof(msg_buf));

		if (heatrun == 1) {
			if (i == 80) i = 0;
			if ((rlen = recv(sock, recv_buf, sizeof(recv_buf), 0)) < 0) {
				perror("recv()");
				return -1;
			}
			draw_wata(i);
			++i;
			usleep(100000);
			if ((slen = send(sock, recv_buf, rlen, 0)) != rlen) {
				perror("send()");
				return -1;
			}
			continue;
		}
		LOG(stderr, "\n******************************************************");
		if ((rlen = recv(sock, recv_buf, sizeof(recv_buf), 0)) < 0) {
			perror("recv()");
			return -1;
		}
		
		//終了フラグが来てないかの確認
		sscanf(recv_buf, "%s %d", msg_buf, &recv_fin_flag);
		if (recv_fin_flag == -1) {
			LOG(stderr, "Received the flag to escape Recv loop");
			break;
		}
		LOG(stderr, "Received this message: %s", recv_buf);
		LOG(stderr, "Forwarding... ");
		if ((slen = send(sock, recv_buf, rlen, 0)) != rlen) {
			perror("send()");
			return -1;
		}
		LOG(stderr, "complete");
		LOG(stderr, "******************************************************");
		if (recv_fin_flag % 10 == 0)
			LOG(stderr, "\n~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~\n");
		LOG(stderr, "\nWait for the message...");
	}

	return 0;
}

/*
 * do_tcp_clt
 * ソケット部やら、recvのループやらの関数を呼び出す関数
 * 引数:
 *   cnfqdn: 通信相手さんのfqdn
 *   ip_ver: IPアドレスのバージョン
 *           0 -> IPv4
 *           1 -> IPv6
 */
void 
do_tcp_clt(char *cfqdn, int ip_ver)
{
	int sock;
	if ((sock = socket_clt_tcp(cfqdn, ip_ver)) == -1 ) {
		LOG(stderr, "socket_clt_tcp():error");
		LOG(stderr, "%s", cfqdn);
		exit(1);
	}
	//exit(0);
	if (forwardloop_clt_tcp(sock) == -1) {
		LOG(stderr, "forwardloop_svr_tcp: error");
		close(sock);
		exit(1);
	}

}

/*
 * ~~ socket_clt_tcp ~~
 * ソケット部周辺のあれこれをしてくれる
 *
 * 返り値:
 *   socketで取得したファイルディスクリプタの値
 * 引数:
 *   hostnm: 通信相手さんのfqdn
 *   ip_ver: IPアドレスのバージョン
 *           0 -> IPv4
 *           1 -> IPv6
 */
uint32_t 
socket_clt_tcp(const uint8_t *hostnm, int ip_type)
{
	struct addrinfo hints, *res;
	int err;
	int sock;
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	if (ip_type == 0) {
		hints.ai_family = AF_INET;
	} else {
		hints.ai_family = AF_INET6;
	}
	hints.ai_protocol = IPPROTO_TCP;
	
	LOG(stderr, "getaddrinfo()を開始します。");
	if (getaddrinfo(hostnm, PORT_NUM, &hints, &res) < 0) {
		perror("getaddrinfo()");
		return -1;
	} else {
	        LOG(stderr, "getaddrinfo()に成功しました。");
                struct in_addr addr;
                addr.s_addr = ((struct sockaddr_in *)(res->ai_addr))->sin_addr.s_addr;
                LOG(stderr, "通信相手のアドレス : %s", inet_ntoa(addr));
        }
	if (res == NULL) {
		LOG(stderr, "Couldn't connect");
		return -1;
	}

	LOG(stderr, "sockeを生成します。");
	if ((sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
		perror("socket()");
		return -1;
	}
	LOG(stderr, "sockeを生成しました。");
	
	if (ip_type == 0) {
		//must
		((struct sockaddr_in *)res->ai_addr)->sin_port = htons(12345);
	} else {
		((struct sockaddr_in6 *)res->ai_addr)->sin6_port = htons(12345);
	}

	
	LOG(stderr, "connect()を開始します。");
	if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
	        LOG(stderr, "connect()が失敗しました。");
		close(sock);
		freeaddrinfo(res);
		return -1;
        }else {
	        LOG(stderr, "connect()に成功しました。");
        }
	
	LOG(stderr, "freeaddrinfo()");
	freeaddrinfo(res);
	
	LOG(stderr, "return sock");
	return sock;
}

/*
 * forwardloop_clt_tcp
 * "send -> recv" のループをする
 * 返り値:
 *   0 -> 正常終了
 *  -1 -> 何かしらのエラー
 * 引数:
 *   sock -> 察して下さい
 */
int
forwardloop_clt_tcp(uint32_t sock)
{
	int slen, rlen;
	int i, c;
	char send_buf[128] = "GOBUSATADESU: 1";
	char msg_buf[128];     //send_buf内のメッセージ部用のバッファ(GOBUSATADESU部)
	char recv_buf[128];
	int num_of_send = 1;  //send_buf内の送信回数更新用
	int max_value = pow(2, sizeof(num_of_send)*8) / 2 - 1;  //num_of_sendの表現範囲の最大値

	fd_set fds, rfds;
	struct timeval tv;
	int retval;

	FD_ZERO(&rfds);
	FD_SET(sock, &rfds);
	
	for (i = 0;;) {
		memset(msg_buf, 0, sizeof(msg_buf));
		memset(recv_buf, 0, sizeof(recv_buf));
                
                //Heatrunする場合
		if (heatrun == 1) {
			usleep(100000);
			if (i == 80)
                                i = 0;
			if ((slen = send(sock, send_buf, sizeof(send_buf), 0)) < 0) {
                                LOG(stderr, "パケットの送信に失敗しました。");
                                perror("send()");
				return -1;
			}
			if ((rlen = recv(sock, recv_buf, slen, 0)) != slen) {
				LOG(stderr, "Packet length is not match.(sent packet & received packet)");
				return -1;
			}
			draw_suzu(i);
			++i;
			continue;
		}
		
		LOG(stderr, "Send this: %s -> Press the EnterKey", send_buf);
		LOG(stderr, "Escape from forwardloop -> enter \'q\'");
		LOG(stderr, "Enter or \'q\': ");
		while ((c = getchar()) != '\n') {
			if (c == 'q') {
				getchar();
				break;
			}
		}
		if (c == 'q') {
			sscanf(send_buf, "%s %d", msg_buf, &num_of_send);
			memset(send_buf, '\0', sizeof(send_buf));
			num_of_send = -1;  //送受信ループ終了フラグを入れる
			sprintf(send_buf, "%s %d", msg_buf, num_of_send);
			LOG(stderr, "Send termination message: %s", send_buf);
			if ((slen = send(sock, send_buf, sizeof(send_buf), 0)) < 0) {
				perror("send()");
				return -1;
			}
			LOG(stderr, "complete.");
			break;
		}
		LOG(stderr, "\n******************************************************");
		LOG(stderr, "Send this message: %s", send_buf);
		if ((slen = send(sock, send_buf, sizeof(send_buf), 0)) < 0) {
                        LOG(stderr, "パケットの送信に失敗しました。");
			perror("send()");
			return -1;
		}
		LOG(stderr, "complete");

		memcpy(&fds, &rfds, sizeof(fd_set));
		tv.tv_sec = timeout_sec;
		tv.tv_usec = 0;
		retval = select(sock+1, &fds, NULL, NULL, &tv);
		if (retval == -1) {
			perror("select()");
			return -1;
		} else if (FD_ISSET(sock, &fds)) {
			if ((rlen = recv(sock, recv_buf, slen, 0)) != slen) {
				LOG(stderr, "Packet length is not match.(sent packet & received packet)");
				return -1;
			}
			LOG(stderr, "Received this packet: %s", recv_buf);
		} else {
			LOG(stderr, "TIMEOUT!!");
			LOG(stderr, "Discontinue recvfrom()");
		}

		/*recv_buf内のデータを、メッセージ部と、送信回数部に分割し、
        送信回数をインクリメントしてからsend_bufに格納し直す*/
		sscanf(recv_buf, "%s %d", msg_buf, &num_of_send);
		++num_of_send;
		memset(send_buf, 0, sizeof(send_buf));
		sprintf(send_buf, "%s %d", msg_buf, num_of_send);

		if (num_of_send == max_value) {
			LOG(stderr, "~~~~~~~Initialize num_of_send(variable)~~~~~~~");
			num_of_send = 0;
		}
		LOG(stderr, "******************************************************\n");
		if (num_of_send % 10 == 1)
			LOG(stderr, "\n~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~+~\n");

	}

	return 0;
}

void draw_suzu(int row) {
	switch (row) {
	case 0:
		LOG(stderr, " , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , . . . , , , . . , , , . . . . . , , , , , ,");
		break;
	case 1:
		LOG(stderr, " , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , . . . , , , . . , , , . . . . . , , , , , ,");
		break;
	case 2:
		LOG(stderr, " , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , . . . , , , . . , , , . . . . . , , , , , ,");
		break;
	case 3:
		LOG(stderr, " , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , . . . . . . . . . . . . . . . . . . . . . . . .");
		break;
	case 4:
		LOG(stderr, " , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , . . . . . . . . . . . . . . . . . . . . . . . .");
		break;
	case 5:
		LOG(stderr, " , , , , , , , , , , , , , , , , , , , , , , , , , , , , , , . . . . . . . . . . . . . . . . . . . . . . . .");
		break;
	case 6:
		LOG(stderr, " , , , , , , , , , , , , , , , , , , , , , , , , . . . , , , . . . . . . . . . . . . . . . . . . . . . . . .");
		break;
	case 7:
		LOG(stderr, " , , , , , , , , , , , , , , , , , , , , , , , , . . . . . . . . . . . . . . . . . . . . . . . . . . . . . .");
		break;
	case 8:
		LOG(stderr, " , , , , , , , , , , , , , , , , , , , . . , , , , , . . , , . , . . . . . . . . . . . . . . . . . . . . . .");
		break;
	case 9:
		LOG(stderr, " , , , , , , , , , , , , , , , , , , , . , , , . , , , , ' , , , , . . . . . . . . . . . . . . . . . . . . .");
		break;
	case 10:
		LOG(stderr, " , , , , , , , , , , , , , , . . . . . , , , , , , : : ' # + + ' : . , . . . . . . . . . . . . . . . . . . .");
		break;
	case 11:
		LOG(stderr, " , , , , , , , , , , , , , , , , . . . , , . : : ' + # # + + # : : , , , . . . . . . . . . . . . . . . . . .");
		break;
	case 12:
		LOG(stderr, " , , , , , , , , , , , , , , , , , : : ' # # + # # + # # # # # # ' ' : , , . , . . . . . . . . . . . . . . .");
		break;
	case 13:
		LOG(stderr, " , , , , , , , , , , , , , , , , : # # # # # # # # # # # # # # # # # + + ; , , . . . . . . . . . . . . . . .");
		break;
	case 14:
		LOG(stderr, " , , , , , , , , . . . . . , , : ' ' + # # # # @ # # # # # # # # + # # # + ' . . . . . . . . . . . . . . . .");
		break;
	case 15:
		LOG(stderr, " , , , , , , , , . . . . . , ; + + # # @ @ @ @ # @ # # # # # # # @ # # + # + ' , . . . . . . . . . . . . . .");
		break;
	case 16:
		LOG(stderr, " , , , , , , . . . . . . . : # + # # @ @ @ @ # # @ @ # # # @ # # # # # # # + ' ; , . . . . . . . . . . . . .");
		break;
	case 17:
		LOG(stderr, " , , , , , , . . . . . . : # # # # # # # @ # # # # # # # # # # @ # # # # # # + + ' . . . . . . . . . . . . .");
		break;
	case 18:
		LOG(stderr, " , , , . . . . . . . . , ; # # # # # # # # @ @ @ @ @ @ # # @ # # # # @ # # # # + + : . . . . . . . . . . . .");
		break;
	case 19:
		LOG(stderr, " , , , . . . . . . . . . + # # # # # # # @ + # # @ @ # # # # @ @ @ # # # # # # # ' + . . . . . . . . . . . .");
		break;
	case 20:
		LOG(stderr, " , , , . . . . . . . . , @ # # # # # # # # # # # # # # # # # # # # # # # # # # # # ; , . . . . . . . . . . .");
		break;
	case 21:
		LOG(stderr, " . . . . . . . . . . . + # # # # # # # # # # # @ # # # # # # # # # @ # # # # # # # ; . . . . . . . . . . . .");
		break;
	case 22:
		LOG(stderr, " . . . . . . . . . . , ' @ # # # # @ # # @ # + + + # # # ' ; ' + # # # # # # # # # # . . . . . . . . . . . .");
		break;
	case 23:
		LOG(stderr, " . . . . . . . . . . , @ # @ # @ @ # # # + + ' ' ' ; ' ' ; ; : ' ' ' + # # # # # # # : . . . . . . . . . . .");
		break;
	case 24:
		LOG(stderr, " . . . . . . . . . . : # # @ @ @ # # # + ' ' ; ; ; ; : ; : : : : ; ; ; # + @ # # # # ' . . . . . . . . . . .");
		break;
	case 25:
		LOG(stderr, " . . . . . . . . . , ; # # @ @ @ # + ' ' ; ; ; ; ; ; : : : : : , , : : ' ' # # # # # ; . . . . . . . . . . .");
		break;
	case 26:
		LOG(stderr, " . . . . . . . . . . : @ # @ @ # # + ' ' ; ; ; ; : : : , , , , , , , : : ; + # # @ # : . . . . . . . . . . .");
		break;
	case 27:
		LOG(stderr, " . . . . . . . . . . , @ @ # @ # # + ' ' ; ; ; : : : : : : : : , , , : : : + # # # @ : . . . . . . . . . . .");
		break;
	case 28:
		LOG(stderr, " . . . . . . . . . . , @ @ @ @ # # + ' ' ; ; : : : : : : : : , , , , , : : + # # # # : . . . . . . . . . . .");
		break;
	case 29:
		LOG(stderr, " . . . . . . . . . . , @ @ @ @ # + ' ' ' ' ; : : : : , , , , , , , , : , : ; # # # # , . . . . . . . . . . .");
		break;
	case 30:
		LOG(stderr, " . . . . . . . . . . , ' @ @ # # ' ' ' ' ' + + ' ; : : , : : : ; ; : : : : : ' @ # # . . . . . . . . . . . .");
		break;
	case 31:
		LOG(stderr, " . . . . . . . . . . . + # # # + # # + + + + + ' ' ; ; : ; ; ; ; : : , : : , : # # + . . . . . . . . . . . .");
		break;
	case 32:
		LOG(stderr, " . . . . . . . . . . . ' # @ # ' # + ' ; ; ; ; : ; + + ' ' ; ; ; : : : : ; , ' @ @ + . . . . . . . . . . . .");
		break;
	case 33:
		LOG(stderr, " . . . . . . . . . . . + ' ' @ + + ' ' # ; # + # ; + ' ; ' ; ' # + ; + ; ' ; + ' , : . . . . . . . . . . . .");
		break;
	case 34:
		LOG(stderr, " . . . . . . . . . . : ' ; + @ ' ' ; ' ; ; ; ; ; ' + : : , : ; ; ; : , ; ; ; : # , : , . . . . . . . . . . .");
		break;
	case 35:
		LOG(stderr, " . . . . . . . . . . ; ' ; + # ' ; ; ; ; : : : ; ' ' : , : , , : : , , , : , : # . , : . . . . . . . . . . .");
		break;
	case 36:
		LOG(stderr, " . . . . . . . . . . , ' ' + @ ' ; ; ; ; : ; : ; ' ; : , , : , , , , , , , , : # , , : ` . . . . . . . . . .");
		break;
	case 37:
		LOG(stderr, " . . . . . . . . . . . ' ' + # ' ; ; ; : : : ; ' ' ; : , , , , , , , , , , , : ' : , , ` ` . . . . . . . . .");
		break;
	case 38:
		LOG(stderr, " . . . . . . . . . . . + ' ' # ' ' ; ; : : : ; ' ' ; : , , : , , , , , , , , : ; : , , ` ` ` ` ` ` . . . . .");
		break;
	case 39:
		LOG(stderr, " . . . . . . . . . . . , ' ; ' ' ' ' ; ; : : : ' ; ; : , , , : , , , , , , : ; , , , . ` ` ` ` ` ` ` ` . . .");
		break;
	case 40:
		LOG(stderr, " . . . . . . . . ` ` ` . : ; ' ' ' ' ; ; ; : ; + ' # ' ; ' ; : : , , , , , : ; , . , ` ` ` ` ` ` ` ` ` ` ` `");
		break;
	case 41:
		LOG(stderr, " . . . . . . ` ` ` ` ` ` ` . ; + ' ' ; ; ; : ; ' ' ' ; ; , : , , , , , , , : : , . ` ` ` ` ` ` ` ` ` ` ` ` `");
		break;
	case 42:
		LOG(stderr, " . . . . . ` ` ` ` ` ` ` ` ` . . ; ' ' ; ; ; ; ; ; ; , ; , , , , , , , , : : : ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");
		break;
	case 43:
		LOG(stderr, " . . . ` ` ` ` ` ` ` ` ` ` ` ` . ; ; ' ' ; ; ; ; ; : , : , , , : : , : : : : : ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");
		break;
	case 44:
		LOG(stderr, " . ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` # ' ' ' ; ; + ' ' ' ' ; ; ; : ; : : : : : : ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");
		break;
	case 45:
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` : ; ' ' ' ; + + ' ; : : : : ; ; ; : : : : , ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");
		break;
	case 46:
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` , ' ' ' ; ; ' ' ' ; ; ; : , , : : : : , ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");
		break;
	case 47:
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ' ' ' ; ' ' ; ; ; : : , , , : : : ; ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");
		break;
	case 48:
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ' ' ' ' ' ' ; ; : : : , : : : : ; : ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");
		break;
	case 49:
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ' ' ' ' ' ' ; ; ; : : , : , : ; : : ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");
		break;
	case 50:
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ' ' ' ' ' + ' ' ; ; : ; ; : ; : : : ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");
		break;
	case 51:
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `   ' ' ' ' + + + + ' ' ' ' ' ; : : , :         ` ` ` ` ` ` ` ` ` ` ` ` ` `");
		break;
	case 52:
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` . ; ' ' ' ' + + + + + + ' ; : : , , , `                   ` ` ` ` ` ` ` `");
		break;
	case 53:
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` `   . : ' ' ' ' ' ' ' ' ' ' ; : : : , , , : ` `                 ` ` ` ` ` ` ` `");
		break;
	case 54:
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` `         ` @ : : ' ; ' ' ' ' ' ; : : : : : , , , . ` #                     ` ` ` ` ` `");
		break;
	case 55:
		LOG(stderr, " ` ` ` ` ` ` ` ` ` `         , + @ : : : ' ; ' ' ' ' ; : : : : , , , : ` ` # #                         ` ` `");
		break;
	case 56:
		LOG(stderr, " ` ` ` ` ` ` ` `     ` `   ' # # # : : : , ' ; ' ; ; ; ; : : : : , :   ` ` # + + `                          ");
		break;
	case 57:
		LOG(stderr, " ` ` ` ` ` `   `   `   . # # # # # : : : , . : ; ; ; ; : : , , , :   `   ` # + + + '                        ");
		break;
	case 58:
		LOG(stderr, " ` ` ` ` ` ` `   ` # + + + @ # # # ; : : , . . . : : : : : , : ,         ` + # + + + ' + `                  ");
		break;
	case 59:
		LOG(stderr, " ` ` `     ` . # + + # # # # # # # # : : , , . ` ` ; : : : ; .           . + + + + + ' ' ' + `              ");
		break;
	case 60:
		LOG(stderr, " ` ` `   , # + + + # # # # # # # # # ; : , , . ` ` ` ; : ; , `         ` ; + + + + + + + + + ' + .          ");
		break;
	case 61:
		LOG(stderr, "   . ' + + + + + # # # # # # # # # # + ; : , , . ' ; : : ; ' +         ` @ + + + + + + + + + + ' ' ' :      ");
		break;
	case 62:
		LOG(stderr, " ' + + + + + + + # # + # # # # # # # # : : , , ' # ; ' ; # # ' #     ` ` + + + + + + + + + + + + ' ' ' ' '  ");
		break;
	case 63:
		LOG(stderr, " + + + + + + + + # # # # # # # # # # # : , : , # # # + ; ' ' + ' ' ` ` . + + + + + + + + # # + + + + + + ' '");
		break;
	case 64:
		LOG(stderr, " + + + + + + + # # # # # # # # # # # # ; , : , ; # # # + + ' + ; ; ` ` : + # + + + + + + + + + + + + + + + '");
		break;
	case 65:
		LOG(stderr, " + + + + + + + # # # # # # # # # # # # ' . : , ; ; # + ' + # , ; ; : ` ' + + + + + + + + + + + + + + + + + +");
		break;
	case 66:
		LOG(stderr, " + + + + + + + # # # # # # # # # # # # + : . , , ; ' + + ' , . , : ; ` + # # + + + + + + + + + + + + + + + +");
		break;
	case 67:
		LOG(stderr, " + + + + + + # # # # # # # # # # # # # @ ; . . . ; : + + ' ` ` . . : ` # + # + + + + + + + + + + + + + + + +");
		break;
	case 68:
		LOG(stderr, " + + + # # # # # # # # # # # # # # # # # ` , . . : ; ' # ' + ` ` ` . : # + + + + + # # + + + + + + + + + + +");
		break;
	case 69:
		LOG(stderr, " + + + # # # + # # # # # # # # # # # # # . , . . : # + + ; # ` . . ` : # # + + + + + + # + + + + + + + + + +");
		break;
	case 70:
		LOG(stderr, " # # # # # # # # # # # # # # # # # # # # + , , , ; # ' ; + + ` . ` ` ; # + + # + # # + + + + + + + + + + + +");
		break;
	default:
		LOG(stderr, "************************************************************************************************************");
		break;
	}

}


void draw_wata(int row){
	switch (row) {
	case 0:
	LOG(stderr, " ` ` ` ` ` ` ` ` ` ` `                                                   `                                      ");
		break;
	case 1:
		LOG(stderr, " ` ` ` ` ` ` ` ` ` `   ` ` `                                           `                                                ");
		break;
	case 2:
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `                       `                 ` `                     ` ` `  ");  
		break;
	case 3:
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `   ` ` `   . : : : : ; ' ' : ` ` ` ` ` ` ` ` `       ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 4:
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `   ` ; : . , , : ' ' : ' ' + + , ` `   ` ` `       ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 5:
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `   : ' + . : ; @ . : . : # : + ' + ' '   ` ` ` `       ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 6:
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` `   ` ` : ' ; ; + ; ' , : : , ; ; + ; : ; ' ' ; ' : ; ` `   ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 7:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `   : ' ' # ; ' ; ; : ; ' ; : : ' ' : # # # ' + ; ; , :     ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 8:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` `   ` ; ' @ + + ; . ; ; : + # + ' : ; , ; ' + + ; ' ' ' ; ; '   ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 9:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` , ; ; # ' ; ; ' ' ; ' + : ; , . ; + ; ; ; + + ; + # : ; ' ' : ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 10:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` . ; ' + ' ; + # ; ' ; , , : : ; + + ; # # ' + ' ; + + , ; + + + ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 11:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ' ' ' ' + + + ' ' ' ; + # # + + @ # # + + + # + # ' ; : ; ; # + +     ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 12:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` + + ; # # + + + ; , ' + # # # @ # # # # # # + # + # + + , : + ' + # # ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 13:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ; + ; # # ' ; ' ' , + # # @ # # # # + + # + # + # + + # + ' ; ; + + + +   ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 14:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` `   ' # + + + + ; ; # + # + # # + + # + ' ; ; ' + ' + + + # ' ' ' + ' + + # # ` `   ` ` ` ` ` ` ` ` `");  
		break;
	case 15:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` + # + + ' ' # + + # + ' , , , , : : , , , : ; ' ' ' + # + ' + ; + + # # +   ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 16:	
		LOG(stderr, " ` ` ` ` ` ` ` ` `   ; + + # + + + # # # ' , , , . . . , , , , : : : ; ; ' ' ' + ' + + ' + + + + : `   ` ` ` ` ` ` ` ` `");  
		break;
	case 17:	
		LOG(stderr, " ` ` ` ` ` ` ` `   ` ' + # # # ' # # + ; , , , . . . . , , , , : : : : ; ; ; ' ' + ' + # + + + + @ ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 18:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` , + + @ # + # ' ' , , , . , . . , , , , , : : : : ; ; ; ; ; ' + + + ' + + + # # `   ` ` ` ` ` ` ` ` `");  
		break;
	case 19:	
		LOG(stderr, " ` ` ` ` ` ` ` ` : ' + # # # + ' ; , , , , , , . , . . , , , : : : ; ; ; ; ; ' ' + + + ' + + + # +   ` ` ` ` ` ` ` ` ` `");  
		break;
	case 20:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ; + + # + + ' ' ; , , , , , , , . , , . , , : : : : ; ; ; ; ' ' ' + ' + : + + # + .   ` ` ` ` ` ` ` ` `");  
		break;
	case 21:	
		LOG(stderr, " ` ` ` ` ` ` `   ; ' ' + ' ' ' : : : , , , , . , . . . . . , : : : : ; : ; ; ' ' + + # ' + ' + + + ' ` ` ` ` ` ` ` ` ` `");  
		break;
	case 22:	
		LOG(stderr, " ` ` ` ` ` `   ` ; ' + ; + ' ; : : , , , , . ` ` ` ` . . . , , , , : : : : ; ; ' + + + ' ' ' + # + ' ` ` ` ` ` ` ` ` ` `");  
		break;
	case 23:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ' + ; ' ' + : : : : ; , : , . . . . . . . . . , : : : ; : ; ; ' ' + + + + + ' + + + ` ` ` ` ` ` ` ` ` `");  
		break;
	case 24:	
		LOG(stderr, " ` ` ` ` ` ` ` ` + ; ' + # : : : ' ; ; ; ; : : , , . , . , , : ; ' + ' ' ; ' ' + + + # # # + + + ' +   ` ` ` ` ` ` ` ` `");  
		break;
	case 25:	
		LOG(stderr, " ` ` ` ` ` ` ` . ; ' ' # # : : ; ; : , , , , , , , , , , , , : ; ' ' ' ; ' ' ' ' + + + # # + + ' ' ' ` ` ` ` ` ` ` ` ` `");  
		break;
	case 26:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ' ' # # # : ; : : : , , , , , , , , , , , : ; : , . . , : ; ; ' ' + + + # + ' : ' ' ` ` ` ` ` ` ` ` ` `");  
		break;
	case 27:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ' ' + # # : ' ; : : : : ' ; : : : , , , : ' ; , ` ` , : : ; ; ' ' + + + # + + ; ' ' ` ` ` ` ` ` ` ` ` `");  
		break;
	case 28:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ' + + # # ; + : : : + , ; + ' ; : , . . : ' : : : + ' # ' ; ; ' ' ' + # # + + + ; ; ` ` ` ` ` ` ` ` ` `");  
		break;
	case 29:	
		LOG(stderr, " ` ` ` ` ` ` ` ` , + + @ : : : : : : ; , # + : : , , : , ; ' : , . . # + : + ' ' ' ' ' + # # + + ' ' ` ` ` ` ` ` ` ` ` `");  
		break;
	case 30:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` # + ; ; : ; ; : , : , . : : : . . : , ; ; : , , , : : ; ; ; ; ; ' ' + # + + + ' ' ` ` ` ` ` ` ` ` ` `");  
		break;
	case 31:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` + ' ; ' ; : , : , . . . . , , , : , , ; ; : : , , . , : : : ; ; ' ' ' + # + ' ' + ` ` ` ` ` ` ` ` ` `");  
		break;
	case 32:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` . + ; + ; : : , , , . ` , , . . , : , ; ' ; ; , , , , : : ; ; ' ' ; ' # # + ' + + ` ` ` ` ` ` ` ` ` `");  
		break;
	case 33:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ; ' + ; : : , . , , , , . . , , : , ; ' ; : , , , , : ; ; ; ' ; ; ' # # ' ' + ' ` ` ` ` ` ` ` ` ` `");  
		break;
	case 34:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ' + ; : : , , . . . . . . , , , , ; ' ' ; , , . , , : ; ; ; ; ; ' # # ' + + ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 35:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ; # ; : , , . . . . . . , , : , , : ; ' ; : , , , : : ; ; ; ; ; ' + # + # + ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 36:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` : + ' : : , , . . . ` . , : , , , : ; + ' , : , , : : ; ; ; ; ; + + + + ' . ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 37:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` . ' + ; : , , . . . . , : : : . . : ; ' ' : , : , : : : ; ; ; ; + + + + + ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 38:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` : ; ; : : , , . . . , , , , , , : ; ' ' ' , , , : : ; ' ; ; ; + + ' ' ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 39:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` `   ` : ; : : , , , . , , , , ; , : : ' # + ' , , , : : ; ; ; ; ; ' ' ' ; ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 40:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ; : , , , , , , . . ; , , ' # # + ' ; , , , : : ; ; ; ; ; ' + ' . ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 41:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` + : , , , , , , . . , , . : : : ' ; ; ; , , : ; ; ; ; ; ; ' # + . ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 42:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` + ; : , , , , . , , , , , : , ; ; ; ; ; : , : : ; ; ; ; ; # @ : ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 43:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ' ; : , , , , , , : : : , , : ; ; ; ; ' ; : : ; ; ; ; ; ; # @ . ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 44:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` , + : : , , , , : : , : : : ; ; ' ' ' ' ' ; : ; ; ; ' ; ; # ; ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 45:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ; ; : : , , : : : ' ; ' ; ' # + + + ' ' ; : ; ; ' ' ; + # . ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 46:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` , : : , , : + : ; ; : : : ; ; ' ' ; ; : : ; ; ; ' ; + @ ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 47:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` : : , , , , , : : : ; ' ; ' ' ' ; ; : ; ; ' ' ; ' ; ; ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 48:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` : : : , , , , : : : : : : : ; ; ; ; : ; ; ; ' ; ; ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 49:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` : : : , , , , , : , : : : ; ; ; ; ; ; ; ' ; ' . ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 50:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ; : : : , , , , : , , : : ; ; ; : ; ; ' ' ; ' ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 51:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` + : : ; : , , , , , , , : : : ; ; ; ; ' ' ' ; '   ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 52:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` . ' : . , ' ; : , , , , : : : ; ; ; ; ; ' ' ' ; ' ' + ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 53:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` . # ` ` : . ; ' : , , : : : : : ; ; ' + + ' ' ' ' ' , @ ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 54:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` # @ ` ` ` . , ; ' ; ; ; ; ; ; ' ' + + ' ' ' ' ' ' + . # , ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 55:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` ` ` ` ` # # +       , , , ; ' + ' + + + + + + + ' ' ' ' ' ' ' , @ # ` ` ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 56:	
		LOG(stderr, " ` ` ` ` ` ` ` ` . . ` ` ` ' # # + `   `   , , : : , : ' + + + + + ' ' ' ' ' + # , . + # # . ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 57:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` . ` ' + # # @ `   `     : , , , . , : : ; ; ' ' ' ' ' + # ; , . + # # # ` ` ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 58:	
		LOG(stderr, " ` ` ` ` ` ` ` ` ` ` , # # # # # #   `       ` , , , . . . , , : ; ; ' ' + + ; , , , + # # # # . ` ` ` ` ` ` ` ` ` ` ` `");  
		break;
	case 59:	
		LOG(stderr, " . ` ` . . . ` . # + # + # # # # # `           ` : , , . . , , ; ; ; ' ' ' ; . , , , # # # # # # + , ` . . . ` ` ` ` ` `");  
		break;
	case 60:	
		LOG(stderr, " ` ` . ` . ' + + + + # # # # # # # `             ` , : , , , : ; ; ; ' ' ; ` . , , , # # # # # # + + + + ` . . ` ` ` ` `");  
		break;
	case 61:	
		LOG(stderr, " ` ` ; ' # + + + # # + # # # # # # + `             . , , : ; ; ; ; ' ' ' ` ` . , , , # # # # # # + + + # + + ; ` ` ` ` `");  
		break;
	case 62:	
		LOG(stderr, " ' ' + + + + + # # + + # # # # # # @                 . , , : : : ; ' +   ` ` . , , , # # # # # # # # # + + + + + + ' . `");  
		break;
	case 63:	
		LOG(stderr, " # + + + + # # # + + + # # # # # # #                 ` . : : : : ; '   ` ` ` . , , : # # # # # # # # # # + + + + + + + +");  
		break;
	case 64:	
		LOG(stderr, " + + + # # # # # # # # # # # # # # # ; `                 ` ; : : ;   `   ` . . , , ; # # # # # # # # # # + + + + + + + +");  
		break;
	case 65:	
		LOG(stderr, " + + # # # # # # # # # # # # # # # # @     `           ` ` ` ' :   ` ` ` ` . , , : ' # # # # # # # # # # # + + + + + + +");  
		break;
	case 66:	
		LOG(stderr, " # # # # # # # + # # # # # # # # # # # `   `                 # , + # ` ` ` . , : ; + # # # # # # # # # # # # # # # # + +");  
		break;
	case 67:	
		LOG(stderr, " # # # # # # # # # # # # # # # # # # # ,                   ' ; ' + + # . . . , : ; ' # # # # # # # # # # # # # # # # # #");  
		break;
	case 68:	
		LOG(stderr, " # # # # # # # # # # # # # # # # # # # ;                 # ; : ' ; # # + . . , : ; ; # # # # # # # # # # # # # # # # # #");  
		break;
	case 69:	
		LOG(stderr, " # # # # # # # # # # # # # # # # # # # #               ' ; : : ' + + # # : , , : ; ' # # # # # # # # # # # # # # # # # #");  
		break;
	case 70:	
		LOG(stderr, " # # # # # # # # # # # # # # # # # # # #             , # + ; : ' + + # # + , , : : # # # # # # # # # # # # # # # # # # #");  
		break;
	case 71:	
		LOG(stderr, " # # # # # # # # # # # # # # # # # # # # `         ` # @ + ' ; ' + # # + ; ` : : : # # # # # # # # # # # # # # # # # # #");  
		break;
	default :
		LOG(stderr, "************************************************************************************************************************");
		break;

	}
}


