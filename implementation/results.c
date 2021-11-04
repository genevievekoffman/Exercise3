benchmark 1: 
6 machines 160k msgs, 2 machines 0 msgs
time = 124 sec

changed the opening file  
went to 121 

tuned: 

changed the window size from 10:

window = 20
time = 103

window = 30 
time = 99 

window = 40
time = 101 s so we kept it at 30


next change: got rid of all our mallocs (we were allocating dynamic memory every time we sent a message)
so now we just have one malloc in the start and free it at the end and we read the pkt into it
time = 94 s

next change: stopped using E-handle events and called read_msg in a infinite for loop
time = 87 s


last change:
our payload was at first array of length 1300 but then we changed this to 325 (because 4*325=1300 bytes)
time = 23 sec
time = 24 s
Our design/flow control:
    First we wait for all processes to join before beginning the communication. 
    Every time we receive a membership message, we check to see if the number of processes that have joined this group (the information Spread gives us) equals num_processes. Since Spread will deliver the membership msg to all participants simultaneously, we are guaranteed to all begin at the same time.

    Then once all participants have joined, we trigger the transfer process by sending an initial burst of size WINDOW. 
    Once the transfer has been triggered, we do the following until its over:

        -Listen for messages from Spread. Since Spread gaurantees us Agreed order, we immediately write the packet contents to our file knowing that all other processes are doing the same. 
        -if we receive a packet with our machine index, this is equivalent to an acknolwedgment that everyones gotten our msg so we can 'shift' our window. We send the next msg then return to listening. 

        -if we receive a final msg: we increment our final msg counter. At all times, this informs us of how many processes are done. When our final msg counter = num_processes, this means we can terminate.  

Our initial approach:
    At first we sent messages in bursts of size WINDOW. We were thinking about our second projects design. We tried to adjust the number of packets being sent(burst) based on how open the Spread daemon was. For example, we knew at any time, at most num_processes * WINDOW would be on the Spread network so when a process finished, we wanted to make up for the pkts it wasn't sending and send more on the machines that were not done. We were focused/thinking about the capacity of the Spread daemon. Even though it worked, we realized this was not the optimal approach. 
