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

struct command {
	char *name;
	char *desc;
	char* (*func)(struct command *command, const char *data, size_t len);
};

static char *set_func(struct command *command, const char *data, size_t len)
{
    char *key;
    char *value;
    size_t endline;
    data+=4;//'set '
    endline = strcspn(data, " \r\n");
    if (endline > 0) {
        key = strndup_p(data,endline);   
        endline = strcspn(data, "\r\n");
        if (endline > 0) {
            data+=endline+2;
            endline = strcspn(data, "\r\n");
            if (endline > 0) {
                value = strndup_p(data,endline);
                void *o = sp_document(db);
                sp_setstring(o, "key", &key[0], strlen(key));
                sp_setstring(o, "value", &value[0], strlen(value));
                int res = sp_set(db, o);
                //printf("key:%s value:%s res:%d\n",key,value,res);
                free(value);
                if (res == 0) {
                    return "STORED\r\n";
                }       
            }
        }
    }
    return "NOT_STORED\r\n";
}

static char *get_func(struct command *command, const char *data, size_t len) {
    char *key;
    char *val;
    char *ptr;
    char *resp;
    int size;
    data+=4;//'get '
    size_t cmdend = strcspn(data, " \r\n");
    if (cmdend > 0) {
        key = strndup_p(data,cmdend);
        /* get */
        void *o = sp_document(db);
        sp_setstring(o, "key", &key[0], strlen(key));
        o = sp_get(db, o);
        if (o) {
            ptr = sp_getstring(o, "value", &size);
            //printf("key:%s value:%s res:%d\n",key,ptr,size);
            val = (char*)ptr;//strndup_p(ptr,size);
            sp_destroy(o);
        }        
        char *format = "VALUE %s 0 %d\r\n%s\r\nEND\r\n";
        resp = malloc(strlen(format)*2 + size);
        snprintf(resp, (strlen(format)*2 + size),format,key,size,val);
        //printf("key:%s%lu",resp,sizeof(resp));
        free(key);
        return resp;//strndup_p(resp,sizeof(resp));
    }
    return "ERROR\r\n";
}

static char *quit_func(struct command *command, const char *data, size_t len) {
    return "quit";
}

static struct command commands[] = {
	{ "set", "set key 0 0 5\r\nvalue\r\n", set_func },
    { "get", "get key\r\n", get_func },
    { "quit", "quit\r\n", quit_func }
};

size_t strpos(const char *data,char *needle) {
    size_t pos = -1;
    char *p = strstr(data, needle);
    if (p) pos = p - data;
    return pos;
}

char *handle_read(const char *data,size_t len) {
    //printf("msg:%.*s\n",(int)len,data);
    char *cmd;
    char *result;
    char error[] = "ERROR\r\n";
    
    size_t cmdend = strcspn(data, " \r\n");
    if (cmdend > 0) {
        cmd = strndup_p(data,cmdend);
        //printf("cmd:%s\n",cmd);
    }
	// Execute the command, if it is valid
    int i;
	for(i = 0; i < ARRAY_SIZE(commands); i++) {
		if(!strcmp(cmd, commands[i].name)) {
			//INFO_OUT("Running command %s\n", commands[i].name);
			result = commands[i].func(&commands[i], data, len);
			break;
		}
    }
    if(i == ARRAY_SIZE(commands)) {
        result = strndup_p(error,sizeof(error));
    }
    if (cmdend) {
        free(cmd);
    }
    return result;
}