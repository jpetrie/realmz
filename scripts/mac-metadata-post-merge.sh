#!/bin/bash

echo "Restoring Mac metadata..."

git diff HEAD@{1} --name-status -z | scripts/save_mac_metadata.py -rvg
