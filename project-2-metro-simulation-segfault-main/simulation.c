#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <string.h>

/*
<----------------->
< Data Structures >
<----------------->
*/

typedef struct Train{
    int train_id;
    int lenght;
    struct tm* creation_time;
    int start_point; 
    int end_point;
} Train;

typedef struct Queue_elm{
    struct Queue_elm *next;
    struct Train *element;
} Queue_elm;

typedef struct{
    Queue_elm *front, *back;
    int elm_num;
} Train_queue;

typedef struct Path_thread_args{
    int path_id;
    Train_queue *que;
} Path_thread_args;

typedef struct Train_thread_args{
    Train_queue *qeues;
    double p;
    int q_id;
} Train_thread_args;

typedef struct Control_thread_args{
    Train_queue *qeues[4];
    double p;
} Control_thread_args;

typedef struct Control_log_args{
    Train_queue *qeues[4];
} Control_log_args;


/*
<------------------->
< Global variables >
<------------------->
*/

int simulate = 1;
int total_trains = 0;
int overloading = 0;
int train_id = 0;
int do_tunnel_check;
int log_type;

char *queue_elems;
char *event;

Train *entering_train;


/*
<----------------->
< Synchronization >
<----------------->
*/


pthread_cond_t log_check = PTHREAD_COND_INITIALIZER;
pthread_cond_t control_check =  PTHREAD_COND_INITIALIZER;
pthread_cond_t event_set =  PTHREAD_COND_INITIALIZER;
pthread_cond_t tunnel_passage = PTHREAD_COND_INITIALIZER;
pthread_cond_t new_train[5] = { PTHREAD_COND_INITIALIZER,
                                PTHREAD_COND_INITIALIZER,
                                PTHREAD_COND_INITIALIZER,
                                PTHREAD_COND_INITIALIZER,
                                PTHREAD_COND_INITIALIZER
                                };
                                

pthread_mutex_t q_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t tunnel = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t path_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t control_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t control_log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t train_log_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER;



/*
<------------------->
< Data Manipulation >
<------------------->
*/

//create a train queue
Train_queue* create_train_queue(){

    Train_queue *new_queue = (Train_queue*)malloc(sizeof(Train_queue));
    new_queue->front = NULL;
    new_queue->back = NULL;
    new_queue->elm_num = 0;

    return new_queue;
}

void enqueue (Train_queue *queue, Train *train){ //enQ to the front
    
    Queue_elm *node = (Queue_elm*) malloc(sizeof(Queue_elm));
    node->element = train;
    node->next = NULL;
     
    if(queue->elm_num == 0){
    
        queue->front = node;
        queue->back = node;
    
    }else{

        queue->front->next = node;
        queue->front = node;
    
    }

    queue->elm_num++;
}

Queue_elm* dequeue(Train_queue *queue){ //deQ from back

    Queue_elm *node = queue->back;

    if(queue->elm_num == 1){

        queue->back = NULL;
        queue->front = NULL;

    }else{

        queue->back = node->next;

    }
    
    queue->elm_num--;
    return node; 
}

int assign_lenght(int p){ // assing lenght with probability 0.7 to be 200m
    double len_prob = (double) rand() / RAND_MAX;
    if(len_prob<0.7)
        return 100;
    else
        return 200;

}



void assign_ending_point(Train *t){ //assign the ending point with 
    int start = t->start_point;
    double start_prob = (double) rand() / RAND_MAX;
    if(start == 0 || start == 1){
        if(start_prob<0.5){
            t->end_point =  2;
            return;
        }else{
            t->end_point = 3;
            return;
        }
    }else{
        if(start_prob<0.5){
            t->end_point = 0;
            return;
        }else{
            t->end_point = 1;
            return;
        }
    }
}

