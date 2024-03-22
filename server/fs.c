#define _GNU_SOURCE
#include "fs.h"
#include "lib/hash.h"
#include "lock.h"
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "tecnicofs-api-constants.h"
#include "lib/inodes.h"
#include "lib/bst.h"
#include "constants.h"

void err_dump(char* error, int exitCode) {
    perror(error);
    exit(exitCode);
} 

tecnicofs* new_tecnicofs(int buckets){
	int i;
	tecnicofs* fs = (tecnicofs*) malloc(sizeof(tecnicofs));
	if (!fs) 
		err_dump("Failed to allocate tecnicofs.\n", EXIT_FAILURE);

	fs->bstRoot = (node**) malloc(sizeof(node*) * buckets);
	if (!(fs->bstRoot)) {
		perror("Failed to allocate memory for bstRoots array.\n");
		exit(EXIT_FAILURE);
	}

	fs->lockArr = (LOCK_TYPE*) malloc(sizeof(LOCK_TYPE) * buckets);
	if (!(fs->lockArr))
		err_dump("Failed to allocate memory for locks array.\n", EXIT_FAILURE);

	for (i = 0; i < buckets; i++) {
		fs->bstRoot[i] = NULL;
		initLock(&(fs->lockArr[i]));
	}

	fs->nextINumber = 0;
	fs->buckets = buckets;
	return fs;
}

void free_tecnicofs(tecnicofs* fs){
	int i;

	for (i = 0; i < fs->buckets; i++) {
		destroyLock(&(fs->lockArr[i]));
		free_tree(fs->bstRoot[i]);
	}
	free(fs->lockArr);
	free(fs->bstRoot);
	free(fs);
}

void create(int sockfd, tecnicofs* fs, char* filename, char* perm, struct ucred* ucred) {
	int slot = hash(filename, fs->buckets);
	setLock(&(fs->lockArr[slot]));
	int iNumber;
	if (search(fs->bstRoot[slot], filename)) {
		dprintf(sockfd, "%c", TECNICOFS_ERROR_FILE_ALREADY_EXISTS);
		unLock(&(fs->lockArr[slot]));
		return;
	}

	if (((perm[0]-'0') != RW && (perm[0]-'0') != WRITE && (perm[0]-'0') != READ) || 
		((perm[1]-'0') != RW && (perm[1]-'0') != WRITE && (perm[1]-'0') != READ)) {
		dprintf(sockfd, "%c", TECNICOFS_ERROR_OTHER);
		unLock(&(fs->lockArr[slot]));
		return;
	}

    iNumber = inode_create(ucred->uid, perm[0] - '0', perm[1] - '0');
    if (iNumber == -1) {
    	err_dump("inode_create error", EXIT_FAILURE);
	}

	fs->bstRoot[slot] = insert(fs->bstRoot[slot], filename, iNumber);

	dprintf(sockfd, "%c", TECNICOFS_SUCCESS);
	unLock(&(fs->lockArr[slot]));
}

void delete(int sockfd, tecnicofs* fs, char *filename, clientFile* clientFiles, struct ucred* ucred) {
	int slot = hash(filename, fs->buckets);
	int deleteRes;
	setLock(&(fs->lockArr[slot]));
	uid_t owner;
	node* searchResult = search(fs->bstRoot[slot], filename);
	if (!searchResult) {
		dprintf(sockfd, "%c", TECNICOFS_ERROR_FILE_NOT_FOUND);
		unLock(&(fs->lockArr[slot]));
		return;
	}

	if(inode_get(searchResult->inumber, &owner, NULL, NULL, NULL, 0) == -1) {
        err_dump("inode_get error", EXIT_FAILURE);
	}
    
	if (ucred->uid != owner) {
		dprintf(sockfd, "%c", TECNICOFS_ERROR_PERMISSION_DENIED);
		unLock(&(fs->lockArr[slot]));
		return;
	}

	deleteRes = inode_delete(searchResult->inumber);

	if (deleteRes == -1)
        err_dump("inode_delete error", EXIT_FAILURE);

	else if (deleteRes == -2) {
		dprintf(sockfd, "%c", TECNICOFS_ERROR_FILE_IS_OPEN);
		unLock(&(fs->lockArr[slot]));
		return;
	}
	fs->bstRoot[slot] = remove_item(fs->bstRoot[slot], filename);
	dprintf(sockfd, "%c", TECNICOFS_SUCCESS);
	unLock(&(fs->lockArr[slot]));
}

int lookup(tecnicofs* fs, char *name) {
	int inumber;
	int slot = hash(name, fs->buckets);
	setLockRD(&(fs->lockArr[slot]));
	node* searchNode = search(fs->bstRoot[slot], name);
	if ( searchNode ) {
		inumber = searchNode->inumber;
		unLock(&(fs->lockArr[slot]));
		return inumber;
	}
	unLock(&(fs->lockArr[slot]));
	return -1;

}

