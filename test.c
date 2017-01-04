#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "commands.h"

//gcc -o test test.c commands.c && ./test
int process(char *input) {
    char *command;
    char *string = strdup(input);
    command = strtok(string, " ");
    if (!command) {
        sprintf(input, "Error: Empty command\n");
        return -1;
    }
    printf("cmd:%s\n",command);
    if (strcmp(command, "set") == 0) {
        printf("set\n");
        while( (string = strtok(NULL," ")) ) {
            printf("string:%s\n",string);
        }
    }
    free(string);
    return 0;
}

int main(void) {
    //char* str = "set key 0 0 3\r\n123\r\n";
    char* str = "get key\r\n";
    char *res = handle_read(str,strlen(str));
    printf("res:%s\n",res);
    //process(str);
    return 0;
}