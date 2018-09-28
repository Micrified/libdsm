#!/bin/bash

if ! [ -x "$(command -v dsm_daemon)" ]; then
	echo Cannot locate program dsm_daemon!
	exit 1
fi

echo Building tests...
make > test.log
echo Running tests...
dsm_daemon >> test.log &
./dsm_test_daemon 127.0.0.1 4200
./dsm_test_server 127.0.0.1 4200
./dsm_test_ptab
./dsm_test_stab
./dsm_test_holes
./dsm_test_signals
echo Done.
make clean >> test.log
kill $(pgrep -f dsm_daemon)
