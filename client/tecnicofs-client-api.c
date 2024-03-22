#include "tecnicofs-api-constants.h"
#include "tecnicofs-client-api.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int sockfd = -1;

void err_dump(char* error, int exitCode) {
    perror(error);
    exit(exitCode);
} 

int tfsMount(char* address) {
    struct sockaddr_un serv_addr;
    int servlen;

    if (address == NULL || address[0] == '\0')
        return TECNICOFS_ERROR_OTHER;

    if (sockfd != -1) 
        err_dump("Client already has an open session with a TecnicoFS server.", TECNICOFS_ERROR_OPEN_SESSION);

    if ((sockfd = socket(AF_UNIX, SOCK_STREAM, 0) ) < 0)
        err_dump("Error creating socket", TECNICOFS_ERROR_OTHER);
    
    bzero((char *) &serv_addr, sizeof(serv_addr));
    
    serv_addr.sun_family = AF_UNIX;
    strcpy(serv_addr.sun_path, address);
    servlen = strlen(serv_addr.sun_path) + sizeof(serv_addr.sun_family);

    if (connect(sockfd, (struct sockaddr *) &serv_addr, servlen) < 0)
        err_dump("Communication failed.", TECNICOFS_ERROR_CONNECTION_ERROR);
        
    return 0;
}

void readFromSocket(char* feedback) {
    int n = read(sockfd, feedback, 100);
    if (n == 0)
        err_dump("Error empty message in socket.", TECNICOFS_ERROR_OTHER);
    else if (n < 0)
        err_dump("Error in fucntion read", TECNICOFS_ERROR_OTHER);
    feedback[n] = 0;
}


int tfsCreate(char *filename, permission ownerPermissions, permission othersPermissions) {
    if (filename == NULL || filename[0] == '\0')
        return TECNICOFS_ERROR_OTHER;

    if (ownerPermissions != RW && ownerPermissions != WRITE && ownerPermissions != READ)
        return TECNICOFS_ERROR_OTHER;

    if (othersPermissions != RW && othersPermissions != WRITE && othersPermissions != READ)
        return TECNICOFS_ERROR_OTHER;

    if (sockfd == -1)
        return TECNICOFS_ERROR_NO_OPEN_SESSION;

    char feedback[2];
    dprintf(sockfd, "c %s %d%d", filename, ownerPermissions, othersPermissions);
     
    readFromSocket(feedback);
    return feedback[0];
}

int tfsDelete(char *filename) {
    if (filename == NULL || filename[0] == '\0')
        return TECNICOFS_ERROR_OTHER;

    if (sockfd == -1)
        return TECNICOFS_ERROR_NO_OPEN_SESSION;

    char feedback[2];
    dprintf(sockfd, "d %s", filename);
    readFromSocket(feedback);
    return feedback[0];

}

int tfsRename(char *filenameOld, char *filenameNew) {
    if ((filenameOld == NULL || filenameNew == NULL) || (filenameOld[0] == '\0' || filenameNew[0] == '\0'))
        return TECNICOFS_ERROR_OTHER;

    if (sockfd == -1)
        return TECNICOFS_ERROR_NO_OPEN_SESSION;

    char feedback[2];
    dprintf(sockfd, "r %s %s", filenameOld, filenameNew);
    readFromSocket(feedback);
    return feedback[0];
}

int tfsWrite(int fd, char *buffer, int len) {
    if (!(fd >= 0 && fd <= 4))
        return TECNICOFS_ERROR_OTHER;
    
    if (buffer == NULL || buffer[0] == '\0')
        return TECNICOFS_ERROR_OTHER;

    if (len < 0)
        return TECNICOFS_ERROR_OTHER;

    if (sockfd == -1)
        return TECNICOFS_ERROR_NO_OPEN_SESSION;

    char feedback[2];
    dprintf(sockfd, "w %d %.*s", fd, len, buffer);    
    readFromSocket(feedback);
    return feedback[0];
}

int tfsRead(int fd, char *buffer, int len) {
    int feedLen, sizeBuf;
    if (!(fd >= 0 && fd <= 4))
        return TECNICOFS_ERROR_OTHER;
    
    if (buffer == NULL)
        return TECNICOFS_ERROR_OTHER;

    if (len < 0)
        return TECNICOFS_ERROR_OTHER;

    if (sockfd == -1)
        return TECNICOFS_ERROR_NO_OPEN_SESSION;

    char feedback[len + 1];
    dprintf(sockfd, "l %d %d", fd, len);
    readFromSocket(feedback);

    feedLen = strlen(feedback);
    sizeBuf = (int)sizeof(buffer);

    if (feedLen == 1) {
        if (feedback[0] == TECNICOFS_EMPTY_FILECONTENT) {
            buffer[0] = 0;
            return 0;
        }
        else if (feedback[0] < 0) {
            return feedback[0]; 
        }   
    }
    else if (feedLen >= (int)sizeof(buffer)) {    // if the arg buffer's size is smaller or equal to the feedback strlen
        strncpy(buffer, feedback, sizeBuf - 1);   // write what fits into the buffer and return that length
    }
    else {
        strcpy(buffer, feedback);                 // or else disregard the number of chars to copy since the buffer 
    }                                             // should be larger than the feedback string
    return strlen(buffer);
}

int tfsOpen(char *filename, permission mode) {
    if (filename == NULL || filename[0] == '\0')
        return TECNICOFS_ERROR_OTHER;
    if (mode != RW && mode != WRITE && mode != READ)
        return TECNICOFS_ERROR_OTHER;

    if (sockfd == -1)
        return TECNICOFS_ERROR_NO_OPEN_SESSION;

    char feedback[2];
    dprintf(sockfd, "o %s %d", filename, mode);
    readFromSocket(feedback);
    return feedback[0];
}

int tfsClose(int fd) {
    if (!(fd >= 0 && fd <= 4))
        return TECNICOFS_ERROR_OTHER;

    if (sockfd == -1)
        return TECNICOFS_ERROR_NO_OPEN_SESSION;

    char feedback[2];
    dprintf(sockfd, "x %d", fd);
    readFromSocket(feedback);
    return feedback[0];
}

int tfsUnmount() {
    if (sockfd == -1)
        return TECNICOFS_ERROR_NO_OPEN_SESSION;
    
    dprintf(sockfd, "f end");

    if (close(sockfd) == -1)
        err_dump("Client close error", EXIT_FAILURE);
    return 0;
}
