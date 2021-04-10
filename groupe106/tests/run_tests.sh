# Run the same test, but this time with valgrind
mkdir logFile

START=$(date +%s.%N)
START=$(date +%s.%N)
echo "A very simple test, with Valgrind"
VALGRIND=1 ./tests/simple_test.sh
END=$(date +%s.%N)
DIFF=$(echo "$END - $START" | bc)
echo "temps du programme:" $DIFF

START=$(date +%s.%N)
echo "test Delay"
./tests/testDelay.sh
END=$(date +%s.%N)
DIFF=$(echo "$END - $START" | bc)
echo "temps du programme:" $DIFF

START=$(date +%s.%N)
echo "test loss"
./tests/testLoss.sh
END=$(date +%s.%N)
DIFF=$(echo "$END - $START" | bc)
echo "temps du programme:" $DIFF

START=$(date +%s.%N)
echo "test cut"
./tests/testCut.sh
END=$(date +%s.%N)
DIFF=$(echo "$END - $START" | bc)
echo "temps du programme:" $DIFF

START=$(date +%s.%N)
echo "test error"
./tests/testError.sh
END=$(date +%s.%N)
DIFF=$(echo "$END - $START" | bc)
echo "temps du programme:" $DIFF

START=$(date +%s.%N)
echo "test all de flag of link_sim"
./tests/testAll.sh
END=$(date +%s.%N)
DIFF=$(echo "$END - $START" | bc)
echo "temps du programme:" $DIFF

START=$(date +%s.%N)
echo "test with a large file: 237440 caractere"
./tests/large_file.sh
END=$(date +%s.%N)
DIFF=$(echo "$END - $START" | bc)
echo "temps du programme:" $DIFF

START=$(date +%s.%N)
echo "test avec les flags ajoute dans le sender et receiver '-l' -w' "
./tests/testNewFlag.sh
END=$(date +%s.%N)
DIFF=$(echo "$END - $START" | bc)
echo "temps du programme:" $DIFF
