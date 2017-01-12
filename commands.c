#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "shared.h"
#include "sophia.h"

#define INFO_OUT(...) {\
	printf("%s:%d: %s():\t", __FILE__, __LINE__, __FUNCTION__);\
	printf(__VA_ARGS__);\
}
// Size of array (Caution: references its parameter multiple times)
#define ARRAY_SIZE(array) (sizeof((array)) / sizeof((array)[0]))

// str must have at least len bytes to copy
static char *strndup_p(const char *str, size_t len)
{
	char *newstr;

	newstr = malloc(len + 1);
	if(newstr == NULL) {
        printf("ERROR ALOCATE%zu\n",len);
		return NULL;
	}

	memcpy(newstr, str, len);
	newstr[len] = 0;

	return newstr;
}

int get_int_len (int value){
  int l=1;
  while(value>9){ l++; value/=10; }
  return l;
}

struct command {
	char *name;
	char *desc;
	int (*func)(struct command *command, char** data, int* len);
};


static int set_func(struct command *command, char** data, int* len) {
    printf("set data:'%s'\n",*data);
    char *key;
    char *value;
    char *result;
    int shift = 0;
    size_t endline;
    result =  "NOT_STORED\r\n";
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
                int res = sp_set(db, o);
                INFO_OUT("key:%s value:%s res:%d\n",key,value,res);
                free(value);
                free(key);
                if (res == 0) {
                    result = "STORED\r\n";
                }
            }
        }
    }
    INFO_OUT("data:'%.*s'\n", *len,*data);
    //move pointer on original address
    *data-=shift;
    //realloc
    *len = strlen(result);
    *data = realloc(*data,*len);
    if (!data) {
        //error in realloc
        return -1;
    }
    memcpy(*data,result,*len);
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
    //min cmd len 4
    if (*len > 3) {
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
    }
    if (processed != 0) {
        //no commands
        char *result="ERROR\r\n";
        *data = realloc(*data,strlen(result));
        memcpy(*data,result,strlen(result));
        *len = strlen(result);
    }
}
