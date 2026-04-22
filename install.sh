#!/bin/bash

PREFIX="${HOME}/tool-inst"
SOURCES=$(dirname $0)

mkdir -p "${PREFIX}/include/TASTE-SAMRH71-RTEMS-Drivers/src"
rm -rf "${PREFIX}/include/TASTE-SAMRH71-RTEMS-Drivers/src/*"
cp -r "${SOURCES}/src/samrh71_rtems_spw" "${PREFIX}/include/TASTE-SAMRH71-RTEMS-Drivers/"
cp -r "${SOURCES}/configurations" "${PREFIX}/include/TASTE-SAMRH71-RTEMS-Drivers/configurations"
