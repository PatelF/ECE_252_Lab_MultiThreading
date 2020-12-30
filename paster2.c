#include <curl/curl.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/time.h>

#include "type_definitions.h"
#include "catpng.c"


simple_PNG_p call_strips(char *url);
size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata);
size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata);
int recv_buf_init(RECV_BUF *ptr, size_t max_size);
int recv_buf_cleanup(RECV_BUF *ptr);
void *get_Strips(void *arg);
void produce(queuePNG queue, char *URL, int *count);
void consume(queuePNG queue, allStrips strips, int sleep_length);

queueEntry dequeue(queuePNG queue);
void enqueue(queuePNG queue, char *buffer, int stripNum);
void printQueue(queuePNG queue);
chunk_p inflate_data(chunk_p IDAT);

sem_t *count_sem;
sem_t *buffer_sem;
sem_t *enq_sem;
sem_t *deq_sem;
sem_t *strips_sem;

sem_t *createPNG;

U8* sampleBuffer;
int main(int argc, char **argv)
{   
    double times[2];
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[0] = (tv.tv_sec) + tv.tv_usec/1000000.;

    char url[256]; /*Holds the URL*/
    char *c;

    if (argc < 6)
    {
        printf("need 5 parameters\n");
        exit(1);
    }

    int B = strtol(argv[1], &c, 10); //Max size of Queue
    int P = strtol(argv[2], &c, 10); //Number of Producers
    int C = strtol(argv[3], &c, 10); //Number of Consumers
    int X = strtol(argv[4], &c, 10); //How long the consumer should sleep
    int N = strtol(argv[5], &c, 10); //Which image url to use (1,2, or 3)

    // printf("Values given are B: %d, P: %d, C: %d, X: %d, N: %d\n", B, P, C, X, N);

    /*Check what the value of n is*/
    if (N == 2)
    {
        strcpy(url, IMG_URL2);
    }
    else if (N == 3)
    {
        strcpy(url, IMG_URL3);
    }
    else
    {
        strcpy(url, IMG_URL1);
    }

    /*Get shared memory IDSs*/
    // all of the queue entries
    int queue_entry_shmid = shmget(IPC_PRIVATE, B * sizeof(struct Queue_Entry), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    // shared memory to eventually hold each unique strip
    int queue_shmid = shmget(IPC_PRIVATE, sizeof(struct Queue), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    int strip_shmid = shmget(IPC_PRIVATE, STRIP_AMOUNT * sizeof(struct Image_Strip), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    // the unique strips that are found (ranges from 0 - 50)
    int strips_found_shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    // all of the image strips are stored into an array
    int all_strips_shmid = shmget(IPC_PRIVATE, sizeof(struct All_Strips), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    int sampleBuff_shmid = shmget(IPC_PRIVATE, BUF_SIZE*sizeof(U8), IPC_CREAT | IPC_EXCL | S_IRUSR | S_IWUSR);

    if (queue_entry_shmid == -1 || queue_shmid == -1)
    {
        perror("shmget");
        abort();
    }

    /*Initialize shared memory (attach shared segment to process which is calling it)*/
    queuePNG queue = shmat(queue_shmid, NULL, 0);
    queueEntry queue_Entry = shmat(queue_entry_shmid, NULL, 0);
    int *strips_found = shmat(strips_found_shmid, NULL, 0);
    imageStrip strip = shmat(strip_shmid, NULL, 0);
    allStrips strips = shmat(all_strips_shmid, NULL, 0);
    sampleBuffer = shmat(sampleBuff_shmid, NULL, 0);

    *strips_found = 0;
    queue->count = 0;
    queue->head = -1;
    queue->tail = -1;
    queue->max = B;
    queue->queueE = queue_Entry;
    strips->counter = 0;
    strips->strips_array = strip;

    /*Initialize Semaphores*/
    // # of unique strips within the buffer (0-49)
    count_sem = sem_open("count", O_CREAT, 0644, 1);
    sem_init(count_sem, 1, 1);

    // counts the strips being sent to the queue (space semaphore)
    buffer_sem = sem_open("buffer", O_CREAT, 0644, 1);
    sem_init(buffer_sem, 1, B); // changed 0 to 1

    // enqueue and dequeue (mutex locks for producers and consumers)
    enq_sem = sem_open("enqueue", O_CREAT, 0644, 1);
    sem_init(enq_sem, 1, 1);
    deq_sem = sem_open("dequeue", O_CREAT, 0644, 1);
    sem_init(deq_sem, 1, 1);

    // semaphore for the array holding all the strips
    strips_sem = sem_open("strips", O_CREAT, 0644, 1);
    sem_init(strips_sem, 1, 1);

    // semaphore for the array holding all the strips
    createPNG = sem_open("createPNG", O_CREAT, 0644, 1);
    sem_init(createPNG, 1, 1);

    pid_t child;
    int status;

    //Create Producers
    for (int i = 0; i < P; i++)
    {
        child = fork();

        if (child == 0)
        {
            produce(queue, url, strips_found);
            return 0;
        }
        else if (child < 0)
        {
            printf("Error\n");
            exit(1);
        }
    }

    //Create Consumers
    for (int i = 0; i < C; i++)
    {
        child = fork();

        if (child == 0)
        {
            consume(queue, strips, X);
            return 0;
        }
        else if (child < 0)
        {
            printf("Error\n");
            exit(1);
        }
    }

    while ((child = waitpid(-1, &status, 0)) != -1);

    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday");
        abort();
    }
    times[1] = (tv.tv_sec) + tv.tv_usec/1000000.;
    printf("paster2 execution time: %.6lf seconds\n", times[1] - times[0]);

    return 0;
}

void produce(queuePNG queue, char *url, int *count)
{
    CURL *curl_handle;
    CURLcode res;

    // initialize
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl_handle = curl_easy_init();

    if (curl_handle == NULL)
    {
        fprintf(stderr, "curl_easy_init: returned NULL\n");
        exit(1);
    }

    // count ranges from 0 to 50
    while (*count < STRIP_AMOUNT)
    {
        /*Create URL for the image we want*/
        sem_wait(count_sem); //Semaphore called to ensure nobody intereferes
        char newURL[100];
        strcpy(newURL, url);
        int a = *count;
        char arr[10] = "";
        sprintf(arr, "%d", a);
        strcat(newURL, arr);
        *count += 1;
        sem_post(count_sem);

        /* specify URL to get */
        curl_easy_setopt(curl_handle, CURLOPT_URL, newURL);

        /* register write call back function to process received data */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_cb_curl3);

        /* register header call back function to process received header data */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERFUNCTION, header_cb_curl);

        /* some servers requires a user-agent field */
        curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

        RECV_BUF recv_buf;
        // initialize
        recv_buf_init(&recv_buf, BUF_SIZE);

        /* user defined data structure passed to the call back function */
        curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&recv_buf);

        /* user defined data structure passed to the call back function */
        curl_easy_setopt(curl_handle, CURLOPT_HEADERDATA, (void *)&recv_buf);
        /* get it! */
        res = curl_easy_perform(curl_handle);

        // make sure that it didn't fail
        if (res != CURLE_OK)
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }
        else
        {
            //Wait for buffer_sem && wait for enq_sem
            sem_wait(buffer_sem);
            sem_wait(enq_sem);
                
            enqueue(queue, recv_buf.buf, recv_buf.seq); //Enqueue
            // printf("Received: %d\n", recv_buf.seq);
            sem_post(enq_sem);
        }
        recv_buf_cleanup(&recv_buf);
    }

    /* cleaning up */
    curl_easy_cleanup(curl_handle);
    curl_global_cleanup();
}

