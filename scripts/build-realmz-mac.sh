#!/bin/bash

FAILED=0

# Remove and recreate the build directory, including the logs.
rm -rf build
mkdir -p build/logs

# First, initialize the repository.
echo "Setting up repository..."
scripts/mac-repository-setup.command
setup_succeeded=$?

# If there is a build artifacts directory, copy this early in case it failed.
if [ ! -z $BUILD_ARTEFACTS ]; then
    cp -r build/logs "${BUILD_ARTEFACTS}"
fi

if [[ $setup_succeeded -ne 0 ]]; then
    FAILED=true
    echo "Setup failed! Aborting build."
    exit -1
else
    echo "Setup succeeded."
fi

# Build the Classic version.
if [ ! -z $BUILD_CLASSIC ]; then
    echo "Building Classic version..."
    osascript scripts/build-realmz-classic.applescript -o > build/logs/classic.log
    classic_succeeded=$?

    if [[ $classic_succeeded -ne 0 ]]; then
        echo "Classic build failed."
        FAILED=1
        classic_succeeded=no
    else
        echo "Classic build succeeded."
        classic_succeeded=yes
    fi
else
    echo "Skipping Classic build."
    classic_succeeded="n/a"
fi

# Build the Carbon version.
echo "Building Carbon version..."
scripts/build-realmz-carbon.sh
carbon_succeeded=$?

if [[ $carbon_succeeded -ne 0 ]]; then
    echo "Carbon build failed."
    FAILED=1
    carbon_succeeded=no
else
    echo "Carbon build succeeded."
    carbon_succeeded=yes
fi

echo
echo "*********************************"
echo 

# We don't do the deploy step yet.
if [[ $FAILED -eq 0 ]]; then
    echo "Full build succeeded!"
else
    echo "Full build failed!"
fi

echo 
echo "Classic build succeeded: " $classic_succeeded
echo "Carbon build succeeded:  " $carbon_succeeded

echo
echo "*********************************"
echo

# Copy build logs to the build artifacts directory for viewing.
if [ ! -z $BUILD_ARTEFACTS ]; then
    cp -R build/logs "${BUILD_ARTEFACTS}"
fi

if [[ $FAILED -eq 0 ]]; then
    echo "Building deployment package..."
    # Deploy the built product.
    scripts/deploy-realmz-mac.sh
    FAILED=$?
    if [[ $FAILED -ne 0 ]]; then
        echo "Deployment failed."
    else
        echo "Deployment succeeded."
    fi
else
    echo "Build failed, skipping deployment."
fi

exit $FAILED

