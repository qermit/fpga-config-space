#!/bin/bash

if [ "$#" != "3" ]; then
	echo "usage: $0 <driver> <firmware>"
	exit 1
fi

DEVNAME=$1
FIRMWARE_FNAME=$2

if [ ! -f /sys/class/firmware/$DEVNAME ]; then
	echo "Failed to load firmware (sysfs files missing). Check driver"
	exit
fi

if [ -f $FIRMWARE_FNAME ]; then
	echo 1 > /sys/class/firmware/$DEVNAME/loading
	cat $FIRMWARE_FNAME > /sys/class/firmware/$DEVNAME/data
	echo 0 > /sys/class/firmware/$DEVNAME/loading
	echo "Firmware loaded"
else
	echo -1 > /sys/class/firmware/$DEVNAME/loading
	echo "Failed to load firmware. File $FIRMWARE_FNAME not found"
fi
