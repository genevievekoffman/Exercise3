#define MAX_MACHINES 10
#define WINDOW 30

#include "sp.h"
#include "packets.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_VSSETS      10
#define MAX_MESSLEN     5000 
#define MAX_MEMBERS     10

static  char    group[] = "group_GT"; 
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
static  int     tag;
static  int     msg_size = sizeof(data_pkt);
time_t          start, end;
char            sender[MAX_GROUP_NAME];
static  void    Read_message();
static  void    Send_message();
static  void    Usage( int argc, char *argv[] );
static  void    Bye();
static  data_pkt *new_pkt;
static  char     *buf;
FILE             *fw; // pointer to dest file, which we write

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
    char filename[] = "/tmp/ts_";
    //char filename[] = "ts_";
    strcat(filename, argv[2]);

    if ( (fw = fopen( (strcat(filename,".txt") ) , "w") ) == NULL ) {
        perror("fopen");
        exit(0);
    }

    srand ( time(NULL) ); //init random num generator
    
    /* must first join the same group */
    ret = SP_join( Mbox, group ); 
    if ( ret < 0 ) SP_error( ret );
    for (;;)
        Read_message();
}

static  void    Send_message()
{

    int burst = 1;

    //send first burst of messages
    if (transfer) 
        burst = WINDOW;

    while ( burst > 0 && packet_index+1 <= num_msgs+1 ) 
    {
        //send a packet msg
        packet_index++;
        tag = (packet_index == num_msgs+1) ? 1 : 0;
        new_pkt->pkt_index = packet_index;
        new_pkt->rand_num = rand() % 1000000 + 1; //generates random number 1 to 1 mil
        int ret = SP_multicast( Mbox, AGREED_MESS, group, tag, msg_size, (char*)new_pkt); 
        if( ret < 0 ){
            SP_error( ret );
            Bye();
        } 
        burst--;
    }
}

static  void    Read_message() 
{
    static  char    mess[MAX_MESSLEN];
    char            target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
    membership_info memb_info;
    int             service_type;
    int16           mess_type;
    int             endian_mismatch;
    int             num_groups; 
    vs_set_info     vssets[MAX_VSSETS];
    int             num_vs_sets;
    unsigned int    my_vsset_index;
    char            members[MAX_MEMBERS][MAX_GROUP_NAME];
    int             ret;
    
    service_type = 0; 

    ret = SP_receive( Mbox, &service_type, sender, 10, &num_groups, target_groups, &mess_type, &endian_mismatch, msg_size, buf);

    if (ret < 0 )
    {
        if( ! To_exit )
        {
            SP_error( ret );
            printf("\nBye.\n");
        }
        exit(0);
    }

    if( Is_regular_mess( service_type ) )
    {   
        mess[ret] = 0;
        switch ( mess_type )
        {
            case 0: ; //data_pkt
                data_pkt *pkt = (data_pkt*) buf;
                char buf_write[sizeof(pkt->rand_num)];
                sprintf(buf_write, "%d", pkt->rand_num);
                fprintf(fw, "%2d, %8d, %8d\n", pkt->machine_index, pkt->pkt_index, pkt->rand_num);
                //we have recieved an ack and can slide our window
                if ( pkt->machine_index == machine_index ) 
                    Send_message();
                break;
            case 1: ; //final_pkt
                final_msgs++;
                if (final_msgs == num_processes) 
                    Bye();
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
            
            /* when theres num_proccesses on the SPREAD network, data transfers begins */
            if ( num_groups == num_processes ) 
            {
                time(&start);
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
}

static void Usage(int argc, char *argv[])
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
    printf("execution time = %f\n", difftime(end, start));
    SP_disconnect( Mbox );
    exit(0);
}
