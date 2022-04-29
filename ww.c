#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>

#define BUFFER_SIZE 16
struct pathName {
    char *prefix;
    char *fileName;
};

struct node {
    struct pathName *path;
    struct node *next;
};
struct queue {
    struct node *start;
    struct node *end;
    pthread_mutex_t lock;
    pthread_cond_t dequeue_ready;
    int closed;
} *fileQueue, *dirQueue;

struct flag {
    pthread_mutex_t lock;
    int status;
} *exitFlag, *activeDThreads;

void queue_close(struct queue *queue){
    pthread_mutex_lock(&queue->lock);

        queue->closed = 1;

        pthread_cond_broadcast(&queue->dequeue_ready);

    pthread_mutex_unlock(&queue->lock);

    return;
}

/*
* char *output: a string containing a filename which needs an output file to write to
*
* Returns a string with the prefix "wrap." in front of the string passed to the char *output parameter.
*/
char *getInputName(struct pathName *file){
    int plen = strlen(file->prefix);
    int nlen = strlen(file->fileName);
    char *inputName;

    if(plen != 0){
        inputName = (char *)malloc(sizeof(char)*(plen + nlen + 2));
        memcpy(inputName, file->prefix, plen);
        inputName[plen] = '/';
        memcpy(inputName + plen + 1, file->fileName, nlen);
        inputName[plen + nlen + 1] = '\0';
    } else {
        inputName = (char *)malloc(sizeof(char)*(nlen + 1));
        memcpy(inputName, file->fileName, nlen);
        inputName[nlen] = '\0';
    }
    return inputName;
}

char *getOutputName(struct pathName *file){
    int plen = strlen(file->prefix);
    int nlen = strlen(file->fileName);
    char wrap[] = {'w','r','a','p','.'};
    char *outputName;
    
    if(plen != 0){
        outputName = (char *)malloc(sizeof(char)*(plen + nlen + 7));
        memcpy(outputName, file->prefix, plen);
        outputName[plen] = '/';
        memcpy(outputName + plen + 1, wrap, 5);
        memcpy(outputName + plen + 6, file->fileName, nlen);
        outputName[plen + nlen + 6] = '\0';
    } else {
        outputName = (char *)malloc(sizeof(char)*(nlen + 6));
        memcpy(outputName, wrap, 5);
        memcpy(outputName + 5, file->fileName, nlen);
        outputName[nlen + 5] = '\0';
    }

    return outputName;
}

