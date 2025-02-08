#!/usr/bin/env bash

set -e

# NOTE: fxc is tested on Linux with https://github.com/mozilla/fxc2

which fxc &>/dev/null && HAVE_FXC=1 || HAVE_FXC=0
which dxc &>/dev/null && HAVE_DXC=1 || HAVE_DXC=0
which spirv-cross &>/dev/null && HAVE_SPIRV_CROSS=1 || HAVE_SPIRV_CROSS=0

[ "$HAVE_FXC" != 0 ] || echo "fxc not in PATH; D3D11 shaders will not be rebuilt"
[ "$HAVE_DXC" != 0 ] || echo "dxc not in PATH; D3D12 shaders will not be rebuilt"
[ "$HAVE_SPIRV_CROSS" != 0 ] || echo "spirv-cross not in PATH; D3D11, D3D12, Metal shaders will not be rebuilt"

USE_FXC=${USE_FXC:-$HAVE_FXC}
USE_DXC=${USE_DXC:-$HAVE_DXC}
USE_SPIRV_CROSS=${USE_SPIRV_CROSS:-$HAVE_SPIRV_CROSS}

spirv_bundle="spir-v.h"
dxbc50_bundle="dxbc50.h"
dxil60_bundle="dxil60.h"
metal_bundle="metal.h"

rm -f "$spirv_bundle"
[ "$USE_SPIRV_CROSS" != 0 ] && rm -f "$metal_bundle"
[ "$USE_SPIRV_CROSS" != 0 ] && [ "$USE_FXC" != 0 ] && rm -f "$dxbc50_bundle"
[ "$USE_SPIRV_CROSS" != 0 ] && [ "$USE_DXC" != 0 ] && rm -f "$dxil60_bundle"

make-header() {
    xxd -i "$1" | sed \
        -e 's/^unsigned /const unsigned /g' \
        -e 's,^const,static const,' \
        > "$1.h"
}

compile-hlsl-dxbc() {
    local src="$1"
    local profile="$2"
    local output_basename="$3"
    local var_name="$(echo "$output_basename" | sed -e 's/\./_/g')"

    fxc "$src" /E main /T $2 /Fh "$output_basename.tmp.h" || exit $?
    sed \
        -e "s/g_main/$var_name/;s/\r//g" \
        -e 's,^const,static const,' \
        -e 's,const unsigned,const signed,' \
        < "$output_basename.tmp.h" \
        > "$output_basename.h"
    rm -f "$output_basename.tmp.h"
}

compile-hlsl-dxil() {
    local src="$1"
    local profile="$2"
    local output_basename="$3"
    local var_name="$(echo "$output_basename" | sed -e 's/\./_/g')"

    dxc "$src" -E main -T $2 -Fh "$output_basename.tmp.h" -O3 || exit $?
    sed \
        -e "s/g_main/$var_name/;s/\r//g" \
        -e 's,^const,static const,' \
        < "$output_basename.tmp.h" \
        > "$output_basename.h"
    rm -f "$output_basename.tmp.h"
}

for i in *.vert *.frag; do
    spv="$i.spv"
    metal="$i.metal"
    hlsl50="$i.sm50.hlsl"
    dxbc50="$i.sm50.dxbc"
    hlsl60="$i.sm60.hlsl"
    dxil60="$i.sm60.dxil"

    glslangValidator -g0 -Os "$i" -V -o "$spv" --quiet

    make-header "$spv"
    echo "#include \"$spv.h\"" >> "$spirv_bundle"

    if [ "$USE_SPIRV_CROSS" = "0" ]; then
        continue
    fi

    spirv-cross "$spv" --hlsl --shader-model 50 --hlsl-enable-compat --output "$hlsl50"
    spirv-cross "$spv" --hlsl --shader-model 60 --hlsl-enable-compat --output "$hlsl60"

    if [ "${i##*.}" == "frag" ]; then
        hlsl_stage="ps"
    else
        hlsl_stage="vs"
    fi

    if [ "$USE_FXC" != "0" ]; then
        compile-hlsl-dxbc "$hlsl50" ${hlsl_stage}_5_0 "$dxbc50"
        echo "#include \"$dxbc50.h\"" >> "$dxbc50_bundle"
    fi

    if [ "$USE_DXC" != "0" ]; then
        compile-hlsl-dxil "$hlsl60" ${hlsl_stage}_6_0 "$dxil60"
        echo "#include \"$dxil60.h\"" >> "$dxil60_bundle"
    fi

    spirv-cross "$spv" --msl --output "$metal"
    make-header "$metal"
    echo "#include \"$metal.h\"" >> "$metal_bundle"
done
