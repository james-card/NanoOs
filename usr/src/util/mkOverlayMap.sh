#!/bin/bash

# Include the root header.
echo '#include "NanoOsUser.h"' > OverlayMap.c
echo '' >> OverlayMap.c

# Grab all the function prototypes.
grep "^void\* [_a-zA-Z]*(void \*args) {" ${@} | sed -e 's/.*:\(.*\) *{$/\1;/' >> OverlayMap.c
echo '' >> OverlayMap.c

# Build the exports.
echo 'NanoOsOverlayExport exports[] = {' >> OverlayMap.c
functionNames="$(grep "^void\* [_a-zA-Z]*(void \*args) {" ${@} | sed -e 's/.*:void\* \([_a-zA-Z]*\)(void \*args) {/\1/' | sort)"
for functionName in ${functionNames}; do
	echo "  {\"${functionName}\", ${functionName}}," >> OverlayMap.c
done
echo '};' >> OverlayMap.c
echo '' >> OverlayMap.c

# Build the map.
echo '__attribute__((section(".overlay_header")))' >> OverlayMap.c
echo 'NanoOsOverlayMap overlayMap = {' >> OverlayMap.c
echo '  .header = {' >> OverlayMap.c
echo '    .magic = NANO_OS_OVERLAY_MAGIC,' >> OverlayMap.c
echo '    .version = (0 << 24) | (0 << 16) | (1 << 8) | (0 << 0),' >> OverlayMap.c
echo '    .unixApi = NULL,' >> OverlayMap.c
echo '    .callOverlayFunction = NULL,' >> OverlayMap.c
echo '    .numExports = sizeof(exports) / sizeof(exports[0]),' >> OverlayMap.c
echo '    .env = NULL,' >> OverlayMap.c
echo '  },' >> OverlayMap.c
echo '  .exports = exports,' >> OverlayMap.c
echo '};' >> OverlayMap.c

