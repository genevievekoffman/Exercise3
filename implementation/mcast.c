#define MAX_MACHINES 10
#define WINDOW 30

#include "sp.h"
#include "packets.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include<time.h>

static  char    group[] = "group_GEN"; 
static  char    User[80];
static  char    Spread_name[80];
static  mailbox Mbox;
static  char    Private_group[MAX_GROUP_NAME];
static  int     To_exit = 0;
static  int     transfer = 1; //when it = 1 we can begin transferring data
static  int     num_processes;
static  int     packet_index = 0; //the number of pkts it sent
static  int     num_msgs;
static  int     machine_index;
static  int     final_msgs = 0; //num of final msgs recieved
static  int     recv_msgs = 0; //num of final msgs recieved
static  int     extra_msgs = 0; //num of final msgs recieved

static data_pkt *new_pkt;
static char     *buf;
time_t start, end;

#define MAX_VSSETS      10
#define MAX_MESSLEN     102400 //should probably be smaller/ the size of our packets
#define MAX_MEMBERS     10

char     sender[MAX_GROUP_NAME];
static  void    Read_message();
static  void    Send_message();
static  void    Usage( int argc, char *argv[] );
static  void    Bye();

FILE               *fw; // pointer to dest file, which we write

int main(int argc, char **argv)
{
    sp_time test_timeout;
    test_timeout.sec = 5;
    test_timeout.usec = 0;

    /* handle arguments */
    if ( argc != 4 ) {
        printf("Usage: mcast <num_msgs> <process_index> <num_processes>\n");
        exit(0);
    }

    char *str = argv[1];
    num_msgs = atoi(str);
    
    str = argv[2];
    machine_index = atoi(str); //converts numeric str to int
    if ( machine_index > MAX_MACHINES || machine_index < 1 ) {
        printf("invalid machine_index\n");
        exit(0);
    }

    str = argv[3];
    num_processes = atoi(str);
    if ( num_processes > MAX_MACHINES || num_processes < 1 ) {
        printf("invalid num_processes\n");
        exit(0);
    }

    new_pkt = malloc(sizeof(data_pkt));
    new_pkt->machine_index = machine_index;
    buf = malloc(sizeof(data_pkt));

    printf("\n>>num_msgs = %d, process_index = %d, num_processes = %d\n", num_msgs, machine_index, num_processes);
    
    Usage( argc, argv );

    int ret;
    /* connect to SPREAD */
    ret = SP_connect_timeout( Spread_name, User, 0, 1, &Mbox, Private_group, test_timeout );
    if( ret != ACCEPT_SESSION ) {
        SP_error( ret );
        Bye();
    }
    printf("User: connected to %s with private group %s\n", Spread_name, Private_group );

    //creates the destination file for writing
    //char file_name[ sizeof(machine_index) ];
    char filename[] = "/tmp/ts_";
    //char filename[] = "ts_";
    //sprintf ( file_name, "%d", machine_index ); //converts machine_index to a string
    strcat(filename, argv[2]);

    //if ( (fw = fopen( (strcat(file_name,".txt") ) , "w") ) == NULL ) {
    if ( (fw = fopen( (strcat(filename,".txt") ) , "w") ) == NULL ) {
        perror("fopen");
        exit(0);
    }

    E_init();
    
    //sending a msg has lowest priority & reading a msg has highest priority
    E_attach_fd( transfer, READ_FD, Send_message, 0, NULL, LOW_PRIORITY );
    E_attach_fd( Mbox, READ_FD, Read_message, 0, NULL, HIGH_PRIORITY ); 
    /* must first join the same group */
    ret = SP_join( Mbox, group ); //make sure they SP_leave(Mbox, group); at end
    if( ret < 0 ) SP_error( ret );
    
    /* wait until theres num_proccesses on the SPREAD network before data transfers begin */
    E_handle_events();
}

