#!/bin/bash

PREFIX="${HOME}/tool-inst"
SOURCES=$(dirname $0)
TARGET_DIR="${PREFIX}/include/TASTE-SAMRH71-RTEMS-Drivers/samrh71_rtems_spw"

rm -rdf "${PREFIX}/include/TASTE-SAMRH71-RTEMS-Drivers"
mkdir -p "${PREFIX}/include/TASTE-SAMRH71-RTEMS-Drivers/samrh71_rtems_spw"

cp "${SOURCES}/src/samrh71_rtems_spw/samrh71_rtems_spw.h" "${TARGET_DIR}/"
cp "${SOURCES}/src/samrh71_rtems_spw/samrh71_rtems_spw.c" "${TARGET_DIR}/"
cp -r "${SOURCES}/src/samrh71_rtems_spw/n7s-spw/src/microchip_spw/." "${TARGET_DIR}/"
mv "${PREFIX}/include/TASTE-SAMRH71-RTEMS-Drivers/samrh71_rtems_spw/spw.h" "${PREFIX}/include/TASTE-SAMRH71-RTEMS-Drivers/samrh71_rtems_spw/spw_registers.h"
cp -r "${SOURCES}/src/samrh71_rtems_spw/n7s-spw/src/." "${TARGET_DIR}/"
cp -r "${SOURCES}/src/samrh71_rtems_spw/n7s-spw/stubs/." "${TARGET_DIR}/"
rm -rdf "${PREFIX}/include/TASTE-SAMRH71-RTEMS-Drivers/samrh71_rtems_spw/microchip_spw"

find "$TARGET_DIR" -type f -exec sed -i 's|#include "microchip_spw/spw.h"|#include "microchip_spw/spw_registers.h"|g' {} +
find "$TARGET_DIR" -type f -exec sed -i 's|microchip_spw/||g' {} +

cp -r "${SOURCES}/src/samrh71_rtems_serial" "${PREFIX}/include/TASTE-SAMRH71-RTEMS-Drivers/"

cp -r "${SOURCES}/configurations" "${PREFIX}/include/TASTE-SAMRH71-RTEMS-Drivers/configurations"


# mkdir -p "${PREFIX}/include/TASTE-SAMRH71-RTEMS-Drivers/src"
# rm -rf "${PREFIX}/include/TASTE-SAMRH71-RTEMS-Drivers/src/*"
# cp -r "src/samrh71_rtems_serial" "${PREFIX}/include/TASTE-SAMRH71-RTEMS-Drivers/"
# cp -r "configurations" "${PREFIX}/include/TASTE-SAMRH71-RTEMS-Drivers/"
