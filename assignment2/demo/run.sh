#!/bin/bash

cd 2020CS10869_proj
mkdir bin obj output
make
./bin/server_udp sth&
pid=$!
./bin/client_udp -o output.txt
kill -15 $pid