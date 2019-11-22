#!/bin/bash

# clean
rm -f result0.txt

echo "============= TEST glob : link_sim ============="

# receiver : our implementation
./receiver -o result%d.txt :: 12350 &> recv.log & receiver_pid=$! 

# link_sim
./linksimulator/link_sim -p 1341 -P 12350 -d 100 -j 10 -c 10 -e 10 -l 10 &> link.log & link_pid=$! 

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

kill $receiver_pid
kill $link_pid

# verifying that files are the same
if [[ "$(md5sum files/test1.txt | awk '{print $1}')" == "$(md5sum result0.txt | awk '{print $1}')" ]]; then
  echo "Test global OK"
fi

rm -f result0.txt