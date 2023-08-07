set -e

cd ../src
make clean
make host/buildvm
#make libluajit.a CC="clang" CFLAGS="-fsanitize=fuzzer-no-link,address,undefined -DLUAI_ASSERT=1 -DLUA_USE_APICHECK=1 -g -O2 -DLIBC_FATAL_STDERR_=1 -DMALLOC_CHECK_=3" LDFLAGS="-fsanitize=fuzzer-no-link,address,undefined"
make libluajit.a CC="clang" CFLAGS="-fsanitize=fuzzer-no-link,address -DLUAI_ASSERT=1 -DLUA_USE_APICHECK=1 -g -O2 -DLIBC_FATAL_STDERR_=1 -DMALLOC_CHECK_=3 -DLUAJIT_USE_SYSMALLOC=1" LDFLAGS="-fsanitize=fuzzer-no-link,address"
cd -

clang -fsanitize=fuzzer,address,undefined -g -O2 -DLIBC_FATAL_STDERR_=1 -DMALLOC_CHECK_=3 -I../src/ fuzz_loader.c ../src/libluajit.a -o fuzz_loader
