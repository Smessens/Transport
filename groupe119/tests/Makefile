#default file to transfer
FILE=text.txt

all: normalCase delayCase jitterCase lossCase errorCase cutCase bigCase cleanall


normalCase:
	./receiver -o received_file :: 64341 &
	./link_sim -p 64342 -P 64341 &
	../sender -f $(FILE) ::1 64342
	./compare.sh "Normal case" $(FILE)

jitterCase:
	./receiver -o received_file :: 64341 &
	./link_sim -p 64342 -P 64341 -d 200 -j 100 &
	../sender -f $(FILE) ::1 64342
	./compare.sh "Jitter case" $(FILE)

lossCase:
	./receiver -o received_file :: 64341 &
	./link_sim -p 64342 -P 64341 -l 50 &
	../sender -f $(FILE) ::1 64342
	./compare.sh "Loss case" $(FILE)

errorCase:
	./receiver -o received_file :: 64341 &
	./link_sim -p 64342 -P 64341 -e 50 &
	../sender -f $(FILE) ::1 64342
	./compare.sh "Error case" $(FILE)

delayCase:
	./receiver -o received_file :: 64341 &
	./link_sim -p 64342 -P 64341 -d 500 &
	../sender -f $(FILE) ::1 64342
	./compare.sh "Delay case" $(FILE)

cutCase:
	./receiver -o received_file :: 64341 &
	./link_sim -p 64342 -P 64341 -c 50 &
	../sender -f $(FILE) ::1 64342
	./compare.sh "Cut case" $(FILE)

bigCase:
	./receiver -o received_file :: 64341 &
	./link_sim -p 64342 -P 64341 -d 500 -j 100 -l 5 -e 5 -c 5 &
	../sender -f $(FILE) ::1 64342
	./compare.sh "Big case" $(FILE)

cleanall:
	rm -f received_file
