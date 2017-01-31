import 
  net, 
  threadpool, 
  times,
  pudgeclient,
  random,
  os,
  unittest,
  strutils

{.experimental.}

const NL = chr(13) & chr(10)

proc newTaskAsync(task:int) = 
  var socket = newClient("127.0.0.1",5555)
  let start = task * 100000 + 1
  let endd = start + 99999
  echo $start," :",$endd
  randomize()
  for i in start..endd:
    let rnd = random(1_000_000)+1
    let res = socket.get($rnd)
    #echo $i
  socket.quit()


proc mainAsync()=
  echo "start"
  var t = toSeconds(getTime())
  t = toSeconds(getTime())
  parallel:
    for i in 0..9:
      spawn newTaskAsync(i)
  
  echo "Read time [s] ", $(toSeconds(getTime()) - t)  
  echo "sleep 10 sec"
  echo "end"
    


suite "test suite for pudge":
  setup:
    let result = 4
    var size = 1_000_000
    const bytes = 8
    const content = repeatStr(bytes-7, "x")
  
  test "2 + 2 = 4":
    check(2+2 == result)
  
  test "insert:":
    check(2+2 == result)
  #[]
    var
      client = newClient("127.0.0.1",5555)
      key:string = nil
      val:string = nil
      res:bool
      t = toSeconds(getTime())

    echo "start: ",$(t)
    let len = intToStr(size).len
    for i in 1..size:

      key = $i
      val = newStringOfCap(bytes)
      val.add(intToStr(i,len))
      val.add(content)
      res = client.set(key, val)
      #echo res
      if res == false:
        break
    check(res == true)
    echo "Insert time [s] ", $(toSeconds(getTime()) - t)
    client.quit()]#

  test "read":
    var
      client = newClient("127.0.0.1",5555)
      key:string = nil
      val:string = nil
      t = toSeconds(getTime())
    echo "size:",$size
    echo "start: ",$(t)
    for i in 1..size:

      key = $i
      val =  client.get(key)
      if (i == size):
        echo "val:", val
    #check(val ==  "1000000x")
    echo "Read time [s] ", $(toSeconds(getTime()) - t)
    client.quit()

  test "random read":
    mainAsync()