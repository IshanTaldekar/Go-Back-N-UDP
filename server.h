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
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netdb.h>
#include <iostream>
#include <sys/errno.h>

using namespace std;

#define WINDOW_SIZE 7
#define MAX_BUFFER_LENGTH 38
#define MAX_SEQUENCE_NUMBERS 8

struct talker_variables
{
    int socket_fd;
    struct addrinfo* p;
};

struct listener_variables
{
    int socket_fd;
    struct addrinfo *p;
};