/*
*   int inputFD: a file descriptor which points to source of input data,
*   int lineLength: describes number of characters in a line for output formatting
*   int outputFD: a file descriptor pointing to the file to write formatted input to
*      (if NULL, outputs to STDOUT)
*   
*   Uses an input buffer to read data from input file fd,
*   processes data from input buffer character-by-character,
*   composing non-whitespace characters into words via output buffer,
*   and then using whitespace characters as cues for writing words 
*   or starting new paragraphs in the output file.
*/
int normalize(int inputFD, int lineLength, int outputFD){
    
    int lineSpot = 0;           //keeps track of the spot we are at in the current output file line
    int seenTerminator = 0;     //keeps track of consecutive newlines; if the last character was a newline, seenTerminator = 1
    int newPG = 0;              //keeps track of when to start a new paragraph before writing a new word
    int i;

    char buffer[BUFFER_SIZE];                                           //char array buffer for data read from input
    char *currWord = (char *)malloc(sizeof(char)*(lineLength + 1));     //string buffer for words to write to output
    int currWordSize = sizeof(char)*(lineLength + 1);                   //keep track of output buffer size for calculations & error-checking
    memset(currWord, '\0', currWordSize);                               //initialize buffer with null chars (it is a string)
    
    int bytesRead; // variable keeping track of how many bytes were read in

    bytesRead = read(inputFD, buffer, BUFFER_SIZE);
    if(bytesRead == 0){         //if file is empty, we can continue no further
        puts("ERROR: Input appears to contain zero bytes.");
        free(currWord);
        return EXIT_FAILURE;
    }         
    while(bytesRead > 0){

        for(i = 0; i < bytesRead; i++){         //iterate thru bytes read, one char at a time
            if(isspace(buffer[i])){ 

                if(buffer[i] == '\n'){
                    if(seenTerminator){
                        newPG = 1;
                        lineSpot = 0;
                    } else {
                        seenTerminator = 1;
                    }
                } else {
                    seenTerminator = 0;
                }

                if(currWord[0] != '\0'){        // go on to write currWord to output on reaching whitespace if currWord is non-empty
                    if(newPG){   //start a new paragraph before the next word is written if needed
                        write(outputFD, "\n\n", 2);
                        newPG = 0;
                        lineSpot = 0;
                    }
                    if(lineSpot == 0){                                  //if at the beginning of the line, write just the current token regardless of length
                        write(outputFD, currWord, strlen(currWord));
                        lineSpot += strlen(currWord);
                    } else if(lineSpot + 1 + strlen(currWord) <= lineLength){ //if not at the beginning of a line and the current word plus a space fits, add it to the current line
                        write(outputFD, " ", 1);
                        write(outputFD, currWord, strlen(currWord));    //write currWord preceded by a space
                        lineSpot += (1 + strlen(currWord));
                    } else {                                            //currWord does not fit at the end of the current line. start a new line
                        write(outputFD, "\n", 1);                       
                        write(outputFD, currWord, strlen(currWord));
                        lineSpot = strlen(currWord);
                    }
                    memset(currWord, '\0', currWordSize);           //clear currWord buffer and start a new word
                }

            } else {                                                //if the current character is non-whitespace, then we can add it to the write buffer
                seenTerminator = 0;
                if(strlen(currWord) == (currWordSize - 1)){         //if adding a character will exceed string buffer (-1 for the final \0 character), 
                    currWord = (char *)realloc(currWord, (currWordSize*2));   
                    currWordSize *= 2;      //currWordSize will serve as an indicator for word > lineLength error
                    memset((currWord + currWordSize/2), '\0', (currWordSize/2));   
                }
                currWord[strlen(currWord)] = (char)buffer[i];
            }
        }
        // Resetting the buffer before reassigning to it
        memset(buffer, '\0', sizeof(buffer));            
        bytesRead = read(inputFD, buffer, BUFFER_SIZE);
    }

    //Need to print last word after program is finished reading input
    if(currWord[0] != '\0'){
        if(newPG){   //start a new paragraph before the next word is written if needed
            write(outputFD, "\n\n", 2);
            lineSpot = 0;
        }
        if(lineSpot == 0){                                        //if we're at the beginning of the line, just write the word
            write(outputFD, currWord, strlen(currWord));
        } else if(lineSpot + 1 + strlen(currWord) <= lineLength){ 
            write(outputFD, " ", 1);
            write(outputFD, currWord, strlen(currWord));          //write currWord preceded by a space
        } else {                                                  //if the word plus a space didn't fit, we need to put it on a new line
            write(outputFD, "\n", 1);
            write(outputFD, currWord, strlen(currWord));
        }
    }

    //every file must end in a newline
    write(outputFD, "\n", 1);

    if(currWordSize > (sizeof(char)*(lineLength + 1))){       //we had a word larger than a line; need to error out
        puts("ERROR: Input contains a word longer than specified line length.");
        free(currWord);
        return EXIT_FAILURE;
    }

    free(currWord);
    return EXIT_SUCCESS;
}

void init_queue(struct queue *queue){
    queue->start = NULL;
    queue->end = NULL;
    queue->closed = 0;
    pthread_mutex_init(&queue->lock, NULL);
    pthread_cond_init(&queue->dequeue_ready, NULL);
}

void enqueue(struct queue *queue, struct pathName *file){
    pthread_mutex_lock(&queue->lock);

        struct node *new = (struct node *)malloc(sizeof(struct node));

        // checking to see if malloc returned correctly
        if(new == NULL){
            puts("Failed to malloc new node for enqueue");
            pthread_mutex_unlock(&queue->lock);
            return;
        }
        // allocating new node accordingly
        new->next = NULL;
        new->path = file;
        
        // if the end exists, attach the new node to the next pointer of end
        if(queue->end != NULL){
            queue->end->next = new;
        }
        queue->end = new; // change the end to point to the new node

        if(queue->start == NULL){ // make sure that head always points to something if data exists
            queue->start = new;
        }
        
        pthread_cond_signal(&queue->dequeue_ready);
        //printf("Thread %d successfully enqueued file %s\n", pthread_self(), getInputName(file));

    pthread_mutex_unlock(&queue->lock);
    return;
}

