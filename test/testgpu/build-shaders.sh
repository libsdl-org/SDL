# Rebuilds the shaders needed for the GPU cube test.
# Requires glslangValidator and spirv-cross, which can be obtained from the LunarG Vulkan SDK.

# On Windows, run this via Git Bash.

export MSYS_NO_PATHCONV=1

# SPIR-V
glslangValidator cube.vert -V -o cube.vert.spv --quiet
glslangValidator cube.frag -V -o cube.frag.spv --quiet
xxd -i cube.vert.spv | perl -w -p -e 's/\Aunsigned /const unsigned /;' > cube_vert.h
xxd -i cube.frag.spv | perl -w -p -e 's/\Aunsigned /const unsigned /;' > cube_frag.h
cat cube_vert.h cube_frag.h > testgpu_spirv.h

# Platform-specific compilation
if [[ "$OSTYPE" == "darwin"* ]]; then

    # MSL
    spirv-cross cube.vert.spv --msl --output cube.vert.metal
    spirv-cross cube.frag.spv --msl --output cube.frag.metal

    # Xcode
    generate_shaders()
    {
        fileplatform=$1
        compileplatform=$2
        sdkplatform=$3
        minversion=$4

        xcrun -sdk $sdkplatform metal -c -std=$compileplatform-metal1.1 -m$sdkplatform-version-min=$minversion -Wall -O3 -o ./cube.vert.air ./cube.vert.metal || exit $?
        xcrun -sdk $sdkplatform metal -c -std=$compileplatform-metal1.1 -m$sdkplatform-version-min=$minversion -Wall -O3 -o ./cube.frag.air ./cube.frag.metal || exit $?

        xcrun -sdk $sdkplatform metal-ar rc cube.vert.metalar cube.vert.air || exit $?
        xcrun -sdk $sdkplatform metal-ar rc cube.frag.metalar cube.frag.air || exit $?

        xcrun -sdk $sdkplatform metallib -o cube.vert.metallib cube.vert.metalar || exit $?
        xcrun -sdk $sdkplatform metallib -o cube.frag.metallib cube.frag.metalar || exit $?

        xxd -i cube.vert.metallib | perl -w -p -e 's/\Aunsigned /const unsigned /;' >./cube.vert_$fileplatform.h
        xxd -i cube.frag.metallib | perl -w -p -e 's/\Aunsigned /const unsigned /;' >./cube.frag_$fileplatform.h

        rm -f cube.vert.air cube.vert.metalar cube.vert.metallib
        rm -f cube.frag.air cube.frag.metalar cube.frag.metallib
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
    rm -f cube.vert.metal cube.frag.metal
    rm -f cube.vert_macos.h cube.frag_macos.h
    rm -f cube.vert_iphonesimulator.h cube.frag_iphonesimulator.h
    rm -f cube.vert_tvsimulator.h cube.frag_tvsimulator.h
    rm -f cube.vert_ios.h cube.frag_ios.h
    rm -f cube.vert_tvos.h cube.frag_tvos.h

elif [[ "$OSTYPE" == "cygwin"* ]] || [[ "$OSTYPE" == "msys"* ]]; then

    # HLSL
    spirv-cross cube.vert.spv --hlsl --shader-model 50 --output cube.vert.hlsl
    spirv-cross cube.frag.spv --hlsl --shader-model 50 --output cube.frag.hlsl

    # FXC
    # Assumes fxc is in the path.
    # If not, you can run `export PATH=$PATH:/c/Program\ Files\ \(x86\)/Windows\ Kits/10/bin/x.x.x.x/x64/`
    fxc cube.vert.hlsl /T vs_5_0 /Fh cube.vert.h
    fxc cube.frag.hlsl /T ps_5_0 /Fh cube.frag.h

    cat cube.vert.h | perl -w -p -e 's/BYTE/unsigned char/;s/main/vert_main/;' > cube_vert.h
    cat cube.frag.h | perl -w -p -e 's/BYTE/unsigned char/;s/main/frag_main/;' > cube_frag.h
    cat cube_vert.h cube_frag.h > testgpu_dxbc.h

fi

# cleanup
rm -f cube.vert.spv cube.frag.spv
rm -f cube.vert.h cube.frag.h
rm -f cube_vert.h cube_frag.h
rm -f cube.vert.hlsl cube.frag.hlsl