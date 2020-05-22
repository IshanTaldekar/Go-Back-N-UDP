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


#include "server.h"
#include "packet.cpp"

struct listener_variables listener;
struct talker_variables talker;

bool verbose_flag = false;


/*

   To ensure reliable transmission, the GBN server should behave as follows:

     1. The server should verify that the packet received is in order by checking its sequence number.

     2. If the packet is in order, the server should send an acknowledgement packet back to the client with the
        sequence number of the received packet.

     3. If the packet is out of order, the server should discard the packet and resend an acknowledgement for the most
        recently received in-order packet.

     4. After the server has received all data packets and an End-Of-Transmission (EOT) packet from the client, it
        should send an EOT packet with the type field set to 2, and then exit.

 */

int driver(char *file_name) {

    ofstream destination_file(file_name), arrlog_file("arrival.log");
    int num_bytes, expected_sequence_number = 0;
    char buffer[MAX_BUFFER_LENGTH], payload[MAX_BUFFER_LENGTH], send_payload[30];
    struct sockaddr_storage client_addr;
    socklen_t addr_len;

    packet *received_packet = new packet(-1, -1, -1, payload);
    bool first_iteration = true, termination_flag = false;

    while (!termination_flag) {

        if (verbose_flag)
        {
            if (!first_iteration) {
                cout << "===================================================" << endl << endl << endl << endl;
            }

            if (verbose_flag) cout << "===================================================" << endl << endl;
            first_iteration = false;
        }

        if (verbose_flag) cout << "[STATE]: Server is listening" << endl << endl;
        if (verbose_flag) cout << "Expected Sequence Number: " << expected_sequence_number << endl << endl;

        // Wait for the first packet to arrive.
        if ((num_bytes = recvfrom(listener.socket_fd, buffer, sizeof(buffer), 0,
                                  (struct sockaddr *) &client_addr,
                                  &addr_len)) == -1) {
            perror("(server) error when calling recvfrom\n");
            exit(EXIT_FAILURE);
        }

        received_packet->deserialize(buffer);

        if (verbose_flag) cout << "[STATE]: Packet with sequence number " <<  received_packet->getSeqNum() << " received" << endl << endl;

        // Check if the packet is received in the correct order.
        if (received_packet->getSeqNum() == expected_sequence_number) {

            if (verbose_flag) cout << "Packet in the correct order" << endl << endl;

            // Check if its a data packet, and perform the appropriate actions if it is.
            if (received_packet->getType() == 1) {

                destination_file << received_packet->getData();
                arrlog_file << received_packet->getSeqNum() << endl;

                packet *acknowledgement = new packet(0, received_packet->getSeqNum(), 0, NULL);
                acknowledgement->serialize(payload);
                delete acknowledgement;

                // Send a message to the client socket using UDP datagrams.
                if ((num_bytes = sendto(talker.socket_fd, payload, strlen(payload), 0, talker.p->ai_addr,
                                        talker.p->ai_addrlen)) == -1) {
                    perror("(server) error when calling sendto\n");
                    exit(1);
                }

                if (verbose_flag) cout << "[STATE]: Acknowledgement of packet sent to Client" << endl << endl;

                expected_sequence_number = (expected_sequence_number + 1) % MAX_SEQUENCE_NUMBERS;

            } else {

                // If the incoming packet is an EOT packet, send an EOT back and close connection.
                if (received_packet->getType() == 3) {

                    if (verbose_flag) cout << "[STATE]: Server received an EOT packet" << endl << endl;

                    arrlog_file << received_packet->getSeqNum() << endl;

                    packet *acknowledgement = new packet(2, received_packet->getSeqNum(), 0, send_payload);
                    acknowledgement->serialize(payload);
                    delete acknowledgement;

                    // Send a message to the client socket using UDP datagrams.
                    if ((num_bytes = sendto(talker.socket_fd, payload, strlen(payload), 0, talker.p->ai_addr,
                                            talker.p->ai_addrlen)) == -1) {
                        perror("(server) error when calling sendto\n");
                        exit(1);
                    }

                    if (verbose_flag) cout << "[STATE]: Acknowledgement of EOT sent to Client" << endl;

                    if (verbose_flag) cout << endl << "===================================================" << endl;
                    termination_flag = true;
                }
            }
        }
        // If the incoming packet is out of order, then resend an acknowledgement for the last in-order packet.
        else {

            if (verbose_flag) cout << "[STATE]: Packet is out of order" << endl << endl;

            packet *acknowledgement = new packet(0, expected_sequence_number, 0, NULL);
            acknowledgement->serialize(payload);
            delete acknowledgement;

            // Send a message to the client socket using UDP datagrams.
            if ((num_bytes = sendto(talker.socket_fd, payload, strlen(payload), 0, talker.p->ai_addr,
                                    talker.p->ai_addrlen)) == -1) {
                perror("(server) error when calling sendto\n");
                exit(1);
            }

            if (verbose_flag) cout << "[STATE]: Acknowledgement of the last in-order packet sent" << endl;

        }
    }

    delete received_packet;
    arrlog_file.close();
    return 0;
}