static  void    Send_message()
{
    srand ( time(NULL) ); //init random num generator

    int burst = 1;

    //send first burst of messages
    if (transfer) burst = WINDOW;

    while ( packet_index+1 <= num_msgs+1 && burst > 0) 
    {
        //send a packet msg
        packet_index++;
        //data_pkt *new_pkt = malloc(sizeof(data_pkt));
        if (packet_index == num_msgs+1) new_pkt->tag = 1;
        else new_pkt->tag = 0;
        new_pkt->pkt_index = packet_index;
        new_pkt->rand_num = rand() % 1000000 + 1; //generates random number 1 to 1 mil
        //printf("\nsent: pkt_index = %d,rand_num = %d", new_pkt->pkt_index, new_pkt->rand_num);
        int ret = SP_multicast( Mbox, AGREED_MESS, group, 2, sizeof(data_pkt), (const char*)new_pkt); //wants a const char
        if( ret < 0 ){
            SP_error( ret );
            Bye();
        } else {
            //printf("\nI sent a pkt, pkt_index = %d\n", new_pkt->pkt_index);
        }
        burst--;
        extra_msgs = 0;
    }
    //fflush(stdout);
}

static  void    Read_message() 
{
    static  char    mess[MAX_MESSLEN];
    char            target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
    membership_info memb_info;
    int             service_type;
    int16           mess_type;
    int             endian_mismatch;
    int             num_groups; //should be 1
    vs_set_info     vssets[MAX_VSSETS];
    
    service_type = 0; //SHOULD ALWAYS BE AGREEED RIGHT
    int             num_vs_sets;
    unsigned int    my_vsset_index;
    char            members[MAX_MEMBERS][MAX_GROUP_NAME];
    int             ret;

    //mess should be changed to a packet type!
    //max_groups(parameter 4), i set to 1 because we only have this group
    ret = SP_receive( Mbox, &service_type, sender, 10, &num_groups, target_groups, &mess_type, &endian_mismatch, sizeof(data_pkt), buf);
     //^^service_type should always be AGREED_MESS
    data_pkt *pkt = (data_pkt*) buf;

     //printf("\n============================\n");
     if( ret < 0 ) {
        if ( (ret == GROUPS_TOO_SHORT) || (ret == BUFFER_TOO_SHORT) ) {
            service_type = DROP_RECV;
            printf("\n========Buffers or Groups too Short=======\n");
            ret = SP_receive( Mbox, &service_type, sender, 10, &num_groups, target_groups, &mess_type, &endian_mismatch, sizeof(data_pkt), buf);
        }
    }
    
    if (ret < 0 )
    {
        if( ! To_exit )
        {
            SP_error( ret );
            printf("\n============================\n");
            printf("\nBye.\n");
        }
        exit(0);
    }

    if( Is_regular_mess( service_type ) )
    {   
        mess[ret] = 0;
        /*
        if     ( Is_unreliable_mess( service_type ) ) printf("received UNRELIABLE ");
        else if( Is_reliable_mess(   service_type ) ) printf("received RELIABLE ");
        else if( Is_fifo_mess(       service_type ) ) printf("received FIFO ");
        else if( Is_causal_mess(     service_type ) ) printf("received CAUSAL ");
        else if( Is_agreed_mess(     service_type ) ) printf("received AGREED "); //should always be this ...
        else if( Is_safe_mess(       service_type ) ) printf("received SAFE ");
        printf("message from %s, of type %d, (endian %d) to %d groups \n(%d bytes): %s\n",
            sender, mess_type, endian_mismatch, num_groups, ret, buf ); //we want to read it into a packet
        */
        /* switch case based on head.tag */
        switch ( pkt->tag )
        {
            case 0: ; //data_pkt
                //printf("data pkt\n");
                recv_msgs++;
                char buf_write[sizeof(pkt->rand_num)];
                sprintf(buf_write, "%d", pkt->rand_num);
                fprintf(fw, "%2d, %8d, %8d\n", pkt->machine_index, pkt->pkt_index, pkt->rand_num);
                
                //we have recieved an ack and can slide our window
                if(pkt->machine_index == machine_index) Send_message();

                break;
            case 1: ; //final_pkt
                printf("final pkt\n");
                final_msgs++;
                if (final_msgs == num_processes) Bye();
                extra_msgs = WINDOW/(num_processes - final_msgs);
                break;
        }

    }else if( Is_membership_mess( service_type ) ) 
    {
        ret = SP_get_memb_info( buf, service_type, &memb_info );
        if (ret < 0) 
        {
            printf("BUG: membership message does not have valid body\n");
            SP_error( ret );
            exit( 1 );
        }
        if ( Is_reg_memb_mess( service_type ) )
        {
            printf("Received REGULAR membership for group %s with %d members, where I am member %d:\n",
                sender, num_groups, mess_type );
            for( int i=0; i < num_groups; i++ )
                printf("\t%s\n", &target_groups[i][0] );
            printf("grp id is %d %d %d\n",memb_info.gid.id[0], memb_info.gid.id[1], memb_info.gid.id[2] );

            if( Is_caused_join_mess( service_type ) ){
                printf("Due to the JOIN of %s\n", memb_info.changed_member );
            }else if( Is_caused_leave_mess( service_type ) ){
                printf("Due to the LEAVE of %s\n", memb_info.changed_member );
            }else if( Is_caused_disconnect_mess( service_type ) ){
                printf("Due to the DISCONNECT of %s\n", memb_info.changed_member );
            }else if( Is_caused_network_mess( service_type ) )
            {
                printf("Due to NETWORK change with %u VS sets\n", memb_info.num_vs_sets);
                num_vs_sets = SP_get_vs_sets_info( mess, &vssets[0], MAX_VSSETS, &my_vsset_index );
                if (num_vs_sets < 0) 
                {
                    printf("BUG: membership message has more then %d vs sets. Recompile with larger MAX_VSSETS\n", MAX_VSSETS);
                    SP_error( num_vs_sets );
                    exit( 1 );
                }
                for(int i = 0; i < num_vs_sets; i++ )
                {
                    printf("%s VS set %d has %u members:\n",
                       (i  == my_vsset_index) ?
                       ("LOCAL") : ("OTHER"), i, vssets[i].num_members );
                    ret = SP_get_vs_set_members(mess, &vssets[i], members, MAX_MEMBERS);
                    if (ret < 0) {
                        printf("VS Set has more then %d members. Recompile with larger MAX_MEMBERS\n", MAX_MEMBERS);
                        SP_error( ret );
                        exit( 1 );
                    }
                    for(int j = 0; j < vssets[i].num_members; j++ )
                        printf("\t%s\n", members[j] );
                }
            }
            
            //only when there are membership changes??
            if ( num_groups == num_processes ) 
            {
                time(&start);
                printf("\nGROUP HAS CORRECT(%d) MEMBERS & CAN NOW BEGIN TRANSMITTING DATA\n", num_groups);
                transfer = 1;
                //first burst
                Send_message();
                transfer = 0;
            }

        } else if (Is_transition_mess(   service_type ) ) {
            printf("received TRANSITIONAL membership for group %s\n", sender );
        } else if( Is_caused_leave_mess( service_type ) ){
            printf("received membership message that left group %s\n", sender );
        } else printf("received incorrecty membership message of type 0x%x\n", service_type );
    
    } else printf("received message of unknown message type 0x%x with ret %d\n", service_type, ret);
    //printf("\n");
    //printf("User> ");
    //fflush(stdout);

}

static  void    Usage(int argc, char *argv[])
{
sprintf( User, "user" );
sprintf( Spread_name, "4803"); //always just connects here
}


static void Bye()
{
    To_exit = 1;
    printf("\nBye.\n");
    free(new_pkt);
    free(buf);
    time(&end);
    printf("execution time = %f", difftime(end, start));
    SP_disconnect( Mbox );
    exit(0);
}
/*
static double threshhold() 
{
    //checking to see if there is room on the network to send or we should wait
    int tot = num_processes*WINDOW; //maxium packets on the network at any given time
    int cur = *WINDOW(num_processes - final_msgs); //considers the num of processes finished sending msgs

    return cur/tot;
}*/
