# libpag for Linux

## environment dependency
   ### nodejs
    1) download
    wget https://nodejs.org/dist/v17.3.1/node-v17.3.1-linux-x64.tar.xz
    sudo mkdir -p /usr/local/lib/nodejs
    sudo tar -xJvf node-v17.3.1-linux-x64.tar.xz -C /usr/local/lib/nodejs 
    
    2) Set the environment variable ~/.profile, add below to the end:
    export PATH=/usr/local/lib/nodejs/node-v17.3.1-linux-x64/bin:$PATH
    source ~/.profile
    
    3) Test installation using
     $ node -v
     $ npm version
     $ npx -v
     
  ### depsync
     npm install depsync@1.2.5 -g   
     
  ### ninja
     yum install re2c
     git clone git://github.com/ninja-build/ninja.git 
     cd ninja
     ./configure.py --bootstrap
     cp ninja /usr/bin/      
     
  ### X11, swiftshader depends on some header files
     yum install libX11-devel --nogpg
     
## compile libpag  
    Execute in the root directory
       ./build_linux.sh 
    You will get the library files in the vendor/pag directory

## Linux demo
    The library files that the sample project depends on come from the previous step.
     
  #test
 