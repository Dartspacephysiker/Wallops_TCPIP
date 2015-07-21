#!/usr/bin/bash


TM1="192.168.2.201"
TM1PORT="8999"
PORT=${5:-5010}
SLEEP="2"
ACQTIME=${3:-45}
DIAG=""
#Diagnostic mode
[ -n "${4}" ] && DIAG="-D" && echo -e "Diagnostic mode...\n";


OUTDIR="/home/spencerh/data/CAPER/Andoya/cals/TM1"
FILE_PREFIX="Y2--Input_test--Direct"

print_defaults () {
    echo "${0} <MODE> <CHANNEL NAME> <ACQUISITION TIME: (default:${ACQTIME} s)> <DIAGNOSTIC (no acq)> <PORT (default: ${PORT})>"
    echo ""
    echo -e "MODES"
    echo -e "====="
    echo -e "\t0: Stop data transfer"
    echo -e "\t1: Start transfer of MSB/LSB chans"
    echo -e "\t2: Start transfer and acquisition of MSB/LSB chans"
    echo -e "\t3: Start transfer, acquisition, and RTD of MSB/LSB chans"
    echo -e "\t\tin ${OUTDIR}"
    echo ""
    echo -e "CHANNEL NAMES"
    echo -e "============="
    echo -e "\n"
    echo -e "\tDartmouth:\tSKIN-{LO,HI}"
    echo -e "\t\t\tELF-{A,B}{LO,HI}"
    echo -e "\t\t\tVF-{A,B}{LO,HI}"
    echo -e "\t\t\tVLF{A,B}"
    echo -e "\t\t\tVLF{A,B}-AGC"
    echo -e "\t\t\tHF-AGC"
    echo -e "\n"
    echo -e "\tOslo:\t\tLPC{1,2,3,4}"
    echo -e "\t\t"
}

telnet_start () {
    echo "######################"
    echo "#telnet ${TM1}#"
    echo "######################"
    {   #echo "listusedchs"; sleep ${SLEEP}; 
	sleep $((SLEEP * 2));
	echo "/stx preparetransfer"; sleep ${SLEEP}; 
	echo "ch ${MSBCH}"; sleep ${SLEEP}; 
	echo "ch ${LSBCH}"; sleep ${SLEEP}; 
	echo "/etx"; sleep ${SLEEP};
	echo "starttransfer ${PORT}"; sleep ${SLEEP};
	sleep ${ACQTIME};
	#echo "exit"; sleep ${SLEEP};
    } | tee >(telnet ${TM1} ${TM1PORT});


}

telnet_stop () {
    echo "######################"
    echo "#telnet ${TM1}#"
    echo "######################"
    {       sleep $((SLEEP * 2));
	echo "stoptransfer"; sleep ${SLEEP};
	echo "exit"; sleep ${SLEEP};
    } | tee >(telnet ${TM1} ${TM1PORT});
}

#Which channels to look at?
case $2 in
    "VLFA" | "VLF" ) 
	MSBCH="26"; LSBCH="27";
	echo "Selected VLFA...";   
	PORT=5026;
	ACQSZ=16384;
        RTD="VLFA";
        RTDSZ="2048";;
    "VLFB" ) 
	MSBCH="28"; LSBCH="29";
	echo "Selected VLFB...";
	PORT=5028;
	ACQSZ=16384;
	RTD="VLFB";
        RTDSZ="2048";;
    "VLFA-AGC" | "VLF-AGCA" | "VLFAGC" | "VLF-AGC" | "VLFAGCA" | "VLFAAGC" ) 
	MSBCH="30"; LSBCH="31";
	echo "Selected VLFA-AGC...";
	PORT=5030;
	ACQSZ=16384;
	RTD="VLFAGCA";
        RTDSZ="2048";;
    "VLFB-AGC" | "VLF-AGCB" | "VLFAGCB" | "VLFBAGC" )
	MSBCH="32"; LSBCH="33";
	echo "Selected VLFB-AGC...";
	PORT=5032;
	ACQSZ=16384;
	RTD="VLFAGCB";
        RTDSZ="2048";;
    "VF-ALO" | "VFALO" ) 
	MSBCH="34"; LSBCH="35";
	PORT=5034;
	ACQSZ=8192;
	RTD="VFALO";
        RTDSZ=1024;
	echo "Selected VF-ALO...";; 
    "VF-AHI" | "VFAHI" | "VF" | "VFA" ) 
	MSBCH="36"; LSBCH="37";
	PORT=5036;
	ACQSZ=8192;
	RTD="VFAHI";
        RTDSZ=1024;
	echo "Selected VF-AHI...";;
    "VF-BLO" | "VFBLO" ) 
	MSBCH="38"; LSBCH="39";
	PORT=5038;
	ACQSZ=8192;
	RTD="VFBLO";
        RTDSZ=1024;
	echo "Selected VF-BLO...";;
    "VF-BHI" | "VFBHI" ) 
	MSBCH="40"; LSBCH="41";
	PORT=5040;
	ACQSZ=8192;
	RTD="VFBHI";
        RTDSZ=1024;
	echo "Selected VF-BHI...";;
    "ELF-ALO" | "ELFALO" ) 
	MSBCH="42"; LSBCH="43";
	PORT=5042;
	ACQSZ=4096;
	RTD="ELFALO";
        RTDSZ=512;
