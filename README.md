# Go-Back-N-UDP

An implementation of the Go-Back-N Automatic Repeat reQuest (ARQ) protocol. This data link layer protocol
uses a sliding window method for reliable and sequential delivery of data frames. The GBN is a sliding
window protocol with a send window size of N and a receiving window size of 1. Please refer to the assignment pdf for more information.

The packet.cpp, packet.h, and emulator files do not belong to me. Do not replicate.  

## To ensure reliable transmission, the GBN client behaves as follows:

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
   
## To ensure reliable transmission, the GBN server behaves as follows:

  1. The server should verify that the packet received is in order by checking its sequence number.

  2. If the packet is in order, the server should send an acknowledgement packet back to the client with the
  sequence number of the received packet.

  3. If the packet is out of order, the server should discard the packet and resend an acknowledgement for the most
  recently received in-order packet.

  4. After the server has received all data packets and an End-Of-Transmission (EOT) packet from the client, it
  should send an EOT packet with the type field set to 2, and then exit.
  
## Execution, Testing, and Results

The program has been thoroughly tested and performes to the specifications. It is able to handle upto 90% (the maximum drop rate) of the packets being lost in transit.

### 50% packet loss

[![Demo Video 50% Drop Rate](Drop-Rate-50-Demo.gif)](https://www.youtube.com/watch?v=lHbZSz0J5fo)


