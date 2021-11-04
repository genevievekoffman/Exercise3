Our design/flow control:
-----------------------
    First we wait for all processes to join before beginning the communication. 
    Every time we receive a membership message, we check to see if the number of processes that have joined this group (the information Spread gives us) equals num_processes. Since Spread will deliver the membership msg to all participants simultaneously, we are guaranteed to all begin at the same time.

    Then once all participants have joined, we trigger the transfer process by sending an initial burst of size WINDOW. 
    Once the transfer has been triggered, we do the following until its over:

        -Listen for messages from Spread. Since Spread gaurantees us Agreed order, we immediately write the packet contents to our file knowing that all other processes are doing the same. 
        -if we receive a packet with our machine index, this is equivalent to an acknolwedgment that everyones gotten our msg so we can 'shift' our window. We send the next msg then return to listening. 

        -if we receive a final msg: we increment our final msg counter. At all times, this informs us of how many processes are done. When our final msg counter = num_processes, this means we can terminate.  

Our initial approach:
--------------------
    At first we sent messages in bursts of size WINDOW. We were thinking about our second projects design. We tried to adjust the number of packets being sent(burst) based on how open the Spread daemon was. For example, we knew at any time, at most num_processes * WINDOW would be on the Spread network so when a process finished, we wanted to make up for the pkts it wasn't sending and send more on the machines that were not done. We were focused/thinking about the capacity of the Spread daemon. Even though it worked, we realized this was not the optimal approach. 

Changes made to enhance performance:
-----------------------------------
        Open file in tmp directory instead of home directory, to reduce time spent writing to file (time = 121)
        
        Experimented with the window size
                Window size was originally 10, changed to 20 (time = 103)
                changed to 30 (time = 99)
                when we changed to 40 time increased (time = 101)
                Best window size was 30 (time = 99)
        
        At first we allocated and freed dynamic memory for each packet we sent and recieved which is many unecessary steps which took up time so we switched to allocating dynamic memory for one packet in the beginning and then freed it at the end. (time = 94)

        Modified design to have program listening in a loop rather than using event handler. Our initial approach was to have processes read and send messages (reading was a higher priority) so it would always be called over sending a message. This was extraneous work so we got rid of the event handler and put our methods in a loop based on window shifting and packets received (time = 87)

        Changed payload from being an array of length 1300 to 1300 bytes
        This was the biggest improvement in time. We mistakenly had a packets payload array with length 1300 which meant it was 1300*4 bytes so there was an enormous amount of data to be transfered for every packet sent and received. We changed the length to 325 which meant we would have payload of 325*4=1300 Bytes. Our time improved from ~100 to ~22! 
tuned: 

Benchmarking:
time = 23 sec
time = 24 sec
time = 22 sec
time = 21 sec


