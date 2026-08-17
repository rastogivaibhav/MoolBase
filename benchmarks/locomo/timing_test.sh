#!/bin/bash
set -e
cd /work
rm -f /tmp/timing.db
mkdir -p /tmp/timing.db.parent
CLI=./build/graphenedb_cli
$CLI init /tmp/timing.db 64
echo "--- timing 20 put-node calls ---"
time (for i in $(seq 1 20); do
  vec=$(python3 -c "print(','.join(['0.'+str($i%9+1)]*64))")
  $CLI put-node /tmp/timing.db 64 "msg $i" "$vec" $((1000+i)) root > /dev/null
done)
echo "--- timing 19 put-edge calls ---"
time (for i in $(seq 1 19); do
  $CLI put-edge /tmp/timing.db 64 $((i-1)) $i causal > /dev/null
done)
echo "--- inspect ---"
$CLI inspect /tmp/timing.db 64 | grep -E "nodes_visible|edges_visible"
echo "--- one reason call timing ---"
vec=$(python3 -c "print(','.join(['0.5']*64))")
time $CLI reason /tmp/timing.db 64 "$vec" 9999
