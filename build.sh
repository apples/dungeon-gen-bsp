#!/bin/env bash
set -e

export CPPFLAGS=""
export CXXFLAGS="-std=c++1y -g -Wall -Wfatal-errors"
export LDFLAGS=""
respite

mv a.respite gen.exe