char* queue_ids(Queue_elm* front) {
    
    char* result = (char*)malloc(200 * sizeof(char)); 
    result[0] = '\0'; 

    Queue_elm* current = front;
    while (current != NULL) {
        Train* train = current->element;       
        char temp[5]; 
        sprintf(temp, "%d", train->train_id);

        strcat(result, temp); 

        if (current->next != NULL) {
            strcat(result, ", ");
        }

        current = current->next; 
    }
    return result;

}

struct tm* get_current_time(){
    time_t currentTime;
    time(&currentTime);
    return localtime(&currentTime);

}

Train* create_train(int id , double p, int start){
    
    Train *train = (Train*)malloc(sizeof( Train));
    train->train_id = id;
    train->lenght = assign_lenght(p);
    train->start_point = start;
    struct tm *current_time = get_current_time();
    train->creation_time = (struct tm*)malloc(sizeof(struct tm));
    memcpy(train->creation_time, current_time, sizeof(struct tm));
    train->end_point = -1;
    return train;
    
};



/*
<--------->
< Logging >
<--------->
*/

void logMessage(char* message, char* target){// generic log function which is used by both control.log and train.log
    
    FILE *logFile = fopen(target, "a");

    if (logFile == NULL) {
        printf("cannot open file\n");
        return;
    }

    struct tm *localTime = get_current_time(); // it will print current time in the beginning of every log. It is usefull for debugging

    fprintf(logFile, "[%04d-%02d-%02d %02d:%02d:%02d] %s\n",
            localTime->tm_year + 1900, localTime->tm_mon + 1, localTime->tm_mday,
            localTime->tm_hour, localTime->tm_min, localTime->tm_sec, message);

    fclose(logFile);
}


char intToPoint(int point){ // destionation points of the trains are determined in terms of integers 
                            // To print char corresponds to the integer intToPoint is used
    if (point==0){
        return 'A';
    }
    else if (point == 2){
        return 'E';
    }
    else if (point == 3){
        return 'F';
    }
    else {
        return 'B';
    }
}
char* get_waiting_trains(Control_thread_args *args){
    
    char* result = (char*)malloc(sizeof(char) * 1000); 
    strcpy(result, "");

        for (int i = 0; i < 4; ++i) { // iterate over queues and put the train ids to result string
            if (args->qeues[i]->elm_num>0) {
                Queue_elm *current = args->qeues[i]->back;

                while (current != NULL) {
                    char trainID[20]; 
                    sprintf(trainID, "%d,", current->element->train_id);

                    strcat(result, trainID);
                    current = current->next;
                }
            }
        }

    // remove last coma in the result
    if (strlen(result) > 0) {
        result[strlen(result) - 1] = '\0';
    }

    return result;

}

// open the log files and clear them
int clear_log_file(){

    FILE *logFile;

    logFile = fopen("./log/train.log", "w");

    if (logFile == NULL) {
        printf("cannot open file\n");
        return 0;
    }

    fclose(logFile);


    logFile = fopen("./log/control.log", "w");

   if (logFile == NULL) {
        printf("cannot open file\n");
        return 0;
    }

    fclose(logFile);

    return 0;
}


void train_log(Train *train){
    
    time_t currentTime = time(NULL);

    // fto calculate arrival time
    time_t arriveTime = currentTime + 1; // train will arrive to its destination 1 second after exiting the tunnel

    // Format future time
    struct tm *arriveTimeinfo;
    arriveTimeinfo = localtime(&arriveTime); 
    
    char logg[200]; // message to be printed in train.log

    sprintf(logg,"%d       \t%c             \t%c                 \t%d\t      %02d:%02d:%02d    \t%02d:%02d:%02d\n",
            train->train_id,intToPoint(train->start_point),intToPoint(train->end_point),train->lenght,
            train->creation_time->tm_hour, train->creation_time->tm_min, train->creation_time->tm_sec,
            arriveTimeinfo->tm_hour, arriveTimeinfo->tm_min, arriveTimeinfo->tm_sec); 
    
    logMessage(logg,"./log/train.log");                   
    
    
}

