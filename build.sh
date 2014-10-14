#!/bin/env bash
set -e

export CPPFLAGS=""
export CXXFLAGS="-std=c++1y -O2 -pg -Wall -Wfatal-errors -DBETTER_ASSERT_OFF"
export LDFLAGS="-O2 -pg"
respite

mv a.respite gen.exe
