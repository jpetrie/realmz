#!/bin/bash

git status --porcelain -uno -z | scripts/save_mac_metadata.py -pg0 | xargs -0 git add --no-ignore-removal  --ignore-errors
#oo
