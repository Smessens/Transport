#!/bin/bash

# clean
rm -f result0.txt

echo "============= TEST3 : paquets tronqués ============="

# receiver : our implementation
./receiver -o result%d.txt :: 12350 &> recv.log & receiver_pid=$!

# link_sim
./linksimulator/link_sim -p 1341 -P 12350 -c 10 -R &> link.log & link_pid=$!

sleep 1

# kill all processes
cleanup()
{
  kill -9 $receiver_pid
  kill -0 $link_pid
}
trap cleanup SIGINT

# sender - transfert
./executables/sender -f files/test1.txt ::1 1341

# the transfert is done
sleep 5

kill -9 $receiver_pid
kill -9 $link_pid

# verifying that files are the same
if [[ "$(md5sum files/test1.txt | awk '{print $1}')" == "$(md5sum result0.txt | awk '{print $1}')" ]]; then
  echo "Test 3 OK"
fi