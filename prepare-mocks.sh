#!/bin/bash


pushd dummy-project
make clean
make samrh71 debug # might fail, but it is ok
popd

set -e

cp dummy-project/work/dataview/C/asn1crt.h src/RuntimeMocks/
cp dummy-project/work/dataview/C/asn1crt.c src/RuntimeMocks/
cp dummy-project/work/build/system_spec/system_spec.h src/RuntimeMocks/
cp dummy-project/work/build/system_spec/system_spec.c src/RuntimeMocks/
cp dummy-project/work/build/DriversConfig/drivers_config.h src/RuntimeMocks/
cp dummy-project/work/build/DriversConfig/drivers_config.c src/RuntimeMocks/
cp dummy-project/work/build/DriversConfig/samrh71-rtems-serial-driver.h src/RuntimeMocks/
cp dummy-project/work/build/DriversConfig/samrh71-rtems-serial-driver.c src/RuntimeMocks/
cp dummy-project/work/build/DriversConfig/samrh71-rtems-can-driver.h src/RuntimeMocks/
cp dummy-project/work/build/DriversConfig/samrh71-rtems-can-driver.c src/RuntimeMocks/
cp dummy-project/work/build/DriversConfig/samrh71-rtems-spw-driver.h src/RuntimeMocks/
cp dummy-project/work/build/DriversConfig/samrh71-rtems-spw-driver.c src/RuntimeMocks/
