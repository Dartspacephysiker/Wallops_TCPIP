#!/usr/bin/bash

PORT=${5:-5003}


#TM2="192.168.2.202"
TM2="192.168.1.80"
TM2_CH="6"
TM2_PORT=${PORT}

#If we ever get them running on a single unit, use these
#TM3_CH="14"
#TM3="192.168.1.80"

#...But for now, use these
TM3_CH="7"
TM3="192.168.1.24"

TM3_PORT=$((PORT + 1))

TMPORT="8999"
SLEEP="2"
ACQTIME=${3:-45}

#DIAG={$4:+"-D}"
#DIAG=""
#Diagnostic mode
#[ -n "${4}" ] && DIAG="-D" && echo -e "Diagnostic mode...\n";



OUTDIR="/home/spencerh/data/CAPER/Wallops/vibe/RxDSP"
FILE_PREFIX="_sequence"

print_defaults () {
    echo "${0} <MODE> <RxDSP NAME (master|slave)> <ACQUISITION TIME: (default:${ACQTIME} s)> <Diagnostic (no acq)[0|1]> <PORT (default: ${PORT})>"
    echo ""
    echo -e "MODES"
    echo -e "====="
    echo -e "\t0: Stop data transfer"
    echo -e "\t1: Start telnet transfer of selected RxDSP [master|slave]"
    echo -e "\t2: Start telnet transfer of master and slave RxDSP"
    echo -e "\t3: Start transfer, acquisition, and RTD of MSB/LSB chans"
    echo -e "\t\tin ${OUTDIR}"
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

RxDSP_stop () {
    echo "######################"
    echo "#telnet ${1}#"
    echo "######################"
    {       sleep $((SLEEP * 2));
	echo "stoptransfer"; sleep ${SLEEP};
	echo "exit"; sleep ${SLEEP};
    } | tee >(telnet ${1} ${TMPORT});
}

case $2 in
  "2" | "TM2" | "Master" | "MASTER" | "master" | "m" ) 
	TM=${TM2}
	CH=${TM2_CH};
	PORT=${5-"5003"};
	ACQSZ=262244;
	RTD="tcpRxDSP";
        RTDSZ=65536;
	echo "Selected Master RxDSP...";; 
    "3" | "TM3" | "Slave" | "SLAVE" | "slave" | "s" ) 
	TM=${TM3}
	CH=${TM3_CH};
	PORT=${5:-"5004"};
	ACQSZ=262244;
	RTD="tcpRxDSP";
        RTDSZ=65536;
	echo "Selected Slave RxDSP...";; 
    "all" | "ALL" | "BOTH" )
	echo "Biz";;
    "-h" | "--help" )
	print_defaults;
	exit;;
    * )
	echo -e "Invalid RxDSP/no RxDSP specified!\n"
	print_defaults;
	exit;;
esac

case $1 in
    "0" )
	RxDSP_stop ${TM} ${CH};;
    "1" )
	RxDSP_start ${TM} ${CH} ${PORT};;
    "2" )
	RxDSP_start ${TM2} ${TM2_CH} ${TM2_PORT} & RxDSP_start ${TM3} ${TM3_CH} ${TM3_PORT};;
    * )
	echo -e "Invalid mode/no mode given!\n";
	print_defaults;
	exit;;

esac

