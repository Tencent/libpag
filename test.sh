# Immediately clear screen and set up status line to override command display
echo "Starting test execution..."

for i in {1..100}
do
    echo "Run test ${i}..."
    rm -fr /Users/markffan/workspace/opensource/libpag/build
    mkdir /Users/markffan/workspace/opensource/libpag/build
    cd /Users/markffan/workspace/opensource/libpag/build
    /Applications/CLion.app/Contents/bin/cmake/mac/aarch64/bin/cmake -DCMAKE_BUILD_TYPE=Debug -DPAG_BUILD_TESTS=ON -DCMAKE_MAKE_PROGRAM=/Applications/CLion.app/Contents/bin/ninja/mac/aarch64/ninja -G Ninja -S /Users/markffan/workspace/opensource/libpag -B /Users/markffan/workspace/opensource/libpag/build
    cmake --build /Users/markffan/workspace/opensource/libpag/build --target PAGFullTest -j 8
    ./PAGFullTest
    test_result=$?
    # Restore status line after test execution
    if [ $test_result -eq 0 ]; then
        echo "Test ${i} passed"
    else
        echo "Test ${i} failed"
        exit 1
    fi
    cd /Users/markffan/workspace/opensource/libpag
done
