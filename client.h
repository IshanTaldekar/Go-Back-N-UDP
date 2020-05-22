/*

 CSE 4153 - DATA COMMUNICATION NETWORKS: Programming Assignment 2

 * Author:
   Name: Ishan Taldekar
   Net-id: iut1
   Student-id: 903212069

 * Description:
   An implementation of the Go-Back-N Automatic Repeat reQuest (ARQ) protocol. This data link layer protocol
   uses a sliding window method for reliable and sequential delivery of data frames. The GBN is a sliding
   window protocol with a send window size of N and a receiving window size of 1.
   (refer: https://www.tutorialspoint.com/a-protocol-using-go-back-n)

 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <netdb.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/mman.h>
#include <netinet/in.h>
#include <iostream>
#include <sys/errno.h>
#include <list>
#include <map>

using namespace std;

#define WINDOW_SIZE 7
#define MAX_BUFFER_LENGTH 38
#define MAX_SEQUENCE_NUMBERS 8

struct talker_variables {
    int socket_fd;
    struct addrinfo *p;
};

struct listener_variables {
    int socket_fd;
    struct addrinfo *p;
};

struct client_state {

    bool full_window = false;
    bool resend_window = false;
    bool empty_window = true;
    bool send_eot = false;
    bool do_nothing = false;

    bool server_sent_eot_flag = false;
    bool eof_encountered_flag = false;
    bool update_state_flag = true;
    bool instream_error_state_flag = false;
    bool clear_instream_error_flag = false;
    bool verbose_flag = false;

    int window_base = 0;
    int outstanding_acknowledgements = 0;
    int total_unique_packets_acknowledged = 0;
    int total_unique_packets_sent = 0;
    int next_sequence_number = 0;
    int current_file_seek = 0;
    int last_acked_file_seek = -1;
    int total_packets_in_file = 0;

};