struct pathName *dequeue(struct queue *queue){
    pthread_mutex_lock(&queue->lock);

        while(queue->start == NULL){
            if(dirQueue->closed){
                pthread_mutex_unlock(&queue->lock);
                return NULL;
            }
            pthread_cond_wait(&queue->dequeue_ready, &queue->lock);
        }

        struct node *temp = queue->start;

        if(queue->start == queue->end)
            queue->end = temp->next;
        queue->start = temp->next;

        struct pathName *dequeuedFile = temp->path;
        
        free(temp);

        //printf("Thread %d successfully dequeued file %s\n", pthread_self(), getInputName(dequeuedFile));

    pthread_mutex_unlock(&queue->lock);

    return dequeuedFile; //return pathname of temp
}

void *fileWorker(void * arg){
    //printf("FileThread %d starting. Clock: %d\n", pthread_self(), clock());
    int lineLength = (int)arg;
    struct pathName *dequeuedFile;
    while(dequeuedFile = dequeue(fileQueue)){

        char *inFile = getInputName(dequeuedFile);
        char *outFile = getOutputName(dequeuedFile);
        free(dequeuedFile->fileName);
        free(dequeuedFile->prefix);
        free(dequeuedFile);

        int inFD = open(inFile, O_RDONLY);
        free(inFile);

        if(inFD <= 0){ // checking to see if file is opened successfully
            puts("ERROR: Could not open a file for reading.");
            pthread_mutex_lock(&exitFlag->lock);
            exitFlag->status = EXIT_FAILURE;
            pthread_mutex_unlock(&exitFlag->lock);
            free(outFile);
            continue;
        }

        int outFD = open(outFile, O_WRONLY|O_CREAT|O_TRUNC, 0700);
        free(outFile);

        if(outFD <= 0){ // checking to see if file is opened successfully
            puts("ERROR: Could not open a file for writing to.");
            pthread_mutex_lock(&exitFlag->lock);
            exitFlag->status = EXIT_FAILURE;
            pthread_mutex_unlock(&exitFlag->lock);
            continue;
        }

        // reformatting file
        if(normalize(inFD, lineLength, outFD) == EXIT_FAILURE){
            pthread_mutex_lock(&exitFlag->lock);
            exitFlag->status = EXIT_FAILURE;
            pthread_mutex_unlock(&exitFlag->lock);
        }

        close(inFD);
        close(outFD);
    }
    //printf("FileThread %d exiting. Clock: %d\n", pthread_self(), clock());
    return NULL;
}

void *dirWorker(void * arg){
    //printf("DirThread %d starting. Clock: %d\n", pthread_self(), clock());
    struct pathName *dequeuedFile;
    while(dequeuedFile = dequeue(dirQueue)){
        pthread_mutex_lock(&activeDThreads->lock);            
        activeDThreads->status++;
        pthread_mutex_unlock(&activeDThreads->lock);

        struct dirent *dp;
        struct stat statbuf; // Holds file information to determine its type

        char *currDir = getInputName(dequeuedFile);
        free(dequeuedFile->fileName);
        free(dequeuedFile->prefix);
        free(dequeuedFile);

        DIR *dr = opendir(currDir); 
        if(dr == NULL){
            puts("ERROR: Could not open a directory.");
            pthread_mutex_lock(&exitFlag->lock);
            exitFlag->status = EXIT_FAILURE;
            pthread_mutex_unlock(&exitFlag->lock);
            free(currDir);
            pthread_mutex_lock(&activeDThreads->lock);            
            activeDThreads->status--;
            pthread_mutex_unlock(&activeDThreads->lock);
            continue;
        }

        // iterating through current directory    
        struct pathName *newPath;
        while((dp = readdir(dr)) != NULL){
            if(strstr(dp->d_name, ".") == dp->d_name || strstr(dp->d_name, "wrap.") == dp->d_name){
                continue;
            }
            newPath = (struct pathName *)malloc(sizeof(struct pathName));

            // checking to see if a file or directory was read
            newPath->prefix = (char *)malloc(sizeof(char)*(strlen(currDir) + 1));
            newPath->fileName = (char *)malloc(sizeof(char)*(strlen(dp->d_name) + 1));
            memcpy(newPath->fileName, dp->d_name, strlen(dp->d_name) + 1);
            memcpy(newPath->prefix, currDir, strlen(currDir) + 1);
            char *newFileName = getInputName(newPath);

            stat(newFileName, &statbuf);
        
            if(S_ISDIR(statbuf.st_mode)){
                enqueue(dirQueue, newPath);
            } else {
                enqueue(fileQueue, newPath);
            }
            free(newFileName);
        }
        closedir(dr);
        free(currDir);

        pthread_mutex_lock(&activeDThreads->lock);     //NOTE: locking two mutexes here is required, but this is the only place activeDthreads is locked at the same time as dirQueue, so it is safe
        pthread_mutex_lock(&dirQueue->lock);         
        activeDThreads->status--;
        if(!activeDThreads->status && dirQueue->start == NULL){
            pthread_mutex_unlock(&dirQueue->lock);
            pthread_mutex_unlock(&activeDThreads->lock);
            queue_close(dirQueue);
        } else {
            pthread_mutex_unlock(&dirQueue->lock);
            pthread_mutex_unlock(&activeDThreads->lock);
        }
   }
   //printf("DirThread %d exiting. Clock: %d\n", pthread_self(), clock());
   return NULL;
}

