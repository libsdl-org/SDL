# Rebuilds the shaders needed for the GPU cube test.
# For SPIR-V: requires glslangValidator and spirv-cross, which can be obtained from the LunarG Vulkan SDK.
# For DXBC compilation: requires FXC, which is part of the Windows SDK.
# For DXIL compilation, requires DXC, which can be obtained via the Windows SDK or via here: https://github.com/microsoft/DirectXShaderCompiler/releases
# For Metal compilation: requires Xcode

# On Windows, run this via Git Bash.
# To add the Windows SDK (FXC/DXC) to your path, run the command:
#   `export PATH=$PATH:/c/Program\ Files\ \(x86\)/Windows\ Kits/10/bin/x.x.x.x/x64/`

export MSYS_NO_PATHCONV=1

# SPIR-V
glslangValidator cube.glsl -V -S vert -o cube.vert.spv --quiet -DVERTEX
glslangValidator cube.glsl -V -S frag -o cube.frag.spv --quiet
xxd -i cube.vert.spv | perl -w -p -e 's/\Aunsigned /const unsigned /;' > cube.vert.h
xxd -i cube.frag.spv | perl -w -p -e 's/\Aunsigned /const unsigned /;' > cube.frag.h
cat cube.vert.h cube.frag.h > testgpu_spirv.h
rm -f cube.vert.h cube.frag.h cube.vert.spv cube.frag.spv

# Platform-specific compilation
if [[ "$OSTYPE" == "darwin"* ]]; then

    # FIXME: Needs to be updated!

    # Xcode
    generate_shaders()
    {
        fileplatform=$1
        compileplatform=$2
        sdkplatform=$3
        minversion=$4

        xcrun -sdk $sdkplatform metal -c -std=$compileplatform-metal1.1 -m$sdkplatform-version-min=$minversion -Wall -O3 -DVERTEX=1 -o ./cube.vert.air ./cube.metal || exit $?
        xcrun -sdk $sdkplatform metal -c -std=$compileplatform-metal1.1 -m$sdkplatform-version-min=$minversion -Wall -O3 -o ./cube.frag.air ./cube.metal || exit $?

        xcrun -sdk $sdkplatform metallib -o cube.vert.metallib cube.vert.air || exit $?
        xcrun -sdk $sdkplatform metallib -o cube.frag.metallib cube.frag.air || exit $?

        xxd -i cube.vert.metallib | perl -w -p -e 's/\Aunsigned /const unsigned /;' >./cube.vert_$fileplatform.h
        xxd -i cube.frag.metallib | perl -w -p -e 's/\Aunsigned /const unsigned /;' >./cube.frag_$fileplatform.h

        rm -f cube.vert.air cube.vert.metallib
        rm -f cube.frag.air cube.frag.metallib
    }

    generate_shaders macos macos macosx 10.11
    generate_shaders ios ios iphoneos 8.0
    generate_shaders iphonesimulator ios iphonesimulator 8.0
    generate_shaders tvos ios appletvos 9.0
    generate_shaders tvsimulator ios appletvsimulator 9.0

    # Bundle together one mega-header
    rm -f testgpu_metallib.h
    echo "#if defined(SDL_PLATFORM_IOS)" >> testgpu_metallib.h
        echo "#if TARGET_OS_SIMULATOR" >> testgpu_metallib.h
            cat cube.vert_iphonesimulator.h >> testgpu_metallib.h
            cat cube.frag_iphonesimulator.h >> testgpu_metallib.h
        echo "#else" >> testgpu_metallib.h
            cat cube.vert_ios.h >> testgpu_metallib.h
            cat cube.frag_ios.h >> testgpu_metallib.h
        echo "#endif" >> testgpu_metallib.h
    echo "#elif defined(SDL_PLATFORM_TVOS)" >> testgpu_metallib.h
        echo "#if TARGET_OS_SIMULATOR" >> testgpu_metallib.h
            cat cube.vert_tvsimulator.h >> testgpu_metallib.h
            cat cube.frag_tvsimulator.h >> testgpu_metallib.h
        echo "#else" >> testgpu_metallib.h
            cat cube.vert_tvos.h >> testgpu_metallib.h
            cat cube.frag_tvos.h >> testgpu_metallib.h
        echo "#endif" >> testgpu_metallib.h
    echo "#else" >> testgpu_metallib.h
        cat cube.vert_macos.h >> testgpu_metallib.h
        cat cube.frag_macos.h >> testgpu_metallib.h
    echo "#endif" >> testgpu_metallib.h

    # Clean up
    rm -f cube.vert_macos.h cube.frag_macos.h
    rm -f cube.vert_iphonesimulator.h cube.frag_iphonesimulator.h
    rm -f cube.vert_tvsimulator.h cube.frag_tvsimulator.h
    rm -f cube.vert_ios.h cube.frag_ios.h
    rm -f cube.vert_tvos.h cube.frag_tvos.h

elif [[ "$OSTYPE" == "cygwin"* ]] || [[ "$OSTYPE" == "msys"* ]]; then

    # DXC
    dxc cube.hlsl /E VSMain /T vs_6_0 /Fh cube.vert.h
    dxc cube.hlsl /E PSMain /T ps_6_0 /Fh cube.frag.h

    cat cube.vert.h | perl -w -p -e 's/BYTE/unsigned char/;s/g_VSMain/D3D12_CubeVert/;' > cube.vert.temp.h
    cat cube.frag.h | perl -w -p -e 's/BYTE/unsigned char/;s/g_PSMain/D3D12_CubeFrag/;' > cube.frag.temp.h
    cat cube.vert.temp.h cube.frag.temp.h > testgpu_dxil.h
    rm -f cube.vert.h cube.frag.h cube.vert.temp.h cube.frag.temp.h

fi
