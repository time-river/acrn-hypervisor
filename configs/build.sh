#!/bin/bash

export TOPDIR=$(pwd)
export BOARD=${TOPDIR}/configs/qemu-acrn-pa.board.xml
export SCENARIO=${TOPDIR}/configs/scenario.xml

#make hypervisor
make devicemodel