void renameFile(int sockfd, tecnicofs* fs, char *filename, char *newFilename, struct ucred* ucred) {
	int oldINumber; 
	int oldSlot = hash(filename, fs->buckets);
	int newSlot = hash(newFilename, fs->buckets);
	double sleepyTime;
	uid_t owner;
	while (1) {
		setLock(&(fs->lockArr[oldSlot]));
		node* searchNew;
		node* searchOld = search(fs->bstRoot[oldSlot], filename);

		if (searchOld) {
			oldINumber = searchOld->inumber;

			if (newSlot == oldSlot || !(setTryLock(&(fs->lockArr[newSlot])))) {  // if the bucket is the same in removing and inserting
				searchNew = search(fs->bstRoot[newSlot], newFilename);                  // the first lock is enough.
				if (!searchNew) {

					if (inode_get(searchOld->inumber, &owner, NULL, NULL, NULL, 0) == -1)
        				err_dump("inode_get error", EXIT_FAILURE);
					
					if (ucred->uid != owner) {
						dprintf(sockfd, "%c", TECNICOFS_ERROR_PERMISSION_DENIED);
						break;
					}

					fs->bstRoot[newSlot] = insert(fs->bstRoot[newSlot], newFilename, oldINumber);
					fs->bstRoot[oldSlot] = remove_item(fs->bstRoot[oldSlot], filename);

					dprintf(sockfd, "%c", TECNICOFS_SUCCESS);
					break;
				}
				dprintf(sockfd, "%c", TECNICOFS_ERROR_FILE_ALREADY_EXISTS);
				break;
			}
			unLock(&(fs->lockArr[oldSlot]));
			sleepyTime = ((double) rand() / (RAND_MAX));
			sleep(sleepyTime);
			continue;
		}
		dprintf(sockfd, "%c", TECNICOFS_ERROR_FILE_NOT_FOUND);
		unLock(&(fs->lockArr[oldSlot]));
		return;
	}
	if (newSlot != oldSlot)
		unLock(&(fs->lockArr[newSlot]));
	unLock(&(fs->lockArr[oldSlot]));
}

void openFile(int sockfd, clientFile* clientFiles, char* filename, int mode, struct ucred* ucred, tecnicofs* fs) {
	int slot = hash(filename, fs->buckets);
	setLock(&(fs->lockArr[slot])); // Lock due to the gap between inode_set and inode_get
	int i;
	
	node* searchNode = search(fs->bstRoot[slot], filename);

    uid_t owner;
    permission ownerPerm, othersPerm;

    if (searchNode == NULL) {
        dprintf(sockfd, "%c", TECNICOFS_ERROR_FILE_NOT_FOUND);
		unLock(&(fs->lockArr[slot]));
        return;
    }

	if (mode != RW && mode != WRITE && mode != READ) {
		dprintf(sockfd, "%c", TECNICOFS_ERROR_OTHER);
		unLock(&(fs->lockArr[slot]));
		return;
	}

	for (i = 0; i < MAX_OPEN_FILES; i++) {
		if (clientFiles[i].iNumber == searchNode->inumber) {
			dprintf(sockfd, "%c", TECNICOFS_ERROR_FILE_IS_OPEN);
			unLock(&(fs->lockArr[slot]));
			return;
		}
	}
   
    if (inode_get(searchNode->inumber, &owner, &ownerPerm, &othersPerm, NULL, 0) == -1)
        err_dump("inode_get error", EXIT_FAILURE);

    if (ucred->uid == owner) {
		if ( (ownerPerm == RW && (mode == RW || mode == WRITE)) || (ownerPerm == mode) ) {
                tryOpenFile(sockfd, clientFiles, mode, searchNode->inumber);
				unLock(&(fs->lockArr[slot]));
                return;
        }
        dprintf(sockfd, "%c", TECNICOFS_ERROR_PERMISSION_DENIED);
    }
    else {
		if ( (othersPerm == RW && (mode == RW || mode == WRITE)) || (othersPerm == mode) ) {
                tryOpenFile(sockfd, clientFiles, mode, searchNode->inumber);
				unLock(&(fs->lockArr[slot]));
                return;
        }
        dprintf(sockfd, "%c", TECNICOFS_ERROR_PERMISSION_DENIED);
    }
	unLock(&(fs->lockArr[slot]));
}

void tryOpenFile(int sockfd, clientFile * clientFiles, int openMode, int iNumber) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (clientFiles[i].iNumber == -1) {
			incrementNumClients(iNumber);
            clientFiles[i].openMode = openMode;
            clientFiles[i].iNumber = iNumber;
            dprintf(sockfd, "%c", i);
            return;
        }
    }
    dprintf(sockfd, "%c", TECNICOFS_ERROR_MAXED_OPEN_FILES);
}

