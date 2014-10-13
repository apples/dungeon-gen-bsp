#!/bin/env bash
set -e

export CPPFLAGS=""
export CXXFLAGS="-std=c++1y -pg -Wall -Wfatal-errors"
export LDFLAGS="-pg"
respite

mv a.respite gen.exe
