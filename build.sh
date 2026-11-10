#!/bin/bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
export JAVA_HOME="${JAVA_HOME:-/usr/lib/jvm/java-21-openjdk-amd64}"
export PATH="$JAVA_HOME/bin:$PATH"
export ANDROID_HOME="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-$HOME/Android/Sdk}}"
aosp_root="${AUDIOPHILE_AOSP_ROOT:-$(cd ../../../.. && pwd)}"

./gradlew --no-daemon --max-workers=2 -Patmosphere :app:assembleRelease
mkdir -p build
"$ANDROID_HOME/build-tools/36.0.0/apksigner" sign \
    --key "$aosp_root/device/rockchip/common/security/platform.pk8" \
    --cert "$aosp_root/device/rockchip/common/security/platform.x509.pem" \
    --out build/ATMOSphere-Audiophile.apk \
    app/build/outputs/apk/release/app-release-unsigned.apk
echo "$PWD/build/ATMOSphere-Audiophile.apk"
