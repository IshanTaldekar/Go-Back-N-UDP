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

#include "client.h"
#include "packet.cpp"

struct talker_variables talker;
struct listener_variables listener;
struct client_state state;

struct sockaddr recv_from;

/*

   To ensure reliable transmission, the GBN client should behave as follows:

     1. Should the client have a packet to send, it first checks if the window is full. If the window is not full, the
        packet is sent and the appropriate variables are updated.

     2. The client should make use of a single timer set for the oldest transmitted-but-not-yet-acknowledged packet.

     3. If the receiver times out, the client should resend all packets that have been previously sent but that have
        not been acknowledged. If an ACK is received corresponding to an unacknowledged packet within the window, the
        timer should be restarted. If there are no outstanding packets, the timer should be stopped.

     4. When the client receives an acknowledgement with sequence number n, the acknowledgement should be treated as a
        cumulative acknowledgement, indicating that all packets with sequence number up to and including n have been
        correctly received at the server.

     5. The client should fill the window and then obtain an acknowledgement and wait to obtain an acknowledgement.
        When that acknowledgement comes in, it should check to see if the window has space and send again. Then, wait
        for an acknowledgement again, and so on. This ensures that the client's window always keeps full. (This is an
        assignment requirement)

 */


int driver(char *file_name) {

    ifstream source_file(file_name);
    ofstream seqlog_file("clientseqnum.log"), acklog_file("clientack.log");
    char send_packet_data[30], buffer[MAX_BUFFER_LENGTH], payload[MAX_BUFFER_LENGTH];
    char received_payload[30];
    int seek_offset, num_bytes, send_packet_data_length, ack_sequence_number, last_sequence_number = 0;

    struct sockaddr client_addr;
    socklen_t addr_len;

    list<int> window_number_sequence;
    list<int> window_file_seek_sequence;

    struct sockaddr* ptr = talker.p->ai_addr;
    recv_from = *ptr;
    ptr = &recv_from;

    // Count the total number of characters in the input file.
    source_file.seekg(0, source_file.end);
    int characters_in_file = source_file.tellg();
    source_file.seekg(0, source_file.beg);

    // Compute the total number of packets that can be created.
    state.total_packets_in_file = characters_in_file / 30;
    if (characters_in_file % 30 != 0) {
        state.total_packets_in_file++;
    }

    if (state.verbose_flag) {
        cout << "File data can be broken down into " << state.total_packets_in_file << " packets" << endl << endl;
    }

    // GBN sender must respond to three types of events: [EVENT 1] Invocation from above, [EVENT 2] Receipt of an ACK,
    // and [EVENT 3] A timeout event.
    while (!state.server_sent_eot_flag) {

        // Updates relevant state variables
        if (state.update_state_flag) {

            if (state.outstanding_acknowledgements == WINDOW_SIZE) {
                state.full_window = true;
            } else {
                state.full_window = false;
            }

            if (state.outstanding_acknowledgements == 0) {
                state.empty_window = true;
            } else {
                state.empty_window = false;
            }
        } else {
            state.update_state_flag = true;
        }

        // If the failbit or badbit flags are set, remove the flag(s) and allow further operations
        if (source_file.fail()) {
            source_file.clear();
        }

        // If the do_nothing flag is set, the client stop transmitting data but actively listens for acknowledgements.
        // In this state, any incoming acknowledgements are discarded.
        if (!state.do_nothing) {

            if (state.verbose_flag) {

            if (state.total_unique_packets_sent != 0) {
                cout << endl << "===================================================" << endl << endl << endl << endl;
            }

            cout << "===================================================" << endl;
            cout << "Full Window: " << state.full_window << endl;
            cout << "Empty Window: " << state.empty_window << endl;
            cout << "Resend Window: " << state.resend_window << endl;
            cout << "Do Nothing: " << state.do_nothing << endl;
            cout << "Send EOT: " << state.send_eot << endl;
            cout << "Acknowledgements Pending: " << state.outstanding_acknowledgements << endl;
            cout << "Total Acknowledgements: " << state.total_unique_packets_acknowledged << endl;
            cout << "Total Packets sent: " << state.total_unique_packets_sent << endl;
            cout << "EOF encountered: " << state.eof_encountered_flag << endl;
            cout << "File seek: " << state.current_file_seek << endl;
            cout << "Window Base: " << state.window_base << endl;
            cout << "..................................................." << endl << endl;

            }

            // [Event 1]: Checks if window is full, if it isn't full a packet is created and sent. The window is kept
            // full before listening for acknowledgements.

            // The window cannot be empty. If there is data left to send, make packets and sent them until window full.
            if (state.empty_window && state.outstanding_acknowledgements == 0) {

                if (state.verbose_flag) cout << "[STATE]: Window is empty" << endl << endl;

                int packet_sequence_number = state.window_base % MAX_SEQUENCE_NUMBERS;
                int sequence_number_outside_window = (state.window_base + WINDOW_SIZE) % MAX_SEQUENCE_NUMBERS;
                int file_seek = state.current_file_seek;

                // While the window isn't full and there is data to send, make a packet and send it.
                while (packet_sequence_number != sequence_number_outside_window && !state.eof_encountered_flag) {

                    memset(&payload[0], '\0', sizeof(payload));
                    memset(&send_packet_data[0], '\0', sizeof(send_packet_data));

                    // Read a appropriate chunk of data from the file.
                    source_file.seekg(file_seek * sizeof(send_packet_data));
                    source_file.read(send_packet_data, sizeof(send_packet_data));

                    // If the number of characters read is less than the packet data size available, either EOF has
                    // been reached or there is a file stream error.
                    if (source_file.gcount() < sizeof(send_packet_data)) {
                        state.eof_encountered_flag = true;
                        state.update_state_flag = true;

                        if (source_file.gcount() == 0) {
                            state.instream_error_state_flag = true;
                            break;
                        }
                    }

                    packet *send_packet = new packet(1, packet_sequence_number, sizeof(send_packet_data),
                                                     send_packet_data);  // new operator used for dynamic memory allocation.
                    send_packet->serialize(payload);
                    delete send_packet;  // delete operator deallocates send_packet from heap memory.

                    // Send a message to the server socket using UDP datagrams.
                    if ((num_bytes = sendto(talker.socket_fd, payload, sizeof(payload), 0,
                                            (const sockaddr *) &recv_from,
                                            sizeof(recv_from))) == -1) {
                        perror("(client) error when calling sendto:");
                        exit(EXIT_FAILURE);
                    }

                    if (state.verbose_flag) {
                        cout << "Client sent a packet with sequence number " << packet_sequence_number << endl << endl;
                    }

                    // Write the packet's sequence number to the log file
                    seqlog_file << packet_sequence_number << endl;

                    // Add the current file seek and sequence number combo to their respective lists.
                    window_number_sequence.push_back(packet_sequence_number);
                    window_file_seek_sequence.push_back(file_seek);

                    packet_sequence_number = (packet_sequence_number + 1) % MAX_SEQUENCE_NUMBERS;
                    state.outstanding_acknowledgements++;
                    state.total_unique_packets_sent++;
                    file_seek++;
                }
                state.next_sequence_number = packet_sequence_number;
                state.current_file_seek = file_seek;
            }

            // The code block below verifies that the window has space before sending new packets keeping window full.
            if (!state.full_window && !state.empty_window && !state.eof_encountered_flag) {

                if (state.verbose_flag) cout << "[STATE]: Window is neither full nor empty" << endl << endl;
                int packet_sequence_number = state.next_sequence_number % MAX_SEQUENCE_NUMBERS;
                int file_seek = state.current_file_seek;

                while (!state.eof_encountered_flag && state.outstanding_acknowledgements < WINDOW_SIZE) {

                    memset(&send_packet_data[0], '\0', sizeof(send_packet_data));

                    // Read a appropriate chunk of data from the file.
                    source_file.seekg(file_seek * sizeof(send_packet_data));
                    source_file.read(send_packet_data, sizeof(send_packet_data));

                    // If the number of characters read from source file is less than the packet data size expected,
                    // then end of file flag is set
                    if (source_file.gcount() < sizeof(send_packet_data)) {
                        state.eof_encountered_flag = true;
                    }

                    packet *send_packet = new packet(1, packet_sequence_number, sizeof(send_packet_data),
                                                     send_packet_data);  // new operator used for dynamic memory allocation.

                    send_packet->serialize(payload);
                    delete send_packet;  // delete operator deallocates send_packet from heap memory.

                    // Send a message to the server socket using UDP datagrams.
                    if ((num_bytes = sendto(talker.socket_fd, payload, sizeof(payload), 0,
                                            (const sockaddr *) &recv_from,
                                            sizeof(recv_from))) == -1) {
                        perror("(client) error when calling sendto:");
                        exit(EXIT_FAILURE);
                    }

                    if (state.verbose_flag) {
                        cout << "Client sent a packet with sequence number " << packet_sequence_number << endl << endl;
                    }

                    // Write the packet's sequence number to the log file
                    seqlog_file << packet_sequence_number << endl;

                    // Add the current file seek and sequence number combo to their respective lists.
                    window_number_sequence.push_back(packet_sequence_number);
                    window_file_seek_sequence.push_back(file_seek);

                    packet_sequence_number = (packet_sequence_number + 1) % MAX_SEQUENCE_NUMBERS;
                    state.outstanding_acknowledgements++;
                    state.total_unique_packets_sent++;
                    file_seek++;
                }
                state.next_sequence_number = packet_sequence_number;
                state.current_file_seek = file_seek;
            }

            // In case of a timeout, all packets with outstanding acknowledgements are retransmitted to the server.
            if (state.resend_window) {

                if (state.verbose_flag) cout << "[STATE]: Window will resend" << endl << endl;

                list<int>::iterator packet_number = window_number_sequence.begin();
                list<int>::iterator seek_at = window_file_seek_sequence.begin();

                int packet_sequence_number;
                int file_seek;

                // Resend sequence number and packet data until interator points to Null.
                while (packet_number != window_number_sequence.end()) {

                    packet_sequence_number = *packet_number;
                    file_seek = *seek_at;

                    memset(&send_packet_data[0], '\0', sizeof(send_packet_data));

                    // Read a appropriate chunk of data from the file.
                    source_file.seekg(file_seek * sizeof(send_packet_data));
                    source_file.read(send_packet_data, sizeof(send_packet_data));

                    // If the number of characters read from source file is less than the packet data size expected,
                    // then end of file flag is set
                    if (source_file.gcount() < sizeof(send_packet_data)) {
                        state.eof_encountered_flag = true;
                    }

                    packet *send_packet = new packet(1, packet_sequence_number, sizeof(send_packet_data),
                                                     send_packet_data);  // new operator used for dynamic memory allocation.

                    send_packet->serialize(payload);
                    delete send_packet;  // delete operator deallocates send_packet from heap memory.

                    // Send a message to the server socket using UDP datagrams.
                    if ((num_bytes = sendto(talker.socket_fd, payload, sizeof(payload), 0,
                                            (const sockaddr *) &recv_from,
                                            sizeof(recv_from))) == -1) {
                        perror("(client) error when calling sendto:");
                        exit(EXIT_FAILURE);
                    }

                    if (state.verbose_flag) {
                        cout << "Client sent a packet with sequence number " << packet_sequence_number << endl << endl;
                    }

                    packet_number++;
                    seek_at++;
                }
                state.resend_window = false;
            }
        }

        memset(&buffer[0], '\0', sizeof(buffer));

        // [Event 2]: Waits for acknowledgement from server.
        num_bytes = recvfrom(listener.socket_fd, buffer, MAX_BUFFER_LENGTH - 1, 0,
                (struct sockaddr *) &client_addr, &addr_len);

        // [Event 3]: A timeout event when packets are lost or overly delayed. All unacknowledged packets will be
        // resent to the server. The resend_window is raised.
        if (num_bytes == -1) {

            if (errno == EAGAIN || errno == EWOULDBLOCK) {

                if (state.verbose_flag) {
                    cout << "[STATE]: Timeout occured when waiting for acknowledgement" << endl << endl;
                }

                if (state.outstanding_acknowledgements > 0) {
                    if (!state.do_nothing) state.resend_window = true;
                } else {
                    if(state.do_nothing) {
                        state.do_nothing = false;
                    }

                    if (state.eof_encountered_flag && state.total_unique_packets_acknowledged == state.total_packets_in_file) {
                        state.send_eot = true;
                        state.do_nothing = true;
                    }
                }
            } else {
                perror("(client) error when calling recvfrom");
                exit(EXIT_FAILURE);
            }
        } else {

            packet *acknowledgement = new packet(0, 0, 0, received_payload);
            acknowledgement->deserialize(buffer);
            ack_sequence_number = acknowledgement->getSeqNum();
            delete acknowledgement;

            // Discard any incoming acknowledgements, if do_nothing flag is set.
            if (state.do_nothing) {
                if (state.verbose_flag) {
                    cout << "[STATE]: Acknowledgement for packet " << ack_sequence_number << " out of order. ";
                    cout << " Acknowledgement dropped" << endl << endl;
                }
                continue;
            }

            if (state.verbose_flag) {
                cout << "[STATE]: Client received an acknowledgement for packet " << ack_sequence_number << endl << endl;
            }

            // Add acknowledged sequence number to log file.
            acklog_file << ack_sequence_number << endl;

            int packets_acknowledged = 0;
            bool acknowledgement_in_window = false;

            // Checks the current window for the acknowledged sequence number and counts the number of packets that
            // are acknowledged cumulatively.
            for (list<int>::iterator element = window_number_sequence.begin(); element != window_number_sequence.end(); element++) {

                packets_acknowledged++;

                if (*element == ack_sequence_number)
                {
                    acknowledgement_in_window = true;
                    break;
                }

                if (packets_acknowledged >= WINDOW_SIZE) {
                    packets_acknowledged = 0;
                    break;
                }
            }

            if (state.total_unique_packets_sent == state.total_packets_in_file && state.outstanding_acknowledgements < WINDOW_SIZE - 1)
            {
                if (ack_sequence_number == state.total_unique_packets_sent % MAX_SEQUENCE_NUMBERS) {
                    acknowledgement_in_window = true;
                    packets_acknowledged = window_number_sequence.size();
                }
            }

            // If a packet is acknowledged then the window's base is incremented and the appropriate state values are
            // updated.
            if (acknowledgement_in_window) {

                for (int counter = 0; counter < packets_acknowledged; counter++) {

                    if (state.verbose_flag) {
                        if (state.verbose_flag) {
                            int packet_number = window_number_sequence.front();
                            cout << "Packet with sequence number " << packet_number << " acknowledged" << endl << endl;
                        }
                    }

                    state.window_base = (state.window_base + 1) % MAX_SEQUENCE_NUMBERS;
                    state.total_unique_packets_acknowledged++;
                    state.outstanding_acknowledgements--;

                    window_file_seek_sequence.pop_front();
                    window_number_sequence.pop_front();
                }
            } else {

                // If the acknowledgement was outside the current window, the window must slide back and resend.
                if (state.verbose_flag) {
                    cout << "[STATE]: Acknowledgement for a packet outside current window, window slides back";
                    cout << endl << endl;
                }

                state.empty_window = true;
                state.eof_encountered_flag = false;
                state.update_state_flag = true;
                state.full_window = false;
                state.do_nothing = true;
                state.outstanding_acknowledgements = 0;
                state.total_unique_packets_acknowledged--;
                state.total_unique_packets_sent = state.total_unique_packets_acknowledged;
                state.window_base = ack_sequence_number;
                state.current_file_seek = state.total_unique_packets_acknowledged;
                state.next_sequence_number = 0;
                window_number_sequence.clear();
                window_file_seek_sequence.clear();
            }
        }

        // If there are no outstanding acknowledgements, and there is no new data to read from the source file, then
        // send end-of-transmission packet and close the connection after performing the appropriate tasks.
        if (state.send_eot) {

            if (state.verbose_flag) cout << "[STATE]: Transmission complete, sending EOT to server" << endl << endl;

            memset(&send_packet_data[0], '\0', sizeof(send_packet_data));
            memset(&buffer[0], '\0', sizeof(buffer));

            packet *send_packet = new packet(3, state.window_base % MAX_SEQUENCE_NUMBERS, 0, NULL);
            send_packet->serialize(payload);

            // Send an EOT packet to the server over UDP datagrams.
            if ((num_bytes = sendto(talker.socket_fd, payload, sizeof(payload), 0, &recv_from,
                                    sizeof(recv_from))) == -1) {
                perror("(client) error when calling sendto\n");
                exit(1);
            }

            if (state.verbose_flag) {
                cout << "Client sent an EOT packet with sequence number " << state.window_base << endl << endl;
            }

            // Update log file with EOT sequence number
            seqlog_file << state.window_base % MAX_SEQUENCE_NUMBERS << endl;

            // Wait for an EOT packet from the server.
            if ((num_bytes = recvfrom(listener.socket_fd, buffer, MAX_BUFFER_LENGTH - 1, 0,
                                      (struct sockaddr *) &client_addr, &addr_len)) == -1) {
                perror("(client) error when calling recvfrom");
                exit(EXIT_FAILURE);
            }

            packet *acknowledgement = new packet(0, 0, 0, received_payload);
            acknowledgement->deserialize(buffer);

             // If an EOT packet is received, terminate connection.
            if (acknowledgement->getType() == 2) {

                if (state.verbose_flag) {
                    cout << "Client received an EOT packet with sequence number " << state.window_base << endl << endl;
                    cout << "===================================================" << endl;
                }

                // Add acknowledgement to the log file.
                acklog_file << acknowledgement->getSeqNum() << endl;
                state.server_sent_eot_flag = true;

            } else {

                // If EOT packet is not received, resend window.
                if (acknowledgement->getType() == 0)
                {
                    state.window_base = acknowledgement->getSeqNum() % MAX_SEQUENCE_NUMBERS;
                }

                if (state.verbose_flag) {
                    cout << "Client received an out-of-order EOT packet with sequence number ";
                    cout << state.window_base << endl << endl;
                    cout << endl << "===================================================" << endl;

                }
                state.empty_window = true;
                state.update_state_flag = false;
                state.eof_encountered_flag = false;
                if (source_file.fail()) {
                    source_file.clear();
                }
            }
        }
    }

    // Close file streams.
    seqlog_file.close();
    acklog_file.close();

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
    struct timeval wait_time;
    int yes = 1;
    wait_time.tv_sec = 2;
    wait_time.tv_usec = 0;

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
            perror("(client) error when calling setsockopt (SO_REUSEADDR) in listener");
            exit(EXIT_FAILURE);
        }

        // Reference: https://stackoverflow.com/questions/39840877/c-recvfrom-timeout
        // make this a non-blocking socket
        if (setsockopt(listener.socket_fd, SOL_SOCKET, SO_RCVTIMEO, &wait_time, sizeof(struct timeval)) == -1) {
            perror("(client) error when calling setsockopt (SO_RCVTIMEO) in listener");
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

    // ensure that required entries are provided at run-time.
    if (argc != 5) {
        fprintf(stderr,
                "usage: client <emulatorName: host address of the emulator> <sendToEmulator: UDP port number-\n");
        fprintf(stderr,
                "-used by the emulator to receive data from the client> <receiveFromEmulator: UDP port number-\n");
        fprintf(stderr,
                "-by the client to receive ACKs for the emulator><fileName: name of the file to be transferred>\n");
        exit(EXIT_FAILURE);
    }

    host_name = argv[1];
    port1 = argv[2];
    port2 = argv[3];
    file_name = argv[4];

    char user_input;
    cout << endl << endl << "Verbose? (Yes: y \\ No: n):" << endl;
    cin >> user_input;
    cout << endl << endl;

    if (user_input == 'y'){
        state.verbose_flag = true;
    }

    initialize_listener(port2);
    initialize_talker(host_name, port1);

    if (driver(file_name) != 0) {
        fprintf(stderr, "\nTERMINATED\n");
        exit(EXIT_FAILURE);
    } else {
        exit(EXIT_SUCCESS);
    }

    return 0;
}