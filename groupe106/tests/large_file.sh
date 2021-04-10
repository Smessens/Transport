#!/bin/bash

# cleanup d'un test précédent
rm -f received_file input_file

# Fichier au contenu aléatoire de 512 octets
dd if=/dev/urandom of=input_file bs=1 count=30523 &> /dev/null

valgrind_sender=""
valgrind_receiver=""
valgrind_link_sim=""
if [ ! -z "$VALGRIND" ] ; then
    valgrind_sender="valgrind --leak-check=full --log-file=valgrind_sender_large_file.log"
    valgrind_receiver="valgrind --leak-check=full --log-file=valgrind_receiver_large_file.log"
    valgrind_link_sim="valgrind --leak-check=full --log-file=valgrind_link_large_file.log"
fi

# On lance le receiver et capture sa sortie standard
$valgrind ./receiver :: 64341 -s receiver.csv 2> logFile/receiver_largefile.log > received_file &
receiver_pid=$!

cleanup()
{
    kill -9 $receiver_pid
    kill -9 $link_pid
    exit 0
}
trap cleanup SIGINT  # Kill les process en arrière plan en cas de ^-C

$valgrind ./tests/link_sim -p 64342 -P 64341 -R -d 200 -j 50 2>logFile/link_sim_largefile.log &
link_sim_pid=$!
# On démarre le transfert
if ! $valgrind ./sender ::1 64342 -f text.txt -s sender.csv 2> logFile/sender_largefile.log ; then
  echo "Crash du sender!"
  cat sender_largefile.log
  err=1  # On enregistre l'erreur
fi

sleep 5 # On attend 5 seconde que le receiver finisse

if kill -0 $receiver_pid &> /dev/null ; then
  echo "Le receiver ne s'est pas arreté à la fin du transfert!"
  kill -9 $receiver_pid
  err=1
else  # On teste la valeur de retour du receiver
  if ! wait $receiver_pid ; then
    echo "Crash du receiver!"
    cat receiver_largefile.log
    err=1
  fi
fi

# On vérifie que le transfert s'est bien déroulé
if [[ "$(md5sum text.txt | awk '{print $1}')" != "$(md5sum received_file | awk '{print $1}')" ]]; then
  echo "Le transfert a corrompu le fichier!"
  echo "Diff binaire des deux fichiers: (attendu vs produit)"
  diff -C 9 <(od -Ax -t x1z input_file) <(od -Ax -t x1z received_file)
  exit 1
else
  echo "Le transfert est réussi!"
  exit ${err:-0}  # En cas d'erreurs avant, on renvoie le code d'erreur
fi