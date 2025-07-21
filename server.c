#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <signal.h>
#include "server.h"

int no_of_players = 0;
pthread_mutex_t m_np;
int server_sockfd;

void signal_handler(int signum) {
	close(server_sockfd);
	exit(0);
}

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

        memset(buffer, 0, sizeof(buffer));
        fscanf(fp, "%s", buffer);
        token = strtok(buffer, "=\n");
        if (strcmp(token, "SERVER_PORT")) {
                printf("Invalid string in client.conf file, it should be SERVER_PORT=<server-port-number>\n");
                goto out;
        }
        while(token != NULL) {
                token = strtok(NULL, "=\n");
                if (token == NULL) break;
                server_port = atoi(token);
        }
        printf("server port number is %d\n", server_port);
out:
        fclose(fp);

        return;
}

void print_error(const char *msg)
{
	printf("msg\n");
	pthread_exit(NULL);
}

void write_to_client_str(int client_sockfd, char *str)
{
	int n = write(client_sockfd, str, strlen(str));
	if (n < 0) {
		print_error("ERROR writing msg to client socket");
	}
}

void write_to_both_client_str(int *client_sockfd, char *str)
{
	write_to_client_str(client_sockfd[0], str);
	write_to_client_str(client_sockfd[1], str);
}

void write_to_client_int(int client_sockfd, int msg)
{
	int n = write(client_sockfd, &msg, sizeof(int));
	if (n < 0) {
		print_error("ERROR writing int to client socket");
	}
}

void write_to_both_client_int(int *client_sockfd, int msg)
{
    write_to_client_int(client_sockfd[0], msg);
    write_to_client_int(client_sockfd[1], msg);
}

int recv_int_from_client(int client_sockfd)
{
	int msg = 0;
	int n = read(client_sockfd, &msg, sizeof(int));

	if (n < 0 || n != sizeof(int))  return -1;

	//printf("[DEBUG] Received int: %d\n", msg);

	return msg;
}

int srv_socket_bind_to_listen(int portno)
{
	int sockfd;
	struct sockaddr_in serv_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		print_error("ERROR opening listener socket.");
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;	
	serv_addr.sin_addr.s_addr = INADDR_ANY;	
	serv_addr.sin_port = htons(portno);		

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		print_error("ERROR binding listener socket.");
	}

	//printf("[DEBUG] Listener set.\n");    

	return sockfd;
}

void get_both_client_accept(int srv_sockfd, int *client_sockfd)
{
	socklen_t client_len;
	struct sockaddr_in serv_addr, client_addr;

	//printf("[DEBUG] Listening for clients...\n");

	int num_conn = 0;
	while(num_conn < 2)
	{
		/*
		 * listen for the two client connect request in this function, we always needed one pair to play the game
		 * 2nd parameter is for backlog client requests i.e. no of backlog clients supported by the server in its queue
		 */

		listen(srv_sockfd, 253 - no_of_players);

		memset(&client_addr, 0, sizeof(client_addr));
		client_len = sizeof(client_addr);
		client_sockfd[num_conn] = accept(srv_sockfd, (struct sockaddr *) &client_addr, &client_len);
		if (client_sockfd[num_conn] < 0) {
			print_error("ERROR accepting a connection from a client.");
		}

		//printf("[DEBUG] Accepted connection from client %d\n", num_conn);

		//write num_conn to client
		write(client_sockfd[num_conn], &num_conn, sizeof(int));

		//printf("[DEBUG] Sent client %d it's ID.\n", num_conn); 

		pthread_mutex_lock(&m_np);
		no_of_players++;
		printf("Number of players is now %d.\n", no_of_players);
		pthread_mutex_unlock(&m_np);

		if (num_conn == 0) {
			//get first client request now send him to hold till secodn player join to start the game
			write_to_client_str(client_sockfd[0],"HLD");

			//printf("[DEBUG] Told client 0 to hold.\n");
		}
		num_conn++;
	}
}

int get_player_move(int client_sockfd)
{
	//printf("[DEBUG] Getting player move...\n");

	//write a "TRN" message to client, so that client can send his move to the server and then srv can send it to 2ndplayer
	//write_to_client_str(client_sockfd, "TRN");

	//recv the move in term of integer value between 0-8 from the player
	return recv_int_from_client(client_sockfd);
}

int check_player_move(char board[][3], int move, int player_id)
{
	if ((move == 9) || (board[move/3][move%3] == ' ')) { 
		//printf("[DEBUG] Player %d's move was valid.\n", player_id);
		return 1;
	}
	else {
		//printf("[DEBUG] Player %d's move was invalid.\n", player_id);
		return 0;
	}
}

void update_board(char board[][3], int move, int player_id)
{
	board[move/3][move%3] = player_id ? 'X' : 'O';

	//printf("[DEBUG] Board updated.\n");
}

