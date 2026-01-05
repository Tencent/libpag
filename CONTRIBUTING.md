# Contributing to libpag
Welcome to [report Issues](https://github.com/Tencent/libpag/issues) or [pull requests](https://github.com/Tencent/libpag/pulls). It's recommended to read the following Contributing Guide first before contributing. 

## Issues
We use Github Issues to track public bugs and feature requests.

### Search Known Issues First
Please search the existing issues to see if any similar issue or feature request has already been filed. You should make sure your issue isn't redundant.

### Reporting New Issues
If you open an issue, the more information the better. Such as detailed description, screenshot or video of your problem, logcat and xlog or code blocks for your crash.

## Pull Requests
We strongly welcome your pull request to make libpag better. 

### Branch Management
There are two main branches here:

1. `main` branch.
	1. It is our active developing branch. After full testing, we will periodically pull the pre-release branch based on the main branch. 
    2. **You are recommended to submit bugfix or feature PR on `main` branch.**
2. `release/{version}` branch.  
	1. This is our stable release branch, which is fully tested and already used in many apps.

Normal bugfix or feature request should be submitted to `main` branch. 


### Make Pull Requests
The code team will monitor all pull request, we run some code check and test on it. After all tests passed, we will accecpt this PR. 

Before submitting a pull request, please make sure the followings are done:

1. Fork the repo and create your branch from `main`.
2. Update code or documentation if you have changed APIs.
3. Add the copyright notice to the top of any new files you've added.
4. Check your code lints and checkstyles.
5. Test and test again your code.
6. Now, you can submit your pull request on `main` branch.

## Code Style Guide
Use [Code Style for C/C++](http://zh-google-styleguide.readthedocs.io/en/latest/google-cpp-styleguide/).

It is recommended to configure the clang-format we provide in the IDE

* 2 spaces for indentation rather than tabs


## License
By contributing to libpag, you agree that your contributions will be licensed
under its [Apache License Version 2.0](./LICENSE.txt)
