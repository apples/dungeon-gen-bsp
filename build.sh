#!/bin/env bash
set -e

export CPPFLAGS=""
export CXXFLAGS="-std=c++1y -pg -Wall -Wfatal-errors -DBETTER_ASSERT_OFF"
export LDFLAGS="-pg"
respite

mv a.respite gen.exe