void passage(Control_thread_args *args){
    queue_elems = get_waiting_trains(args);
    int passage_time = entering_train->lenght/100;
    passage_time = passage_time + 1; // train is out of tunnel when it has no part left in the tunnel 
    double breakdown_prob = (double) rand() / RAND_MAX;      

    event = "Tunnel Passing"; // set event to be displayed in control.log  
    pthread_cond_signal(&control_check); // train is passing inform control.log thread

    sleep(1);   // we want our trains to be brokedown inside the tunnel so we dont check the breakdown until it is inside the tunnel
    passage_time = passage_time - 1;

    if (breakdown_prob<=0.1){  //in case of breakdown, add 4 more seconds to passage_time
            passage_time+=4;                   
            event = "Breakdown"; // set event to be displayed in control.log 
            queue_elems = get_waiting_trains(args); // get current waiting trains ids for control.log
            pthread_cond_signal(&control_check); //breakdown occured inform control.log thread  
    }

    
    sleep(passage_time); // let the train to pass through the tunnel
}



void control_log(Train *train, char *event, char *wating_trains){
        
        char log[200];
        struct tm *current_time = get_current_time(); // Event Time
        // To allign spaces between different type of event logs, each events are seperated with if statements

        if(strcmp(event,"Tunnel Passing") == 0){
            sprintf(log,"%s   %02d:%02d:%02d    \t%d             \t%s\n",
               event,current_time->tm_hour,current_time->tm_min,current_time->tm_sec,train->train_id,wating_trains);
        }
        else if(strcmp(event,"Overload") == 0){
            sprintf(log,"%s         %02d:%02d:%02d   \t#            \t%s\n",
                event,current_time->tm_hour,current_time->tm_min,current_time->tm_sec,wating_trains);

        }
        else {
            sprintf(log,"%s        %02d:%02d:%02d   \t%d             \t%s\n",
               event,current_time->tm_hour,current_time->tm_min,current_time->tm_sec,train->train_id,wating_trains);
        }
        logMessage(log,"./log/control.log");    
}







/*
<------------------>
< Thread Functions >
<------------------>
*/

//signal the control center for passage
void request_passage(Train_queue *queue){

    pthread_mutex_lock(&tunnel);
    pthread_cond_signal(&tunnel_passage);
    pthread_mutex_unlock(&tunnel);    
}



//thread for the lanes
void *path_thread(void* args){
    
    Path_thread_args *path_args = args;
     
    while(simulate){
        //whenever a train arrives requests passage from the control center
        pthread_cond_wait(&new_train[path_args->path_id],&path_mutex);
        sleep(1); //wait for it arrive at the tunnel
        request_passage(path_args->que);
    }
     
    pthread_exit(NULL);
    

}
//given p and lane calculate if train coming to the lane
int creating_new_train(double p , int id){
    
    double prob = (double) rand() / RAND_MAX;
    if(id != 1){
        if(prob<p){
            return 1;
        }
        return 0;
    }

    if(prob < 1 - p){
            return 1;
    }
    return 0;
    

}

//thread for creating train to the lanes
void *train_thread(void* args){
    
    Train_thread_args *train_args = args;

    //command line input p for probability
    double p = train_args->p;

    while(simulate){  
        
        // acquire the lock for adding to the queue,
        
        if(total_trains < 12){
            pthread_mutex_lock(&q_mutex);
            if(total_trains >= 11){
                pthread_mutex_unlock(&q_mutex);
                break;
            }

            if(!overloading){
                //create a train
                if(creating_new_train(p,train_args->q_id)){
                    train_id++;
                    Train *t = create_train(train_id,train_args->p,train_args->q_id);
                    enqueue(train_args->qeues, t);
                    total_trains++;
                    // signal the lane that a train has arrived
                    pthread_cond_signal(&new_train[train_args->q_id]);
                }
            }

            pthread_mutex_unlock(&q_mutex);

            //create a train every 1 sec, also give 
            //time for created trains time to arrive at tunnel
            //since time to travel to tunnel is 1 sec also
            sleep(1);
        } 
    }
    
    pthread_exit(NULL);

}




