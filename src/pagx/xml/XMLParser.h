/////////////////////////////////////////////////////////////////////////////////////////////////
//
//  Tencent is pleased to support the open source community by making libpag available.
//
//  Copyright (C) 2026 Tencent. All rights reserved.
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

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace pagx {

/**
 * SAX-style XML parser based on expat.
 * Subclasses override the callback methods to handle parsing events.
 */
class XMLParser {
 public:
  XMLParser();
  virtual ~XMLParser();

  /**
   * Parses XML data from a memory buffer.
   * @param data Pointer to XML data.
   * @param length Length of the data in bytes.
   * @return true if parsing is successful, false otherwise.
   */
  bool parse(const uint8_t* data, size_t length);

  /**
   * Parses XML data from a file.
   * @param filePath Path to the XML file.
   * @return true if parsing is successful, false otherwise.
   */
  bool parseFile(const std::string& filePath);

 protected:
  /**
   * Called when an element start tag is encountered.
   * Override in subclasses to handle element start events.
   * @param element The element name.
   * @return true to stop parsing, false to continue.
   */
  virtual bool onStartElement(const std::string& element) = 0;

  /**
   * Called for each attribute of the current element.
   * Override in subclasses to handle attributes.
   * @param name The attribute name.
   * @param value The attribute value.
   * @return true to stop parsing, false to continue.
   */
  virtual bool onAddAttribute(const std::string& name, const std::string& value) = 0;

  /**
   * Called when an element end tag is encountered.
   * Override in subclasses to handle element end events.
   * @param element The element name.
   * @return true to stop parsing, false to continue.
   */
  virtual bool onEndElement(const std::string& element) = 0;

  /**
   * Called when text content is encountered.
   * Override in subclasses to handle text content.
   * @param text The text content.
   * @return true to stop parsing, false to continue.
   */
  virtual bool onText(const std::string& text) = 0;

 public:
  // Public for internal parser library calls, not intended for client use.
  bool startElement(const std::string& element);
  bool addAttribute(const std::string& name, const std::string& value);
  bool endElement(const std::string& element);
  bool text(const std::string& text);
};

}  // namespace pagx