void consume(queuePNG queue, allStrips strips, int sleep_length)
{
    while (strips->counter < STRIP_AMOUNT)
    {
        // put consumer to sleep for X time in milliseconds
        usleep(1000 * sleep_length);

        // wait on dequeue semaphor
        sem_wait(deq_sem);
        // get a queue entry
        queueEntry received_entry = dequeue(queue);

        // make sure the recieved entry exists
        if (received_entry != NULL)
        {

            simple_PNG_p PNG = getPNGInfo(received_entry->buffer);

            simple_PNG_p uncompressedPNG = inflatePNGS(PNG);

            if (strips->counter == 0) {
                memcpy(sampleBuffer, received_entry->buffer, BUF_SIZE);
            }

            // get strip number of the recieved entry
            int receivedStripNum = received_entry->stripNum;

            int IDAT_length = uncompressedPNG->p_IDAT->length;
            // set the IDAT_data of the strip in the specified location within the array
            memcpy(strips->strips_array[receivedStripNum].IDAT_data, uncompressedPNG->p_IDAT->p_data, IDAT_length);

            // set the length of the IDAT data
            strips->strips_array[receivedStripNum].IDATlength = IDAT_length;

            // set the strip number
            strips->strips_array[receivedStripNum].stripNum = receivedStripNum;

            // increment
            strips->counter++;
            // post on buffer_sem because taking item out of the buffer
            sem_post(buffer_sem);
        }
        sem_post(deq_sem);
    }

    sem_wait(createPNG);

    //Once we get all 50 PNG strips
    if(strips->counter == 50){

        simple_PNG_p allPNGS[STRIP_AMOUNT];

        simple_PNG_p samplePNG = malloc(sizeof(struct simple_PNG));

        //Taking a PNG "strip" from before and breaking it into its components
        //We only want the IHDR and IEND data as that's the same for all strips
        samplePNG = getPNGInfo(sampleBuffer);


        for (int i = 0; i < 50; i++){
            simple_PNG_p s = malloc(sizeof(struct simple_PNG));
            chunk_p IHDR = malloc(sizeof(struct chunk));
            chunk_p IDAT = malloc(sizeof(struct chunk));
            chunk_p IEND = malloc(sizeof(struct chunk));

            IHDR->length = samplePNG->p_IHDR->length;
            U8* IHDR_data_buffer = malloc(IHDR->length*sizeof(U8*));
            memcpy(IHDR_data_buffer, samplePNG->p_IHDR->p_data, samplePNG->p_IHDR->length);
            IHDR->p_data = IHDR_data_buffer;
            memcpy(IHDR->type, samplePNG->p_IHDR->type, 4);
            IHDR->crc = samplePNG->p_IHDR->crc;

            //Get each strip's IDAT data from strips->strips_array
            IDAT->length = strips->strips_array[i].IDATlength;
            IDAT->p_data = strips->strips_array[i].IDAT_data;
            memcpy(IDAT->type, samplePNG->p_IDAT->type, 4);
            IDAT->crc = samplePNG->p_IDAT->crc;

            IEND->length = samplePNG->p_IEND->length;
            IEND->p_data = samplePNG->p_IEND->p_data;
            memcpy(IEND->type, samplePNG->p_IEND->type, 4);
            IEND->crc = samplePNG->p_IEND->crc;

            s->magicNumBuffer = samplePNG->magicNumBuffer;
            s->p_IHDR = IHDR;
            s->p_IDAT = IDAT;
            s->p_IEND = IEND;

            allPNGS[i] = s;
        }

        /*Combine the array of PNGS*/
        simple_PNG_p combinedPNG;
        combinedPNG = malloc(sizeof(struct simple_PNG));
        combinedPNG = combinePNGS(allPNGS, 50);

        /*Write PNG to a file*/
        createPNGFile(combinedPNG);
        strips->counter += 1;
    }

    sem_post(createPNG);
}

