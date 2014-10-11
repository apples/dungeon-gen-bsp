#!/bin/env bash
set -e

export CPPFLAGS=""
export CXXFLAGS="-std=c++1y -O3 -flto -Wall -Wfatal-errors"
export LDFLAGS=""
respite

mv a.respite gen.exe
