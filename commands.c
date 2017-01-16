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

int find_last_of(char * str, int str_len, char template) {
    int pos = str_len - 1;
    while (pos >= 0) {
        if (str[pos] == template) {
            return pos;
        }
        pos--;
    }
    return -1;
}


static int set_func(struct command *command, char** data, int* len) {

    char *key;
    char *value;
    char *result;
    int res = -1;
    size_t endline;
    result =  malloc(strlen(ST_NOTSTORED));
    memcpy(result,ST_NOTSTORED,strlen(ST_NOTSTORED));
    //INFO_OUT("pointer:%p\n",(void*)*data);
    char * data_cpy = *data;
    data_cpy+=4;//'set '
    *len-=4;
    endline = strcspn(data_cpy, " \r\n");
    if (endline < *len) {
        key = strndup_p(data_cpy,endline);
        endline = strcspn(data_cpy, "\r\n");
        int last_ws_pos = find_last_of(data_cpy, endline, ' ');
        if (last_ws_pos < 0) {
            free(key);
            return -1;
        }
        char value_len_str[20];
        last_ws_pos += 1;
        memcpy(value_len_str, data_cpy + sizeof(char) * last_ws_pos, endline - last_ws_pos);
        value_len_str[endline - last_ws_pos] = '\0';
        int value_len = atoi(value_len_str);
        if (value_len <= (int)(*len - endline - 4)) {
            data_cpy+=endline+2;
            *len-=endline+2;
            endline = strcspn(data_cpy, "\r\n");
            if (endline < *len) {
                value = strndup_p(data_cpy,endline);
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
            } else {
                free(key);
                return 0;
            }
        } else {
            free(key);
            return 0;
        }
    } else {
        return 0;
    }
    INFO_OUT("data:'%.*s'\n", *len,data_cpy);
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
    return *len;
}



static int get_func(struct command* command, char** data, int* len) {
    char *key = NULL;
    char *val = NULL;
    char *ptr;
    char *resp = NULL;
    int size;
    int shift = 0;
    char * data_cpy = *data;
    data_cpy+=4;//'get '
    size_t cmdend = strcspn(data_cpy, "\r\n");
    INFO_OUT("cmdend:%d, len = %d\n",(int)cmdend, *len);
    if (cmdend < ((int)*len - 4)) {
        key = strndup_p(data_cpy,cmdend);
        INFO_OUT("key:'%.*s'\n", (int)strlen(key),key);
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
        return *len;
    } else {
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
size_t handle_read(char** data,int* len) {
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
    if (processed < 0) {
        //no commands
        *len = strlen(ST_ERROR);
        *data = realloc(*data,*len);
        memcpy(*data,ST_ERROR,*len);
    }

    return processed != 0 ? 1 : 0;
}