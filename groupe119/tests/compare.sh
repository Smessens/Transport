#!/bin/bash

if diff received_file $2 >/dev/null ; then
	echo "Test $1 - Succes"
else
	echo "Test $1 - Failed"
fi
