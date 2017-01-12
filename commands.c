#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "shared.h"
#include "sophia.h"

struct command {
	char *name;
	char *desc;
	int (*func)(struct command *command, char** data, int* len);
};


static int set_func(struct command *command, char** data, int* len) {

    char *key;
    char *value;
    char *result;
    int res = -1;
    int shift = 0;
    size_t endline;
    result =  malloc(strlen(ST_NOTSTORED));
    memcpy(result,ST_NOTSTORED,strlen(ST_NOTSTORED));
    shift+=4;
    //INFO_OUT("pointer:%p\n",(void*)*data);
    *data+=4;//'set '
    *len-=4;
    endline = strcspn(*data, " \r\n");
    if (endline < *len) {
        key = strndup_p(*data,endline);   
        endline = strcspn(*data, "\r\n");
        if (endline < *len) {
            shift+=endline+2;
            *data+=endline+2;
            *len-=endline+2;
            endline = strcspn(*data, "\r\n");
            if (endline < *len) {
                value = strndup_p(*data,endline);
                void *o = sp_document(db);
                sp_setstring(o, "key", &key[0], strlen(key));
                sp_setstring(o, "value", &value[0], strlen(value));
                res = sp_set(db, o);
                INFO_OUT("key:%s value:%s res:%d\n",key,value,res);
                free(value);
                free(key);
                if (res == 0) {
                    result = realloc(result,strlen(ST_STORED));
                    memcpy(result,ST_STORED,strlen(ST_STORED));
                }
            }
        }
    }
    INFO_OUT("data:'%.*s'\n", *len,*data);
    //move pointer on original address
    *data-=shift;
    //size (success or not)
    if (res == 0) *len = strlen(ST_STORED);
    else *len = strlen(ST_NOTSTORED);
    //realloc
    *data = realloc(*data,*len);
    if (!data) {
        free(result);
        //error in realloc
        return -1;
    }
    memcpy(*data,result,*len);
    free(result);
    //success processed
    return 0;
}



static int get_func(struct command* command, char** data, int* len) {
    char *key = NULL;
    char *val = NULL;
    char *ptr;
    char *resp = NULL;
    int size;
    int shift = 0;
    shift+=4;
    *data+=4;//'get '
    size_t cmdend = strcspn(*data, " \r\n");
    INFO_OUT("cmdend:%d\n",(int)cmdend);
    if (cmdend <= *len) {
        key = strndup_p(*data,cmdend);
        INFO_OUT("key:'%.*s'\n", (int)strlen(key),key);
        *data-=shift;
        // get 
        
        void *o = sp_document(db);
        sp_setstring(o, "key", &key[0], strlen(key));
        o = sp_get(db, o);
        if (o) {
            ptr = sp_getstring(o, "value", &size);
            val = strndup_p((char*)ptr,size);
            if (!val) {
                goto error;
            }
            sp_destroy(o);
        }
        else {
            val = "";
            //TODO return empty string for val?
        }        
        INFO_OUT("val:'%.*s'\n", size,val);
        
        char *format = "VALUE %s 0 %d\r\n%s\r\nEND\r\n";
        int resp_size = (strlen(format) -3*2/* % exclude */) + strlen(key) + size + get_int_len(size) + 1/* \0*/;
        resp = malloc(resp_size);
        snprintf(resp, resp_size,format,key,size,val);
        INFO_OUT("resp:'%.*s' %d\n", resp_size,resp,resp_size);
        //realloc
        *len = resp_size-1;//\0 remove
        *data = realloc(*data,*len);
        if (!data) {
            //error in realloc
            goto error;
        }
        memcpy(*data,resp,*len);
        if (strcmp(val,"")) free(val);
        free(key);
        free(resp);
        //success processed
        return 0;
    }
error:;
    free(val);
    free(key);
    free(resp);
    return -1;
}

static struct command commands[] = {
	{ "set", "set key 0 0 5\r\nvalue\r\n", set_func },
    { "get", "get key\r\n", get_func }//,
    //{ "quit", "quit\r\n", quit_func }
};

//you must free data ater use
void handle_read(char** data,int* len) {
    int processed = -1;

    size_t cmdend = strcspn(*data, " \r\n");
    char *cmd = strndup_p(*data,cmdend);
    INFO_OUT("Command:%s\n", cmd);
    // Execute the command, if it is valid
    int i;
    for(i = 0; i < ARRAY_SIZE(commands); i++) {
        if(!strcmp(cmd, commands[i].name)) {
            //INFO_OUT("Running command %s\n", commands[i].name);
            processed = commands[i].func(&commands[i], data, len);
            break;
        }
    }
    free(cmd);
    if (processed != 0) {
        //no commands
        *len = strlen(ST_ERROR);
        *data = realloc(*data,*len);
        memcpy(*data,ST_ERROR,*len);
    }
}
