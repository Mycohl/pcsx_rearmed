#!/bin/sh
if ndk-build -j4; 
	then
		ant $1;
	else
		 exit $?;
fi