queueEntry dequeue(queuePNG queue)
{
    // queueEntry returnVal;

    if (queue->head == -1)
    {
        return NULL;
    }
    queueEntry returnVal = malloc(sizeof(struct Queue_Entry));

    memcpy(returnVal->buffer, queue->queueE[queue->head].buffer, BUF_SIZE);
    returnVal->stripNum = queue->queueE[queue->head].stripNum;
    

    if (queue->head == queue->tail)
    {
        queue->head = -1;
        queue->tail = -1;
        // printf("NO SIZE: %i\n", queue->queueE[queue->head].stripNum);
    }
    else if (queue->head == (queue->max - 1))
    {
        queue->head = 0;
    }
    else
    {
        queue->head += 1;
    }
    queue->count--;

    return returnVal;
}

void enqueue(queuePNG queue, char *buffer, int stripNum)
{
    /*This should never happen but having it cause why not*/
    if ((queue->head == 0 && queue->tail == (queue->max - 1)) || (queue->tail == (queue->head - 1) % (queue->max - 1)))
    {
        // printf("\n--Queue is Full");
        return;
    }

    else if (queue->head == -1) /* Insert First Element */
    {
        queue->head = 0;
        queue->tail = 0;
        memcpy(queue->queueE[queue->tail].buffer, buffer, BUF_SIZE);
        queue->queueE[queue->tail].stripNum = stripNum;
        queue->count++;
        
        // printf("stripNum: %i\n", queue->queueE[queue->tail].stripNum);
    }

    else if ((queue->tail == (queue->max - 1)) && (queue->head != 0))
    {
        queue->tail = 0;
        memcpy(queue->queueE[queue->tail].buffer, buffer, BUF_SIZE);
        queue->queueE[queue->tail].stripNum = stripNum;
        queue->count++;
        // printf("----reset\n");
    }
    else
    {
        queue->tail += 1;
        memcpy(queue->queueE[queue->tail].buffer, buffer, BUF_SIZE);
        queue->queueE[queue->tail].stripNum = stripNum;
        queue->count++;
        // printf("------------------------enqueued\n");
    }
    // printf("enqueue tail: %d\n", queue->tail);
    // printf("enqueue head: %d\n", queue->head);
}

