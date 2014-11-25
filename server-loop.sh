#!/bin/sh

while true; do
	./server-mips -run main.lua
	pkill avahi-daemon
	sleep 1
done

