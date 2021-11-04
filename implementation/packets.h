#define BYTES 325 //since 325*4=1300 bytes

typedef struct dummy_data {
    int machine_index;
    int pkt_index;
    int rand_num;
    int payload[BYTES];
}data_pkt;
