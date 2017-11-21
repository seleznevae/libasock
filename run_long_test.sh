#!/bin/bash

red=`tput setaf 1`
green=`tput setaf 2`
reset=`tput sgr0`

cd bin

for i in `seq 1000`;
do
    ./test_asock
    if [ $? != 0 ]; then
        echo "Test ${red}FAILED${reset} on $i-th iteration"
        exit 1
    fi
done

echo "Test passed ${green}SUSSECFULLY${reset}"

exit 0
