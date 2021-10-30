#define MAX_MACHINES 10

#include "sp.h"
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static  char    group[] = "group_GEN"; 
static  char    User[80];
static  char    Spread_name[80];
static  mailbox Mbox;
static  char    Private_group[MAX_GROUP_NAME];
static  int     To_exit = 0;
static  int     transfer = 0; //when it = 1 we can begin transferring data
static  int      num_processes;

#define MAX_VSSETS      10
#define MAX_MESSLEN     102400 //should probably be smaller/ the size of our packets
#define MAX_MEMBERS     10
char     sender[MAX_GROUP_NAME];
static  void    Read_message();
static  void    Send_message();
static  void    Usage( int argc, char *argv[] );
static  void    Bye();

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
    int num_msgs = atoi(str);
    
    str = argv[2];
    int machine_index = atoi(str); //converts numeric str to int
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
    
    E_init();
    
    //sending a msg has lowest priority & reading a msg has highest priority
    E_attach_fd( 0, READ_FD, Send_message, 0, NULL, LOW_PRIORITY );
    E_attach_fd( Mbox, READ_FD, Read_message, 0, NULL, HIGH_PRIORITY ); 
    /* must first join the same group */
    ret = SP_join( Mbox, group ); //make sure they SP_leave(Mbox, group); at end
    if( ret < 0 ) SP_error( ret );
    
    /* wait until theres num_proccesses on the SPREAD network before data transfers begin */
    E_handle_events();
}

static  void    Send_message()
{
    printf("SENDING A MSG FUNC\n");
    fflush(stdout);
}

static  void    Read_message() 
{
    printf("READING A MSG FUNC\n");
    static  char     mess[MAX_MESSLEN];
    char     target_groups[MAX_MEMBERS][MAX_GROUP_NAME];
    membership_info  memb_info;
    int      service_type;
    int16    mess_type;
    int      endian_mismatch;
    int      num_groups; //should be 1
    vs_set_info      vssets[MAX_VSSETS];
    service_type = 0; //SHOULD ALWAYS BE AGREEED RIGHT
    int      num_vs_sets;
    unsigned int     my_vsset_index;
    char     members[MAX_MEMBERS][MAX_GROUP_NAME];
    int ret;

    //mess should be changed to a packet type!
    //max_groups(parameter 4), i set to 1 because we only have this group
     ret = SP_receive( Mbox, &service_type, sender, 10, &num_groups, target_groups, &mess_type, &endian_mismatch, sizeof(mess), mess );
     //^^service_type should always be AGREED_MESS
        
    //num_groups says the # of current members

     printf("\n============================\n");
     if( ret < 0 )
     {
        if ( (ret == GROUPS_TOO_SHORT) || (ret == BUFFER_TOO_SHORT) ) {
            service_type = DROP_RECV;
            printf("\n========Buffers or Groups too Short=======\n");
            ret = SP_receive( Mbox, &service_type, sender, 10, &num_groups, target_groups, &mess_type, &endian_mismatch, sizeof(mess), mess );
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
        if     ( Is_unreliable_mess( service_type ) ) printf("received UNRELIABLE ");
        else if( Is_reliable_mess(   service_type ) ) printf("received RELIABLE ");
        else if( Is_fifo_mess(       service_type ) ) printf("received FIFO ");
        else if( Is_causal_mess(     service_type ) ) printf("received CAUSAL ");
        else if( Is_agreed_mess(     service_type ) ) printf("received AGREED "); //should always be this ...
        else if( Is_safe_mess(       service_type ) ) printf("received SAFE ");
        printf("message from %s, of type %d, (endian %d) to %d groups \n(%d bytes): %s\n",
            sender, mess_type, endian_mismatch, num_groups, ret, mess );
    }else if( Is_membership_mess( service_type ) ) 
    {
        ret = SP_get_memb_info( mess, service_type, &memb_info );
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
        } else if (Is_transition_mess(   service_type ) ) {
            printf("received TRANSITIONAL membership for group %s\n", sender );
        } else if( Is_caused_leave_mess( service_type ) ){
            printf("received membership message that left group %s\n", sender );
        } else printf("received incorrecty membership message of type 0x%x\n", service_type );
    
    } else printf("received message of unknown message type 0x%x with ret %d\n", service_type, ret);
    printf("\n");
    printf("User> ");
    fflush(stdout);

    //checks if we have num_processes yet and can begin transferring data
    if ( num_groups == num_processes ) 
    {
        printf("\nGROUP HAS CORRECT(%d) MEMBERS & CAN NOW BEGIN TRANSMITTING DATA\n", num_groups);
        transfer = 1;
        //call send_msg somehow ...
    }

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
    SP_disconnect( Mbox );
    exit(0);
}




