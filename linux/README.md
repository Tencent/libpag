English | [简体中文](./README.zh_CN.md)

# Linux Demo

## Build Prerequisites

Please refer to [README.md](../README.md)

## Install NodeJS

1) download

```
wget https://nodejs.org/dist/v17.3.1/node-v17.3.1-linux-x64.tar.xz
sudo mkdir -p /usr/local/lib/nodejs
sudo tar -xJvf node-v17.3.1-linux-x64.tar.xz -C /usr/local/lib/nodejs
```

2) Set the environment variable

Go to ~/.profile, add the following line to the end:

```
export PATH=/usr/local/lib/nodejs/node-v17.3.1-linux-x64/bin:$PATH
```

3) Test installation

```
$ node -v
$ npm version
$ npx -v
```

## Install Ninja

```
yum install re2c
git clone git://github.com/ninja-build/ninja.git
cd ninja
./configure.py --bootstrap
cp ninja /usr/bin/
```

## Install X11

swiftshader depends on some header files.

```
yum install libX11-devel --nogpg
```

## Build PAG

Execute the following script in the linux/ directory:

```
./build_pag.sh 
```

You will get the library files in the vendor/libpag directory

## Build Demo

Execute the following commands in the linux/ directory:

```
cmake -B ./build -DCMAKE_BUILD_TYPE=Release
cmake --build ./build -- -j 12
```

You will get the demo executable file in the build directory.
     
  
 
