#!/usr/bin/env bash

RTT="$(dirname $0)/rtt-ain.sh"
AINDIR="$(dirname $0)/ain"

shopt -s nullglob
for f in $AINDIR/*.ain
do
    $RTT "$f"
done
