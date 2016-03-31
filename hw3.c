/**
  * @Author Mark Judy (mjudy1@umbc.edu)
  * This is the main file for CMSC421 Homework 3.
  * This program accepts 3 command line arguments that correspond to
  * 1) a number of threads to spawn, representing Uber drivers, 2) a
  * data file to read requests for the Uber drivers from and 3) a seed
  * to give to a PRNG for debugging purposes. The program will then
  * simulate the Uber drivers picking up passengers from a hotel
  * and delivering them to another destination hotel whilre requests
  * remain in the request pool.
*/

/**
  * For this homework I implemented the request pool as a linked list.
  * Each node of the list stores data and a pointer to the next node in
  * the list. I chose to use a singly-linked list because the homework
  * did not call for traversing the list very often, and I only needed
  * to access the front and rear of the list for operations. A mutex lock
  * and condition variable are used to protect global data and prevent
  * race conditions. When there is no further work to be done, the main
  * thread notifies the children by changing a boolean flag to false
  * and sending a broadcast on the condition variable.
*/

#define _POSIX_C_SOURCE 199309L
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

void *carThread(void *args);

//Request Pool implementation as linked list queue
struct pool {
  int srcHotel;
  int destHotel;
  int numPassengers;
  int travelTime;
  struct pool *next;
};

struct stats {
  int totalPassengers;
  int totalTime;
};

int poolSize = 0;
pthread_mutex_t lock;
pthread_cond_t cv;
struct pool *poolHead = NULL;
struct pool *poolCurr = NULL;
struct stats *statsAry;
int *hotelAry;
bool working = true;
int *workLeft;

/**
  * main() - The entry point for the homework 2 program. This
  * function also manages thread creation and reads the data
  * file before broadcasting to the car threads that there
  * is new work added to the request pool.
  *
  * @argc - A count of the commandline arguments.
  * @argv - Array of commandline arguments.
  * Return: 0 on success. Nonzero on error.
*/
int main (int argc, char *argv[]) {
  int numCars, seed, srcHotel, destHotel, numPassengers, 
      minTime, maxTime, travelTime, diffTime;
  int i;
  int numHotels = 0;
  char *fileName, *token;
  char buff[80];
  FILE *fp;
  struct pool *poolPtr = (struct pool*)malloc(sizeof(struct pool));
  poolHead = poolCurr = poolPtr;
  pthread_mutex_init(&lock, NULL);
  pthread_cond_init(&cv, NULL);

  if (argc != 4) {
    perror("Invalid number of arguments");
    exit(EXIT_FAILURE);
  } else {
    numCars = atoi(argv[1]);
    seed = atoi(argv[3]);
    fileName = argv[2];

    //initialize PRNG
    srand(seed);

    //read data file to initialize hotel statistics
    if ((fp = fopen(fileName, "r")) == NULL) {
      perror("fopen");
      exit(EXIT_FAILURE);
    } else {
      if (fgets(buff, 80, fp) != NULL) {
        numHotels = atoi(buff);
        printf("Number of Hotels: %d\n", numHotels);
      }//end if

      printf("Initial hotel counts: \n");
      hotelAry = (int *)malloc(sizeof(int)*numHotels);
      for(i = 0; i < numHotels; i++) {
        if (fgets(buff, 80, fp) != NULL) {
          hotelAry[i] = atoi(buff);
          printf("\tHotel %d: %d\n", i, hotelAry[i]);
        }//end if
      }//end for
    }//end if-else
  
    //spawn threads
    statsAry = (struct stats *)malloc(sizeof(struct stats)*numCars);
    pthread_t tids[numCars];
    for (i = 0; i < numCars; i++) {
      int *arg = malloc(sizeof(*arg));
      *arg = i;
      pthread_create(tids + i, NULL, carThread, arg);
      statsAry[i].totalPassengers = 0;
      statsAry[i].totalTime = 0;
    }//end for

    //critical section?
    while (working) {
      pthread_mutex_lock(&lock);

      if ((workLeft = (int *)fgets(buff, 80, fp)) != NULL) {
        token = strtok(buff, " ");
        srcHotel = atoi(token);
        token = strtok(NULL, " ");
        destHotel = atoi(token);
        token = strtok(NULL, " ");
        numPassengers = atoi(token);
        token = strtok(NULL, " ");
        minTime = atoi(token);
        token = strtok(NULL, " ");
        maxTime = atoi(token);
        
        //calculate travelTime
        diffTime = maxTime - minTime;
        //reuse maxTime to store rand() result
        maxTime = rand() % diffTime;
        travelTime = maxTime + minTime;

        //allocate memory for new node
        poolPtr = (struct pool*)malloc(sizeof(struct pool));
        //set current next to new node
        poolCurr->next = poolPtr;
        //set current pointer to new node
        poolCurr = poolPtr;
        //store data in new node
        poolCurr->srcHotel = srcHotel;
        poolCurr->destHotel = destHotel;
        poolCurr->numPassengers = numPassengers;
        poolCurr->travelTime = travelTime;
        poolCurr->next = NULL;
        poolSize++;
      }//end if
      
      if (poolSize == 0 && workLeft == NULL){
        working = false;
      }

      pthread_cond_broadcast(&cv);
      pthread_mutex_unlock(&lock);
    }//end while

    //clean up threads, lock, and memory here
    for (i = 0; i < numCars; i++) {
      sleep(1);
      pthread_detach((pthread_t)tids + i);
      printf("Thread %u took %d passengers, for %d ms\n", (int)tids[i], 
              statsAry[i].totalPassengers, statsAry[i].totalTime);
    }//end for
    printf("After all requests:\n");
    for (i = 0; i < numHotels; i++) {
      printf("\tHotel %d: %d\n", i, hotelAry[i]);
    }//end for
    pthread_cond_destroy(&cv);
    pthread_mutex_destroy(&lock);
    fclose(fp);
    free(hotelAry);
    free(statsAry);
    poolPtr = poolHead;
    free(poolPtr);
  }//end if-else (line 32)

  exit(EXIT_SUCCESS);
  return 0;
}//end main()

