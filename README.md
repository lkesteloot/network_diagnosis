# Network diagnosis

This is a quick C program to analyze network problems in parallel. It runs a bunch
of pings and DNS lookups and displays the results in a table. I wrote this tool
to solve the common problem where some website isn't responding and I can't tell
if the problem is with my Wi-Fi, with my router, with my ISP, with my ISP's DNS
server, etc. This program runs a bunch of tests in parallel and shows the answer
in a tabular format so that at a glance I can see what's going wrong.

The tests themselves are hard-coded in the C program. Feel free to modify them
for your own use.

# Building

    % make

# Running

In this run the network was unplugged for the first 25 seconds. A dot means that
the spawned program is still working, an "X" means that it returned an error and
a star means that it returned success.

    % ./network_diagnosis
    Ping 192.168.1.1:      ....X....X....X....X....X**********
    Ping 192.168.1.2:      ....X....X....X....X....X**********
    Ping 75.75.75.75:      ....X....X....X....X....X**********
    Ping 75.75.76.76:      ....X....X....X....X....X**********
    Ping 8.8.8.8:          ....X....X....X....X....X**********
    Ping 8.8.4.4:          ....X....X....X....X....X**********
    Ping 209.123.234.146:  ....X....X....X....X....X**********
    DNS 75.75.75.75:       .........X.........X.....**********
    DNS 75.75.76.76:       .........X.........X.....**********
    DNS 8.8.8.8:           .........X.........X.....**********
    DNS 8.8.4.4:           .........X.........X.....**********
    DNS 192.168.1.1:       .........X.........X.....**********

# License

Copyright 2017 Lawrence Kesteloot

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

