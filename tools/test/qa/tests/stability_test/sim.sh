aplay -Dhw:0,1 -f dat -c 2 /dev/zero &
# Get its PID
PID_1=$!

arecord -Dhw:0,1 -f dat -c 2 /dev/null &
# Get its PID
PID_2=$!

aplay -Dhw:0,0 -f dat -c 2 /dev/zero &
# Get its PID
PID_3=$!

# Wait for 5 seconds
sleep 5
# Kill them
kill $PID_1
RET_1=$?
kill $PID_2
RET_2=$?
kill $PID_3
RET_3=$?

if [ $RET_1 -ne 0 ] || [ $RET_2 -ne 0 ] || [ $RET_3 -ne 0 ]
#if [ $RET_1 -ne 0 ] || [ $RET_2 -ne 0 ]
then
	echo "stopping failed!"
	exit -1
else
	exit 0
fi
