Wallops_TCPIP
=============

TCP/IP project for pulling data from the Wallops TM stream

The purpose of the swap_AND_do_lockfile branch is to begin development that will bring us in line with the setup at Wallops. That is, we will be using server.c to receive and a write a datafile while simultaneously writing to a mem-mapped lockfile that will make data available to the RTD gui.