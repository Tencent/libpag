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

#include "pagx/HTMLExporter.h"
#include <filesystem>
#include <fstream>
#include <string>
#include "pagx/html/HTMLBuilder.h"
#include "pagx/html/HTMLWriter.h"
#include "pagx/utils/StringParser.h"

namespace pagx {

//==============================================================================
// Framework Serializers
//==============================================================================

static std::string KebabToCamelCase(const std::string& kebab) {
  std::string result;
  result.reserve(kebab.size());
  bool capitalizeNext = false;
  bool isVendorPrefix = !kebab.empty() && kebab[0] == '-';

  for (size_t i = 0; i < kebab.size(); i++) {
    char c = kebab[i];
    if (c == '-') {
      capitalizeNext = true;
      continue;
    }
    if (capitalizeNext) {
      result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
      capitalizeNext = false;
    } else {
      result += c;
    }
  }

  if (isVendorPrefix && !result.empty()) {
    result[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(result[0])));
  }

  return result;
}

static bool IsNumericValue(const std::string& value) {
  if (value.empty()) {
    return false;
  }
  size_t i = 0;
  bool hasDot = false;
  bool hasDigit = false;

  if (value[i] == '-' || value[i] == '+') {
    i++;
  }

  while (i < value.size()) {
    char c = value[i];
    if (c >= '0' && c <= '9') {
      hasDigit = true;
      i++;
    } else if (c == '.' && !hasDot) {
      hasDot = true;
      i++;
    } else {
      return false;
    }
  }
  return hasDigit;
}

static std::string StyleStringToJSXObject(const std::string& styleStr) {
  if (styleStr.empty()) {
    return "{}";
  }

  std::string result = "{";
  size_t pos = 0;
  bool first = true;

  while (pos < styleStr.size()) {
    size_t semicolonPos = styleStr.find(';', pos);
    if (semicolonPos == std::string::npos) {
      semicolonPos = styleStr.size();
    }

    std::string pair = styleStr.substr(pos, semicolonPos - pos);
    pos = semicolonPos + 1;

    if (pair.empty()) {
      continue;
    }

    auto colonPos = pair.find(':');
    if (colonPos == std::string::npos) {
      continue;
    }

    std::string key = pair.substr(0, colonPos);
    std::string value = pair.substr(colonPos + 1);

    while (!key.empty() && key[0] == ' ') {
      key = key.substr(1);
    }
    while (!key.empty() && key.back() == ' ') {
      key.pop_back();
    }
    while (!value.empty() && value[0] == ' ') {
      value = value.substr(1);
    }
    while (!value.empty() && value.back() == ' ') {
      value.pop_back();
    }

    if (key.empty()) {
      continue;
    }

    if (!first) {
      result += ", ";
    }
    first = false;

    std::string camelKey = KebabToCamelCase(key);
    result += camelKey + ": ";

    if (IsNumericValue(value)) {
      result += value;
    } else {
      // Escape double quotes in value for JSX style object
      std::string escaped = value;
      for (size_t p = 0; p < escaped.size(); p++) {
        if (escaped[p] == '"') {
          escaped.replace(p, 1, "\\\"");
          p++;
        }
      }
      result += "\"" + escaped + "\"";
    }
  }

  result += "}";
  return result;
}

static std::string StyleStringToVueObject(const std::string& styleStr) {
  if (styleStr.empty()) {
    return "{}";
  }

  std::string result = "{";
  size_t pos = 0;
  bool first = true;

  while (pos < styleStr.size()) {
    size_t semicolonPos = styleStr.find(';', pos);
    if (semicolonPos == std::string::npos) {
      semicolonPos = styleStr.size();
    }

    std::string pair = styleStr.substr(pos, semicolonPos - pos);
    pos = semicolonPos + 1;

    if (pair.empty()) {
      continue;
    }

    auto colonPos = pair.find(':');
    if (colonPos == std::string::npos) {
      continue;
    }

    std::string key = pair.substr(0, colonPos);
    std::string value = pair.substr(colonPos + 1);

    while (!key.empty() && key[0] == ' ') {
      key = key.substr(1);
    }
    while (!key.empty() && key.back() == ' ') {
      key.pop_back();
    }
    while (!value.empty() && value[0] == ' ') {
      value = value.substr(1);
    }
    while (!value.empty() && value.back() == ' ') {
      value.pop_back();
    }

    if (key.empty()) {
      continue;
    }

    if (!first) {
      result += ", ";
    }
    first = false;

    std::string camelKey = KebabToCamelCase(key);
    result += camelKey + ": ";

    if (IsNumericValue(value)) {
      result += value;
    } else {
      // Escape single quotes in value for Vue style object
      std::string escaped = value;
      for (size_t p = 0; p < escaped.size(); p++) {
        if (escaped[p] == '\'') {
          escaped.replace(p, 1, "\\\'");
          p++;
        }
      }
      result += "'" + escaped + "'";
    }
  }

  result += "}";
  return result;
}

