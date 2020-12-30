#include <stdio.h>
#include <semaphore.h>

#define IMG_URL1 "http://ece252-1.uwaterloo.ca:2530/image?img=1&part="
#define IMG_URL2 "http://ece252-1.uwaterloo.ca:2530/image?img=2&part="
#define IMG_URL3 "http://ece252-1.uwaterloo.ca:2530/image?img=3&part="
#define DUM_URL "https://example.com/"
#define ECE252_HEADER "X-Ece252-Fragment: "
#define BUF_SIZE 1048576 /* 1024*1024 = 1M */
#define BUF_INC 524288   /* 1024*512  = 0.5M */

#define STRIP_AMOUNT 50 /* this is the amount of strips needed to make a whole image */

#define max(a, b) \
    ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

typedef struct recv_buf2
{
    char *buf;       /* memory to hold a copy of received data */
    size_t size;     /* size of valid data in buf in bytes*/
    size_t max_size; /* max capacity of buf in bytes*/
    int seq;         /* >=0 sequence number extracted from http header */
                     /* <0 indicates an invalid seq number */
} RECV_BUF;

// struct for each entry within the queue
typedef struct Queue_Entry
{
    U8 buffer[BUF_SIZE]; // data retrieved from server
    int stripNum;          // unique strip number
} * queueEntry;

typedef struct Queue
{
    int head;
    int tail;
    int count;
    int max;
    queueEntry queueE; // points to first queue entry
} * queuePNG;

typedef struct Image_Strip
{
    U8 IDAT_data[BUF_SIZE];
    int IDATlength;
    int stripNum;
} * imageStrip;

typedef struct All_Strips
{
    imageStrip strips_array;
    int counter;
} * allStrips;

// struct ()

/*
    Here's what I think should occur (completely up to you):

    new structs for B:

    This struct is for storing the IDAT data to be inflated
    We don't need the IHDR and IEND data as they are constant
    and can be created manually

    struct (entry){
        char* allStrips[BUF_SIZE];
        int IDATlength;
    } 

    This is needed because entries might have diff IDAT lengths
    struct (queue2){
        struct entry* entry;
        int counter -> 0-49 used for while loop
    }
    Create these structs in shared memory, much like for the producer entries

    Code wise:
    The only sempahore shared between producers and consumers will be buffer_sem
    The reason is if producers end early, consumers might still be waiting for more data

    while(counter < STRIP_NUM){

        sem_wait(dequeue);
        dequeue(queue);

        if(dequeue doesn't return null){
            pull apart the queueEntry's buffer to get IDAT data

            store IDAT data and length in struct (queue2)->(entry)
            increment counter
        }

        sem_post(buffer_sem);
        sem_post(dequeue);
    }

    Also note there is a sleep functionality that needs to be implemented according
    to the X value we receive but like, thats just something like sleep(X) when dequeue
    doesnt return null

    Now we inflate each of the values in each queue2->entry
    Once inflated, we assign the IDAT length to the inflated length

    After all of this is done, consumer is finished

    In main: (I can do this part if you want since I did catpng and its fresh in my mind)

    We create the IHDR and IEND data fields (as well as the 8 bytes for PNG)

    we create a simple_PNG_list and each list entry will have a simple_png with all three chunks

    Deflate the values 

    Concatenate

    EZ
    .
    .
    .
    In theory :'(

    */