/**
  * carThread() - This function defines the work to be done
  * by the car threads for this homework. While there are 
  * requests in the pool, this function will attempt to 
  * process the requests after acquiring a lock on the global
  * program data.
  *
  * @args - An array of potential arguments sent to this function
  * from the pthread_create() function.
*/
void *carThread(void *args) {
  int index = *(int *)(args);
  int threadNum = pthread_self();
  int srcHotel = 0, 
      destHotel = 0, 
      numPassengers = 0, 
      travelTime = 0;
  struct pool *poolPtr = NULL;

  while(working) {
    pthread_mutex_lock(&lock);
    while (poolSize == 0 && workLeft != NULL) {
      printf("Thread %d waiting...\n", index);
      pthread_cond_wait(&cv, &lock);
    }//end while

    //work stuff
    if(poolHead->next != NULL) {
        poolPtr = poolHead->next;
      if(hotelAry[poolPtr->srcHotel] >= poolPtr->numPassengers) {      
        srcHotel = poolPtr->srcHotel;
        destHotel = poolPtr->destHotel;
        numPassengers = poolPtr->numPassengers;
        travelTime = poolPtr->travelTime;
        statsAry[index].totalPassengers += numPassengers;
        statsAry[index].totalTime += travelTime;

        //delete completed request
        poolHead->next = poolPtr->next;
        free(poolPtr);
        poolSize--;

        hotelAry[srcHotel] -= numPassengers;
        printf("Thread %u: Taking %d from hotel %d to hotel %d (time %d ms)\n", threadNum, 
              numPassengers, srcHotel, destHotel, travelTime);
        //unlock before sleep
        pthread_mutex_unlock(&lock);
        //sleep for travel time
        struct timespec s;
        s.tv_sec = 0;
        s.tv_nsec = (long)(travelTime * 1000000);
        nanosleep(&s, NULL);
        
        //lock after sleep        
        if(pthread_mutex_lock(&lock) == 0) {
          hotelAry[destHotel] += numPassengers;
        }
      } else {
        if (poolPtr->next != NULL) {
          poolHead->next = poolPtr->next;
          poolCurr->next = poolPtr;
          poolCurr = poolPtr;
          poolCurr->next = NULL;
        } else {
          printf("One request remaining! Cannot defer!\n");
        }//end if-else
      }//end if-else
    }//end if
    if (poolSize == 0 && working == false) {      
      pthread_mutex_unlock(&lock);
      free(args);
      sleep(1);
      pthread_exit(NULL);
    }
    pthread_mutex_unlock(&lock);
  }//end while
  return NULL;
}//end carThread()
