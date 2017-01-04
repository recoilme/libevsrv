# libevsrv
```

cc test.c commands.c -o test && ./test
gcc -o pudge pudge.c sophia.c server.c workqueue.c commands.c -levent -lpthread -L/usr/local/Cellar/libevent/HEAD-3821cca/lib -I/usr/local/Cellar/libevent/HEAD-3821cca/include && ./pudge
```