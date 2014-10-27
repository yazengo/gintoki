#!/bin/sh

while true; do
	./server-mips $*
	pkill avahi-daemon
	sleep 1
done