int determine_passing_train(Control_thread_args *args){
    // cond 1) lane with max num of trains will pass the tunnel
    // cond 2 )tie is broken with priority A>B>E>F

    int path_index = -1;
    int max_elm = -1;
    //if there is not any trains in the system return -1
    if(!total_trains){
        return -1;
    }

    //get the max num of elements
    for(int i = 0; i < 4;i++){
        //since the > the lane with lower id will have presedence preserving cond 2
        if(args->qeues[i]->elm_num > max_elm){
            max_elm = args->qeues[i]->elm_num;
            path_index = i;
        }
    }

    return path_index;
}

void *train_log_thread(){
    
    while(simulate){

        pthread_cond_wait(&log_check,&train_log_mutex); // waiting for signal
        assign_ending_point(entering_train);            // set ending point of the train
        train_log(entering_train);                      // log the train which left the system to train.log
        
    }

    pthread_exit(NULL);
}

void *control_log_thread(void *args){
    
    while(simulate){
        
        pthread_cond_wait(&control_check,&control_log_mutex); // waiting for signal
        char event_buffer[100];
        strcpy(event_buffer,event);                           // event type (Breakdown,Overload,Tunnel passing)

        if(strcmp(event_buffer,"Overload") == 0){
            pthread_cond_signal(&event_set);                  
        }

        control_log(entering_train, event_buffer, queue_elems); // log specific event with requested elements to control.log
        

    }    

    pthread_exit(NULL);

}

//thread for the control center
void *control_thread(void* args){

    //get the arguments
    Control_thread_args *control_t_args = args;

    while(simulate){
        
        // Signaled by the paths to allow for a train's passage 
        pthread_cond_wait(&tunnel_passage,&control_mutex);
        
        //aquire the lock for the queues
        pthread_mutex_lock(&q_mutex);
        
        do{
            if(total_trains == 11){ //if there 11 trains (1 for the tunnel) then there is overloading
                overloading = 1;
                event = "Overload";
                pthread_cond_signal(&control_check); // signal for the overloading
                pthread_cond_wait(&event_set,&event_mutex); // when log registers the event signals back
            }
            
            //determine which lane's train will pass
            int grant_pass = -1;            
            grant_pass = determine_passing_train(control_t_args);

            if(grant_pass!=-1){
                //remove the train from its queue
                Queue_elm *elm = dequeue(control_t_args->qeues[grant_pass]);
                total_trains--;

                //set it to entering train, send it to pass the tunnel
                free(entering_train);
                entering_train = elm->element;
                passage(control_t_args);
                //after the train passed the tunnel signal for train loggin
                pthread_cond_signal(&log_check);
            }else{
                overloading = 0;
            }

        } while (overloading);
     
        pthread_mutex_unlock(&q_mutex);
        
    }

    pthread_exit(NULL);
    
}


/*
<------>
< Main >
<------>
*/

