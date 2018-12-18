i=0
MAXLOOPS=1000
while [ $i -lt $MAXLOOPS ]; do
	echo "iterate $i ..."
	./sim.sh
	if [ $? -eq 0 ]; then
           let i+=1
	else
	   exit 1
	fi
	sleep 1
done
