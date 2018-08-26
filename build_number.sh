#!/bin/bash
number=`cat ../scoreController/build_number`
let number++
echo "$number" | tee ../scoreController/build_number #<-- output and save the number back to file
echo "#define BUILD ""$number" | tee ../scoreController/build_number.h