void initialize_talker(char *host_name, char *server_port) {

    int getaddrinfo_call_status;
    int counter = 0;
    struct addrinfo hints, *server_info;

    // loading up address structs with getaddrinfo():
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;    // use IPv4
    hints.ai_socktype = SOCK_DGRAM;    // UDP sockets

    // get information about a host name and/or service and load up a struct sockaddr with the result.
    if ((getaddrinfo_call_status = getaddrinfo(host_name, server_port, &hints, &server_info)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(getaddrinfo_call_status));
        exit(EXIT_FAILURE);
    }

    // loop through all the results and make a socket
    for (talker.p = server_info; talker.p != NULL; talker.p = talker.p->ai_next) {

        // make a socket using socket call and ensure that it has run error-free
        if ((talker.socket_fd = socket(talker.p->ai_family, (talker.p)->ai_socktype, talker.p->ai_protocol)) == -1) {
            perror("(client) error during socket creation");
            continue;
        }

        break;
    }

    if (talker.p == NULL) {
        fprintf(stderr, "client: failed to create datagram socket\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(server_info);  // the server_info structure is no longer needed
}

void initialize_listener(char *listen_port) {

    int getaddrinfo_call_status;
    struct addrinfo hints, *server_info;
    int yes = 1;

    // loading up address structs with getaddrinfo():
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;  // use IPv4
    hints.ai_socktype = SOCK_DGRAM;  // UDP sockets
    hints.ai_flags = AI_PASSIVE;  // fill in my IP for me

    // get information about a host name and/or service and load up a struct sockaddr with the result.
    if ((getaddrinfo_call_status = getaddrinfo(NULL, listen_port, &hints, &server_info)) != 0) {
        fprintf(stderr, "(client) error when calling getaddrinfo in listener: %s\n",
                gai_strerror(getaddrinfo_call_status));
        exit(EXIT_FAILURE);
    }

    // loop through all the results and bind to the first we can
    for (listener.p = server_info; listener.p != NULL; listener.p = listener.p->ai_next) {

        // make a socket using socket() call, and ensure that it ran error-free
        if ((listener.socket_fd = socket(listener.p->ai_family, listener.p->ai_socktype,
                                         listener.p->ai_protocol)) == -1) {
            perror("(client) error during listener's socket creation");
            continue;
        }

        // helps avoid the "Address already in use" error message.
        if (setsockopt(listener.socket_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("(client) error when calling setsockopt in listener");
            exit(EXIT_FAILURE);
        }

        // associate a socket with an IP address and port number using the bind() call, and make sure it ran error-free.
        if (bind(listener.socket_fd, listener.p->ai_addr, listener.p->ai_addrlen) == -1) {
            close(listener.socket_fd);
            perror("(client) error when binding socket in listener");
            continue;
        }


        break;
    }

    if (listener.p == NULL) {
        fprintf(stderr, "client: failed to bind socket in listener\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(server_info);  // the server_info structure is no longer needed
}

int main(int argc, char *argv[]) {

    char *host_name, *port1, *port2, *file_name;

    // ensure that entries required are provided at run-time.
    if (argc != 5) {
        fprintf(stderr,
                "usage: server <emulatorName: host address of the emulator> <sendToEmulator: UDP port number-\n");
        fprintf(stderr,
                "-used by the emulator to receive data from the server> <receiveFromEmulator: UDP port number-\n");
        fprintf(stderr,
                "-by the server to receive ACKs for the emulator> <fileName: name of the file to be transferred>\n");
        exit(EXIT_FAILURE);
    }

    host_name = argv[1];
    port1 = argv[2];
    port2 = argv[3];
    file_name = argv[4];

    char user_input;
    cout << endl << "Verbose? (Yes: y \\ No: n):" << endl;
    cin >> user_input;

    if (user_input == 'y'){
        verbose_flag = true;
    }
    cout << endl;

    initialize_listener(port1);
    initialize_talker(host_name, port2);

    if (driver(file_name) != 0) {
        fprintf(stderr, "TERMINATED\n");
        exit(EXIT_FAILURE);
    } else {
        exit(EXIT_SUCCESS);
    }
    close(listener.socket_fd);
    close(talker.socket_fd);

    return 0;

}