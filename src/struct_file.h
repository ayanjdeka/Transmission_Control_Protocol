#ifndef STRUCT_FILE_H
#define STRUCT_FILE_H

#include <stdlib.h>

enum packet_t{
    DATA,
    ACK,
    FIN,
    FINACK
};

typedef struct{
    int 	    data_size;
	uint64_t    acknowledgement_index;
    uint64_t 	sequence_index;
	packet_t   packet_type;
	char        data[4000];
} packet;

enum status_type{
    SLOW_START,
    CONGESTION_AVOID,
    FAST_RECOVERY
};

struct compare {
    bool operator()(packet a, packet b) {
        return  a.sequence_index > b.sequence_index; 
    }
};

#endif
