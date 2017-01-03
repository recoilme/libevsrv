# libevsrv
```
gcc -o echoserver_threaded echoserver_threaded.c workqueue.c -levent -lpthread -levent_extra -L/usr/local/Cellar/libevent/HEAD-3821cca/lib -I/usr/local/Cellar/libevent/HEAD-3821cca/include && ./echoserver_threaded

gcc -o echoserver_threaded echoserver_threaded.c workqueue.c commands.c -levent -lpthread -L/usr/local/Cellar/libevent/HEAD-3821cca/lib -I/usr/local/Cellar/libevent/HEAD-3821cca/include

cc test.c commands.c -o test && ./test
```