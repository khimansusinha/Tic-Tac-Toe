#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
//#include <netdb.h>
#include "client.h"

void read_conf_file(char *file_name)
{
        FILE *fp;
        char buffer[MAXLINE];
        char *token;

        fp = fopen(file_name, "r");
        if(!fp) {
                printf("Error in opening input conf file: %s\n", file_name);
                goto out;
        }
        fscanf(fp, "%s", buffer);
        token = strtok(buffer, "=\n");
        if (strcmp(token, "SERVER_IP")) {
                printf("Invalid string in client.conf file, it should be SERVER_IP=<ip-adress>\n");
                goto out;
        }
        while(token != NULL) {
                token = strtok(NULL, "=\n");
                if (token == NULL) break;
                memcpy(server_ip, token, strlen(token));
        }
        memset(buffer, 0, sizeof(buffer));
        fscanf(fp, "%s", buffer);
        token = strtok(buffer, "=\n");
        if (strcmp(token, "SERVER_PORT")) {
                printf("Invalid string in client.conf file, it should be SERVER_PORT=<server-port-number\n");
                goto out;
        }
        while(token != NULL) {
                token = strtok(NULL, "=\n");
                if (token == NULL) break;
                server_port = atoi(token);
        }

out:
        fclose(fp);

        return;
}

void print_error(const char *msg)
{
	printf("msg\n");
	exit(0);
}

void recv_str_from_server(int sockfd, char *str)
{
	memset(str, 0, 4);
	int n = read(sockfd, str, 3);
	if (n < 0 || n != 3) {
		print_error("ERROR reading message from server socket.");
	}

	//printf("[DEBUG] Received message: %s\n", str);
}

int recv_int_from_server(int sockfd)
{
	int msg = 0;
	int n = read(sockfd, &msg, sizeof(int));
	if (n < 0 || n != sizeof(int))  {
		print_error("ERROR reading int from server socket");
	}

	//printf("[DEBUG] Received int: %d\n", msg);

	return msg;
}

void write_to_server_int(int sockfd, int msg)
{
	int n = write(sockfd, &msg, sizeof(int));
	if (n < 0) {
		print_error("ERROR writing int to server socket");
	}

	//printf("[DEBUG] Wrote int to server: %d\n", msg);
}

int connect_to_server()
{
	struct sockaddr_in serv_addr;
	//struct hostent *server;

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		print_error("ERROR opening socket for server.");
	}

	//server = gethostbyname(hostname);
	//if (server == NULL) {
	//	fprintf(stderr,"ERROR, no such host\n");
	//	exit(0);
	//}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(server_port);
	//serv_addr.sin_addr.s_addr = INADDR_ANY;
	if (inet_pton(AF_INET, server_ip, &serv_addr.sin_addr) <= 0) {
		printf("Error in inet_pton\n");
		exit(1);
	}

	//memmove(serv->h_addr, &serv_addr.sin_addr.s_addr, serv->h_length);
	//serv_addr.sin_port = htons(portno); 

	if (connect(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		print_error("ERROR connecting to server");

	//printf("[DEBUG] Connected to server.\n");
	return sockfd;
}

void draw_board(char board[][3])
{
	printf(" %c | %c | %c \n", board[0][0], board[0][1], board[0][2]);
	printf("-----------\n");
	printf(" %c | %c | %c \n", board[1][0], board[1][1], board[1][2]);
	printf("-----------\n");
	printf(" %c | %c | %c \n", board[2][0], board[2][1], board[2][2]);
}

void clear_input_buffer() {
	fd_set readfds;
	struct timeval timeout;
	int ready;

	// Set up the file descriptor set
	FD_ZERO(&readfds);
	FD_SET(STDIN_FILENO, &readfds);

	// Set timeout to zero to avoid waiting
	timeout.tv_sec = 0;
	timeout.tv_usec = 0;

	// Check if there's data in stdin
	ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
	if (ready > 0) {
		// If data is available, read and discard it
		int c;
		while ((c = getchar()) != '\n' && c != EOF);
	}
}

void take_turn(int sockfd)
{
	char buffer[10];

	while (1) {

		clear_input_buffer();
		printf("Enter 0-8 to make a move, or 9 for number of active players: ");
		fgets(buffer, 10, stdin);
		int move = buffer[0] - '0';
		if (move <= 9 && move >= 0){
			printf("\n");
			write_to_server_int(sockfd, move);   
			break;
		} 
		else {
			printf("\nInvalid input. Try again.\n");
		}
	}
}

void get_update(int sockfd, char board[][3])
{
	//take two values from the server one by one, first player id and next move, note each game two player id 0 & 1
	int player_id = recv_int_from_server(sockfd);
	int move = recv_int_from_server(sockfd);
	board[move/3][move%3] = player_id ? 'X' : 'O';    
}


int main(int argc, char *argv[])
{
	if (argc != 2) {
		printf("Please provide server.conf as input of server program\n");
		exit(1);
	}
	read_conf_file(argv[1]);

	int sockfd = connect_to_server();

	//block to get client id from the server, after accept your request server sends you your client ID
	int id = recv_int_from_server(sockfd);

	//printf("[DEBUG] Client ID: %d\n", id);

	char msg[4];
	char board[3][3] = { {' ', ' ', ' '}, 
                             {' ', ' ', ' '}, 
                             {' ', ' ', ' '} };

	printf("Tic-Tac-Toe\n------------\n");

	/*
	 * keep looping till you find "SRT" command from the server to start the game
	 * the server sends "SRT" command only when it finds two player connected to the server
	 */

	do {
		/*
		 * block on read to get "HLD" or "SRT" command in form of th string msg from the server
		 */

		recv_str_from_server(sockfd, msg);
		if (!strcmp(msg, "HLD")) {
			//received "HLD" message from the server i.e. wait till next player or oponent connects to the server
			printf("Waiting for a second player...\n");
		}

	} while ( strcmp(msg, "SRT") );

	/* The game has begun. */
	printf("Game on!\n");

	//client ID:1 has symbol X, while client ID 0 has symbol O
	printf("Your are %c's\n", id ? 'X' : 'O');

	//draw board for the client
	draw_board(board);

	//Now the game starts, keep looping till you find "WIN", "LSE" or "DRW" command string message from the server
	while(1) {

		/*
		 * client blocks on receive command in form of string message from the server
		 * Note, one client doesn't sends message directly to another client, while
		 * one client sends message to another client via server and server maintains the state of the both the clients.
		 */

		recv_str_from_server(sockfd, msg);

		if (!strcmp(msg, "TRN")) { 
			printf("Your move...\n");
			take_turn(sockfd);
		}
		else if (!strcmp(msg, "INV")) { 
			printf("That position has already been played. Try again.\n"); 
		}
		else if (!strcmp(msg, "CNT")) { 
			int num_players = recv_int_from_server(sockfd);
			printf("There are currently %d active players.\n", num_players); 
		}
		else if (!strcmp(msg, "UPD")) { 
			get_update(sockfd, board);
			draw_board(board);
		}
		else if (!strcmp(msg, "WAT")) { 
			printf("Waiting for other players move...\n");
		}
		else if (!strcmp(msg, "WIN")) { 
			printf("You win!\n");
			break;
		}
		else if (!strcmp(msg, "LSE")) { 
			printf("You lost.\n");
			break;
		}
		else if (!strcmp(msg, "DRW")) { 
			printf("Draw.\n");
			break;
		}
		else { 
			print_error("Unknown message.");
		}
	}

	printf("Game over.\n");
	close(sockfd);
	return 0;
}
