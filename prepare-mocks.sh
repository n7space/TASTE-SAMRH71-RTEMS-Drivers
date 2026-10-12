#!/bin/bash


pushd dummy-project
make clean
make samrh71 debug # might fail, but it is ok
popd

set -e

cp dummy-project/work/dataview/C/asn1crt.{h,c} mocks/RuntimeMocks/
cp dummy-project/work/dataview/C/dataview-uniq.{h,c} mocks/RuntimeMocks/
cp dummy-project/work/dataview/C/asn1crt_encoding.{h,c} mocks/RuntimeMocks/
cp dummy-project/work/build/system_spec/system_spec.{h,c} mocks/RuntimeMocks/
cp dummy-project/work/build/DriversConfig/drivers_config.{h,c} mocks/RuntimeMocks/
cp dummy-project/work/build/DriversConfig/samrh71-rtems-serial-driver.{h,c} mocks/RuntimeMocks/
cp dummy-project/work/build/DriversConfig/samrh71-rtems-can-driver.{h,c} mocks/RuntimeMocks/
cp dummy-project/work/build/DriversConfig/samrh71-rtems-spw-driver.{h,c} mocks/RuntimeMocks/