// Decode HTML entities back to raw characters before style parsing.
// This is necessary because escapeAttr() encodes quotes (&#39; &quot;) which
// contain semicolons that would be misinterpreted as CSS property separators.
static std::string DecodeHTMLEntities(const std::string& str) {
  std::string result;
  result.reserve(str.size());
  size_t i = 0;
  while (i < str.size()) {
    if (str[i] == '&') {
      if (str.compare(i, 5, "&#39;") == 0) {
        result += '\'';
        i += 5;
      } else if (str.compare(i, 6, "&quot;") == 0) {
        result += '"';
        i += 6;
      } else if (str.compare(i, 5, "&amp;") == 0) {
        result += '&';
        i += 5;
      } else if (str.compare(i, 4, "&lt;") == 0) {
        result += '<';
        i += 4;
      } else if (str.compare(i, 4, "&gt;") == 0) {
        result += '>';
        i += 4;
      } else {
        result += str[i++];
      }
    } else {
      result += str[i++];
    }
  }
  return result;
}

static std::string TransformStyleAttributes(const std::string& html, bool useDoubleQuotes) {
  std::string result;
  result.reserve(html.size() * 2);
  size_t pos = 0;

  while (pos < html.size()) {
    size_t stylePos = html.find("style=\"", pos);
    if (stylePos == std::string::npos) {
      result += html.substr(pos);
      break;
    }

    result += html.substr(pos, stylePos - pos);

    size_t valueStart = stylePos + 7;
    size_t valueEnd = html.find('"', valueStart);
    if (valueEnd == std::string::npos) {
      result += html.substr(stylePos);
      break;
    }

    std::string styleValue = DecodeHTMLEntities(html.substr(valueStart, valueEnd - valueStart));
    std::string styleObj =
        useDoubleQuotes ? StyleStringToJSXObject(styleValue) : StyleStringToVueObject(styleValue);

    if (useDoubleQuotes) {
      result += "style={" + styleObj + "}";
    } else {
      result += ":style=\"" + styleObj + "\"";
    }

    pos = valueEnd + 1;
  }

  return result;
}

static std::string ReplaceClassWithClassName(const std::string& html) {
  std::string result;
  result.reserve(html.size());
  size_t pos = 0;

  while (pos < html.size()) {
    size_t classPos = html.find("class=\"", pos);
    if (classPos == std::string::npos) {
      result += html.substr(pos);
      break;
    }

    if (classPos > 0) {
      char prevChar = html[classPos - 1];
      if (prevChar != ' ' && prevChar != '\n' && prevChar != '\t' && prevChar != '<') {
        result += html.substr(pos, classPos - pos + 6);
        pos = classPos + 6;
        continue;
      }
    }

    result += html.substr(pos, classPos - pos);
    result += "className=\"";
    pos = classPos + 7;
  }

  return result;
}

static std::string ReplaceHTMLComments(const std::string& html) {
  std::string result;
  result.reserve(html.size());
  size_t pos = 0;

  while (pos < html.size()) {
    size_t commentStart = html.find("<!--", pos);
    if (commentStart == std::string::npos) {
      result += html.substr(pos);
      break;
    }

    result += html.substr(pos, commentStart - pos);

    size_t commentEnd = html.find("-->", commentStart);
    if (commentEnd == std::string::npos) {
      result += html.substr(commentStart);
      break;
    }

    std::string commentContent = html.substr(commentStart + 4, commentEnd - commentStart - 4);
    result += "{/*" + commentContent + "*/}";
    pos = commentEnd + 3;
  }

  return result;
}