// BOILER PLATE CODE BELOW

size_t header_cb_curl(char *p_recv, size_t size, size_t nmemb, void *userdata)
{
    int realsize = size * nmemb;
    RECV_BUF *p = userdata;

    if (realsize > strlen(ECE252_HEADER) &&
        strncmp(p_recv, ECE252_HEADER, strlen(ECE252_HEADER)) == 0)
    {

        /* extract img sequence number */
        p->seq = atoi(p_recv + strlen(ECE252_HEADER));
    }
    return realsize;
}

size_t write_cb_curl3(char *p_recv, size_t size, size_t nmemb, void *p_userdata)
{
    size_t realsize = size * nmemb;
    RECV_BUF *p = (RECV_BUF *)p_userdata;

    if (p->size + realsize + 1 > p->max_size)
    { /* hope this rarely happens */
        /* received data is not 0 terminated, add one byte for terminating 0 */
        size_t new_size = p->max_size + max(BUF_INC, realsize + 1);
        char *q = realloc(p->buf, new_size);
        if (q == NULL)
        {
            perror("realloc"); /* out of memory */
            return -1;
        }
        p->buf = q;
        p->max_size = new_size;
    }

    memcpy(p->buf + p->size, p_recv, realsize); /*copy data from libcurl*/
    p->size += realsize;
    p->buf[p->size] = 0;

    return realsize;
}

int recv_buf_init(RECV_BUF *ptr, size_t max_size)
{
    void *p = NULL;

    if (ptr == NULL)
    {
        return 1;
    }

    p = malloc(max_size);
    if (p == NULL)
    {
        return 2;
    }

    ptr->buf = p;
    ptr->size = 0;
    ptr->max_size = max_size;
    ptr->seq = -1; /* valid seq should be non-negative */
    return 0;
}
int recv_buf_cleanup(RECV_BUF *ptr)
{
    if (ptr == NULL)
    {
        return 1;
    }

    free(ptr->buf);
    ptr->size = 0;
    ptr->max_size = 0;
    return 0;
}
