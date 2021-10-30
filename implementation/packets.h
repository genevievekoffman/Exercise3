#define BYTES 1300

typedef struct dummy_data {
    int tag; //0 = data pkt, 1 = fb_pkt, 2 = final
    int machine_index;
    int pkt_index;
    int rand_num;
    int payload[BYTES];
}data_pkt;
