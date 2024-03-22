/* Authors:
    Andre Luis Raposo Marinho 93687
    Rodrigo Resendes Pedro 93753    */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include "fs.h"
#include "lock.h"
#include "constants.h"
#include "lib/timer.h"
#include "lib/inodes.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>

int buckets = 0;
tecnicofs* fs;
FILE *outputFile;
int servfd;  // server socket

int ctrlCFlag = 1;  // signal flag for CTRL C

void checkOutputFile(FILE* outputFile) {
    if(!outputFile)
        err_dump("Error opening output file.\n", EXIT_FAILURE);
}

void closeFile(FILE* fileToClose) {
    if (fclose(fileToClose) == EOF)
        err_dump("Error closing file.\n", EXIT_FAILURE);
}

static void displayUsage (const char* appName){
    printf("Usage: %s\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs (long argc, char* const argv[]){
    if (argc != 4) {
        perror("Invalid format:\n");
        displayUsage(argv[0]);
    }
    
    if ((buckets = atoi(argv[3])) < 1)
        err_dump("Error: Invalid number of buckets.\n", EXIT_FAILURE);
}

void errorParse() {
    err_dump("Error: command invalid\n", EXIT_FAILURE);
}

int readCommand(char* line, int newsockfd) {  
    int n = read(newsockfd, line, MAX_RENAME_STRING); 
                                    
    line[n] = 0;

    if (n == 0)
        return 0;

    else if (n < 0)
        err_dump("readCommand: readlineerror", EXIT_FAILURE);

    char token;
    char name[MAX_INPUT_SIZE], newName[MAX_INPUT_SIZE];
    int numTokens = sscanf(line, "%c %s %[^\n]", &token, name, newName);

    /* perform minimal validation */
    if (numTokens < 1) {
        err_dump("Invalid number of tokens", EXIT_FAILURE);
    }
    switch (token) {
        case 'd':
        case 'x':
        case 'f':
            if(numTokens != 2)
                err_dump("Error: command invalid\n", EXIT_FAILURE);

            break;
        case 'l':
        case 'c':
        case 'o':        
        case 'w':         
        case 'r':
            if (numTokens != 3)
                err_dump("Error: command invalid\n", EXIT_FAILURE);
        case '#':
            break;
        default: { /* error */
            err_dump("Error: command invalid\n", EXIT_FAILURE);
        }
    }
    return 1;
}

void initClientFiles(clientFile* clientFiles) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        clientFiles[i].openMode = -1;
        clientFiles[i].iNumber = -1;
    }
}

int applyCommand(char* command, clientFile* clientFiles, int newsockfd, struct ucred* ucred) {
    char token;
    char split1[MAX_INPUT_SIZE], split2[MAX_INPUT_SIZE];
    int numTokens = sscanf(command, "%c %s %[^\n]", &token, split1, split2);

    if ((command[0] == 'c' || command[0] == 'r'|| command[0] == 'o'|| command[0] == 'l'|| command[0] == 'w') && numTokens != 3)
        err_dump("Error: invalid command in Queue\n", EXIT_FAILURE);

    else if ((command[0] == 'd' || command[0] == 'x' || command[0] == 'f')&& numTokens != 2)
        err_dump("Error: invalid command in Queue\n", EXIT_FAILURE);  

    switch (token) {
        case 'c':
            create(newsockfd, fs, split1, split2, ucred);
            break;
        case 'l':
            readFile(newsockfd, clientFiles, atoi(split1), atoi(split2), ucred, fs);
            break;
        case 'd':
            delete(newsockfd, fs, split1, clientFiles, ucred);
            break;
        case 'r':
            renameFile(newsockfd, fs, split1, split2, ucred);
            break;
        case 'o':
            openFile(newsockfd, clientFiles, split1, atoi(split2), ucred, fs);
            break;
        case 'x':
            doCloseFile(newsockfd, clientFiles, atoi(split1));
            break;
        case 'w':
            writeFile(newsockfd, clientFiles, atoi(split1), split2, ucred, fs);
            break;
        case 'f':
            return 0;
        default: { /* error */
            err_dump("Error: command to apply\n", EXIT_FAILURE);
        }
    }
    return 1;
}

