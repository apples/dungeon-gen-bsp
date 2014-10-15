#!/bin/env bash
set -e

export CPPFLAGS=""
export CXXFLAGS="-std=c++1y -Ofast -pg -Wall -Wfatal-errors -DBETTER_ASSERT_OFF"
export LDFLAGS="-Ofast -pg"
respite

mv a.respite gen.exe
