/* 
 * File:   sender_main.cpp
 * Author: 
 *
 * Created on 
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>

#include <iostream>
#include <queue>
#include <math.h>
#include <errno.h>
#include <cstdio>
#include "struct_file.h"

using namespace std;

struct sockaddr_in si_other;
int s, slen;


FILE *fp;
uint64_t number_received;
uint64_t number_duplicated;
float congestion_window;
float threshold;
uint64_t number_of_total_packets;
uint64_t bytes_to_send;
uint64_t number_sent;
uint64_t sequence_index;
status_type status;

queue <packet> second_queue;
queue <packet> first_queue;

void diep(const char *s) {
    perror(s);
    exit(1);
}

void check_and_send_packet(packet* pkt){
    if (sendto(s, pkt, sizeof(packet), 0, (struct sockaddr*)&si_other, sizeof(si_other))== -1){
        diep("Failed to send packet");
    }
}


void change_states() {
    switch(status){
        case SLOW_START:
            if (number_duplicated >= 3){
                threshold = congestion_window / 2;
                threshold = max((float)4000, threshold);
                congestion_window = threshold + 3 * 4000;
                congestion_window = max((float)4000, congestion_window);
                status = FAST_RECOVERY;
                if (!second_queue.empty()){
                    check_and_send_packet(&second_queue.front());
                }
                number_duplicated = 0;
            }
            else if (number_duplicated == 0){
                if (congestion_window >= threshold){
                    status = CONGESTION_AVOID;
                    return;
                }
                congestion_window += 4000;
                congestion_window = max((float)4000, congestion_window);
            }
            break;
        case CONGESTION_AVOID:
            if (number_duplicated >= 3){
                threshold = congestion_window / 2;
                threshold = max((float)4000, threshold);
                congestion_window = threshold + 3 * 4000;
                congestion_window = max((float)4000, congestion_window);
                status = FAST_RECOVERY;
                if (!second_queue.empty()){
                    check_and_send_packet(&second_queue.front());
                }
                number_duplicated = 0;
            }
            else if (number_duplicated == 0){
                congestion_window += 4000 * floor(1.0 * 4000 / congestion_window); 
                congestion_window = max((float)4000, congestion_window);
            }
            break;
        case FAST_RECOVERY:
            if (number_duplicated > 0){
                congestion_window += 4000;
                return;
            }
            else if (number_duplicated == 0){
                congestion_window = threshold;
                congestion_window = max((float)4000, congestion_window);
                status = CONGESTION_AVOID;
            }
            break;
        default: 
            break;
    }
}

void push_both_queues() {
    if (bytes_to_send == 0){
        return;
    }
    char buffer[4000];
    memset(buffer, 0, 4000);
    packet pkt;
    for (int i = 0; i < ceil((congestion_window - second_queue.size() * 4000) / 4000); i++){
        int minimum_bytes = min(bytes_to_send, (uint64_t)4000);
        int size_of_read = fread(buffer, sizeof(char), minimum_bytes, fp);
        if (size_of_read > 0) {
            pkt.packet_type = DATA;
            pkt.data_size = size_of_read;
            pkt.sequence_index = sequence_index;
            memcpy(pkt.data, &buffer, size_of_read);
            second_queue.push(pkt);
            first_queue.push(pkt);
            sequence_index += size_of_read;
            bytes_to_send -= size_of_read;
        }
    }
    while (!first_queue.empty()){
        check_and_send_packet(&first_queue.front());
        number_sent++;
        first_queue.pop();
    }
}

void process_acknowledgement (packet* pack) {
     if (pack->acknowledgement_index == second_queue.front().sequence_index){
        number_duplicated++;
        change_states();
    } else if (pack->acknowledgement_index < second_queue.front().sequence_index){
        return;
    }else{
        number_duplicated = 0;
        change_states();
        int number_packets = ceil((pack->acknowledgement_index - second_queue.front().sequence_index) / (1.0 * 4000));
        int count = 0;
        number_received += number_packets;
        while(!second_queue.empty() && count < number_packets){
            second_queue.pop();
            count++;
        }
        if (bytes_to_send != 0) {
	    char buffer[4000];
	    memset(buffer, 0, 4000);
	    packet pkt;
	    for (int i = 0; i < ceil((congestion_window - second_queue.size() * 4000) / 4000); i++){
		int minimum_bytes = min(bytes_to_send, (uint64_t)4000);
		int size_of_read = fread(buffer, sizeof(char), minimum_bytes, fp);
		if (size_of_read > 0) {
		    pkt.packet_type = DATA;
		    pkt.data_size = size_of_read;
		    pkt.sequence_index = sequence_index;
		    memcpy(pkt.data, &buffer, size_of_read);
		    second_queue.push(pkt);
		    first_queue.push(pkt);
		    sequence_index += size_of_read;
		    bytes_to_send -= size_of_read;
		}
	    }
	    while (!first_queue.empty()){
		check_and_send_packet(&first_queue.front());
		number_sent++;
		first_queue.pop();
	    }
        }
    }
}


void reliablyTransfer(char* hostname, unsigned short int hostUDPport, char* filename, unsigned long long int bytesToTransfer) {


	/* Determine how many bytes to transfer */

    slen = sizeof (si_other);

    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_other, 0, sizeof (si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(hostUDPport);
    if (inet_aton(hostname, &si_other.sin_addr) == 0) {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }
    //Open the file
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        diep("Could not open");
    }

    number_of_total_packets = ceil(1.0 * bytesToTransfer / 4000);
    bytes_to_send = bytesToTransfer;
    congestion_window = 4000;
    status = SLOW_START;
    number_duplicated = 0;
    number_sent = 0;
    number_received = 0;
    threshold = congestion_window * float(512);
    sequence_index = 0;

    timeval RTT;
    RTT.tv_sec = 0;
    RTT.tv_usec = 100000;
    if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &RTT, sizeof(RTT)) == -1){
        diep("Timeout Error");
    }
	/* Send data and receive acknowledgements on s*/

    packet pkt;
    push_both_queues();
    while (number_sent < number_of_total_packets || number_received < number_of_total_packets){
        if ((recvfrom(s, &pkt, sizeof(packet), 0, NULL, NULL)) == -1){
            if (errno != EAGAIN || errno != EWOULDBLOCK) {
                diep("Could not receive message");
            }
            if (!second_queue.empty()){
                threshold = congestion_window / 2;
                threshold = max((float)4000 * 256, threshold);
                congestion_window = 4000 * 256;
                status = SLOW_START;

                queue<packet> tmp_q = second_queue; 
                for (int i = 0; i < 32; i++){
                    if (!tmp_q.empty()){
                        check_and_send_packet(&tmp_q.front());
                        tmp_q.pop();
                    }
                }
                number_duplicated = 0;
            }
        }
        else{
            if (pkt.packet_type == ACK){
                process_acknowledgement(&pkt);
            }
        }
    }
    packet close_pkt;
    char temp[sizeof(packet)];
    close_pkt.packet_type = FIN;
    close_pkt.data_size = 0;
    memset(close_pkt.data, 0, 4000);
    check_and_send_packet(&close_pkt);
    while (true){
        slen = sizeof(si_other);
        if (recvfrom(s, temp, sizeof(packet), 0, (struct sockaddr *)&si_other, (socklen_t*)&slen) == -1){
            if (errno != EAGAIN || errno != EWOULDBLOCK){
                diep("Did not receive packet");
            }
            else{
                close_pkt.packet_type = FIN;
                close_pkt.data_size = 0;
                memset(close_pkt.data, 0, 4000);
                check_and_send_packet(&close_pkt);
            }
        }
        else{
            packet ack;
            memcpy(&ack, temp, sizeof(packet));
            if (ack.packet_type == FINACK){
                close_pkt.packet_type = FINACK;
                close_pkt.data_size = 0;
                check_and_send_packet(&close_pkt);
                break;
            }
        }
    }
    fclose(fp);
    return;

}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;
    unsigned long long int numBytes;

    if (argc != 5) {
        fprintf(stderr, "usage: %s receiver_hostname receiver_port filename_to_xfer bytes_to_xfer\n\n", argv[0]);
        exit(1);
    }
    udpPort = (unsigned short int) atoi(argv[2]);
    numBytes = atoll(argv[4]);



    reliablyTransfer(argv[1], udpPort, argv[3], numBytes);


    return (EXIT_SUCCESS);
}
