pids=""
if [ -e server.pid ]
then
	pids=$(cat server.pid)
fi

for i in $pids;
do
	echo kill $i
	kill $i
done

nohup nodejs server.js >>node.log &

echo $!>server.pid