void* processClient(void* arg) {   // each client ignores CTRL C signal
    sigset_t mask;
    if (sigemptyset(&mask) == -1)
        err_dump("sigemptyset error", EXIT_FAILURE);

    if (sigaddset(&mask, SIGINT) == -1)
        err_dump("sigaddset error", EXIT_FAILURE);

    if (pthread_sigmask(SIG_BLOCK, &mask, NULL) < 0)
        err_dump("sigprocmask error", EXIT_FAILURE);

    struct ucred ucred;
	socklen_t ucredlen = sizeof(ucred);
    clientFile clientFiles[MAX_OPEN_FILES];

    initClientFiles(clientFiles);  // each client has a five slots array for files
    char command[MAX_INPUT_SIZE];

    if (getsockopt(*((int *)arg), SOL_SOCKET, SO_PEERCRED, &ucred, &ucredlen) == -1) // sending ucred to all functions to make verifications.
        err_dump("getsockopt error", EXIT_FAILURE);

    while(1) { // loop to read all commands until "f" is read
        if (readCommand(command, *((int *)arg))) {
            if (!applyCommand(command, clientFiles, *((int *)arg), &ucred))
                break;
        }
        else 
            break;
    }

    closeAllFiles(clientFiles);

    if (close(*((int *)arg)) == -1)
        err_dump("close error", EXIT_FAILURE);
    free((int *)arg);
    return NULL;
}

void ctrlcHandler() {
    ctrlCFlag = 0;
}

int tfsMount(char * address) {   // starts the connection with possible clients
    int i;
    pthread_t *slaves = NULL;
    int numSlaves = 0;
    int *clientfd, clilen, servlen;
    struct sockaddr_un cli_addr, serv_addr;

    if ((servfd = socket(AF_UNIX,SOCK_STREAM,0) ) < 0)
        err_dump("server: can't open stream socket", EXIT_FAILURE);
        
    unlink(address);

    bzero((char *)&serv_addr, sizeof(serv_addr));

    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, address);
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);

    if (bind(servfd, (struct sockaddr *) &serv_addr, servlen) < 0)
        err_dump("server, can't bind local address", EXIT_FAILURE);
            
    if (listen(servfd, 5) == -1)
        err_dump("listen error", EXIT_FAILURE);

    inode_table_init();

    while (ctrlCFlag) { 
        clilen = sizeof(cli_addr);
        clientfd = (int*) malloc(sizeof(int));

        if (!clientfd)
            err_dump("Can't allocate memory for clientfd", EXIT_FAILURE);

        *clientfd = accept(servfd, (struct sockaddr *) &cli_addr, (socklen_t *) &clilen);

        if (*clientfd < 0 && ctrlCFlag == 1) 
            err_dump("server: accept error", EXIT_FAILURE);

        else if (*clientfd < 0 && ctrlCFlag == 0) {
            break;
        }

        slaves = (pthread_t*) realloc(slaves, sizeof(pthread_t) * (numSlaves + 1));

        if (!slaves) 
            err_dump("Error allocating memory for thread.\n", EXIT_FAILURE);

        if (pthread_create(&slaves[numSlaves++], 0, processClient, clientfd))
            err_dump("Error creating thread for client process.\n", EXIT_FAILURE);
    }

    free(clientfd);

    for (i = 0; i < numSlaves; i++) {
        if (pthread_join(slaves[i], NULL))
            err_dump("Error pthread join.", EXIT_FAILURE);
    }

    if (close(servfd) == -1)
        err_dump("close error", EXIT_FAILURE);

    unlink(address);

    free(slaves);
    inode_table_destroy();
    return 0;
}

int main(int argc, char* argv[]) {
    struct sigaction a;
    a.sa_handler = ctrlcHandler;
    a.sa_flags = 0;
    
    if (sigemptyset(&a.sa_mask) == -1)
        err_dump("sigemptyset error", EXIT_FAILURE);

    if (sigaction(SIGINT, &a, NULL) == -1)
        err_dump("sigaction error", EXIT_FAILURE);

    TIMER_T startTime, endTime;
    
    parseArgs(argc, argv);
    
    fs = new_tecnicofs(buckets);

    outputFile = fopen(argv[2], "w");
    checkOutputFile(outputFile);      /* Looking for errors opening input and output files. */

    TIMER_READ(startTime);

    tfsMount(argv[1]);

    print_tecnicofs_tree(outputFile, fs);

    TIMER_READ(endTime);

    free_tecnicofs(fs);

    closeFile(outputFile); /* Checking for errors closing outputfile. */
    
    printf("TecnicoFS completed in %.4lf seconds.\n", TIMER_DIFF_SECONDS(startTime, endTime)); 

    exit(EXIT_SUCCESS);
}