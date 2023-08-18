#!/usr/bin/env bash
# --------------------------------------------------------------------
#    This script runs the exploration of a linear 
# --------------------------------------------------------------------

set -o errexit
set -o pipefail
set -o nounset

air-opt \
--air-linalg-codegen="l2-tile-size=64,64,128 l2-promote=true l1-tile-size=32,32,32 l1-promote=true" \
linear.linalg.mlir

