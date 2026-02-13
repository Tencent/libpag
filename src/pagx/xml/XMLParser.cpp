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

#include "XMLParser.h"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include "expat.h"

namespace pagx {

template <typename T, T* P>
struct OverloadedFunctionObject {
  template <typename... Args>
  auto operator()(Args&&... args) const -> decltype(P(std::forward<Args>(args)...)) {
    return P(std::forward<Args>(args)...);
  }
};

template <auto F>
using FunctionObject = OverloadedFunctionObject<std::remove_pointer_t<decltype(F)>, F>;

template <typename T, void (*P)(T*)>
class AutoTCallVProc : public std::unique_ptr<T, FunctionObject<P>> {
  using inherited = std::unique_ptr<T, FunctionObject<P>>;

 public:
  using inherited::inherited;
  AutoTCallVProc(const AutoTCallVProc&) = delete;
  AutoTCallVProc(AutoTCallVProc&& that) noexcept : inherited(std::move(that)) {
  }

  operator T*() const {
    return this->get();
  }
};

static const XML_Memory_Handling_Suite XML_alloc = {malloc, realloc, free};

struct ParsingContext {
  explicit ParsingContext(XMLParser* parser);

  bool flushText();
  void discardBufferedWhitespace();
  void appendText(const char* txt, size_t len);
  bool startElement(const char* element);
  bool addAttribute(const char* name, const char* value);
  bool endElement(const char* element);

  AutoTCallVProc<std::remove_pointer_t<XML_Parser>, XML_ParserFree> _XMLParser = nullptr;

