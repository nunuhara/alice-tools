#!/usr/bin/env bash

RTT="$(dirname $0)/rtt-acx.sh"
ACXDIR="$(dirname $0)/acx"

find "$ACXDIR" -name '*.acx' | xargs -n1 "$RTT"
