#!/bin/env bash
set -e

export CPPFLAGS=""
export CXXFLAGS="-std=c++1y -Ofast -Wall -Wfatal-errors -DBETTER_ASSERT_OFF"
export LDFLAGS="-Ofast"
respite

mv a.respite gen.exe