static std::string SerializeToReactJSX(const std::string& nativeHTML,
                                       const HTMLExportOptions& options) {
  std::string jsx = nativeHTML;

  jsx = ReplaceClassWithClassName(jsx);
  jsx = ReplaceHTMLComments(jsx);
  jsx = TransformStyleAttributes(jsx, true);

  std::string indentStr(static_cast<size_t>(options.indent * 2), ' ');
  std::string singleIndent(static_cast<size_t>(options.indent), ' ');

  std::string result;
  result.reserve(jsx.size() + 200);
  result += "export default function " + options.componentName + "() {\n";
  result += singleIndent + "return (\n";

  size_t lineStart = 0;
  while (lineStart < jsx.size()) {
    size_t lineEnd = jsx.find('\n', lineStart);
    if (lineEnd == std::string::npos) {
      lineEnd = jsx.size();
    }

    std::string line = jsx.substr(lineStart, lineEnd - lineStart);
    result += indentStr + line + "\n";
    lineStart = lineEnd + 1;
  }

  result += singleIndent + ");\n";
  result += "}\n";

  return result;
}

static std::string SerializeToVueSFC(const std::string& nativeHTML,
                                     const HTMLExportOptions& options) {
  std::string vue = TransformStyleAttributes(nativeHTML, false);

  std::string singleIndent(static_cast<size_t>(options.indent), ' ');

  std::string result;
  result.reserve(vue.size() + 200);
  result += "<template>\n";

  size_t lineStart = 0;
  while (lineStart < vue.size()) {
    size_t lineEnd = vue.find('\n', lineStart);
    if (lineEnd == std::string::npos) {
      lineEnd = vue.size();
    }

    std::string line = vue.substr(lineStart, lineEnd - lineStart);
    result += singleIndent + line + "\n";
    lineStart = lineEnd + 1;
  }

  result += "</template>\n\n";
  result += "<script setup>\n";
  result += "// Generated by PAGX HTMLExporter\n";
  result += "</script>\n";

  return result;
}

//==============================================================================
// Main export functions
//==============================================================================

