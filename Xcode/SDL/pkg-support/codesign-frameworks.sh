#!/bin/sh

# WARNING: You may have to run Clean in Xcode after changing CODE_SIGN_IDENTITY! 

# Verify that $CODE_SIGN_IDENTITY is set
if [ -z "$CODE_SIGN_IDENTITY" ] ; then
    echo "CODE_SIGN_IDENTITY needs to be non-empty for codesigning frameworks!"

    if [ "$CONFIGURATION" = "Release" ] ; then
        exit 1
    else
        # Codesigning is optional for non-release builds.
        exit 0
    fi
fi

SAVEIFS=$IFS
IFS=$(echo -en "\n\b")

FRAMEWORK_DIR="${TARGET_BUILD_DIR}"

# Loop through all frameworks
FRAMEWORKS=`find "${FRAMEWORK_DIR}" -type d -name "*.framework" | sed -e "s/\(.*\)/\1\/Versions\/A\//"`
RESULT=$?
if [[ $RESULT != 0 ]] ; then
    exit 1
fi

echo "Found:"
echo "${FRAMEWORKS}"

for FRAMEWORK in $FRAMEWORKS;
do
    echo "Signing '${FRAMEWORK}'"
    `codesign -f -v -s "${CODE_SIGN_IDENTITY}" "${FRAMEWORK}"`
    RESULT=$?
    if [[ $RESULT != 0 ]] ; then
        exit 1
    fi
done

# restore $IFS
IFS=$SAVEIFS