int main(int argc, char **argv)
{
    int recursiveMode;    //condition variable to keep track of mode to run the program in
    
    if(strstr(argv[1], "-r") == argv[1]){
        recursiveMode = 1;
    } else {
        recursiveMode = 0;
    }

    if(recursiveMode && argc < 4){
        puts("FATAL ERROR: Recursive WordWrap requires a line length argument and at least one filename target.");
        return EXIT_FAILURE;
    } else if(!recursiveMode && argc < 2){
        puts("FATAL ERROR: Non-recursive WordWrap requires at least a line length argument.");
        return EXIT_FAILURE;
    }

    //checks to make sure lineLength argument is in the right place and a number
    int i, argLeng;
    argLeng = strlen(argv[(recursiveMode) ? 2 : 1]);
    for (i = 0; i < argLeng; i++){
        if (!isdigit(argv[(recursiveMode) ? 2 : 1][i]))
        {
            printf("FATAL ERROR: Line length parameter | %s | is not a positive integer.\n", argv[(recursiveMode) ? 2 : 1]);
            return EXIT_FAILURE;
        }
    }

    int lineLength = atoi(argv[(recursiveMode) ? 2 : 1]);
    if(lineLength <= 0){
        printf("FATAL ERROR: Line length parameter | %s | is not a positive integer.\n", argv[(recursiveMode) ? 2 : 1]);
        return EXIT_FAILURE;
    }

    if(!recursiveMode && argc == 2)      //STDIN case. Must be non-recursive.
        return normalize(0, lineLength, 1);

    exitFlag = (struct flag *)malloc(sizeof(struct flag));
    exitFlag->status = EXIT_SUCCESS;
    pthread_mutex_init(&exitFlag->lock, NULL);
    
    dirQueue = (struct queue *)malloc(sizeof(struct queue));
    fileQueue = (struct queue *)malloc(sizeof(struct queue));          
    init_queue(dirQueue);
    init_queue(fileQueue);

    struct pathName *newPath;
    struct stat statbuf;

    /*
        ENQUEUEING ALL GIVEN FILE ARGS
    */
    for(i = (recursiveMode) ? 3 : 2; i < (argc); i++){ //skip first 3 args if recursive, first 2 args if non-recursive
        if(strstr(argv[i], ".") == argv[i] || strstr(argv[i], "wrap.") == argv[i]){
            puts("WARNING: A file argument given has a restricted prefix \".\" or \"wrap.\" ; it will be ignored.");
            continue;
        }
        newPath = (struct pathName *)malloc(sizeof(struct pathName));

        char *argName = strrchr(argv[i], '/');

        if(argName == NULL){                                     //file has no prefix
            newPath->prefix = (char *)malloc(1);
            newPath->fileName = (char *)malloc(strlen(argv[i]) + 1);
            memset(newPath->prefix, '\0', 1);
            memcpy(newPath->fileName, argv[i], strlen(argv[i]) + 1);
        } else {                                                 //file given has a prefix, and we must process it
            int argNameLeng = strlen(argName + 1);     //+1 because strrchr returns a pointer to the '/'
            int argLeng = strlen(argv[i]);
            newPath->prefix = (char *)malloc(argLeng - argNameLeng + 1);
            newPath->fileName = (char *)malloc(argNameLeng + 1);
            memcpy(newPath->prefix, argv[i], argLeng - argNameLeng);
            memset(newPath->prefix + (argLeng - argNameLeng), '\0', 1);
            memcpy(newPath->fileName, argv[i] + (argLeng - argNameLeng), argNameLeng);
            memset(newPath->fileName + argNameLeng, '\0', 1);
        }
        
        char *newFileName = getInputName(newPath);
        stat(newFileName, &statbuf);
    
        if(S_ISDIR(statbuf.st_mode)){
            enqueue(dirQueue, newPath);
        } else {
            enqueue(fileQueue, newPath);
        }
        free(newFileName);
    }

    /*
        if no dirs were given as args, we can preemptively close dirQueue
        if not in recursive mode, we expect nothing else to be enqueued so dirQueue can also be closed
        
        if in recursive mode, it will simply clear the fileQueue and no dirQueue work is done
    */
    if(dirQueue->start == NULL || !recursiveMode)
        queue_close(dirQueue);                     


    if(recursiveMode){                          //---RECURSIVE CASE---
        int numOfWrappingThreads;
        int numOfDirectoryThreads;

        activeDThreads = (struct flag *)malloc(sizeof(struct flag));      //recursive mode requires keeping track of active directory threads, we need to init
        activeDThreads->status = 0;
        pthread_mutex_init(&activeDThreads->lock, NULL);

        if(!strcmp(argv[1], "-r")){    //recursive argument is JUST "-r"  --> 1 wrapper, 1 dir thread
            numOfWrappingThreads = 1;
            numOfDirectoryThreads = 1;
        } else { 
            //at this point, thread argument must have started with -r but is not just "-r"
            
            int numsLen = strlen((char *)&argv[1][2]);
            char *argStr = (char *)malloc(sizeof(char)*(numsLen + 1));
            memcpy(argStr, &argv[1][2], numsLen);
            memset(argStr + numsLen, '\0', 1);

            if(!strstr(argStr, ",")){ //if it does not contain a comma, must have only one number

                //-rN     ;   N = wrappers
                numOfDirectoryThreads = 1;
                numOfWrappingThreads = atoi(argStr);
                printf("%s %d\n", argStr, atoi(argStr));
                
                free(argStr);
                if(numOfWrappingThreads <= 0){
                    puts("FATAL ERROR: Thread mode argument requires at least one non-negative, non-zero integer.");
                    return EXIT_FAILURE;
                }
            } else {   //if it does contain a comma, must have exactly two numbers

                char *token = strtok(argStr, ",");  
                char *temp = token;
                token = strtok(NULL, ","); //strtok() returns null if cannot be split
                
                if(token == NULL){ 
                    puts("FATAL ERROR: Thread mode argument must have one integer or two integers separated by a comma.");
                    return EXIT_FAILURE;
                }

                //-rM,N    ;   M = directories  N = wrappers
                numOfDirectoryThreads = atoi(temp); //atoi() returns 0 if argument is not a number

                numOfWrappingThreads = atoi(token);

                free(argStr);
                if(numOfDirectoryThreads <= 0 || numOfWrappingThreads <= 0){      //if num <= 0, then the thread arg number is too small or it was not a number
                    puts("FATAL ERROR: Thread mode argument requires non-negative, non-zero integers.");
                    return EXIT_FAILURE;
                }
            }
        }

        pthread_t wrapperThreads[numOfWrappingThreads];
        pthread_t dirThreads[numOfDirectoryThreads];

        for(int i = 0; i<numOfDirectoryThreads; i++){
            pthread_create(&dirThreads[i], NULL, dirWorker, NULL);
        }
        for(int i = 0; i<numOfWrappingThreads; i++){
            pthread_create(&wrapperThreads[i], NULL, fileWorker, lineLength);
        }
        for(int i = 0; i<numOfDirectoryThreads; i++){
            pthread_join(dirThreads[i], NULL);
        }
        for(int i = 0; i<numOfWrappingThreads; i++){
            pthread_join(wrapperThreads[i], NULL);
        }
        
        pthread_cond_destroy(&fileQueue->dequeue_ready);
        pthread_cond_destroy(&dirQueue->dequeue_ready);
        pthread_mutex_destroy(&fileQueue->lock);
        pthread_mutex_destroy(&dirQueue->lock);
        pthread_mutex_destroy(&activeDThreads->lock);
        free(dirQueue);
        free(fileQueue);
        free(activeDThreads);

        int exitStatus = exitFlag->status;

        free(exitFlag);

        return exitStatus;
    } else {                                                //---NONRECURSIVE CASE---          
        struct pathName *dequeuedFile;
        
        while(dequeuedFile = dequeue(fileQueue)){             //dequeue all files given as arguments first, to output to STDOUT
            char *inFile = getInputName(dequeuedFile);
            free(dequeuedFile->fileName);
            free(dequeuedFile->prefix);
            free(dequeuedFile);

            int inFD = open(inFile, O_RDONLY);

            free(inFile);

            if(inFD <= 0){ // checking to see if file is opened successfully
                exitFlag->status = EXIT_FAILURE;
                continue;            
            }

            // reformatting file
            normalize(inFD, lineLength, STDOUT_FILENO); //FIXME: make global exit status with mutex to keep track of normalize() exit status
            close(inFD);
        }

        while(dequeuedFile = dequeue(dirQueue)){
            struct dirent *dp;
            struct stat statbuf; // Holds file information to determine its type

            char *currDir = getInputName(dequeuedFile);
            free(dequeuedFile->fileName);
            free(dequeuedFile->prefix);
            free(dequeuedFile);

            DIR *dr = opendir(currDir); //FIXME: check if dir opened successfully and update exitFlag
            if(dr == NULL){
                puts("ERROR: Could not open a directory.");
                exitFlag->status = EXIT_FAILURE;
                continue; 
            }

            // iterating through current directory    
            struct pathName *newPath;
            while((dp = readdir(dr)) != NULL){
                if(strstr(dp->d_name, ".") == dp->d_name || strstr(dp->d_name, "wrap.") == dp->d_name){
                    continue;
                }
                newPath = (struct pathName *)malloc(sizeof(struct pathName));

                // checking to see if a file or directory was read
                newPath->prefix = (char *)malloc(sizeof(char)*(strlen(currDir) + 1));
                newPath->fileName = (char *)malloc(sizeof(char)*(strlen(dp->d_name) + 1));
                memcpy(newPath->fileName, dp->d_name, strlen(dp->d_name) + 1);
                memcpy(newPath->prefix, currDir, strlen(currDir) + 1);
                char *newFileName = getInputName(newPath);

                stat(newFileName, &statbuf);
            
                if(!S_ISDIR(statbuf.st_mode)){     //NON-RECURSIVE --> ONLY ENQUEUE FILES, NOT DIRS
                    enqueue(fileQueue, newPath);
                } else {
                    free(newPath->prefix);
                    free(newPath->fileName);
                    free(newPath);
                }
                free(newFileName);
            }
            closedir(dr);
            free(currDir);
        }

        while(dequeuedFile = dequeue(fileQueue)){              //dequeue all files that were within directories
            char *inFile = getInputName(dequeuedFile);
            char *outFile = getOutputName(dequeuedFile);
            free(dequeuedFile->fileName);
            free(dequeuedFile->prefix);
            free(dequeuedFile);

            int inFD = open(inFile, O_RDONLY);
            int outFD = open(outFile, O_WRONLY|O_CREAT|O_TRUNC, 0700);

            free(inFile);
            free(outFile);

            if(inFD <= 0 || outFD <= 0){ // checking to see if file is opened successfully
                puts("ERROR: Could not open a file.");
                exitFlag->status = EXIT_FAILURE;
                continue;            
            }

            // reformatting file
            normalize(inFD, lineLength, outFD); //FIXME: make global exit status with mutex to keep track of normalize() exit status
            close(inFD);
            close(outFD);
        }

        pthread_cond_destroy(&fileQueue->dequeue_ready);
        pthread_cond_destroy(&dirQueue->dequeue_ready);
        pthread_mutex_destroy(&fileQueue->lock);
        pthread_mutex_destroy(&dirQueue->lock);
        pthread_mutex_destroy(&exitFlag->lock);
        free(dirQueue);
        free(fileQueue);

        int exitStatus = exitFlag->status;

        free(exitFlag);

        return exitStatus; 
    }
}