#	ACQ_SLEEP="-s 200";
	echo "Selected ELF-ALO...";; 
    "ELF-AHI" | "ELFAHI" ) 
	MSBCH="44"; LSBCH="45";
	PORT=5044;
	ACQSZ=4096;
	RTD="ELFAHI";
        RTDSZ=512;
#	ACQ_SLEEP="-s 200";
	echo "Using ELF-AHI...";;
    "ELF-BLO" | "ELFBLO" ) 
	MSBCH="46"; LSBCH="47";
	PORT=5046;
	ACQSZ=4096;
	RTD="ELFBLO";
        RTDSZ=512;
	ACQ_SLEEP="-s 200";
	echo "Selected ELF-BLO...";; 
    "ELF-BHI" | "ELFBHI" ) 
	MSBCH="48"; LSBCH="49";
	PORT=5048;
	ACQSZ=4096;
	RTD="ELFBHI";
        RTDSZ=512;
	ACQ_SLEEP="-s 200";
	echo "Selected ELF-BHI...";;
    "SKIN-LO" | "SKINLO" ) 
	MSBCH="50"; LSBCH="51";
	PORT=5050;
	ACQSZ=4096;
	RTD="SKINLO";
	ACQ_SLEEP="-s 200";
        RTDSZ=512;
	echo "Selected SKIN-LO...";; 
    "SKIN-HI" | "SKINHI" | "SKIN" ) 
	MSBCH="52"; LSBCH="53";
	PORT=5052;
	ACQSZ=4096;
	RTD="SKINHI";
	ACQ_SLEEP="-s 200";
        RTDSZ=512;
	echo "Selected SKIN-HI...";;
    "HF-AGC" | "HFAGC" ) 
	MSBCH="54"; LSBCH="55";
	PORT=5054;
	ACQSZ=8092;
	RTD="HFAGC";
        RTDSZ=1024;
	echo "Selected HF-AGC...";; 
    "LPC1" | "LPC" ) 
	MSBCH="4"; LSBCH="5";
	PORT=5064;
	ACQSZ=4096;
	RTD="LPC1";
        RTDSZ=1024;
	echo "Selected Oslo Langmuir Probe CH1...";; 
    "LPC2" ) 
	MSBCH="6"; LSBCH="7";
	PORT=5066;
	ACQSZ=4096;
	RTD="LPC2";
        RTDSZ=1024;
	echo "Selected Oslo Langmuir Probe CH2...";; 
    "LPC3" ) 
	MSBCH="8"; LSBCH="9";
	PORT=5068;
	ACQSZ=4096;
	RTD="LPC3";
        RTDSZ=1024;
	echo "Selected Oslo Langmuir Probe CH3...";; 
    "LPC4" ) 
	MSBCH="10"; LSBCH="11";
	PORT=5070;
	ACQSZ=4096;
	RTD="LPC4";
        RTDSZ=1024;
	echo "Selected Oslo Langmuir Probe CH4...";; 
    "-h" | "--help" )
	print_defaults;
	exit;;
    * )
	echo -e "Invalid channel/no channel specified!\n"
	print_defaults;
	exit;;
esac

case $1 in
    "0" )
	telnet_stop;;
    "1" )
	telnet_start;;
    "2" )
	echo "/usr/src/Wallops_TCPIP/tcp_player -p ${PORT} -P \"TM1_${RTD}${FILE_PREFIX}\" -g -r 6 -A ${ACQSZ} -R ${RTDSZ} -d 1 -m /tmp/rtd/rtd_tcp${RTD}.data -o ${OUTDIR} ${DIAG} & telnet_start";
	/usr/src/Wallops_TCPIP/tcp_player -p ${PORT} -P "TM1_${RTD}${FILE_PREFIX}" -g -r 6 -A ${ACQSZ} -R ${RTDSZ} -d 1 -m /tmp/rtd/rtd_tcp${RTD}.data -o ${OUTDIR} ${DIAG} ${ACQ_SLEEP} & telnet_start;;
    "3" )
	echo "/usr/src/Wallops_TCPIP/tcp_player -p ${PORT} -P \"TM1_${RTD}${FILE_PREFIX}\" -g -r 6 -A ${ACQSZ} -R ${RTDSZ} -d 1 -m /tmp/rtd/rtd_tcp${RTD}.data -o ${OUTDIR} ${DIAG} & telnet_start & /usr/src/prtd/rtd_script.sh 1 tcp${RTD}";	
	/usr/src/Wallops_TCPIP/tcp_player -p ${PORT} -P "TM1_${RTD}${FILE_PREFIX}" -g -r 6 -A ${ACQSZ} -R ${RTDSZ} -d 1 -m /tmp/rtd/rtd_tcp${RTD}.data -o ${OUTDIR} ${DIAG} ${ACQ_SLEEP} & telnet_start & /usr/src/prtd/rtd_script.sh 1 tcp${RTD} ;;
    * )
	echo -e "Invalid mode/no mode given!\n";
	print_defaults;;
esac

