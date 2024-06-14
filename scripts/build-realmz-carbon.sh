#!/bin/bash

CONFIGS="Debug Release"
PROJECT=proj/osx/Realmz\ Carbon/Realmz\ Carbon.xcodeproj
TARGET=Realmz\ Carbon

failed=0

mkdir -p build/logs/carbon

for config in $CONFIGS; do
    echo "Building Carbon $config configuration..."
	xcodebuild -project "${PROJECT}" \
               -target "$TARGET" \
               -configuration "$config" \
               > build/logs/carbon/"${config}".log
    status=$?
    
    if [ $status -ne 0 ]; then
        echo "Configuration $config failed."
        echo "result: $?"
        failed=1
    else
        echo "Configuration $config succeded."
    fi
done

exit $failed
