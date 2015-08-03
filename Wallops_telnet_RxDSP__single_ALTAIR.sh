#!/usr/bin/bash

PORT=${4:-5003}


TM2="192.168.1.80"
TM2_CH="6"
#TM2_CH="4"  #Major frame counter, TM2
TM2_PORT=${PORT}

#If we ever get them running on a single unit, use these
TM3_CH="15"
#TM3_CH="12"    #Major frame counter, TM3
TM3="192.168.1.80"

#...But for now, use these
#TM3_CH="7"
#TM3="192.168.1.24"

#TM3_PORT=$((PORT + 1))
TM3_PORT=$TM2_PORT

TMPORT="8999"
SLEEP="2"
ACQTIME=${2:-45}

#DIAG={$4:+"-D}"
#DIAG=""
#Diagnostic mode
#[ -n "${4}" ] && DIAG="-D" && echo -e "Diagnostic mode...\n";


#OUTDIR="/home/spencerh/data/CAPER/Wallops/vibe/RxDSP"
OUTDIR="/home/spencerh/data/CAPER/Wallops_roundtwo/RxDSP"

#FILE_PREFIX=${5:-"RxDSP_singleTM_test"}

print_defaults () {
    echo "Usage"
    echo "====="
    echo ""
    #   echo "${0} <MODE> <ACQUISITION TIME: (default:${ACQTIME} s)> <DIAGNOSTIC, NO ACQ [0|1] (default: 0)> <PORT (default: ${PORT})> <FILE PREFIX (default: ${FILE_PREFIX})>"
    echo "${0} <MODE> <ACQUISITION TIME: (default:${ACQTIME} s)> <DIAGNOSTIC, NO ACQ [0|1] (default: 0)> <PORT (default: ${PORT})>"
    echo ""
    echo -e "Modes"
    echo -e "====="
#    echo -e "\t0: Stop data transfer for both Master and Slave RxDSPs (regardless of whether both are acquiring)"
    echo -e "\t1: Start telnet transfer of Master RxDSP"
    echo -e "\t2: Start telnet transfer of Slave  RxDSP"
    echo -e "\t3: Start telnet transfer of Master and Slave RxDSP"
#    echo -e "\t4: Start transfer, acquisition, and RTD of MSB/LSB chans"
#    echo -e "\t\tin ${OUTDIR}"
    echo ""
    echo "NOTE: You must already have tcp_player listening on the appropriate port(s) in order for data to be received!"
    echo ""
}

RxDSP_start () {
    echo "######################"
    echo "#telnet ${1}#"
    echo "######################"
    {   #echo "listusedchs"; sleep ${SLEEP}; 
	sleep $((SLEEP * 2));
	echo "preparetransfer ch ${2}"; sleep ${SLEEP}; 
	echo "starttransfer ${3}"; sleep ${SLEEP};
	sleep ${ACQTIME};
	#echo "exit"; sleep ${SLEEP};
    } | tee >(telnet ${1} ${TMPORT});


}

RxDSP_start_both () {
    echo "######################"
    echo "#telnet ${1}#"
    echo "######################"
    {   #echo "listusedchs"; sleep ${SLEEP}; 
	sleep $((SLEEP * 2));
	echo "/stx preparetransfer"; sleep ${SLEEP}; 
	echo "ch ${2}"; sleep ${SLEEP}; 
	echo "ch ${3}"; sleep ${SLEEP}; 
	echo "/etx"; sleep ${SLEEP};
	echo "starttransfer ${4}"; sleep ${SLEEP};
	sleep ${ACQTIME};
	#echo "exit"; sleep ${SLEEP};
    } | tee >(telnet ${1} ${TMPORT});


}

RxDSP_stop () {
    echo "######################"
    echo "#telnet ${1}#"
    echo "######################"
    {       sleep $((SLEEP * 2));
	echo "stoptransfer"; sleep ${SLEEP};
	echo "exit"; sleep ${SLEEP};
    } | tee >(telnet ${1} ${TMPORT});
}

case $1 in
#    "0" )
#	RxDSP_stop ${TM} ${CH};;
    "1" )
	RxDSP_start ${TM2} ${TM2_CH} ${TM2_PORT};;
    "2" )
	RxDSP_start ${TM3} ${TM3_CH} ${TM3_PORT};;
    "3" )
	RxDSP_start_both ${TM2} ${TM2_CH} ${TM3_CH} ${TM2_PORT} & echo "Starting both";;
    * )
	echo -e "Invalid mode/no mode given!\n";
	print_defaults;
	exit;;

esac