 private:
  XMLParser* _parser = nullptr;
  std::string _bufferedText = {};
};

ParsingContext::ParsingContext(XMLParser* parser)
    : _parser(parser), _XMLParser(XML_ParserCreate_MM(nullptr, &XML_alloc, nullptr)) {
}

bool ParsingContext::flushText() {
  if (!_bufferedText.empty()) {
    bool stop = _parser->text(_bufferedText);
    _bufferedText.clear();
    return stop;
  }
  return false;
}

void ParsingContext::discardBufferedWhitespace() {
  if (_bufferedText.find_first_not_of(" \n\r\t") == std::string::npos) {
    _bufferedText.clear();
  }
}

void ParsingContext::appendText(const char* txt, size_t len) {
  _bufferedText.insert(_bufferedText.end(), txt, &txt[len]);
}

bool ParsingContext::startElement(const char* element) {
  return _parser->startElement(element);
}

bool ParsingContext::addAttribute(const char* name, const char* value) {
  return _parser->addAttribute(name, value);
}

bool ParsingContext::endElement(const char* element) {
  return _parser->endElement(element);
}

namespace {

constexpr const void* HASH_SEED = &HASH_SEED;

#define HANDLER_CONTEXT(arg, name) ParsingContext* name = static_cast<ParsingContext*>(arg)

void XMLCALL start_element_handler(void* data, const char* tag, const char** attributes) {
  HANDLER_CONTEXT(data, context);
  if (context->flushText()) {
    XML_StopParser(context->_XMLParser, XML_FALSE);
    return;
  }

  if (context->startElement(tag)) {
    XML_StopParser(context->_XMLParser, XML_FALSE);
    return;
  }

  for (size_t i = 0; attributes[i]; i += 2) {
    if (context->addAttribute(attributes[i], attributes[i + 1])) {
      XML_StopParser(context->_XMLParser, XML_FALSE);
      return;
    }
  }
}

void XMLCALL end_element_handler(void* data, const char* tag) {
  HANDLER_CONTEXT(data, context);
  if (context->flushText()) {
    XML_StopParser(context->_XMLParser, XML_FALSE);
    return;
  }

  if (context->endElement(tag)) {
    XML_StopParser(context->_XMLParser, XML_FALSE);
  }
}

void XMLCALL text_handler(void* data, const char* txt, int len) {
  HANDLER_CONTEXT(data, context);
  context->appendText(txt, static_cast<size_t>(len));
}

void XMLCALL start_cdata_handler(void* data) {
  HANDLER_CONTEXT(data, context);
  // Discard any pure whitespace text buffered before the CDATA section (e.g. formatting
  // indentation between the element start tag and the CDATA section).
  context->discardBufferedWhitespace();
}

void XMLCALL end_cdata_handler(void* data) {
  HANDLER_CONTEXT(data, context);
  // Flush the CDATA content immediately so that any trailing whitespace after the CDATA section
  // does not get merged with it.
  if (context->flushText()) {
    XML_StopParser(context->_XMLParser, XML_FALSE);
  }
}

void XMLCALL entity_decl_handler(void* data, const XML_Char* /*entityName*/,
                                 int /*is_parameter_entity*/, const XML_Char* /*value*/,
                                 int /*value_length*/, const XML_Char* /*base*/,
                                 const XML_Char* /*systemId*/, const XML_Char* /*publicId*/,
                                 const XML_Char* /*notationName*/) {
  HANDLER_CONTEXT(data, context);
  // Disable entity processing to inhibit internal entity expansion. See expat CVE-2013-0340.
  XML_StopParser(context->_XMLParser, XML_FALSE);
}

}  // anonymous namespace

XMLParser::XMLParser() = default;
XMLParser::~XMLParser() = default;

bool XMLParser::parse(const uint8_t* data, size_t length) {
  if (data == nullptr || length == 0) {
    return false;
  }

  ParsingContext parsingContext(this);
  if (!parsingContext._XMLParser) {
    return false;
  }

  // Avoid calls to rand_s if this is not set. This seed helps prevent DOS
  // with a known hash sequence so an address is sufficient. The provided
  // seed should not be zero as that results in a call to rand_s.
  auto seed = static_cast<unsigned long>(reinterpret_cast<size_t>(HASH_SEED) & 0xFFFFFFFF);
  XML_SetHashSalt(parsingContext._XMLParser, seed ? seed : 1);

  XML_SetUserData(parsingContext._XMLParser, &parsingContext);
  XML_SetElementHandler(parsingContext._XMLParser, start_element_handler, end_element_handler);
  XML_SetCharacterDataHandler(parsingContext._XMLParser, text_handler);
  XML_SetCdataSectionHandler(parsingContext._XMLParser, start_cdata_handler, end_cdata_handler);
  XML_SetEntityDeclHandler(parsingContext._XMLParser, entity_decl_handler);

  if (length > static_cast<size_t>(std::numeric_limits<int>::max())) {
    return false;
  }

  auto status =
      XML_Parse(parsingContext._XMLParser, reinterpret_cast<const char*>(data),
                static_cast<int>(length), true);

  return XML_STATUS_ERROR != status;
}

bool XMLParser::parseFile(const std::string& filePath) {
  FILE* file = fopen(filePath.c_str(), "rb");
  if (!file) {
    return false;
  }

  fseek(file, 0, SEEK_END);
  long fileSize = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (fileSize <= 0) {
    fclose(file);
    return false;
  }

  std::string content;
  content.resize(static_cast<size_t>(fileSize));
  size_t bytesRead = fread(&content[0], 1, static_cast<size_t>(fileSize), file);
  fclose(file);

  if (bytesRead != static_cast<size_t>(fileSize)) {
    return false;
  }

  return parse(reinterpret_cast<const uint8_t*>(content.data()), content.size());
}

bool XMLParser::startElement(const char* element) {
  return this->onStartElement(element);
}

bool XMLParser::addAttribute(const char* name, const char* value) {
  return this->onAddAttribute(name, value);
}

bool XMLParser::endElement(const char* element) {
  return this->onEndElement(element);
}

bool XMLParser::text(const std::string& text) {
  return this->onText(text);
}

}  // namespace pagx
