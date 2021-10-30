#define BYTES 1300

typedef struct dummy_header {
    int tag; //0 = data pkt, 1 = fb_pkt, 2 = final
    int machine_index;
}header;

typedef struct dummy_data {
    header head;
    int pkt_index;
    int rand_num;
    int payload[BYTES];
}data_pkt;

typedef struct dummy_feedback{
    header head;
}feedback_pkt;
