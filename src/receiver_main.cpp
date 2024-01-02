/* 
 * File:   receiver_main.cpp
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
#include <iostream>
#include <queue>
#include "struct_file.h"

using namespace std;


struct sockaddr_in si_me, si_other;
int s, slen;

priority_queue<packet, vector<packet>, compare> packet_queue;

void diep(const char *s) {
    perror(s);
    exit(1);
}

void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
    
    slen = sizeof (si_other);
    int acknowledgement_index = 0;


    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
        diep("socket");

    memset((char *) &si_me, 0, sizeof (si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(myUDPport);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    printf("Now binding\n");
    if (bind(s, (struct sockaddr*) &si_me, sizeof (si_me)) == -1)
        diep("bind");


	/* Now receive data and send acknowledgements */   

    FILE* fp = fopen(destinationFile, "wb");
    if (fp == NULL) {
        diep("File does not exist");
    } 

    while (true) {
        packet received_packet;
        if (recvfrom(s, &received_packet, sizeof(packet), 0, (sockaddr*)&si_other, (socklen_t*)&slen) == -1) {
            diep("Did not receive any packet");
        }
        if (received_packet.packet_type == DATA) {
            if (received_packet.sequence_index < acknowledgement_index){
            }
            else if (received_packet.sequence_index > acknowledgement_index) {
                if (packet_queue.size() < 1000) {
                    packet_queue.push(received_packet);
                }
            } else {
                fwrite(received_packet.data, sizeof(char), received_packet.data_size, fp);
                acknowledgement_index += received_packet.data_size;
                while (!packet_queue.empty() && packet_queue.top().sequence_index == acknowledgement_index){
                    packet pkt = packet_queue.top();
                    fwrite(pkt.data, sizeof(char), pkt.data_size, fp);
                    acknowledgement_index += pkt.data_size;
                    packet_queue.pop();
                }
            }
                packet ack;
    		ack.acknowledgement_index = acknowledgement_index;
    		ack.packet_type = ACK;

    		if (sendto(s, &ack, sizeof(packet), 0, (sockaddr*)&si_other, (socklen_t)sizeof (si_other))==-1){
        		diep("Failed to send packet");
    		}
        }else if (received_packet.packet_type == FIN) {
                packet ack;
    		ack.acknowledgement_index = acknowledgement_index;
    		ack.packet_type = FINACK;

    		if (sendto(s, &ack, sizeof(packet), 0, (sockaddr*)&si_other, (socklen_t)sizeof (si_other))==-1){
        		diep("Failed to send packet");
    		}
            break;
        }
    }
    fclose(fp);
    close(s);
	printf("%s received.", destinationFile);
    return;
}

/*
 * 
 */
int main(int argc, char** argv) {

    unsigned short int udpPort;

    if (argc != 3) {
        fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
        exit(1);
    }

    udpPort = (unsigned short int) atoi(argv[1]);

    reliablyReceive(udpPort, argv[2]);
}

