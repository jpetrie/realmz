#!/bin/bash

set -e

STAGING="build/staging"
VERSION=`git describe`
BUILD_NAME="Realmz $VERSION"

SRC="deployment/Realmz"
NSRC="deployment/${BUILD_NAME}"
DST="${STAGING}/${BUILD_NAME}.zip"

mkdir -p "${STAGING}"

# Move the built product to the staged name.
ditto --rsrc "${SRC}" "${NSRC}"

# Really simple deploy procedure here; use ditto to copy to a zip archive.
ditto -c -k -v --keepParent --rsrc --sequesterRsrc "${NSRC}" "${DST}"


# Do we have a Goldberg built artifacts directory?
if [ ! -z $BUILD_ARTEFACTS ]; then
    ditto --rsrc "${DST}" "${BUILD_ARTEFACTS}"
fi

