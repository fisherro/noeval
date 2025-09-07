#!/bin/bash

egrep '\btest-(assert|error)\b' "$@" | wc -l