void doCloseFile(int sockfd, clientFile* clientFiles, int fd) {
    if (clientFiles[fd].iNumber == -1) {
        dprintf(sockfd, "%c", TECNICOFS_ERROR_FILE_NOT_OPEN);
        return;
    }

	if (fd < 0 || fd > 4) {
		dprintf(sockfd, "%c", TECNICOFS_ERROR_OTHER);
		return;
	}

    decrementNumClients(clientFiles[fd].iNumber);
    clientFiles[fd].iNumber = -1;
    dprintf(sockfd, "%c", TECNICOFS_SUCCESS);
}

void writeFile(int sockfd, clientFile* clientFiles, int fd, char* dataInBuffer, struct ucred* ucred, tecnicofs* fs) {
    uid_t owner;
    permission ownerPerm, othersPerm;
    
    if (clientFiles[fd].iNumber == -1) {
        dprintf(sockfd, "%c", TECNICOFS_ERROR_FILE_NOT_OPEN);
        return;
    }

	if (fd < 0 || fd > 4) {
		dprintf(sockfd, "%c", TECNICOFS_ERROR_OTHER);
		return;
	}	

    if (clientFiles[fd].openMode != WRITE && clientFiles[fd].openMode != RW) {
        dprintf(sockfd, "%c", TECNICOFS_ERROR_INVALID_MODE);    
        return;
    }

    if (inode_get(clientFiles[fd].iNumber, &owner, &ownerPerm, &othersPerm, NULL, 0) == -1)
        err_dump("inode_get error", EXIT_FAILURE);
    
    if (ucred->uid == owner) {
        if (ownerPerm == WRITE || ownerPerm == RW) {
            if (inode_set(clientFiles[fd].iNumber, dataInBuffer, strlen(dataInBuffer)) == -1)
                err_dump("inode_set error", EXIT_FAILURE);
            dprintf(sockfd, "%c", TECNICOFS_SUCCESS);
            return;
        }
        dprintf(sockfd, "%c", TECNICOFS_ERROR_PERMISSION_DENIED);
    }
    else {
        if (othersPerm == WRITE || othersPerm == RW) {
            if (inode_set(clientFiles[fd].iNumber, dataInBuffer, strlen(dataInBuffer)) == -1)
                err_dump("inode_set error", EXIT_FAILURE);
            dprintf(sockfd, "%c", TECNICOFS_SUCCESS);
            return;
        }
        dprintf(sockfd, "%c", TECNICOFS_ERROR_PERMISSION_DENIED);
    }
}

void readFile(int sockfd, clientFile* clientFiles, int fd, int len, struct ucred* ucred, tecnicofs* fs) {
    permission ownerPerm, othersPerm;
    uid_t owner;
    char fileContent[MAX_INPUT_SIZE] = "";
	int getRes;

    if (clientFiles[fd].iNumber == -1) {
        dprintf(sockfd, "%c", TECNICOFS_ERROR_FILE_NOT_OPEN);
        return;
    }

	if (fd < 0 || fd > 4) {
		dprintf(sockfd, "%c", TECNICOFS_ERROR_OTHER);
		return;
	}

	if (len < 0) {
		dprintf(sockfd, "%c", TECNICOFS_ERROR_OTHER);
		return;
	}

    if (clientFiles[fd].openMode != RW && clientFiles[fd].openMode != READ) {
        dprintf(sockfd, "%c", TECNICOFS_ERROR_INVALID_MODE);
        return;
    }

    if ((getRes = inode_get(clientFiles[fd].iNumber, &owner, &ownerPerm, &othersPerm, fileContent, len - 1)) == -1)
        err_dump("inode_get error", EXIT_FAILURE);


    if (ucred->uid == owner) {
        if (ownerPerm == READ || ownerPerm == RW) {
			if (getRes == 0) {
				dprintf(sockfd, "%c", TECNICOFS_EMPTY_FILECONTENT);
				return;
			}
            dprintf(sockfd, "%s", fileContent);
            return;
        }
        dprintf(sockfd, "%c", TECNICOFS_ERROR_PERMISSION_DENIED);
    }
    else {
        if (othersPerm == READ || othersPerm == RW) {
            dprintf(sockfd, "%s", fileContent);
            return;
        }
        dprintf(sockfd, "%c", TECNICOFS_ERROR_PERMISSION_DENIED);
    }
}

void closeAllFiles(clientFile* clientFiles) {
	for (int i = 0; i < MAX_OPEN_FILES; i++) {
		if (clientFiles[i].iNumber != -1)
    		decrementNumClients(clientFiles[i].iNumber);
	}
}

void print_tecnicofs_tree(FILE * fp, tecnicofs *fs) {
	int i;
	for (i = 0; i < fs->buckets; i++)
		print_tree(fp, fs->bstRoot[i]);
}