void draw_board(char board[][3])
{
	printf(" %c | %c | %c \n", board[0][0], board[0][1], board[0][2]);
	printf("-----------\n");
	printf(" %c | %c | %c \n", board[1][0], board[1][1], board[1][2]);
	printf("-----------\n");
	printf(" %c | %c | %c \n", board[2][0], board[2][1], board[2][2]);
}

void send_update_to_both_client(int * client_sockfd, int move, int player_id)
{
	//printf("[DEBUG] Sending update...\n");

	//first send "UPD" msg to both the clients
	write_to_both_client_str(client_sockfd, "UPD");

	//send player id to both the clients
	write_to_both_client_int(client_sockfd, player_id);

	//third send one's client move to both the client
	write_to_both_client_int(client_sockfd, move);

	//printf("[DEBUG] Update sent.\n");
}

void send_player_count(int client_sockfd)
{
	//send total player count at any given time to one of the client who hae requested for this

	//first send "CNT" message
	write_to_client_str(client_sockfd, "CNT");

	//second send total no of player count
	pthread_mutex_lock(&m_np);
	int np = no_of_players;
	pthread_mutex_unlock(&m_np);

	write_to_client_int(client_sockfd, np);

	//printf("[DEBUG] Player Count Sent.\n");
}

int check_board(char board[][3], int last_move)
{
	//printf("[DEBUG] Checking for a winner...\n");

	int row = last_move/3;
	int col = last_move%3;

	if ( board[row][0] == board[row][1] && board[row][1] == board[row][2] ) { 
		//printf("[DEBUG] Win by row %d.\n", row);
		return 1;
	}
	else if ( board[0][col] == board[1][col] && board[1][col] == board[2][col] ) { 
		//printf("[DEBUG] Win by column %d.\n", col);
		return 1;
	}
	else if (!(last_move % 2)) {
		if ( (last_move == 0 || last_move == 4 || last_move == 8) 
			&& (board[1][1] == board[0][0] && board[1][1] == board[2][2]) ) {
			//printf("[DEBUG] Win by backslash diagonal.\n");
			return 1;
		}
		if ( (last_move == 2 || last_move == 4 || last_move == 6) 
			&& (board[1][1] == board[0][2] && board[1][1] == board[2][0]) ) {
			//printf("[DEBUG] Win by forwardslash diagonal.\n");
			return 1;
		}
	}

	//printf("[DEBUG] No winner, yet.\n");
	return 0;
}

