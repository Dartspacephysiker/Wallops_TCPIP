#!/usr/bin/bash


TM1="192.168.2.201"
TM1PORT="8999"
PORT=${5:-5010}
SLEEP="2"
ACQTIME=${3:-45}
DIAG=""

OUTDIR="/home/spencerh/data/CAPER/Wallops/post-vibe"
FILE_PREFIX="_Z_vibe"

print_defaults () {
    echo "${0} <CHANNEL NAME> <MODE> <ACQUISITION TIME: (default:${ACQTIME} s)> <DIAGNOSTIC (no acq)> <PORT (default: ${PORT})>"
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

#Diagnostic mode
[ -n "${4}" ] && DIAG="-D" && echo -e "Diagnostic mode...\n";

#Which channels to look at?
case $1 in
    "VLFA" | "VLF" ) 
	MSBCH="27"; LSBCH="28";
	echo "Selected VLFA...";   
	PORT=5026;
	ACQSZ=16384;
        RTD="VLFA";
        RTDSZ="2048";;
    "VLFB" ) 
	MSBCH="29"; LSBCH="30";
	echo "Selected VLFB...";
	PORT=5028;
	ACQSZ=16384;
	RTD="VLFB";
        RTDSZ="2048";;
    "VLFA-AGC" | "VLF-AGCA" | "VLFAGC" | "VLF-AGC" ) 
	MSBCH="31"; LSBCH="32";
	echo "Selected VLFA-AGC...";
	PORT=5030;
	ACQSZ=16384;
	RTD="VLFAGC";
        RTDSZ="2048";;
    "VLFB-AGC" | "VLF-AGCB" )
	MSBCH="33"; LSBCH="34";
	echo "Selected VLFB-AGC...";
	PORT=5032;
	ACQSZ=16384;
	RTD="VLFAGC";
        RTDSZ="2048";;
    "VF-ALO" | "VFALO" ) 
	MSBCH="35"; LSBCH="36";
	PORT=5034;
	ACQSZ=8192;
	RTD="VFALO";
        RTDSZ=1024;
	echo "Selected VF-ALO...";; 
    "VF-AHI" | "VFAHI" | "VF" | "VFA" ) 
	MSBCH="37"; LSBCH="38";
	PORT=5036;
	ACQSZ=8192;
	RTD="VFAHI";
        RTDSZ=1024;
	echo "Selected VF-AHI...";;
    "VF-BLO" | "VFBLO" ) 
	MSBCH="39"; LSBCH="40";
	PORT=5038;
	ACQSZ=8192;
	RTD="VFBLO";
        RTDSZ=1024;
	echo "Selected VF-BLO...";;
    "VFB-HI" | "VFBHI" ) 
	MSBCH="41"; LSBCH="42";
	PORT=5040;
	ACQSZ=8192;
	RTD="VFBHI";
        RTDSZ=1024;
	echo "Selected VF-BHI...";;
    "ELF-ALO" | "ELFALO" ) 
	MSBCH="43"; LSBCH="44";
	PORT=5042;
	ACQSZ=4096;
	RTD="ELFALO";
        RTDSZ=1024;
	echo "Selected ELF-ALO...";; 
    "ELF-AHI" | "ELFAHI" ) 
	MSBCH="45"; LSBCH="46";
	PORT=5044;
	ACQSZ=4096;
	RTD="ELFAHI";
        RTDSZ=1024;
	echo "Using ELF-AHI...";;
    "ELF-BLO" | "ELFBLO" ) 
	MSBCH="47"; LSBCH="48";
	PORT=5046;
	ACQSZ=4096;
	RTD="ELFBLO";
        RTDSZ=1024;
	echo "Selected ELF-BLO...";; 
    "ELF-BHI" | "ELFBHI" ) 
	MSBCH="49"; LSBCH="50";
	PORT=5048;
	ACQSZ=4096;
	RTD="ELFBHI";
        RTDSZ=1024;
	echo "Selected ELF-BHI...";;
    "SKIN-LO" | "SKINLO" ) 
	MSBCH="51"; LSBCH="52";
	PORT=5050;
	ACQSZ=4096;
	RTD="SKINLO";
        RTDSZ=1024;
	echo "Selected SKIN-LO...";; 
    "SKIN-HI" | "SKINHI" | "SKIN" ) 
	MSBCH="53"; LSBCH="54";
	PORT=5052;
	ACQSZ=4096;
	RTD="SKINHI";
        RTDSZ=1024;
	echo "Selected SKIN-HI...";;
    "HF-AGC" | "HFAGC" ) 
	MSBCH="55"; LSBCH="56";
	PORT=5054;
	ACQSZ=4096;
	RTD="HFAGC";
        RTDSZ=1024;
	echo "Selected HF-AGC...";; 
    "LPC1" | "LPC" ) 
	MSBCH="5"; LSBCH="6";
	PORT=5064;
	ACQSZ=4096;
	RTD="LPC1";
        RTDSZ=1024;
	echo "Selected Oslo Langmuir Probe CH1...";; 
    "LPC2" ) 
	MSBCH="7"; LSBCH="8";
	PORT=5066;
	ACQSZ=4096;
	RTD="LPC2";
        RTDSZ=1024;
	echo "Selected Oslo Langmuir Probe CH2...";; 
    "LPC3" ) 
	MSBCH="9"; LSBCH="10";
	PORT=5068;
	ACQSZ=4096;
	RTD="LPC3";
        RTDSZ=1024;
	echo "Selected Oslo Langmuir Probe CH3...";; 
    "LPC4" ) 
	MSBCH="11"; LSBCH="12";
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

case $2 in
    "0" )
	telnet_stop;;
    "1" )
	telnet_start;;
    "2" )
	./tcp_player -p ${PORT} -P "TM1_${RTD}${FILE_PREFIX}" -g -r 6 -A ${ACQSZ} -R ${RTDSZ} -d 1 -m /tmp/rtd/rtd_tcp${RTD}.data -o ${OUTDIR} ${DIAG} & telnet_start;;
    "3" )
	./tcp_player -p ${PORT} -P "TM1_${RTD}${FILE_PREFIX}" -g -r 6 -A ${ACQSZ} -R ${RTDSZ} -d 1 -m /tmp/rtd/rtd_tcp${RTD}.data -o ${OUTDIR} ${DIAG}& telnet_start & /usr/src/prtd/rtd_script.sh 1 tcp${RTD};;
    * )
	echo -e "Invalid mode/no mode given!\n";
	print_defaults;;
esac

