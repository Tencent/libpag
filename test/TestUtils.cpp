/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2021 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  unless required by applicable law or agreed to in writing, software distributed under the
//  license is distributed on an "as is" basis, without warranties or conditions of any kind,
//  either express or implied. see the license for the specific language governing permissions
//  and limitations under the license.
//
/////////////////////////////////////////////////////////////////////////////////////////////////

#include "TestUtils.h"
#include <dirent.h>
#include <stdio.h>

void GetAllPAGFiles(std::string path, std::vector<std::string>& files) {
    struct dirent* dirp;
    DIR* dir = opendir(path.c_str());
    std::string p;

    while ((dirp = readdir(dir)) != nullptr) {
        if (dirp->d_type == DT_REG) {
            std::string str(dirp->d_name);
            std::string::size_type idx = str.find(".pag");
            if (idx != std::string::npos) {
                files.push_back(p.assign(path).append("/").append(dirp->d_name));
            }
        }
    }

    closedir(dir);
}