#!/bin/bash

# Die on any unexpected errors.
set -e

# If this is double-clicked, the working directory isn't reliable.  Change the
# directory to the source location.
DIR=$( dirname "${0}" )
cd "${DIR}"
pwd

LOG=build/logs/setup.log
mkdir -p `dirname "../${LOG}"`

# Link the scripts to the hooks directory.
echo "Linking Git hooks..."
ln -sf ../../scripts/mac-metadata-pre-commit.sh ../.git/hooks/pre-commit
ln -sf ../../scripts/mac-metadata-post-merge.sh ../.git/hooks/post-merge

# Run the restore script from the main repo directory.
echo "Restoring Mac metadata..."
cd ..
scripts/save_mac_metadata.py -rRv * >> $LOG

# Copy the skeleton directory to the deploy directory.
echo "Copying base directory to deployment..."
mkdir -p deployment
cp -Rpv base/* deployment >> $LOG

echo "Setup complete."

exit 0

