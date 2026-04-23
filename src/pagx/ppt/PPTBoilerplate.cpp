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

#include "pagx/ppt/PPTBoilerplate.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include "pagx/ppt/PPTGeomEmitter.h"
#include "pagx/ppt/PPTWriterContext.h"

namespace pagx {

// OOXML ST_SlideSizeCoordinate range: 914400 EMU (1") to 51206400 EMU (56").
static constexpr int64_t MIN_SLIDE_SIZE_EMU = 914400;
static constexpr int64_t MAX_SLIDE_SIZE_EMU = 51206400;

// Standard OOXML part prelude used at the top of every generated XML part.
static constexpr const char* XML_DECL =
    "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>";

// Common xmlns attributes reused across slide / slideMaster / slideLayout / viewProps parts.
static constexpr const char* NS_DRAWINGML =
    " xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\"";
static constexpr const char* NS_OFFICE_REL =
    " xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\"";
static constexpr const char* NS_PRESENTATIONML =
    " xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\"";

// Root element of every .rels part.
static constexpr const char* RELATIONSHIPS_OPEN =
    "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">";

std::string GenerateContentTypes(const PPTWriterContext& ctx) {
  std::string s;
  s.reserve(2048);
  s += XML_DECL;
  s += "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">";
  s += "<Default Extension=\"xml\" ContentType=\"application/xml\"/>";
  s += "<Default Extension=\"rels\" "
       "ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>";
  if (ctx.hasPNG()) {
    s += "<Default Extension=\"png\" ContentType=\"image/png\"/>";
  }
  if (ctx.hasJPEG()) {
    s += "<Default Extension=\"jpeg\" ContentType=\"image/jpeg\"/>";
  }
  s += "<Override PartName=\"/ppt/presentation.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.presentation."
       "main+xml\"/>";
  s += "<Override PartName=\"/ppt/slides/slide1.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.slide+xml\"/>";
  s += "<Override PartName=\"/ppt/slideMasters/slideMaster1.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.slideMaster+"
       "xml\"/>";
  s += "<Override PartName=\"/ppt/presProps.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.presProps+xml\"/"
       ">";
  s += "<Override PartName=\"/ppt/viewProps.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.viewProps+xml\"/"
       ">";
  s += "<Override PartName=\"/ppt/theme/theme1.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.theme+xml\"/>";
  s += "<Override PartName=\"/ppt/tableStyles.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.tableStyles+"
       "xml\"/>";
  s += "<Override PartName=\"/ppt/slideLayouts/slideLayout1.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.presentationml.slideLayout+"
       "xml\"/>";
  s += "<Override PartName=\"/docProps/core.xml\" "
       "ContentType=\"application/vnd.openxmlformats-package.core-properties+xml\"/>";
  s += "<Override PartName=\"/docProps/app.xml\" "
       "ContentType=\"application/vnd.openxmlformats-officedocument.extended-properties+xml\"/>";
  s += "</Types>";
  return s;
}

std::string GenerateRootRels() {
  std::string s;
  s.reserve(512);
  s += XML_DECL;
  s += RELATIONSHIPS_OPEN;
  s += "<Relationship Id=\"rId1\" "
       "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\""
       " Target=\"ppt/presentation.xml\"/>";
  s += "<Relationship Id=\"rId2\" "
       "Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/"
       "core-properties\" Target=\"docProps/core.xml\"/>";
  s += "<Relationship Id=\"rId3\" "
       "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
       "extended-properties\" Target=\"docProps/app.xml\"/>";
  s += "</Relationships>";
  return s;
}

std::string GeneratePresentation(float w, float h) {
  int64_t cx = PxToEMU(w);
  int64_t cy = PxToEMU(h);
  if (cx > MAX_SLIDE_SIZE_EMU || cy > MAX_SLIDE_SIZE_EMU) {
    double scale = std::min(static_cast<double>(MAX_SLIDE_SIZE_EMU) / static_cast<double>(cx),
                            static_cast<double>(MAX_SLIDE_SIZE_EMU) / static_cast<double>(cy));
    cx = static_cast<int64_t>(std::round(static_cast<double>(cx) * scale));
    cy = static_cast<int64_t>(std::round(static_cast<double>(cy) * scale));
  }
  if (cx < MIN_SLIDE_SIZE_EMU || cy < MIN_SLIDE_SIZE_EMU) {
    double scale = std::max(static_cast<double>(MIN_SLIDE_SIZE_EMU) / static_cast<double>(cx),
                            static_cast<double>(MIN_SLIDE_SIZE_EMU) / static_cast<double>(cy));
    cx = static_cast<int64_t>(std::round(static_cast<double>(cx) * scale));
    cy = static_cast<int64_t>(std::round(static_cast<double>(cy) * scale));
  }
  std::string s;
  s.reserve(512);
  s += XML_DECL;
  s += "<p:presentation";
  s += NS_DRAWINGML;
  s += NS_OFFICE_REL;
  s += NS_PRESENTATIONML;
  s += ">";
  s += "<p:sldMasterIdLst><p:sldMasterId id=\"2147483648\" r:id=\"rId1\"/></p:sldMasterIdLst>";
  s += "<p:sldIdLst><p:sldId id=\"256\" r:id=\"rId2\"/></p:sldIdLst>";
  s += "<p:sldSz cx=\"" + std::to_string(cx) + "\" cy=\"" + std::to_string(cy) +
       "\" type=\"custom\"/>";
  s += "<p:notesSz cx=\"" + std::to_string(cx) + "\" cy=\"" + std::to_string(cy) + "\"/>";
  s += "<p:defaultTextStyle>"
       "<a:defPPr><a:defRPr lang=\"en-US\"/></a:defPPr>";
  for (int lvl = 1; lvl <= 9; lvl++) {
    auto lvlStr = std::to_string(lvl);
    auto marL = std::to_string((lvl - 1) * 457200);
    s += "<a:lvl" + lvlStr + "pPr marL=\"" + marL +
         "\" algn=\"l\" defTabSz=\"914400\" rtl=\"0\" eaLnBrk=\"1\" "
         "latinLnBrk=\"0\" hangingPunct=\"1\">"
         "<a:defRPr sz=\"1800\" kern=\"1200\">"
         "<a:solidFill><a:schemeClr val=\"tx1\"/></a:solidFill>"
         "<a:latin typeface=\"+mn-lt\"/><a:ea typeface=\"+mn-ea\"/>"
         "<a:cs typeface=\"+mn-cs\"/>"
         "</a:defRPr></a:lvl" +
         lvlStr + "pPr>";
  }
  s += "</p:defaultTextStyle>";
  s += "</p:presentation>";
  return s;
}

std::string GeneratePresentationRels() {
  std::string s;
  s.reserve(1024);
  s += XML_DECL;
  s += RELATIONSHIPS_OPEN;
  s += "<Relationship Id=\"rId1\" "
       "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster\" "
       "Target=\"slideMasters/slideMaster1.xml\"/>";
  s += "<Relationship Id=\"rId2\" "
       "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide\" "
       "Target=\"slides/slide1.xml\"/>";
  s += "<Relationship Id=\"rId3\" "
       "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/presProps\" "
       "Target=\"presProps.xml\"/>";
  s += "<Relationship Id=\"rId4\" "
       "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/viewProps\" "
       "Target=\"viewProps.xml\"/>";
  s += "<Relationship Id=\"rId5\" "
       "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme\" "
       "Target=\"theme/theme1.xml\"/>";
  s += "<Relationship Id=\"rId6\" "
       "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/tableStyles\" "
       "Target=\"tableStyles.xml\"/>";
  s += "</Relationships>";
  return s;
}

std::string GenerateSlideRels(const PPTWriterContext& ctx) {
  std::string s;
  s.reserve(512);
  s += XML_DECL;
  s += RELATIONSHIPS_OPEN;
  s += "<Relationship Id=\"rId1\" "
       "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout\" "
       "Target=\"../slideLayouts/slideLayout1.xml\"/>";
  for (const auto& img : ctx.images()) {
    s += "<Relationship Id=\"" + img.relId +
         "\" "
         "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/image\" "
         "Target=\"../" +
         img.mediaPath.substr(4) + "\"/>";
  }
  s += "</Relationships>";
  return s;
}

std::string GenerateSlideMaster() {
  std::string s;
  s.reserve(2048);
  s += XML_DECL;
  s += "<p:sldMaster";
  s += NS_DRAWINGML;
  s += NS_OFFICE_REL;
  s += NS_PRESENTATIONML;
  s += ">";
  s += "<p:cSld><p:bg><p:bgPr>"
       "<a:solidFill><a:schemeClr val=\"bg1\"/></a:solidFill>"
       "<a:effectLst/></p:bgPr></p:bg>"
       "<p:spTree><p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/>"
       "<p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>"
       "<p:grpSpPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"0\" cy=\"0\"/>"
       "<a:chOff x=\"0\" y=\"0\"/><a:chExt cx=\"0\" cy=\"0\"/></a:xfrm></p:grpSpPr>"
       "</p:spTree>"
       "</p:cSld>"
       "<p:clrMap bg1=\"lt1\" tx1=\"dk1\" bg2=\"lt2\" tx2=\"dk2\" "
       "accent1=\"accent1\" accent2=\"accent2\" accent3=\"accent3\" "
       "accent4=\"accent4\" accent5=\"accent5\" accent6=\"accent6\" "
       "hlink=\"hlink\" folHlink=\"folHlink\"/>"
       "<p:sldLayoutIdLst>"
       "<p:sldLayoutId id=\"2147483649\" r:id=\"rId1\"/>"
       "</p:sldLayoutIdLst>"
       "<p:txStyles>"
       "<p:titleStyle>"
       "<a:lvl1pPr algn=\"l\" defTabSz=\"914400\" rtl=\"0\" eaLnBrk=\"1\" "
       "latinLnBrk=\"0\" hangingPunct=\"1\">"
       "<a:lnSpc><a:spcPct val=\"90000\"/></a:lnSpc>"
       "<a:spcBef><a:spcPct val=\"0\"/></a:spcBef>"
       "<a:buNone/>"
       "<a:defRPr sz=\"4400\" kern=\"1200\">"
       "<a:solidFill><a:schemeClr val=\"tx1\"/></a:solidFill>"
       "<a:latin typeface=\"+mj-lt\"/><a:ea typeface=\"+mj-ea\"/>"
       "<a:cs typeface=\"+mj-cs\"/>"
       "</a:defRPr></a:lvl1pPr>"
       "</p:titleStyle>"
       "<p:bodyStyle>"
       "<a:lvl1pPr marL=\"228600\" indent=\"-228600\" algn=\"l\" defTabSz=\"914400\" "
       "rtl=\"0\" eaLnBrk=\"1\" latinLnBrk=\"0\" hangingPunct=\"1\">"
       "<a:lnSpc><a:spcPct val=\"90000\"/></a:lnSpc>"
       "<a:spcBef><a:spcPts val=\"1000\"/></a:spcBef>"
       "<a:buFont typeface=\"Arial\" panose=\"020B0604020202020204\" pitchFamily=\"34\" "
       "charset=\"0\"/>"
       "<a:buChar char=\"&#x2022;\"/>"
       "<a:defRPr sz=\"2800\" kern=\"1200\">"
       "<a:solidFill><a:schemeClr val=\"tx1\"/></a:solidFill>"
       "<a:latin typeface=\"+mn-lt\"/><a:ea typeface=\"+mn-ea\"/>"
       "<a:cs typeface=\"+mn-cs\"/>"
       "</a:defRPr></a:lvl1pPr>"
       "</p:bodyStyle>"
       "<p:otherStyle>"
       "<a:defPPr>"
       "<a:defRPr lang=\"en-US\"/>"
       "</a:defPPr>"
       "</p:otherStyle>"
       "</p:txStyles>"
       "</p:sldMaster>";
  return s;
}

std::string GenerateSlideMasterRels() {
  std::string s;
  s.reserve(512);
  s += XML_DECL;
  s += RELATIONSHIPS_OPEN;
  s += "<Relationship Id=\"rId1\" "
       "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout\" "
       "Target=\"../slideLayouts/slideLayout1.xml\"/>";
  s += "<Relationship Id=\"rId2\" "
       "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme\" "
       "Target=\"../theme/theme1.xml\"/>";
  s += "</Relationships>";
  return s;
}

std::string GenerateSlideLayout() {
  std::string s;
  s.reserve(512);
  s += XML_DECL;
  s += "<p:sldLayout";
  s += NS_DRAWINGML;
  s += NS_OFFICE_REL;
  s += NS_PRESENTATIONML;
  s += " type=\"blank\" preserve=\"1\">";
  s += "<p:cSld name=\"Blank\"><p:spTree>"
       "<p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/>"
       "<p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>"
       "<p:grpSpPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"0\" cy=\"0\"/>"
       "<a:chOff x=\"0\" y=\"0\"/><a:chExt cx=\"0\" cy=\"0\"/></a:xfrm></p:grpSpPr>"
       "</p:spTree></p:cSld>"
       "<p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr>"
       "</p:sldLayout>";
  return s;
}

std::string GenerateSlideLayoutRels() {
  std::string s;
  s.reserve(256);
  s += XML_DECL;
  s += RELATIONSHIPS_OPEN;
  s += "<Relationship Id=\"rId1\" "
       "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster\" "
       "Target=\"../slideMasters/slideMaster1.xml\"/>";
  s += "</Relationships>";
  return s;
}

std::string GenerateTheme() {
  std::string s;
  s.reserve(2048);
  s += XML_DECL;
  s += "<a:theme";
  s += NS_DRAWINGML;
  s += " name=\"PAGX\">";
  s += "<a:themeElements>"
       "<a:clrScheme name=\"PAGX\">"
       "<a:dk1><a:srgbClr val=\"000000\"/></a:dk1>"
       "<a:lt1><a:srgbClr val=\"FFFFFF\"/></a:lt1>"
       "<a:dk2><a:srgbClr val=\"44546A\"/></a:dk2>"
       "<a:lt2><a:srgbClr val=\"E7E6E6\"/></a:lt2>"
       "<a:accent1><a:srgbClr val=\"4472C4\"/></a:accent1>"
       "<a:accent2><a:srgbClr val=\"ED7D31\"/></a:accent2>"
       "<a:accent3><a:srgbClr val=\"A5A5A5\"/></a:accent3>"
       "<a:accent4><a:srgbClr val=\"FFC000\"/></a:accent4>"
       "<a:accent5><a:srgbClr val=\"5B9BD5\"/></a:accent5>"
       "<a:accent6><a:srgbClr val=\"70AD47\"/></a:accent6>"
       "<a:hlink><a:srgbClr val=\"0563C1\"/></a:hlink>"
       "<a:folHlink><a:srgbClr val=\"954F72\"/></a:folHlink>"
       "</a:clrScheme>"
       "<a:fontScheme name=\"PAGX\">"
       "<a:majorFont><a:latin typeface=\"Calibri\"/><a:ea typeface=\"\"/>"
       "<a:cs typeface=\"\"/></a:majorFont>"
       "<a:minorFont><a:latin typeface=\"Calibri\"/><a:ea typeface=\"\"/>"
       "<a:cs typeface=\"\"/></a:minorFont>"
       "</a:fontScheme>"
       "<a:fmtScheme name=\"PAGX\">"
       "<a:fillStyleLst><a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill>"
       "<a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill>"
       "<a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill></a:fillStyleLst>"
       "<a:lnStyleLst><a:ln w=\"6350\"><a:solidFill><a:schemeClr val=\"phClr\"/>"
       "</a:solidFill></a:ln><a:ln w=\"12700\"><a:solidFill><a:schemeClr val=\"phClr\"/>"
       "</a:solidFill></a:ln><a:ln w=\"19050\"><a:solidFill><a:schemeClr val=\"phClr\"/>"
       "</a:solidFill></a:ln></a:lnStyleLst>"
       "<a:effectStyleLst><a:effectStyle><a:effectLst/></a:effectStyle>"
       "<a:effectStyle><a:effectLst/></a:effectStyle>"
       "<a:effectStyle><a:effectLst/></a:effectStyle></a:effectStyleLst>"
       "<a:bgFillStyleLst><a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill>"
       "<a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill>"
       "<a:solidFill><a:schemeClr val=\"phClr\"/></a:solidFill></a:bgFillStyleLst>"
       "</a:fmtScheme>"
       "</a:themeElements>"
       "<a:objectDefaults/>"
       "<a:extraClrSchemeLst/>"
       "</a:theme>";
  return s;
}

std::string GeneratePresProps() {
  std::string s;
  s.reserve(256);
  s += XML_DECL;
  s += "<p:presentationPr";
  s += NS_DRAWINGML;
  s += NS_OFFICE_REL;
  s += NS_PRESENTATIONML;
  s += "/>";
  return s;
}

std::string GenerateViewProps() {
  std::string s;
  s.reserve(512);
  s += XML_DECL;
  s += "<p:viewPr";
  s += NS_DRAWINGML;
  s += NS_OFFICE_REL;
  s += NS_PRESENTATIONML;
  s += ">";
  s += "<p:normalViewPr><p:restoredLeft sz=\"15611\"/>"
       "<p:restoredTop sz=\"94658\"/></p:normalViewPr>"
       "<p:slideViewPr><p:cSldViewPr snapToGrid=\"0\">"
       "<p:cViewPr varScale=\"1\">"
       "<p:scale><a:sx n=\"100\" d=\"100\"/><a:sy n=\"100\" d=\"100\"/></p:scale>"
       "<p:origin x=\"0\" y=\"0\"/></p:cViewPr>"
       "<p:guideLst/></p:cSldViewPr></p:slideViewPr>"
       "<p:notesTextViewPr><p:cViewPr>"
       "<p:scale><a:sx n=\"1\" d=\"1\"/><a:sy n=\"1\" d=\"1\"/></p:scale>"
       "<p:origin x=\"0\" y=\"0\"/></p:cViewPr></p:notesTextViewPr>"
       "<p:gridSpacing cx=\"72008\" cy=\"72008\"/>"
       "</p:viewPr>";
  return s;
}

std::string GenerateTableStyles() {
  std::string s;
  s.reserve(256);
  s += XML_DECL;
  s += "<a:tblStyleLst";
  s += NS_DRAWINGML;
  s += " def=\"{5C22544A-7EE6-4342-B048-85BDC9FD1C3A}\"/>";
  return s;
}

std::string GenerateCoreProps() {
  std::string s;
  s.reserve(512);
  s += XML_DECL;
  s += "<cp:coreProperties "
       "xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
       "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" "
       "xmlns:dcterms=\"http://purl.org/dc/terms/\" "
       "xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\" "
       "xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
       "<cp:revision>1</cp:revision>"
       "</cp:coreProperties>";
  return s;
}

std::string GenerateAppProps() {
  std::string s;
  s.reserve(512);
  s += XML_DECL;
  s += "<Properties "
       "xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\" "
       "xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes\">"
       "<TotalTime>0</TotalTime>"
       "<Words>0</Words>"
       "<Application>PAGX</Application>"
       "<Paragraphs>0</Paragraphs>"
       "<Slides>1</Slides>"
       "<Notes>0</Notes>"
       "<HiddenSlides>0</HiddenSlides>"
       "<MMClips>0</MMClips>"
       "<ScaleCrop>false</ScaleCrop>"
       "<LinksUpToDate>false</LinksUpToDate>"
       "<SharedDoc>false</SharedDoc>"
       "<HyperlinksChanged>false</HyperlinksChanged>"
       "</Properties>";
  return s;
}

}  // namespace pagx