static std::string GenerateDiamondGradientScript(
    const std::vector<DiamondGradientInfo>& gradients) {
  if (gradients.empty()) {
    return "";
  }
  // Self-contained WebGL2 diamond gradient renderer.
  // The precomputed unit matrix (combining translate, gradientMatrix, center offset,
  // scale(sqrt2/radius), and rotate(45)) is passed directly. The shader only needs
  // t = max(|transformed.x|, |transformed.y|) for the Chebyshev distance.
  std::string s;
  s += "<script>\n(function(){\n";
  s += "var VS='#version 300 es\\n";
  s += "in vec2 a;out vec2 v;uniform vec2 u_s;";
  s += "void main(){vec2 u=a*.5+.5;v=vec2(u.x*u_s.x,(1.-u.y)*u_s.y);gl_Position=vec4(a,0,1);}';\n";
  s += "var FS='#version 300 es\\nprecision highp float;";
  s += "in vec2 v;out vec4 o;uniform mat3 u_m;";
  s += "uniform int u_n;uniform float u_o[8];uniform vec4 u_cl[8];";
  s += "vec4 sg(float t){t=clamp(t,0.,1.);if(t<=u_o[0])return u_cl[0];";
  s += "for(int i=1;i<8;i++){if(i>=u_n)break;if(t<=u_o[i]){";
  s += "float f=(t-u_o[i-1])/(u_o[i]-u_o[i-1]);return mix(u_cl[i-1],u_cl[i],f);}}";
  s += "return u_cl[u_n-1];}";
  s += "void main(){vec2 p=(u_m*vec3(v,1)).xy;";
  s += "float t=max(abs(p.x),abs(p.y));o=sg(t);}';\n";
  s += "function initDG(id,m,st){\n";
  s += "var c=document.getElementById(id);if(!c)return;\n";
  s += "var g=c.getContext('webgl2');if(!g)return;\n";
  s += "function cs(t,s){var sh=g.createShader(t);g.shaderSource(sh,s);g.compileShader(sh);return "
       "sh;}\n";
  s += "var p=g.createProgram();\n";
  s += "g.attachShader(p,cs(g.VERTEX_SHADER,VS));g.attachShader(p,cs(g.FRAGMENT_SHADER,FS));\n";
  s += "g.linkProgram(p);g.useProgram(p);\n";
  s += "var b=g.createBuffer();g.bindBuffer(g.ARRAY_BUFFER,b);\n";
  s += "g.bufferData(g.ARRAY_BUFFER,new Float32Array([-1,-1,1,-1,-1,1,1,1]),g.STATIC_DRAW);\n";
  s += "var a=g.getAttribLocation(p,'a');g.enableVertexAttribArray(a);\n";
  s += "g.vertexAttribPointer(a,2,g.FLOAT,false,0,0);\n";
  s += "g.uniform2f(g.getUniformLocation(p,'u_s'),c.width,c.height);\n";
  s += "g.uniformMatrix3fv(g.getUniformLocation(p,'u_m'),false,new Float32Array(m));\n";
  s += "g.uniform1i(g.getUniformLocation(p,'u_n'),st.length);\n";
  s += "var of=new Float32Array(8),cl=new Float32Array(32);\n";
  s += "for(var "
       "i=0;i<st.length;i++){of[i]=st[i][0];cl[i*4]=st[i][1];cl[i*4+1]=st[i][2];cl[i*4+2]=st[i]["
       "3];cl[i*4+3]=st[i][4];}\n";
  s += "g.uniform1fv(g.getUniformLocation(p,'u_o'),of);\n";
  s += "g.uniform4fv(g.getUniformLocation(p,'u_cl'),cl);\n";
  s += "g.viewport(0,0,c.width,c.height);g.drawArrays(g.TRIANGLE_STRIP,0,4);}\n";

  for (auto& info : gradients) {
    s += "initDG('" + info.canvasId + "',[";
    for (int i = 0; i < 9; i++) {
      if (i > 0) {
        s += ",";
      }
      s += FloatToString(info.unitMatrix[i]);
    }
    s += "],[";
    for (size_t i = 0; i < info.stops.size(); i++) {
      if (i > 0) {
        s += ",";
      }
      auto& stop = info.stops[i];
      s += "[" + FloatToString(stop.first) + "," + FloatToString(stop.second.red) + "," +
           FloatToString(stop.second.green) + "," + FloatToString(stop.second.blue) + "," +
           FloatToString(stop.second.alpha) + "]";
    }
    s += "]);\n";
  }

  s += "})();\n</script>\n";
  return s;
}

static std::string GenerateImagePatternScript(const std::vector<ImagePatternCanvasInfo>& patterns) {
  if (patterns.empty()) {
    return "";
  }
  std::string s;
  s += "<script>\n(function(){\n";
  s += "var VS='#version 300 es\\n";
  s += "in vec2 a;out vec2 v;";
  s += "void main(){v=a*.5+.5;v.y=1.-v.y;gl_Position=vec4(a,0,1);}';\n";
  s += "var FS='#version 300 es\\nprecision highp float;";
  s += "in vec2 v;out vec4 o;uniform sampler2D u_tex;uniform vec2 u_scale;";
  s += "void main(){o=texture(u_tex,v*u_scale);}';\n";
  s += "function initIP(id,src,ws,wt,fl,cw,ch){\n";
  s += "var img=new Image();img.onload=function(){\n";
  s += "var c=document.getElementById(id);if(!c)return;\n";
  s += "var g=c.getContext('webgl2');if(!g)return;\n";
  s += "function cs(t,s){var sh=g.createShader(t);g.shaderSource(sh,s);g.compileShader(sh);return "
       "sh;}\n";
  s += "var p=g.createProgram();\n";
  s += "g.attachShader(p,cs(g.VERTEX_SHADER,VS));g.attachShader(p,cs(g.FRAGMENT_SHADER,FS));\n";
  s += "g.linkProgram(p);g.useProgram(p);\n";
  s += "var b=g.createBuffer();g.bindBuffer(g.ARRAY_BUFFER,b);\n";
  s += "g.bufferData(g.ARRAY_BUFFER,new Float32Array([-1,-1,1,-1,-1,1,1,1]),g.STATIC_DRAW);\n";
  s += "var a=g.getAttribLocation(p,'a');g.enableVertexAttribArray(a);\n";
  s += "g.vertexAttribPointer(a,2,g.FLOAT,false,0,0);\n";
  s += "var t=g.createTexture();g.bindTexture(g.TEXTURE_2D,t);\n";
  s += "g.texImage2D(g.TEXTURE_2D,0,g.RGBA,g.RGBA,g.UNSIGNED_BYTE,img);\n";
  s += "g.texParameteri(g.TEXTURE_2D,g.TEXTURE_WRAP_S,ws);\n";
  s += "g.texParameteri(g.TEXTURE_2D,g.TEXTURE_WRAP_T,wt);\n";
  s += "g.texParameteri(g.TEXTURE_2D,g.TEXTURE_MIN_FILTER,fl);\n";
  s += "g.texParameteri(g.TEXTURE_2D,g.TEXTURE_MAG_FILTER,fl);\n";
  s += "g.uniform2f(g.getUniformLocation(p,'u_scale'),cw/img.naturalWidth,ch/"
       "img.naturalHeight);\n";
  s += "g.viewport(0,0,c.width,c.height);g.drawArrays(g.TRIANGLE_STRIP,0,4);\n";
  s += "};img.src=src;}\n";

  for (auto& info : patterns) {
    s += "initIP('" + info.canvasId + "','" + info.imageSrc + "'," + std::to_string(info.wrapS) +
         "," + std::to_string(info.wrapT) + "," + std::to_string(info.filter) + "," +
         FloatToString(info.width) + "," + FloatToString(info.height) + ");\n";
  }

  s += "})();\n</script>\n";
  return s;
}

