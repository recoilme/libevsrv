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
                sp_setstring(o, "key", &key, strlen(key));
                sp_setstring(o, "value", &value, strlen(value));
                int res = sp_set(db, o);
                free(value);
                if (res == 0) {
                    return "STORED\r\n";
                }       
            }
        }
    }
    return "NOT_STORED\r\n";
    /*
    token = strtok(cmdline, " ");
    if (!token) {
        free(cmdline);
        return "NOT_STORED\r\n";
    }
    int j = 0;
    size = 0;
    while( (token = strtok(NULL," ")) ) {
        j++;
        if (j == 4) {
            size = strtol(token,NULL,10);
            copy2 = token;
            //printf("SIZE:%ld\n",size);
            break;
        }
    }
    if (endline > 0) {
        free(cmdline);
    }

    
    if (size > 0) {
        //no need cmdline more
        data+=endline+2;
        //if (size>10) printf("value_size:%s\n",value);
        
        endline = strcspn(data, "\r\n");
        
        if (endline > 0) {
            value = strndup_p(data,endline);
            if (size != endline) {
                printf("endline:%zu size:%zu data:'%s' copu2:'%s' \n",endline,size,copy,copy2);
                free(copy);
            }
            //printf("v-a-l-u-e:%s\n",value);
            free(value);
        }
    }
    else {
        return "NOT_STORED\r\n";
    }
    
    return "STORED\r\n";
    */
}

static char *get_func(struct command *command, const char *data, size_t len) {
    char *key;
    char *val;
    char *ptr;
    char resp[256];
    int size;
    data+=4;//'get '
    size_t cmdend = strcspn(data, " \r\n");
    if (cmdend > 0) {
        key = strndup_p(data,cmdend);

        /* get */
        void *o = sp_document(db);
        sp_setstring(o, "key", &key, strlen(key));
        o = sp_get(db, o);
        if (o) {
            /* ensure key and value are correct */
            //int size;
            //char *ptr = sp_getstring(o, "key", &size);
            //assert(size == sizeof(uint32_t));
            //assert(*(uint32_t*)ptr == key);

            ptr = sp_getstring(o, "value", &size);
            //assert(size == sizeof(uint32_t));
            //assert(*(uint32_t*)ptr == key);
            printf("ptr:%s\n",(char*)ptr);
            val = (char*)ptr;//strndup_p(ptr,size);
            sp_destroy(o);
            //return *(char*)ptr;
        }        
        
        snprintf(resp, sizeof(resp),"%s%s%s%d%s%s%s","VALUE ",key," 0 ",size,"\r\n",val,"\r\nEND\r\n");
        //printf("key:%s%lu",resp,sizeof(resp));
        free(key);
        return strndup_p(resp,sizeof(resp));
    }
    return "ERROR\r\n";
}

static struct command commands[] = {
	{ "set", "set key 0 0 5\r\nvalue\r\n", set_func },
    { "get", "get key\r\n", get_func }
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