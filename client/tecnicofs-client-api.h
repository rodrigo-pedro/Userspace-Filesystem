#ifndef TECNICOFS_CLIENT_API_H
#define TECNICOFS_CLIENT_API_H

#include "tecnicofs-api-constants.h"


int tfsCreate(char *filename, permission ownerPermissions, permission othersPermissions);
int tfsDelete(char *filename);
int tfsRename(char *filenameOld, char *filenameNew);
int tfsOpen(char *filename, permission mode);
int tfsClose(int fd);
int tfsRead(int fd, char *buffer, int len);
int tfsWrite(int fd, char *buffer, int len);
int tfsMount(char * address);
int tfsUnmount();
void err_dump(char* error, int exitCode);
void readFromSocket(char* feedback);

#endif /* TECNICOFS_CLIENT_API_H */