int main(int argc, char *argv[]){
    
    //clear log files
    clear_log_file();
    
    //setup log headers
    logMessage("Train ID  Starting Point  Destination Point  Length(m)  Arrival Time  Departure Time\n","./log/train.log");
    logMessage("------------------------------------------------------------------------------------\n","./log/train.log");
    
    logMessage("   Event        Event Time  Train ID  Trains  Waiting Passage","./log/control.log");
    logMessage("------------------------------------------------------------------------------------\n","./log/control.log");
    pthread_t threads[11];
    
    //command line inputs
    double p = strtod(argv[1],NULL);
    int sim_lenght = 900; //default simulation time
    
    //check for -s argument
    if((argc == 4) && strcmp(argv[2],"-s") == 0){
        sim_lenght = atoi(argv[3]);
    }

    //create train queues
    Train_queue *a_c = create_train_queue();
    Train_queue *b_c = create_train_queue();
    Train_queue *e_d = create_train_queue();
    Train_queue *f_d = create_train_queue();
    

    //setup path args
    Path_thread_args *path0_args = (Path_thread_args*)malloc(sizeof(Path_thread_args));
    path0_args->que = a_c;
    path0_args->path_id = 0;

    Path_thread_args *path1_args = (Path_thread_args*)malloc(sizeof(Path_thread_args));
    path1_args->que = b_c;
    path1_args->path_id = 1;


    Path_thread_args *path2_args = (Path_thread_args*)malloc(sizeof(Path_thread_args));
    path2_args->que = e_d;
    path2_args->path_id = 2;

    Path_thread_args *path3_args = (Path_thread_args*)malloc(sizeof(Path_thread_args));
    path3_args->que = f_d;
    path3_args->path_id = 3;

    //setup train thread args
    Train_thread_args *train_thread_a = (Train_thread_args*)malloc(sizeof(Train_thread_args));
    train_thread_a->qeues = a_c;
    train_thread_a->q_id = 0;
    train_thread_a->p = p;

    Train_thread_args *train_thread_b = (Train_thread_args*)malloc(sizeof(Train_thread_args));
    train_thread_b->qeues = b_c;
    train_thread_b->q_id = 1;
    train_thread_b->p = p;
    
    Train_thread_args *train_thread_e = (Train_thread_args*)malloc(sizeof(Train_thread_args));
    train_thread_e->qeues = e_d;
    train_thread_e->q_id = 2;
    train_thread_e->p = p;
    
    Train_thread_args *train_thread_f = (Train_thread_args*)malloc(sizeof(Train_thread_args));
    train_thread_f->qeues = f_d;
    train_thread_f->q_id = 3;
    train_thread_f->p = p;
    
    //setup control thread args
    Control_thread_args *control_thread_arg = (Control_thread_args*)malloc(sizeof(Control_thread_args));
    control_thread_arg->qeues[0] = a_c;
    control_thread_arg->qeues[1] = b_c;
    control_thread_arg->qeues[2] = e_d;
    control_thread_arg->qeues[3] = f_d;

    //setup control log args
    Control_log_args *control_log_arg = (Control_log_args*)malloc(sizeof(Control_log_args));
    control_log_arg->qeues[0] = a_c;
    control_log_arg->qeues[1] = b_c;
    control_log_arg->qeues[2] = e_d;
    control_log_arg->qeues[3] = f_d;

    // Creating the control thread
    pthread_create(&threads[0], NULL, control_thread, control_thread_arg);
    
    // Creating train threads
    pthread_create(&threads[1] ,NULL, train_thread,train_thread_a);
    pthread_create(&threads[2] ,NULL, train_thread,train_thread_b);
    pthread_create(&threads[3] ,NULL, train_thread,train_thread_e);
    pthread_create(&threads[4] ,NULL, train_thread,train_thread_f);

    // Creating Path threads
    pthread_create(&threads[5], NULL, path_thread, path0_args); //Path A
    pthread_create(&threads[6], NULL, path_thread, path1_args); //Path B
    pthread_create(&threads[7], NULL, path_thread, path2_args); //Path E
    pthread_create(&threads[8], NULL, path_thread, path3_args); //Path F

    //Log Threads
    pthread_create(&threads[9], NULL, train_log_thread,NULL);
    pthread_create(&threads[10], NULL, control_log_thread, control_log_arg);
    
    //simulate till the simulation time
    sleep(sim_lenght);
    simulate = 0;
    

    return 0;
}


/*
    Rules:

    A <->           <-> E
            C <-> D
    B <->           <-> F

    A train can have a length of 100 meters with a probability of 0.7 and 200 meters with
    0.3, speed of train 100x. len of tunnel is 100
    
    For efficiency, the Metro control center gives permission to pass the tunnel to a train
    from the section that has the largest number of trains

    If there is a tie, the Metro control center prioritizes trains waiting at A > B > E > F.
    
    At time zero, there is no train in the system.

*/
