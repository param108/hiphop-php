#!/bin/sh
#$1: output directory
#$2: program name
#$3: flags
MYPWD=`pwd`
cp $HPHP_HOME/bin/CMakeLists.base.txt $1/CMakeLists.txt
cd $1
cmake -D PROGRAM_NAME:string=$2 . || exit $?
$3 make -j6 $MAKEOPTS || exit $?
