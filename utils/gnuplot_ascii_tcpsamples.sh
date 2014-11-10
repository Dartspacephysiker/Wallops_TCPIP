#! /bin/bash

#plot '/tmp/rtd/rtd.data' binary skip = 72 format="%int16%int16" using 2 every ::100:0:200:0 with lines, '/tmp/rtd/rtd.data' binary skip = 72 format="%int16%int16" using 1 every ::100:0:200:0 with lines

FNAME=$1

NUMPOINTS=${2-100}

ROW_OFFSET=${3:-1}

#DEFAULT IS TO PRINT SAMPLES AGAINST TIMESTAMPS
COL_X=${4:-4}
COL_Y=${5:-2}

WITHLINES=""
#WITHLINES="with lines"

#PLOTSTR="plot '${FNAME}' format=\"${CHANFORMAT}\" using 2:4 every ::${ROW_OFFSET}:0:$((ROW_OFFSET + NUMPOINTS)):0 ${WITHLINES} title \"Samples ${ROW_OFFSET} to $((ROW_OFFSET + NUMPOINTS))\""
PLOTSTR="plot '${FNAME}' using ${COL_X}:${COL_Y} every ::${ROW_OFFSET}::$((ROW_OFFSET + NUMPOINTS)) ${WITHLINES} title \"Samples ${ROW_OFFSET} to $((ROW_OFFSET + NUMPOINTS))\""

if [ $1 ]; 
then 
    echo "Gnuplotting ${FNAME}.."
    echo "Number of points: ${NUMPOINTS}"
    echo "Row offset:       ${ROW_OFFSET}"
    echo "Column X:         ${COL_X}"
    echo "Column Y:         ${COL_Y}"
    echo ${PLOTSTR}
    gnuplot -persistent gnuplot_opts.gnu -e "${PLOTSTR}"; 
else 
    echo "$0 <filename> <Numpoints [${NUMPOINTS}]> <Row offset [${ROW_OFFSET}]> <Col X ${COL_X}> <COL_Y ${COL_Y}>"; 
fi
