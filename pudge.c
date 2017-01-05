#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "server.h"
#include "sophia.h"
#include "shared.h"

void *env;
void *db;

int init() {
	/* open or create environment and database */
    env = sp_env();
	sp_setstring(env, "sophia.path", "_test", 0);
	sp_setstring(env, "db", "test", 0);
	db = sp_getobject(env, "db.test");
    return sp_open(env);
}

int db_set(uint32_t key) {
    /* set */
	void *o = sp_document(db);
	sp_setstring(o, "key", &key, sizeof(key));
	sp_setstring(o, "value", &key, sizeof(key));
	return sp_set(db, o);
}

uint32_t db_get(uint32_t key) {
	/* get */
	void *o = sp_document(db);
	sp_setstring(o, "key", &key, sizeof(key));
	o = sp_get(db, o);
	if (o) {
		/* ensure key and value are correct */
		int size;
		char *ptr = sp_getstring(o, "key", &size);
		assert(size == sizeof(uint32_t));
		assert(*(uint32_t*)ptr == key);

		ptr = sp_getstring(o, "value", &size);
		assert(size == sizeof(uint32_t));
		assert(*(uint32_t*)ptr == key);

		sp_destroy(o);
        return *(uint32_t*)ptr;
	}
    return -1;
}

int db_del(uint32_t key) {
    /* delete */
	void *o = sp_document(db);
	sp_setstring(o, "key", &key, sizeof(key));
	return sp_delete(db, o);
}

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;

	if (init() == -1)
		goto error;

    if (db_set(2) == -1)
		goto error;

    uint32_t val = db_get(2);
    printf("val:%d\n",val);

	if (db_del(2) == -1)
		goto error;
    runServer();
	/* finish work */
	sp_destroy(env);
	return 0;

error:;
	int size;
	char *error = sp_getstring(env, "sophia.error", &size);
	printf("error: %s\n", error);
	free(error);
	sp_destroy(env);
	return 1;
}