std::string HTMLExporter::ToHTML(const PAGXDocument& doc, const Options& options) {
  HTMLBuilder html(options.indent);
  HTMLBuilder defs(options.indent, 2);
  HTMLWriterContext ctx;
  ctx.docWidth = doc.width;
  ctx.docHeight = doc.height;
  HTMLWriter writer(&defs, &ctx);

  // Root div
  std::string rootStyle = "position:relative;width:" + FloatToString(doc.width) +
                          "px;height:" + FloatToString(doc.height) + "px;overflow:hidden";
  html.openTag("div");
  html.addAttr("class", "pagx-root");
  html.addAttr("data-pagx-version", doc.version);
  html.addAttr("style", rootStyle);
  html.closeTagStart();

  // Render layers into body content
  HTMLBuilder body(options.indent, 1);
  for (auto* layer : doc.layers) {
    writer.writeLayer(body, layer);
  }

  // Insert SVG defs if any
  std::string defsStr = defs.release();
  if (!defsStr.empty()) {
    html.openTag("svg");
    html.addAttr("style", "position:absolute;width:0;height:0;overflow:hidden");
    html.closeTagStart();
    html.openTag("defs");
    html.closeTagStart();
    html.addRawContent(defsStr);
    html.closeTag();  // </defs>
    html.closeTag();  // </svg>
  }

  html.addRawContent(body.release());

  // Inject WebGL2 diamond gradient script if needed
  std::string dgScript = GenerateDiamondGradientScript(ctx.diamondGradients);
  if (!dgScript.empty()) {
    html.addRawContent(dgScript);
  }

  // Inject WebGL2 image pattern script if needed
  std::string ipScript = GenerateImagePatternScript(ctx.imagePatternCanvases);
  if (!ipScript.empty()) {
    html.addRawContent(ipScript);
  }

  html.closeTag();  // </div>

  std::string nativeHTML = html.release();

  // Apply framework-specific transformation
  switch (options.framework) {
    case HTMLFramework::React:
      return SerializeToReactJSX(nativeHTML, options);
    case HTMLFramework::Vue:
      return SerializeToVueSFC(nativeHTML, options);
    case HTMLFramework::Native:
    default:
      return nativeHTML;
  }
}

bool HTMLExporter::ToFile(const PAGXDocument& document, const std::string& filePath,
                          const Options& options) {
  auto html = ToHTML(document, options);
  if (html.empty()) {
    return false;
  }
  auto dirPath = std::filesystem::path(filePath).parent_path();
  if (!dirPath.empty() && !std::filesystem::exists(dirPath)) {
    std::error_code ec = {};
    std::filesystem::create_directories(dirPath, ec);
    if (ec) {
      return false;
    }
  }
  std::ofstream file(filePath, std::ios::binary);
  if (!file) {
    return false;
  }
  file.write(html.data(), static_cast<std::streamsize>(html.size()));
  return file.good();
}

}  // namespace pagx