void *run_game(void *thread_data) 
{
	int *client_sockfd = (int*)thread_data; 
	char board[3][3] = { {' ', ' ', ' '},  
			     {' ', ' ', ' '}, 
			     {' ', ' ', ' '} };

	printf("Game on!\n");

	//send to both the players "SRT" message
	write_to_both_client_str(client_sockfd, "SRT");
	//printf("[DEBUG] Sent start message.\n");

	//draw board for server
	draw_board(board);

	int prev_player_turn = 1;
	int player_turn = 0;
	int game_over = 0;
	int turn_count = 0;

	while(!game_over) {

		/*
		 * send "WAT" to 2nd player to wait at start, let 1st player start the game first
		 * next iteration the first player waits, so send "WAT" to 1st player, for the second player's move
		 * so int first iteration the 2nd player gets the "WAT" and waits and
		 * in second iteration the 1st player gets the "WAT" and waits and
		 * in third iteration again the first player gets the "WAT" and waits and
		 * in fourth iteration again the 2nd player gets the "WAT" and waits and so on..
		 * we can achieve it by  taking two variables one keep the last state and one keeps the current state
		 * if last state is 1 and current state is 0, so the (cur state + 1 % 2) gets "WAT" i.e. the 2nd player
		 * in next iteration the current state = last state and last state = cur state, so current state is 1 now
		 * so (cur state + 1 % 2) i.e. 0 i.e. 1st player gets the "WAT" and so on.....
		 * because you have only two socket fd in server for two players for one game so you have to give turn
		 * to one player at a time so client_sockfd[0][1], 0 index for 1st player and 1 index for 2nd player
		 * so one time you have to send "WAT" to one and get move from the other and in iteration you have to get
		 * from the other and send "WAT" to the first one and so on, so we need to keep switching between these
		 * in each iteration.
		 */
		if (prev_player_turn != player_turn) {
			write_to_client_str(client_sockfd[(player_turn + 1) % 2], "WAT");
		}

		int valid = 0;
		int move = 0;

		while(!valid) {

			fd_set sockfds;
			int maxfd;
			int res;

			FD_ZERO(&sockfds);
			FD_SET(client_sockfd[player_turn], &sockfds);
			FD_SET(client_sockfd[prev_player_turn], &sockfds);
			maxfd = client_sockfd[player_turn] > client_sockfd[prev_player_turn] ? client_sockfd[player_turn] : client_sockfd[prev_player_turn];

			/*
			 * select i.e. block for event on both the sockets as any one player can close the game any time
			 * so we need to do the game over immediately
			 *
			 * first send to player_turn the "TRN" command strign message, now block on select to get the reponse
			 * the use of select is because if waiting in turn client terminates will not be able to handle
			 * withot select, because we are trying to read only from player_turn socket fd and not from other
			 */

			write_to_client_str(client_sockfd[player_turn], "TRN");

			res = select(maxfd + 1, &sockfds, NULL, NULL, NULL);
			if (res < 0) {
				print_error("Error: select error.\n");
				break;
			} else {
				if (FD_ISSET(client_sockfd[player_turn], &sockfds) ){
					//printf("DEBUG: activity on player %d.\n", player_turn);
					move = get_player_move(client_sockfd[player_turn]);
				}
				else if(FD_ISSET(client_sockfd[prev_player_turn], &sockfds) ) {
					//printf("DEBUG: activity on player %d.\n", prev_player_turn);
					move = get_player_move(client_sockfd[prev_player_turn]);
				}
			}
			//keep looping till you don't get valid move from the current player or the player has turn now
			//move = get_player_move(client_sockfd[player_turn]);

			if (move == -1) break;

			printf("Player %d played position %d\n", player_turn, move);

			//check if the player move was valid
			valid = check_player_move(board, move, player_turn);
			if (!valid) { 
				printf("Move was invalid. Let's try this again...\n");
				write_to_client_str(client_sockfd[player_turn], "INV");
			}
		}

		if (move == -1) {
			//if anyone of the player inside a pair presses Ctr-C, game over 
			printf("Player disconnected.\n");
			break;
		}
		else if (move == 9) {
			//player wantes to toal numbers of players are playign at this time
			prev_player_turn = player_turn;
			send_player_count(client_sockfd[player_turn]);
		}
		else {
			//server update its own board, to maintain the statefull nature fo rthe server
			update_board(board, move, player_turn);

			//server send move updates to both the cients/players
			send_update_to_both_client( client_sockfd, move, player_turn );

			//server draw its one board
			draw_board(board);

			//check if the game is over because of the current move.
			game_over = check_board(board, move);
			if (game_over == 1) {
				//send one player to "WIN" and another player to "LSE"
				write_to_client_str(client_sockfd[player_turn], "WIN");
				write_to_client_str(client_sockfd[(player_turn + 1) % 2], "LSE");
				printf("Player %d won.\n", player_turn);
			}
			else if (turn_count == 8) {
				//send both the players "DRW"
				printf("Draw.\n");
				write_to_both_client_str(client_sockfd, "DRW");
				game_over = 1;
			}

			//toggle both the variables so that we cans send "WAT" to one and can get move from other
			prev_player_turn = player_turn;
			player_turn = (player_turn + 1) % 2;

			//increment the total turn count for this game, server maintains the state of both the clients
			turn_count++;
		}
	}

	printf("Game over.\n");

	close(client_sockfd[0]);
	close(client_sockfd[1]);

	pthread_mutex_lock(&m_np);
	no_of_players--;
	printf("Number of players is now %d.", no_of_players);
	no_of_players--;
	printf("Number of players is now %d.", no_of_players);
	pthread_mutex_unlock(&m_np);

	free(client_sockfd);
	pthread_exit(NULL);
}


int main(int argc, char *argv[])
{   
	if (argc != 2) {
		printf("Please provide server.conf as input of server program\n");
		exit(1);
	}
	read_conf_file(argv[1]);

	//to handle Ctrl-C in server for smooth termination of server
	signal(SIGINT, signal_handler);

	server_sockfd = srv_socket_bind_to_listen(server_port); 
	pthread_mutex_init(&m_np, NULL);

	while (1) {
		if (no_of_players <= 252) {

			//total number of players 252 i.e. 128 game in parallel we are supporting
			int *client_sockfd = (int*) malloc(2 * sizeof(int)); 
			memset(client_sockfd, 0, 2 * sizeof(int));

			//now get both players accept, in one iteration two players i.e. one game
			get_both_client_accept(server_sockfd, client_sockfd);

			//printf("[DEBUG] Starting new game thread...\n");

			/*
			 * create a async thread inside the server to serv two players who gets connected in above call
			 * in each iteration we get two players and then spin one thread which handles both the players
			 * one server async thread handles two players to perform the game.
			 * we are supporting 128 parallel games i.e. 128 async can spin if 128 games are getting played
			 * in parallel. i.e. one thread one game i.e. handles two players
			 */

			pthread_t thread;
			int result = pthread_create(&thread, NULL, run_game, (void *)client_sockfd);
			if (result){
				printf("Thread creation failed with return code %d\n", result);
				exit(-1);
			}
			//printf("[DEBUG] New game thread started.\n");
		}
	}

	close(server_sockfd);
	pthread_mutex_destroy(&m_np);

	return 0;
}
