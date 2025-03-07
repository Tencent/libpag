/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2025 THL A29 Limited, a Tencent company. All rights reserved.
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
#include "AEPReader.h"
#include <iostream>

namespace aep {
std::string ReadKeyName(ByteArray* bytes) {
  char name[5];
  name[0] = bytes->readInt8();
  name[1] = bytes->readInt8();
  name[2] = bytes->readInt8();
  name[3] = bytes->readInt8();
  name[4] = '\0';
  return std::string(name);
}

Tag ReadTag(ByteArray* bytes) {
  Tag tag = {};
  tag.name = ReadKeyName(bytes);
  auto length = bytes->readUint32();
  tag.bytes = bytes->readBytes(length);
  if (bytes->position() % 2 == 1) {
    bytes->skip(1);
  }
  return tag;
}

ByteArray ReadBody(ByteArray* bytes) {
  auto tag = ReadTag(bytes);
  tag.bytes.skip(4); // "Egg!"
  return tag.bytes.readBytes(tag.bytes.bytesAvailable());
}

Tag ReadFirstTagByName(ByteArray* bytes, const std::string& tagName) {
  return ReadFirstTagByNames(bytes, {tagName});
}

Tag ReadFirstTagByNames(ByteArray* bytes, const std::vector<std::string>& tagNames) {
  while (bytes->bytesAvailable() > 0) {
    auto tag = ReadTag(bytes);
    if (tag.name == "LIST") {
      auto listName = ReadKeyName(&tag.bytes);
      for (auto& tagName : tagNames) {
        if (listName == tagName) {
          tag.name = listName;
          tag.bytes = tag.bytes.readBytes(tag.bytes.bytesAvailable());
          return tag;
        }
      }

      bytes->setPosition(bytes->position() - tag.bytes.bytesAvailable());
      tag = ReadFirstTagByNames(bytes, tagNames);
      if (tag.bytes.length() > 0) {
        return tag;
      }
    }
    for (auto& tagName : tagNames) {
      if (tag.name == tagName) {
        return tag;
      }
    }
  }
  return {};
}

Tag ReadFirstGroupByMatchName(ByteArray* bytes, const std::string& matchName) {
  return ReadFirstGroupByMatchNames(bytes, {matchName});
}

Tag ReadFirstGroupByMatchNames(ByteArray* bytes, const std::vector<std::string>& matchNames) {
  while (bytes->bytesAvailable() > 0) {
    auto tag = ReadTag(bytes);
    if (tag.name == "tdmn") {
      auto key = tag.bytes.readUTF8String();
      for (auto& matchName : matchNames) {
        if (key == matchName) {
          tag = ReadTag(bytes);
          if (tag.name == "LIST") {
            auto listName = ReadKeyName(&tag.bytes);
            tag.name = listName;
            tag.bytes = tag.bytes.readBytes(tag.bytes.bytesAvailable());
            return tag;
          }
          break;
        }
      }
    }
    if (tag.name == "LIST") {
      auto listName = ReadKeyName(&tag.bytes);
      if (listName == "btdk") {
        continue;
      }
      bytes->setPosition(bytes->position() - tag.bytes.bytesAvailable());
      tag = ReadFirstGroupByMatchNames(bytes, matchNames);
      if (tag.bytes.length() > 0) {
        return tag;
      }
    }
  }
  return {};
}


static void ReadCompositions(ByteArray* bytes, std::vector<Composition>* list) {
  while (bytes->bytesAvailable() > 0) {
    auto item = ReadFirstTagByName(bytes, "Item");
    if (item.bytes.empty()) {
      break;
    }
    auto tag = ReadTag(&item.bytes);
    while (tag.name != "idta" && tag.bytes.bytesAvailable()) {
      tag = ReadTag(&item.bytes);
    }
    if (tag.name != "idta") {
      continue;
    }
    auto itemType = tag.bytes.readUint16();
    tag.bytes.skip(14);
    auto itemId = tag.bytes.readInt32();
    tag = ReadTag(&item.bytes);
    auto itemName = tag.bytes.readUTF8String();
    if (itemType == 1) { // Folder
      auto folder = ReadFirstTagByName(&item.bytes, "Sfdr");
      if (folder.bytes.empty()) {
        continue;
      }
      ReadCompositions(&folder.bytes, list);
    } else if (itemType == 4) { // Composition
      auto compositionBytes = item.bytes.readBytes(item.bytes.bytesAvailable());
      Composition composition = {itemName, itemId, compositionBytes};
      list->push_back(composition);
    }
  }
}

std::vector<Composition> ReadCompositions(ByteArray* bytes) {
  std::vector<Composition> list;
  auto folder = ReadFirstTagByName(bytes, "Fold");
  ReadCompositions(&folder.bytes, &list);
  return list;
}

std::vector<Layer> ReadLayers(ByteArray* bytes) {
  std::vector<Layer> list;
  while (bytes->bytesAvailable() > 0) {
    auto layerTag = ReadFirstTagByName(bytes, "Layr");
    if (layerTag.bytes.empty()) {
      break;
    }
    auto tag = ReadTag(&layerTag.bytes);
    auto layerID = tag.bytes.readInt32();
    tag.bytes.skip(34);
    auto layerFlags = tag.bytes.readUint16();
    tag.bytes.skip(88);
    auto layerType = tag.bytes.readInt32();
    tag = ReadTag(&layerTag.bytes);
    auto layerName = tag.bytes.readUTF8String();
    auto layerBytes = layerTag.bytes.readBytes(layerTag.bytes.bytesAvailable());
    Layer layer = {layerID, layerFlags, layerName, layerType, layerBytes};
    list.push_back(layer);
  }
  return list;
}

static void PrintAEP(ByteArray* bytes) {
  auto compositions = ReadCompositions(bytes);
  for (auto& composition : compositions) {
    std::cout << "[Composition] id: " << composition.id << " name: " << composition.name << std::endl;
    auto layers = ReadLayers(&composition.bytes);
    for (auto& layer : layers) {
      std::cout << "[Layer] id: " << layer.id << " name: " << layer.name <<
                " flags: " << layer.flags << " type: " << layer.type << std::endl;
    }
  }
}

void PrintTags(ByteArray* bytes, int indent) {
  std::string indentText;
  for (int i = 0; i < indent; i++) {
    indentText += " ";
  }
  while (bytes->bytesAvailable() > 0) {
    auto tag = ReadTag(bytes);
    if (tag.name == "LIST") {
      auto listName = ReadKeyName(&tag.bytes);
      if (listName == "btdk") {
        auto textDocument = tag.bytes.readUTF8String();
        std::cout << indentText << "[" << listName << "] : "
                  << textDocument << std::endl;
      } else {
        std::cout << indentText << "[" << listName << "] : "
                  << tag.bytes.length() - 4 << std::endl;
        PrintTags(&tag.bytes, indent + 1);
      }
    } else if (tag.name == "tdmn" || tag.name == "Utf8") {
      auto matchName = tag.bytes.readUTF8String();
      std::cout << indentText << "[" << tag.name << "] : " << matchName << std::endl;
    } else {
      std::cout << indentText << "[" << tag.name << "] : " << tag.bytes.length() << std::endl;
    }
  }
}

void Print(ByteArray* fileBytes) {
  auto bytes = ReadBody(fileBytes);
  PrintAEP(&bytes);
}
}
