[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-24ddc0f5d75046c5622901739e7c5dd533143b0c8e959d652212380cedb1ea36.svg)](https://classroom.github.com/a/KJK_IXWo)

in our implementation there are 4 train path threads, 4 train threads for creating the trains to their respective lanes, 1 control thread, 1 control log thread and 1 train log thread.

in the main needed arguments such p and pointers to all the paths are given to the threads and simulate variable is set to 1 and threads works while simulate.

the train thread creates threads with given probabilies and signal the path and upon receiving a signal for the new train path sleeps for 1 sec (time required to arrive at the tunnel) then sends a signal to request passage. control center recieves this signal and look for a train to pass. This proccess repeats till the 
main procosses sleeps the simulation time and exits.

the loggin happens in the control center where signals are sent to the log threads for the tunnels event and train log after passage of tunnel happened.

