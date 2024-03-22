#ifndef FS_H
#define FS_H
#include "lib/bst.h"
#include "lock.h"
#include "constants.h"
#include <sys/socket.h>
#include <sys/un.h>


typedef struct tecnicofs {
    node** bstRoot;   // array de roots
    int nextINumber, buckets;
    LOCK_TYPE* lockArr;
} tecnicofs;

typedef struct clientFile {
    int openMode;
    int iNumber;
} clientFile;

int obtainNewInumber(tecnicofs* fs);
tecnicofs* new_tecnicofs(int buckets);
void free_tecnicofs(tecnicofs* fs);
void create(int sockfd, tecnicofs* fs, char *filename, char* perm, struct ucred* ucred);
void delete(int sockfd, tecnicofs* fs, char *filename, clientFile* clientFiles, struct ucred* ucred);
int lookup(tecnicofs* fs, char *name);
void renameFile(int sockfd, tecnicofs* fs, char* filename, char* newFilename, struct ucred* ucred);
void print_tecnicofs_tree(FILE * fp, tecnicofs *fs);
void openFile(int sockfd, clientFile* clientFiles, char* filename, int mode, struct ucred* ucred, tecnicofs* fs);
void tryOpenFile(int sockfd, clientFile * clientFiles, int openMode, int iNumber);
void doCloseFile(int sockfd, clientFile* clientFiles, int fd);
void writeFile(int sockfd, clientFile* clientFiles, int fd, char* dataInBuffer, struct ucred* ucred, tecnicofs* fs);
void readFile(int sockfd, clientFile* clientFiles, int fd, int len, struct ucred* ucred, tecnicofs* fs);
void closeAllFiles(clientFile* clientFiles);


void err_dump(char* error, int exitCode);
void writeToSocket(int sockfd, void* out, int n);
#endif /* FS_H */