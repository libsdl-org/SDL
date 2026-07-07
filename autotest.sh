rm -rf build
mkdir -p build && cd build

cmake .. -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DSDL_TESTS=ON \
    -DSDL_ARMSVE2=ON  # 你的SVE2开关，根据实际情况调整

ninja

cd build/test
./testautomation

cd ../..