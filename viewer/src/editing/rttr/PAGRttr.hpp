// Auto-generated code
#pragma once
#include <rttr/registration.h>
#include <iostream>
#include "../../../../include/pag/pag.h"
#include "../../../../include/pag/types.h"
#include "../../../../include/pag/file.h"
#include "../../../../include/pag/gpu.h"

RTTR_REGISTRATION
{
	using namespace rttr;

	std::cout << "Rttr registed!" << std::endl;

	registration::class_<pag::PAGImage>("PAGImage");

	registration::class_<pag::PAGFont>("PAGFont")
		.property_readonly("fontFamily", &pag::PAGFont::fontFamily)
		.property_readonly("fontStyle", &pag::PAGFont::fontStyle);

	registration::class_<pag::PAGLayer>("PAGLayer")
		.property_readonly("externalHandle", &pag::PAGLayer::externalHandle);

	registration::class_<pag::PAGSolidLayer>("PAGSolidLayer");

	registration::class_<pag::PAGTextLayer>("PAGTextLayer");

	registration::class_<pag::PAGShapeLayer>("PAGShapeLayer");

	registration::class_<pag::PAGVideoRange>("PAGVideoRange");

	registration::class_<pag::PAGImageLayer>("PAGImageLayer");

	registration::class_<pag::PAGComposition>("PAGComposition");

	registration::class_<pag::PAGFile>("PAGFile");

	registration::class_<pag::PAGSurface>("PAGSurface");

	registration::class_<pag::PAGPlayer>("PAGPlayer");

	registration::class_<pag::PAGDecoder>("PAGDecoder");

	registration::class_<pag::PAGDiskCache>("PAGDiskCache");

	registration::class_<pag::PAGVideoDecoder>("PAGVideoDecoder");

	registration::class_<pag::PAG>("PAG");

	registration::class_<pag::Color>("Color")
		.property_readonly("red", &pag::Color::red)
		.property_readonly("green", &pag::Color::green)
		.property_readonly("blue", &pag::Color::blue);

	registration::class_<pag::TimeRange>("TimeRange")
		.property_readonly("start", &pag::TimeRange::start)
		.property_readonly("end", &pag::TimeRange::end);

	registration::class_<pag::Point>("Point")
		.property_readonly("x", &pag::Point::x)
		.property_readonly("y", &pag::Point::y);

	registration::class_<pag::Point3D>("Point3D")
		.property_readonly("x", &pag::Point3D::x)
		.property_readonly("y", &pag::Point3D::y)
		.property_readonly("z", &pag::Point3D::z);

	registration::class_<pag::Rect>("Rect")
		.property_readonly("left", &pag::Rect::left)
		.property_readonly("top", &pag::Rect::top)
		.property_readonly("right", &pag::Rect::right)
		.property_readonly("bottom", &pag::Rect::bottom);

	registration::class_<pag::Matrix>("Matrix");

	registration::class_<pag::TextDocument>("TextDocument")
		.property_readonly("applyFill", &pag::TextDocument::applyFill)
		.property_readonly("applyStroke", &pag::TextDocument::applyStroke)
		.property_readonly("baselineShift", &pag::TextDocument::baselineShift)
		.property_readonly("boxText", &pag::TextDocument::boxText)
		.property_readonly("boxTextPos", &pag::TextDocument::boxTextPos)
		.property_readonly("boxTextSize", &pag::TextDocument::boxTextSize)
		.property_readonly("firstBaseLine", &pag::TextDocument::firstBaseLine)
		.property_readonly("fauxBold", &pag::TextDocument::fauxBold)
		.property_readonly("fauxItalic", &pag::TextDocument::fauxItalic)
		.property_readonly("fillColor", &pag::TextDocument::fillColor)
		.property_readonly("fontFamily", &pag::TextDocument::fontFamily)
		.property_readonly("fontStyle", &pag::TextDocument::fontStyle)
		.property_readonly("fontSize", &pag::TextDocument::fontSize)
		.property_readonly("strokeColor", &pag::TextDocument::strokeColor)
		.property_readonly("strokeOverFill", &pag::TextDocument::strokeOverFill)
		.property_readonly("strokeWidth", &pag::TextDocument::strokeWidth)
		.property_readonly("text", &pag::TextDocument::text)
		.property_readonly("justification", &pag::TextDocument::justification)
		.property_readonly("leading", &pag::TextDocument::leading)
		.property_readonly("tracking", &pag::TextDocument::tracking)
		.property_readonly("backgroundColor", &pag::TextDocument::backgroundColor)
		.property_readonly("backgroundAlpha", &pag::TextDocument::backgroundAlpha)
		.property_readonly("direction", &pag::TextDocument::direction);

	registration::class_<pag::Marker>("Marker")
		.property_readonly("startTime", &pag::Marker::startTime)
		.property_readonly("duration", &pag::Marker::duration)
		.property_readonly("comment", &pag::Marker::comment);

	registration::class_<pag::ByteData>("ByteData");

	registration::class_<pag::Ratio>("Ratio")
		.property_readonly("numerator", &pag::Ratio::numerator)
		.property_readonly("denominator", &pag::Ratio::denominator);

	registration::class_<pag::PathData>("PathData")
		.property_readonly("verbs", &pag::PathData::verbs)
		.property_readonly("points", &pag::PathData::points);

	registration::class_<pag::Transform2D>("Transform2D")
		.property_readonly("anchorPoint", &pag::Transform2D::anchorPoint)
		.property_readonly("position", &pag::Transform2D::position)
		.property_readonly("xPosition", &pag::Transform2D::xPosition)
		.property_readonly("yPosition", &pag::Transform2D::yPosition)
		.property_readonly("scale", &pag::Transform2D::scale)
		.property_readonly("rotation", &pag::Transform2D::rotation)
		.property_readonly("opacity", &pag::Transform2D::opacity);

	registration::class_<pag::Transform3D>("Transform3D")
		.property_readonly("anchorPoint", &pag::Transform3D::anchorPoint)
		.property_readonly("position", &pag::Transform3D::position)
		.property_readonly("xPosition", &pag::Transform3D::xPosition)
		.property_readonly("yPosition", &pag::Transform3D::yPosition)
		.property_readonly("zPosition", &pag::Transform3D::zPosition)
		.property_readonly("scale", &pag::Transform3D::scale)
		.property_readonly("orientation", &pag::Transform3D::orientation)
		.property_readonly("xRotation", &pag::Transform3D::xRotation)
		.property_readonly("yRotation", &pag::Transform3D::yRotation)
		.property_readonly("zRotation", &pag::Transform3D::zRotation)
		.property_readonly("opacity", &pag::Transform3D::opacity);

	registration::class_<pag::MaskData>("MaskData")
		.property_readonly("id", &pag::MaskData::id)
		.property_readonly("inverted", &pag::MaskData::inverted)
		.property_readonly("maskMode", &pag::MaskData::maskMode)
		.property_readonly("maskPath", &pag::MaskData::maskPath)
		.property_readonly("maskFeather", &pag::MaskData::maskFeather)
		.property_readonly("maskOpacity", &pag::MaskData::maskOpacity)
		.property_readonly("maskExpansion", &pag::MaskData::maskExpansion);

	registration::class_<pag::Effect>("Effect")
		.property_readonly("uniqueID", &pag::Effect::uniqueID)
		.property_readonly("effectOpacity", &pag::Effect::effectOpacity)
		.property_readonly("maskReferences", &pag::Effect::maskReferences);

	registration::class_<pag::MotionTileEffect>("MotionTileEffect")
		.property_readonly("tileCenter", &pag::MotionTileEffect::tileCenter)
		.property_readonly("tileWidth", &pag::MotionTileEffect::tileWidth)
		.property_readonly("tileHeight", &pag::MotionTileEffect::tileHeight)
		.property_readonly("outputWidth", &pag::MotionTileEffect::outputWidth)
		.property_readonly("outputHeight", &pag::MotionTileEffect::outputHeight)
		.property_readonly("mirrorEdges", &pag::MotionTileEffect::mirrorEdges)
		.property_readonly("phase", &pag::MotionTileEffect::phase)
		.property_readonly("horizontalPhaseShift", &pag::MotionTileEffect::horizontalPhaseShift);

	registration::class_<pag::LevelsIndividualEffect>("LevelsIndividualEffect")
		.property_readonly("inputBlack", &pag::LevelsIndividualEffect::inputBlack)
		.property_readonly("inputWhite", &pag::LevelsIndividualEffect::inputWhite)
		.property_readonly("gamma", &pag::LevelsIndividualEffect::gamma)
		.property_readonly("outputBlack", &pag::LevelsIndividualEffect::outputBlack)
		.property_readonly("outputWhite", &pag::LevelsIndividualEffect::outputWhite)
		.property_readonly("redInputBlack", &pag::LevelsIndividualEffect::redInputBlack)
		.property_readonly("redInputWhite", &pag::LevelsIndividualEffect::redInputWhite)
		.property_readonly("redGamma", &pag::LevelsIndividualEffect::redGamma)
		.property_readonly("redOutputBlack", &pag::LevelsIndividualEffect::redOutputBlack)
		.property_readonly("redOutputWhite", &pag::LevelsIndividualEffect::redOutputWhite)
		.property_readonly("greenInputBlack", &pag::LevelsIndividualEffect::greenInputBlack)
		.property_readonly("greenInputWhite", &pag::LevelsIndividualEffect::greenInputWhite)
		.property_readonly("greenGamma", &pag::LevelsIndividualEffect::greenGamma)
		.property_readonly("greenOutputBlack", &pag::LevelsIndividualEffect::greenOutputBlack)
		.property_readonly("greenOutputWhite", &pag::LevelsIndividualEffect::greenOutputWhite)
		.property_readonly("blueInputBlack", &pag::LevelsIndividualEffect::blueInputBlack)
		.property_readonly("blueInputWhite", &pag::LevelsIndividualEffect::blueInputWhite)
		.property_readonly("blueGamma", &pag::LevelsIndividualEffect::blueGamma)
		.property_readonly("blueOutputBlack", &pag::LevelsIndividualEffect::blueOutputBlack)
		.property_readonly("blueOutputWhite", &pag::LevelsIndividualEffect::blueOutputWhite);

	registration::class_<pag::CornerPinEffect>("CornerPinEffect")
		.property_readonly("upperLeft", &pag::CornerPinEffect::upperLeft)
		.property_readonly("upperRight", &pag::CornerPinEffect::upperRight)
		.property_readonly("lowerLeft", &pag::CornerPinEffect::lowerLeft)
		.property_readonly("lowerRight", &pag::CornerPinEffect::lowerRight);

	registration::class_<pag::BulgeEffect>("BulgeEffect")
		.property_readonly("horizontalRadius", &pag::BulgeEffect::horizontalRadius)
		.property_readonly("verticalRadius", &pag::BulgeEffect::verticalRadius)
		.property_readonly("bulgeCenter", &pag::BulgeEffect::bulgeCenter)
		.property_readonly("bulgeHeight", &pag::BulgeEffect::bulgeHeight)
		.property_readonly("taperRadius", &pag::BulgeEffect::taperRadius)
		.property_readonly("pinning", &pag::BulgeEffect::pinning);

	registration::class_<pag::FastBlurEffect>("FastBlurEffect")
		.property_readonly("blurriness", &pag::FastBlurEffect::blurriness)
		.property_readonly("blurDimensions", &pag::FastBlurEffect::blurDimensions)
		.property_readonly("repeatEdgePixels", &pag::FastBlurEffect::repeatEdgePixels);

	registration::class_<pag::GlowEffect>("GlowEffect")
		.property_readonly("glowThreshold", &pag::GlowEffect::glowThreshold)
		.property_readonly("glowRadius", &pag::GlowEffect::glowRadius)
		.property_readonly("glowIntensity", &pag::GlowEffect::glowIntensity);

	registration::class_<pag::DisplacementMapEffect>("DisplacementMapEffect")
		.property_readonly("useForHorizontalDisplacement", &pag::DisplacementMapEffect::useForHorizontalDisplacement)
		.property_readonly("maxHorizontalDisplacement", &pag::DisplacementMapEffect::maxHorizontalDisplacement)
		.property_readonly("useForVerticalDisplacement", &pag::DisplacementMapEffect::useForVerticalDisplacement)
		.property_readonly("maxVerticalDisplacement", &pag::DisplacementMapEffect::maxVerticalDisplacement)
		.property_readonly("displacementMapBehavior", &pag::DisplacementMapEffect::displacementMapBehavior)
		.property_readonly("edgeBehavior", &pag::DisplacementMapEffect::edgeBehavior)
		.property_readonly("expandOutput", &pag::DisplacementMapEffect::expandOutput);

	registration::class_<pag::RadialBlurEffect>("RadialBlurEffect")
		.property_readonly("amount", &pag::RadialBlurEffect::amount)
		.property_readonly("center", &pag::RadialBlurEffect::center)
		.property_readonly("mode", &pag::RadialBlurEffect::mode)
		.property_readonly("antialias", &pag::RadialBlurEffect::antialias);

	registration::class_<pag::MosaicEffect>("MosaicEffect")
		.property_readonly("horizontalBlocks", &pag::MosaicEffect::horizontalBlocks)
		.property_readonly("verticalBlocks", &pag::MosaicEffect::verticalBlocks)
		.property_readonly("sharpColors", &pag::MosaicEffect::sharpColors);

	registration::class_<pag::BrightnessContrastEffect>("BrightnessContrastEffect")
		.property_readonly("brightness", &pag::BrightnessContrastEffect::brightness)
		.property_readonly("contrast", &pag::BrightnessContrastEffect::contrast)
		.property_readonly("useOldVersion", &pag::BrightnessContrastEffect::useOldVersion);

	registration::class_<pag::HueSaturationEffect>("HueSaturationEffect")
		.property_readonly("channelControl", &pag::HueSaturationEffect::channelControl)
		.property_readonly("hue", &pag::HueSaturationEffect::hue)
		.property_readonly("saturation", &pag::HueSaturationEffect::saturation)
		.property_readonly("lightness", &pag::HueSaturationEffect::lightness)
		.property_readonly("colorize", &pag::HueSaturationEffect::colorize)
		.property_readonly("colorizeHue", &pag::HueSaturationEffect::colorizeHue)
		.property_readonly("colorizeSaturation", &pag::HueSaturationEffect::colorizeSaturation)
		.property_readonly("colorizeLightness", &pag::HueSaturationEffect::colorizeLightness);

	registration::class_<pag::LayerStyle>("LayerStyle")
		.property_readonly("uniqueID", &pag::LayerStyle::uniqueID);

	registration::class_<pag::DropShadowStyle>("DropShadowStyle")
		.property_readonly("blendMode", &pag::DropShadowStyle::blendMode)
		.property_readonly("color", &pag::DropShadowStyle::color)
		.property_readonly("opacity", &pag::DropShadowStyle::opacity)
		.property_readonly("angle", &pag::DropShadowStyle::angle)
		.property_readonly("distance", &pag::DropShadowStyle::distance)
		.property_readonly("size", &pag::DropShadowStyle::size)
		.property_readonly("spread", &pag::DropShadowStyle::spread);

	registration::class_<pag::StrokeStyle>("StrokeStyle")
		.property_readonly("blendMode", &pag::StrokeStyle::blendMode)
		.property_readonly("color", &pag::StrokeStyle::color)
		.property_readonly("size", &pag::StrokeStyle::size)
		.property_readonly("opacity", &pag::StrokeStyle::opacity)
		.property_readonly("position", &pag::StrokeStyle::position);

	registration::class_<pag::GradientColor>("GradientColor")
		.property_readonly("alphaStops", &pag::GradientColor::alphaStops)
		.property_readonly("colorStops", &pag::GradientColor::colorStops);

	registration::class_<pag::GradientOverlayStyle>("GradientOverlayStyle")
		.property_readonly("blendMode", &pag::GradientOverlayStyle::blendMode)
		.property_readonly("opacity", &pag::GradientOverlayStyle::opacity)
		.property_readonly("colors", &pag::GradientOverlayStyle::colors)
		.property_readonly("gradientSmoothness", &pag::GradientOverlayStyle::gradientSmoothness)
		.property_readonly("angle", &pag::GradientOverlayStyle::angle)
		.property_readonly("style", &pag::GradientOverlayStyle::style)
		.property_readonly("reverse", &pag::GradientOverlayStyle::reverse)
		.property_readonly("alignWithLayer", &pag::GradientOverlayStyle::alignWithLayer)
		.property_readonly("scale", &pag::GradientOverlayStyle::scale)
		.property_readonly("offset", &pag::GradientOverlayStyle::offset);

	registration::class_<pag::OuterGlowStyle>("OuterGlowStyle")
		.property_readonly("blendMode", &pag::OuterGlowStyle::blendMode)
		.property_readonly("opacity", &pag::OuterGlowStyle::opacity)
		.property_readonly("noise", &pag::OuterGlowStyle::noise)
		.property_readonly("colorType", &pag::OuterGlowStyle::colorType)
		.property_readonly("color", &pag::OuterGlowStyle::color)
		.property_readonly("colors", &pag::OuterGlowStyle::colors)
		.property_readonly("gradientSmoothness", &pag::OuterGlowStyle::gradientSmoothness)
		.property_readonly("technique", &pag::OuterGlowStyle::technique)
		.property_readonly("spread", &pag::OuterGlowStyle::spread)
		.property_readonly("size", &pag::OuterGlowStyle::size)
		.property_readonly("range", &pag::OuterGlowStyle::range)
		.property_readonly("jitter", &pag::OuterGlowStyle::jitter);

	registration::class_<pag::TextPathOptions>("TextPathOptions")
		.property_readonly("path", &pag::TextPathOptions::path)
		.property_readonly("reversedPath", &pag::TextPathOptions::reversedPath)
		.property_readonly("perpendicularToPath", &pag::TextPathOptions::perpendicularToPath)
		.property_readonly("forceAlignment", &pag::TextPathOptions::forceAlignment)
		.property_readonly("firstMargin", &pag::TextPathOptions::firstMargin)
		.property_readonly("lastMargin", &pag::TextPathOptions::lastMargin);

	registration::class_<pag::TextMoreOptions>("TextMoreOptions")
		.property_readonly("anchorPointGrouping", &pag::TextMoreOptions::anchorPointGrouping)
		.property_readonly("groupingAlignment", &pag::TextMoreOptions::groupingAlignment);

	registration::class_<pag::TextSelector>("TextSelector");

	registration::class_<pag::TextRangeSelector>("TextRangeSelector")
		.property_readonly("start", &pag::TextRangeSelector::start)
		.property_readonly("end", &pag::TextRangeSelector::end)
		.property_readonly("offset", &pag::TextRangeSelector::offset)
		.property_readonly("units", &pag::TextRangeSelector::units)
		.property_readonly("basedOn", &pag::TextRangeSelector::basedOn)
		.property_readonly("mode", &pag::TextRangeSelector::mode)
		.property_readonly("amount", &pag::TextRangeSelector::amount)
		.property_readonly("shape", &pag::TextRangeSelector::shape)
		.property_readonly("smoothness", &pag::TextRangeSelector::smoothness)
		.property_readonly("easeHigh", &pag::TextRangeSelector::easeHigh)
		.property_readonly("easeLow", &pag::TextRangeSelector::easeLow)
		.property_readonly("randomizeOrder", &pag::TextRangeSelector::randomizeOrder)
		.property_readonly("randomSeed", &pag::TextRangeSelector::randomSeed);

	registration::class_<pag::TextWigglySelector>("TextWigglySelector")
		.property_readonly("mode", &pag::TextWigglySelector::mode)
		.property_readonly("maxAmount", &pag::TextWigglySelector::maxAmount)
		.property_readonly("minAmount", &pag::TextWigglySelector::minAmount)
		.property_readonly("basedOn", &pag::TextWigglySelector::basedOn)
		.property_readonly("wigglesPerSecond", &pag::TextWigglySelector::wigglesPerSecond)
		.property_readonly("correlation", &pag::TextWigglySelector::correlation)
		.property_readonly("temporalPhase", &pag::TextWigglySelector::temporalPhase)
		.property_readonly("spatialPhase", &pag::TextWigglySelector::spatialPhase)
		.property_readonly("lockDimensions", &pag::TextWigglySelector::lockDimensions)
		.property_readonly("randomSeed", &pag::TextWigglySelector::randomSeed);

	registration::class_<pag::TextAnimatorColorProperties>("TextAnimatorColorProperties")
		.property_readonly("fillColor", &pag::TextAnimatorColorProperties::fillColor)
		.property_readonly("strokeColor", &pag::TextAnimatorColorProperties::strokeColor);

	registration::class_<pag::TextAnimatorTypographyProperties>("TextAnimatorTypographyProperties")
		.property_readonly("trackingType", &pag::TextAnimatorTypographyProperties::trackingType)
		.property_readonly("trackingAmount", &pag::TextAnimatorTypographyProperties::trackingAmount)
		.property_readonly("position", &pag::TextAnimatorTypographyProperties::position)
		.property_readonly("scale", &pag::TextAnimatorTypographyProperties::scale)
		.property_readonly("rotation", &pag::TextAnimatorTypographyProperties::rotation)
		.property_readonly("opacity", &pag::TextAnimatorTypographyProperties::opacity);

	registration::class_<pag::TextAnimator>("TextAnimator")
		.property_readonly("selectors", &pag::TextAnimator::selectors)
		.property_readonly("colorProperties", &pag::TextAnimator::colorProperties)
		.property_readonly("typographyProperties", &pag::TextAnimator::typographyProperties);

	registration::class_<pag::ShapeTransform>("ShapeTransform")
		.property_readonly("anchorPoint", &pag::ShapeTransform::anchorPoint)
		.property_readonly("position", &pag::ShapeTransform::position)
		.property_readonly("scale", &pag::ShapeTransform::scale)
		.property_readonly("skew", &pag::ShapeTransform::skew)
		.property_readonly("skewAxis", &pag::ShapeTransform::skewAxis)
		.property_readonly("rotation", &pag::ShapeTransform::rotation)
		.property_readonly("opacity", &pag::ShapeTransform::opacity);

	registration::class_<pag::ShapeElement>("ShapeElement");

	registration::class_<pag::ShapeGroupElement>("ShapeGroupElement")
		.property_readonly("blendMode", &pag::ShapeGroupElement::blendMode)
		.property_readonly("transform", &pag::ShapeGroupElement::transform)
		.property_readonly("elements", &pag::ShapeGroupElement::elements);

	registration::class_<pag::RectangleElement>("RectangleElement")
		.property_readonly("reversed", &pag::RectangleElement::reversed)
		.property_readonly("size", &pag::RectangleElement::size)
		.property_readonly("position", &pag::RectangleElement::position)
		.property_readonly("roundness", &pag::RectangleElement::roundness);

	registration::class_<pag::EllipseElement>("EllipseElement")
		.property_readonly("reversed", &pag::EllipseElement::reversed)
		.property_readonly("size", &pag::EllipseElement::size)
		.property_readonly("position", &pag::EllipseElement::position);

	registration::class_<pag::PolyStarElement>("PolyStarElement")
		.property_readonly("reversed", &pag::PolyStarElement::reversed)
		.property_readonly("polyType", &pag::PolyStarElement::polyType)
		.property_readonly("points", &pag::PolyStarElement::points)
		.property_readonly("position", &pag::PolyStarElement::position)
		.property_readonly("rotation", &pag::PolyStarElement::rotation)
		.property_readonly("innerRadius", &pag::PolyStarElement::innerRadius)
		.property_readonly("outerRadius", &pag::PolyStarElement::outerRadius)
		.property_readonly("innerRoundness", &pag::PolyStarElement::innerRoundness)
		.property_readonly("outerRoundness", &pag::PolyStarElement::outerRoundness);

	registration::class_<pag::ShapePathElement>("ShapePathElement")
		.property_readonly("shapePath", &pag::ShapePathElement::shapePath);

	registration::class_<pag::FillElement>("FillElement")
		.property_readonly("blendMode", &pag::FillElement::blendMode)
		.property_readonly("composite", &pag::FillElement::composite)
		.property_readonly("fillRule", &pag::FillElement::fillRule)
		.property_readonly("color", &pag::FillElement::color)
		.property_readonly("opacity", &pag::FillElement::opacity);

	registration::class_<pag::StrokeElement>("StrokeElement")
		.property_readonly("blendMode", &pag::StrokeElement::blendMode)
		.property_readonly("composite", &pag::StrokeElement::composite)
		.property_readonly("color", &pag::StrokeElement::color)
		.property_readonly("opacity", &pag::StrokeElement::opacity)
		.property_readonly("strokeWidth", &pag::StrokeElement::strokeWidth)
		.property_readonly("lineCap", &pag::StrokeElement::lineCap)
		.property_readonly("lineJoin", &pag::StrokeElement::lineJoin)
		.property_readonly("miterLimit", &pag::StrokeElement::miterLimit)
		.property_readonly("dashOffset", &pag::StrokeElement::dashOffset)
		.property_readonly("dashes", &pag::StrokeElement::dashes);

	registration::class_<pag::GradientFillElement>("GradientFillElement")
		.property_readonly("blendMode", &pag::GradientFillElement::blendMode)
		.property_readonly("composite", &pag::GradientFillElement::composite)
		.property_readonly("fillRule", &pag::GradientFillElement::fillRule)
		.property_readonly("fillType", &pag::GradientFillElement::fillType)
		.property_readonly("opacity", &pag::GradientFillElement::opacity)
		.property_readonly("startPoint", &pag::GradientFillElement::startPoint)
		.property_readonly("endPoint", &pag::GradientFillElement::endPoint)
		.property_readonly("colors", &pag::GradientFillElement::colors);

	registration::class_<pag::GradientStrokeElement>("GradientStrokeElement")
		.property_readonly("blendMode", &pag::GradientStrokeElement::blendMode)
		.property_readonly("composite", &pag::GradientStrokeElement::composite)
		.property_readonly("fillType", &pag::GradientStrokeElement::fillType)
		.property_readonly("lineCap", &pag::GradientStrokeElement::lineCap)
		.property_readonly("lineJoin", &pag::GradientStrokeElement::lineJoin)
		.property_readonly("miterLimit", &pag::GradientStrokeElement::miterLimit)
		.property_readonly("startPoint", &pag::GradientStrokeElement::startPoint)
		.property_readonly("endPoint", &pag::GradientStrokeElement::endPoint)
		.property_readonly("colors", &pag::GradientStrokeElement::colors)
		.property_readonly("opacity", &pag::GradientStrokeElement::opacity)
		.property_readonly("strokeWidth", &pag::GradientStrokeElement::strokeWidth)
		.property_readonly("dashOffset", &pag::GradientStrokeElement::dashOffset)
		.property_readonly("dashes", &pag::GradientStrokeElement::dashes);

	registration::class_<pag::MergePathsElement>("MergePathsElement")
		.property_readonly("mode", &pag::MergePathsElement::mode);

	registration::class_<pag::TrimPathsElement>("TrimPathsElement")
		.property_readonly("start", &pag::TrimPathsElement::start)
		.property_readonly("end", &pag::TrimPathsElement::end)
		.property_readonly("offset", &pag::TrimPathsElement::offset)
		.property_readonly("trimType", &pag::TrimPathsElement::trimType);

	registration::class_<pag::RepeaterTransform>("RepeaterTransform")
		.property_readonly("anchorPoint", &pag::RepeaterTransform::anchorPoint)
		.property_readonly("position", &pag::RepeaterTransform::position)
		.property_readonly("scale", &pag::RepeaterTransform::scale)
		.property_readonly("rotation", &pag::RepeaterTransform::rotation)
		.property_readonly("startOpacity", &pag::RepeaterTransform::startOpacity)
		.property_readonly("endOpacity", &pag::RepeaterTransform::endOpacity);

	registration::class_<pag::RepeaterElement>("RepeaterElement")
		.property_readonly("copies", &pag::RepeaterElement::copies)
		.property_readonly("offset", &pag::RepeaterElement::offset)
		.property_readonly("composite", &pag::RepeaterElement::composite)
		.property_readonly("transform", &pag::RepeaterElement::transform);

	registration::class_<pag::RoundCornersElement>("RoundCornersElement")
		.property_readonly("radius", &pag::RoundCornersElement::radius);

	registration::class_<pag::CameraOption>("CameraOption")
		.property_readonly("zoom", &pag::CameraOption::zoom)
		.property_readonly("depthOfField", &pag::CameraOption::depthOfField)
		.property_readonly("focusDistance", &pag::CameraOption::focusDistance)
		.property_readonly("aperture", &pag::CameraOption::aperture)
		.property_readonly("blurLevel", &pag::CameraOption::blurLevel)
		.property_readonly("irisShape", &pag::CameraOption::irisShape)
		.property_readonly("irisRotation", &pag::CameraOption::irisRotation)
		.property_readonly("irisRoundness", &pag::CameraOption::irisRoundness)
		.property_readonly("irisAspectRatio", &pag::CameraOption::irisAspectRatio)
		.property_readonly("irisDiffractionFringe", &pag::CameraOption::irisDiffractionFringe)
		.property_readonly("highlightGain", &pag::CameraOption::highlightGain)
		.property_readonly("highlightThreshold", &pag::CameraOption::highlightThreshold)
		.property_readonly("highlightSaturation", &pag::CameraOption::highlightSaturation);

	registration::class_<pag::Cache>("Cache");

	registration::class_<pag::Layer>("Layer")
		.property_readonly("uniqueID", &pag::Layer::uniqueID)
		.property_readonly("id", &pag::Layer::id)
		.property_readonly("name", &pag::Layer::name)
		.property_readonly("stretch", &pag::Layer::stretch)
		.property_readonly("startTime", &pag::Layer::startTime)
		.property_readonly("duration", &pag::Layer::duration)
		.property_readonly("autoOrientation", &pag::Layer::autoOrientation)
		.property_readonly("motionBlur", &pag::Layer::motionBlur)
		.property_readonly("transform", &pag::Layer::transform)
		.property_readonly("transform3D", &pag::Layer::transform3D)
		.property_readonly("isActive", &pag::Layer::isActive)
		.property_readonly("blendMode", &pag::Layer::blendMode)
		.property_readonly("trackMatteType", &pag::Layer::trackMatteType)
		.property_readonly("trackMatteLayer", &pag::Layer::trackMatteLayer)
		.property_readonly("timeRemap", &pag::Layer::timeRemap)
		.property_readonly("masks", &pag::Layer::masks)
		.property_readonly("effects", &pag::Layer::effects)
		.property_readonly("layerStyles", &pag::Layer::layerStyles)
		.property_readonly("markers", &pag::Layer::markers)
		.property_readonly("cachePolicy", &pag::Layer::cachePolicy);

	registration::class_<pag::NullLayer>("NullLayer");

	registration::class_<pag::SolidLayer>("SolidLayer")
		.property_readonly("solidColor", &pag::SolidLayer::solidColor)
		.property_readonly("width", &pag::SolidLayer::width)
		.property_readonly("height", &pag::SolidLayer::height);

	registration::class_<pag::TextLayer>("TextLayer")
		.property_readonly("sourceText", &pag::TextLayer::sourceText)
		.property_readonly("pathOption", &pag::TextLayer::pathOption)
		.property_readonly("moreOption", &pag::TextLayer::moreOption)
		.property_readonly("animators", &pag::TextLayer::animators);

	registration::class_<pag::ShapeLayer>("ShapeLayer")
		.property_readonly("contents", &pag::ShapeLayer::contents);

	registration::class_<pag::ImageFillRule>("ImageFillRule")
		.property_readonly("scaleMode", &pag::ImageFillRule::scaleMode)
		.property_readonly("timeRemap", &pag::ImageFillRule::timeRemap);

	registration::class_<pag::ImageLayer>("ImageLayer")
		.property_readonly("imageBytes", &pag::ImageLayer::imageBytes)
		.property_readonly("imageFillRule", &pag::ImageLayer::imageFillRule);

	registration::class_<pag::PreComposeLayer>("PreComposeLayer")
		.property_readonly("composition", &pag::PreComposeLayer::composition)
		.property_readonly("compositionStartTime", &pag::PreComposeLayer::compositionStartTime);

	registration::class_<pag::CameraLayer>("CameraLayer")
		.property_readonly("cameraOption", &pag::CameraLayer::cameraOption);

	registration::class_<pag::Composition>("Composition")
		.property_readonly("uniqueID", &pag::Composition::uniqueID)
		.property_readonly("id", &pag::Composition::id)
		.property_readonly("width", &pag::Composition::width)
		.property_readonly("height", &pag::Composition::height)
		.property_readonly("duration", &pag::Composition::duration)
		.property_readonly("frameRate", &pag::Composition::frameRate)
		.property_readonly("backgroundColor", &pag::Composition::backgroundColor)
		.property_readonly("audioBytes", &pag::Composition::audioBytes)
		.property_readonly("audioMarkers", &pag::Composition::audioMarkers)
		.property_readonly("audioStartTime", &pag::Composition::audioStartTime)
		.property_readonly("staticTimeRanges", &pag::Composition::staticTimeRanges);

	registration::class_<pag::VectorComposition>("VectorComposition")
		.property_readonly("layers", &pag::VectorComposition::layers);

	registration::class_<pag::ImageBytes>("ImageBytes")
		.property_readonly("uniqueID", &pag::ImageBytes::uniqueID)
		.property_readonly("id", &pag::ImageBytes::id)
		.property_readonly("width", &pag::ImageBytes::width)
		.property_readonly("height", &pag::ImageBytes::height)
		.property_readonly("anchorX", &pag::ImageBytes::anchorX)
		.property_readonly("anchorY", &pag::ImageBytes::anchorY)
		.property_readonly("scaleFactor", &pag::ImageBytes::scaleFactor)
		.property_readonly("fileBytes", &pag::ImageBytes::fileBytes);

	registration::class_<pag::BitmapRect>("BitmapRect")
		.property_readonly("x", &pag::BitmapRect::x)
		.property_readonly("y", &pag::BitmapRect::y)
		.property_readonly("fileBytes", &pag::BitmapRect::fileBytes);

	registration::class_<pag::Sequence>("Sequence")
		.property_readonly("width", &pag::Sequence::width)
		.property_readonly("height", &pag::Sequence::height)
		.property_readonly("frameRate", &pag::Sequence::frameRate)
		.property_readonly("duration", &pag::Sequence::duration);

	registration::class_<pag::BitmapFrame>("BitmapFrame")
		.property_readonly("isKeyframe", &pag::BitmapFrame::isKeyframe)
		.property_readonly("bitmaps", &pag::BitmapFrame::bitmaps);

	registration::class_<pag::BitmapSequence>("BitmapSequence")
		.property_readonly("frames", &pag::BitmapSequence::frames)
		.property_readonly("duration", &pag::BitmapSequence::duration);

	registration::class_<pag::BitmapComposition>("BitmapComposition")
		.property_readonly("sequences", &pag::BitmapComposition::sequences);

	registration::class_<pag::VideoFrame>("VideoFrame")
		.property_readonly("isKeyframe", &pag::VideoFrame::isKeyframe)
		.property_readonly("frame", &pag::VideoFrame::frame)
		.property_readonly("fileBytes", &pag::VideoFrame::fileBytes);

	registration::class_<pag::VideoSequence>("VideoSequence")
		.property_readonly("alphaStartX", &pag::VideoSequence::alphaStartX)
		.property_readonly("alphaStartY", &pag::VideoSequence::alphaStartY)
		.property_readonly("frames", &pag::VideoSequence::frames)
		.property_readonly("headers", &pag::VideoSequence::headers)
		.property_readonly("staticTimeRanges", &pag::VideoSequence::staticTimeRanges)
		.property_readonly("MP4Header", &pag::VideoSequence::MP4Header)
		.property_readonly("duration", &pag::VideoSequence::duration);

	registration::class_<pag::VideoComposition>("VideoComposition")
		.property_readonly("sequences", &pag::VideoComposition::sequences);

	registration::class_<pag::FontData>("FontData")
		.property_readonly("fontFamily", &pag::FontData::fontFamily)
		.property_readonly("fontStyle", &pag::FontData::fontStyle);

	registration::class_<pag::FileAttributes>("FileAttributes")
		.property_readonly("timestamp", &pag::FileAttributes::timestamp)
		.property_readonly("pluginVersion", &pag::FileAttributes::pluginVersion)
		.property_readonly("aeVersion", &pag::FileAttributes::aeVersion)
		.property_readonly("systemVersion", &pag::FileAttributes::systemVersion)
		.property_readonly("author", &pag::FileAttributes::author)
		.property_readonly("scene", &pag::FileAttributes::scene)
		.property_readonly("warnings", &pag::FileAttributes::warnings);

	registration::class_<pag::File>("File")
		.property_readonly("timeStretchMode", &pag::File::timeStretchMode)
		.property_readonly("scaledTimeRange", &pag::File::scaledTimeRange)
		.property_readonly("fileAttributes", &pag::File::fileAttributes)
		.property_readonly("path", &pag::File::path)
		.property_readonly("images", &pag::File::images)
		.property_readonly("compositions", &pag::File::compositions)
		.property_readonly("editableImages", &pag::File::editableImages)
		.property_readonly("editableTexts", &pag::File::editableTexts)
		.property_readonly("imageScaleModes", &pag::File::imageScaleModes)
		.property_readonly("duration", &pag::File::duration)
		.property_readonly("frameRate", &pag::File::frameRate)
		.property_readonly("width", &pag::File::width)
		.property_readonly("height", &pag::File::height)
		.property_readonly("tagLevel", &pag::File::tagLevel)
		.property_readonly("backgroundColor", &pag::File::backgroundColor)
		.property_readonly("numTexts", &pag::File::numTexts)
		.property_readonly("numImages", &pag::File::numImages)
		.property_readonly("numVideos", &pag::File::numVideos)
		.property_readonly("numLayers", &pag::File::numLayers)
		.property_readonly("rootLayer", &pag::File::getRootLayer)
		.property_readonly("hasScaledTimeRange", &pag::File::hasScaledTimeRange);

	registration::class_<pag::Codec>("Codec");

	registration::class_<pag::BackendTexture>("BackendTexture");

	registration::class_<pag::BackendRenderTarget>("BackendRenderTarget");

	registration::class_<pag::BackendSemaphore>("BackendSemaphore");

	registration::enumeration<pag::LayerType>("LayerType")
     (
		value("Unknown", pag::LayerType::Unknown),
		value("Null", pag::LayerType::Null),
		value("Solid", pag::LayerType::Solid),
		value("Text", pag::LayerType::Text),
		value("Shape", pag::LayerType::Shape),
		value("Image", pag::LayerType::Image),
		value("PreCompose", pag::LayerType::PreCompose),
		value("Camera", pag::LayerType::Camera)
     );

	registration::enumeration<pag::PAGScaleMode>("PAGScaleMode")
     (
		value("None", pag::PAGScaleMode::None),
		value("Stretch", pag::PAGScaleMode::Stretch),
		value("LetterBox", pag::PAGScaleMode::LetterBox),
		value("Zoom", pag::PAGScaleMode::Zoom)
     );

	registration::enumeration<pag::PAGTimeStretchMode>("PAGTimeStretchMode")
     (
		value("None", pag::PAGTimeStretchMode::None),
		value("Scale", pag::PAGTimeStretchMode::Scale),
		value("Repeat", pag::PAGTimeStretchMode::Repeat),
		value("RepeatInverted", pag::PAGTimeStretchMode::RepeatInverted)
     );

	registration::enumeration<pag::ParagraphJustification>("ParagraphJustification")
     (
		value("LeftJustify", pag::ParagraphJustification::LeftJustify),
		value("CenterJustify", pag::ParagraphJustification::CenterJustify),
		value("RightJustify", pag::ParagraphJustification::RightJustify),
		value("FullJustifyLastLineLeft", pag::ParagraphJustification::FullJustifyLastLineLeft),
		value("FullJustifyLastLineRight", pag::ParagraphJustification::FullJustifyLastLineRight),
		value("FullJustifyLastLineCenter", pag::ParagraphJustification::FullJustifyLastLineCenter),
		value("FullJustifyLastLineFull", pag::ParagraphJustification::FullJustifyLastLineFull)
     );

	registration::enumeration<pag::TextDirection>("TextDirection")
     (
		value("Default", pag::TextDirection::Default),
		value("Horizontal", pag::TextDirection::Horizontal),
		value("Vertical", pag::TextDirection::Vertical)
     );

	registration::enumeration<pag::BlendMode>("BlendMode")
     (
		value("Normal", pag::BlendMode::Normal),
		value("Multiply", pag::BlendMode::Multiply),
		value("Screen", pag::BlendMode::Screen),
		value("Overlay", pag::BlendMode::Overlay),
		value("Darken", pag::BlendMode::Darken),
		value("Lighten", pag::BlendMode::Lighten),
		value("ColorDodge", pag::BlendMode::ColorDodge),
		value("ColorBurn", pag::BlendMode::ColorBurn),
		value("HardLight", pag::BlendMode::HardLight),
		value("SoftLight", pag::BlendMode::SoftLight),
		value("Difference", pag::BlendMode::Difference),
		value("Exclusion", pag::BlendMode::Exclusion),
		value("Hue", pag::BlendMode::Hue),
		value("Saturation", pag::BlendMode::Saturation),
		value("Color", pag::BlendMode::Color),
		value("Luminosity", pag::BlendMode::Luminosity),
		value("Add", pag::BlendMode::Add)
     );

	registration::enumeration<pag::TagCode>("TagCode")
     (
		value("End", pag::TagCode::End),
		value("FontTables", pag::TagCode::FontTables),
		value("VectorCompositionBlock", pag::TagCode::VectorCompositionBlock),
		value("CompositionAttributes", pag::TagCode::CompositionAttributes),
		value("ImageTables", pag::TagCode::ImageTables),
		value("LayerBlock", pag::TagCode::LayerBlock),
		value("LayerAttributes", pag::TagCode::LayerAttributes),
		value("SolidColor", pag::TagCode::SolidColor),
		value("TextSource", pag::TagCode::TextSource),
		value("TextMoreOption", pag::TagCode::TextMoreOption),
		value("ImageReference", pag::TagCode::ImageReference),
		value("CompositionReference", pag::TagCode::CompositionReference),
		value("Transform2D", pag::TagCode::Transform2D),
		value("MaskBlock", pag::TagCode::MaskBlock),
		value("ShapeGroup", pag::TagCode::ShapeGroup),
		value("Rectangle", pag::TagCode::Rectangle),
		value("Ellipse", pag::TagCode::Ellipse),
		value("PolyStar", pag::TagCode::PolyStar),
		value("ShapePath", pag::TagCode::ShapePath),
		value("Fill", pag::TagCode::Fill),
		value("Stroke", pag::TagCode::Stroke),
		value("GradientFill", pag::TagCode::GradientFill),
		value("GradientStroke", pag::TagCode::GradientStroke),
		value("MergePaths", pag::TagCode::MergePaths),
		value("TrimPaths", pag::TagCode::TrimPaths),
		value("Repeater", pag::TagCode::Repeater),
		value("RoundCorners", pag::TagCode::RoundCorners),
		value("Performance", pag::TagCode::Performance),
		value("DropShadowStyle", pag::TagCode::DropShadowStyle),
		value("CachePolicy", pag::TagCode::CachePolicy),
		value("FileAttributes", pag::TagCode::FileAttributes),
		value("TimeStretchMode", pag::TagCode::TimeStretchMode),
		value("Mp4Header", pag::TagCode::Mp4Header),
		value("BitmapCompositionBlock", pag::TagCode::BitmapCompositionBlock),
		value("BitmapSequence", pag::TagCode::BitmapSequence),
		value("ImageBytes", pag::TagCode::ImageBytes),
		value("ImageBytesV2", pag::TagCode::ImageBytesV2),
		value("ImageBytesV3", pag::TagCode::ImageBytesV3),
		value("VideoCompositionBlock", pag::TagCode::VideoCompositionBlock),
		value("VideoSequence", pag::TagCode::VideoSequence),
		value("LayerAttributesV2", pag::TagCode::LayerAttributesV2),
		value("MarkerList", pag::TagCode::MarkerList),
		value("ImageFillRule", pag::TagCode::ImageFillRule),
		value("AudioBytes", pag::TagCode::AudioBytes),
		value("MotionTileEffect", pag::TagCode::MotionTileEffect),
		value("LevelsIndividualEffect", pag::TagCode::LevelsIndividualEffect),
		value("CornerPinEffect", pag::TagCode::CornerPinEffect),
		value("BulgeEffect", pag::TagCode::BulgeEffect),
		value("FastBlurEffect", pag::TagCode::FastBlurEffect),
		value("GlowEffect", pag::TagCode::GlowEffect),
		value("LayerAttributesV3", pag::TagCode::LayerAttributesV3),
		value("LayerAttributesExtra", pag::TagCode::LayerAttributesExtra),
		value("TextSourceV2", pag::TagCode::TextSourceV2),
		value("DropShadowStyleV2", pag::TagCode::DropShadowStyleV2),
		value("DisplacementMapEffect", pag::TagCode::DisplacementMapEffect),
		value("ImageFillRuleV2", pag::TagCode::ImageFillRuleV2),
		value("TextSourceV3", pag::TagCode::TextSourceV3),
		value("TextPathOption", pag::TagCode::TextPathOption),
		value("TextAnimator", pag::TagCode::TextAnimator),
		value("TextRangeSelector", pag::TagCode::TextRangeSelector),
		value("TextAnimatorPropertiesTrackingType", pag::TagCode::TextAnimatorPropertiesTrackingType),
		value("TextAnimatorPropertiesTrackingAmount", pag::TagCode::TextAnimatorPropertiesTrackingAmount),
		value("TextAnimatorPropertiesFillColor", pag::TagCode::TextAnimatorPropertiesFillColor),
		value("TextAnimatorPropertiesStrokeColor", pag::TagCode::TextAnimatorPropertiesStrokeColor),
		value("TextAnimatorPropertiesPosition", pag::TagCode::TextAnimatorPropertiesPosition),
		value("TextAnimatorPropertiesScale", pag::TagCode::TextAnimatorPropertiesScale),
		value("TextAnimatorPropertiesRotation", pag::TagCode::TextAnimatorPropertiesRotation),
		value("TextAnimatorPropertiesOpacity", pag::TagCode::TextAnimatorPropertiesOpacity),
		value("TextWigglySelector", pag::TagCode::TextWigglySelector),
		value("RadialBlurEffect", pag::TagCode::RadialBlurEffect),
		value("MosaicEffect", pag::TagCode::MosaicEffect),
		value("EditableIndices", pag::TagCode::EditableIndices),
		value("MaskBlockV2", pag::TagCode::MaskBlockV2),
		value("GradientOverlayStyle", pag::TagCode::GradientOverlayStyle),
		value("BrightnessContrastEffect", pag::TagCode::BrightnessContrastEffect),
		value("HueSaturationEffect", pag::TagCode::HueSaturationEffect),
		value("LayerAttributesExtraV2", pag::TagCode::LayerAttributesExtraV2),
		value("EncryptedData", pag::TagCode::EncryptedData),
		value("Transform3D", pag::TagCode::Transform3D),
		value("CameraOption", pag::TagCode::CameraOption),
		value("StrokeStyle", pag::TagCode::StrokeStyle),
		value("OuterGlowStyle", pag::TagCode::OuterGlowStyle),
		value("ImageScaleModes", pag::TagCode::ImageScaleModes),
		value("Count", pag::TagCode::Count)
     );

	registration::enumeration<pag::PathDataVerb>("PathDataVerb")
     (
		value("MoveTo", pag::PathDataVerb::MoveTo),
		value("LineTo", pag::PathDataVerb::LineTo),
		value("CurveTo", pag::PathDataVerb::CurveTo),
		value("Close", pag::PathDataVerb::Close)
     );

	registration::enumeration<pag::KeyframeInterpolationType>("KeyframeInterpolationType")
     (
		value("None", pag::KeyframeInterpolationType::None),
		value("Linear", pag::KeyframeInterpolationType::Linear),
		value("Bezier", pag::KeyframeInterpolationType::Bezier),
		value("Hold", pag::KeyframeInterpolationType::Hold)
     );

	registration::enumeration<pag::MaskMode>("MaskMode")
     (
		value("None", pag::MaskMode::None),
		value("Add", pag::MaskMode::Add),
		value("Subtract", pag::MaskMode::Subtract),
		value("Intersect", pag::MaskMode::Intersect),
		value("Lighten", pag::MaskMode::Lighten),
		value("Darken", pag::MaskMode::Darken),
		value("Difference", pag::MaskMode::Difference),
		value("Accum", pag::MaskMode::Accum)
     );

	registration::enumeration<pag::EffectType>("EffectType")
     (
		value("Unknown", pag::EffectType::Unknown),
		value("Fill", pag::EffectType::Fill),
		value("MotionTile", pag::EffectType::MotionTile),
		value("LevelsIndividual", pag::EffectType::LevelsIndividual),
		value("CornerPin", pag::EffectType::CornerPin),
		value("Bulge", pag::EffectType::Bulge),
		value("FastBlur", pag::EffectType::FastBlur),
		value("Glow", pag::EffectType::Glow),
		value("DisplacementMap", pag::EffectType::DisplacementMap),
		value("RadialBlur", pag::EffectType::RadialBlur),
		value("Mosaic", pag::EffectType::Mosaic),
		value("BrightnessContrast", pag::EffectType::BrightnessContrast),
		value("HueSaturation", pag::EffectType::HueSaturation)
     );

	registration::enumeration<pag::BlurDimensionsDirection>("BlurDimensionsDirection")
     (
		value("All", pag::BlurDimensionsDirection::All),
		value("Horizontal", pag::BlurDimensionsDirection::Horizontal),
		value("Vertical", pag::BlurDimensionsDirection::Vertical)
     );

	registration::enumeration<pag::DisplacementMapSource>("DisplacementMapSource")
     (
		value("Red", pag::DisplacementMapSource::Red),
		value("Green", pag::DisplacementMapSource::Green),
		value("Blue", pag::DisplacementMapSource::Blue),
		value("Alpha", pag::DisplacementMapSource::Alpha),
		value("Luminance", pag::DisplacementMapSource::Luminance),
		value("Hue", pag::DisplacementMapSource::Hue),
		value("Lightness", pag::DisplacementMapSource::Lightness),
		value("Saturation", pag::DisplacementMapSource::Saturation),
		value("Full", pag::DisplacementMapSource::Full),
		value("Half", pag::DisplacementMapSource::Half),
		value("Off", pag::DisplacementMapSource::Off)
     );

	registration::enumeration<pag::DisplacementMapBehavior>("DisplacementMapBehavior")
     (
		value("CenterMap", pag::DisplacementMapBehavior::CenterMap),
		value("StretchMapToFit", pag::DisplacementMapBehavior::StretchMapToFit),
		value("TileMap", pag::DisplacementMapBehavior::TileMap)
     );

	registration::enumeration<pag::RadialBlurMode>("RadialBlurMode")
     (
		value("Spin", pag::RadialBlurMode::Spin),
		value("Zoom", pag::RadialBlurMode::Zoom)
     );

	registration::enumeration<pag::RadialBlurAntialias>("RadialBlurAntialias")
     (
		value("Low", pag::RadialBlurAntialias::Low),
		value("High", pag::RadialBlurAntialias::High)
     );

	registration::enumeration<pag::ChannelControlType>("ChannelControlType")
     (
		value("Master", pag::ChannelControlType::Master),
		value("Reds", pag::ChannelControlType::Reds),
		value("Yellows", pag::ChannelControlType::Yellows),
		value("Greens", pag::ChannelControlType::Greens),
		value("Cyans", pag::ChannelControlType::Cyans),
		value("Blues", pag::ChannelControlType::Blues),
		value("Magentas", pag::ChannelControlType::Magentas),
		value("Count", pag::ChannelControlType::Count)
     );

	registration::enumeration<pag::LayerStyleType>("LayerStyleType")
     (
		value("Unknown", pag::LayerStyleType::Unknown),
		value("DropShadow", pag::LayerStyleType::DropShadow),
		value("Stroke", pag::LayerStyleType::Stroke),
		value("GradientOverlay", pag::LayerStyleType::GradientOverlay),
		value("OuterGlow", pag::LayerStyleType::OuterGlow)
     );

	registration::enumeration<pag::LayerStylePosition>("LayerStylePosition")
     (
		value("Above", pag::LayerStylePosition::Above),
		value("Blow", pag::LayerStylePosition::Blow)
     );

	registration::enumeration<pag::StrokePosition>("StrokePosition")
     (
		value("Outside", pag::StrokePosition::Outside),
		value("Inside", pag::StrokePosition::Inside),
		value("Center", pag::StrokePosition::Center),
		value("Invalid", pag::StrokePosition::Invalid)
     );

	registration::enumeration<pag::GlowColorType>("GlowColorType")
     (
		value("SingleColor", pag::GlowColorType::SingleColor),
		value("Gradient", pag::GlowColorType::Gradient)
     );

	registration::enumeration<pag::GlowTechniqueType>("GlowTechniqueType")
     (
		value("Softer", pag::GlowTechniqueType::Softer),
		value("Precise", pag::GlowTechniqueType::Precise)
     );

	registration::enumeration<pag::LineCap>("LineCap")
     (
		value("Butt", pag::LineCap::Butt),
		value("Round", pag::LineCap::Round),
		value("Square", pag::LineCap::Square)
     );

	registration::enumeration<pag::LineJoin>("LineJoin")
     (
		value("Miter", pag::LineJoin::Miter),
		value("Round", pag::LineJoin::Round),
		value("Bevel", pag::LineJoin::Bevel)
     );

	registration::enumeration<pag::FillRule>("FillRule")
     (
		value("NonZeroWinding", pag::FillRule::NonZeroWinding),
		value("EvenOdd", pag::FillRule::EvenOdd)
     );

	registration::enumeration<pag::GradientFillType>("GradientFillType")
     (
		value("Linear", pag::GradientFillType::Linear),
		value("Radial", pag::GradientFillType::Radial),
		value("Angle", pag::GradientFillType::Angle),
		value("Reflected", pag::GradientFillType::Reflected),
		value("Diamond", pag::GradientFillType::Diamond)
     );

	registration::enumeration<pag::AnchorPointGrouping>("AnchorPointGrouping")
     (
		value("Character", pag::AnchorPointGrouping::Character),
		value("Word", pag::AnchorPointGrouping::Word),
		value("Line", pag::AnchorPointGrouping::Line),
		value("All", pag::AnchorPointGrouping::All)
     );

	registration::enumeration<pag::TextRangeSelectorUnits>("TextRangeSelectorUnits")
     (
		value("Percentage", pag::TextRangeSelectorUnits::Percentage),
		value("Index", pag::TextRangeSelectorUnits::Index)
     );

	registration::enumeration<pag::TextSelectorBasedOn>("TextSelectorBasedOn")
     (
		value("Characters", pag::TextSelectorBasedOn::Characters),
		value("CharactersExcludingSpaces", pag::TextSelectorBasedOn::CharactersExcludingSpaces),
		value("Words", pag::TextSelectorBasedOn::Words),
		value("Lines", pag::TextSelectorBasedOn::Lines)
     );

	registration::enumeration<pag::TextSelectorMode>("TextSelectorMode")
     (
		value("None", pag::TextSelectorMode::None),
		value("Add", pag::TextSelectorMode::Add),
		value("Subtract", pag::TextSelectorMode::Subtract),
		value("Intersect", pag::TextSelectorMode::Intersect),
		value("Min", pag::TextSelectorMode::Min),
		value("Max", pag::TextSelectorMode::Max),
		value("Difference", pag::TextSelectorMode::Difference)
     );

	registration::enumeration<pag::TextRangeSelectorShape>("TextRangeSelectorShape")
     (
		value("Square", pag::TextRangeSelectorShape::Square),
		value("RampUp", pag::TextRangeSelectorShape::RampUp),
		value("RampDown", pag::TextRangeSelectorShape::RampDown),
		value("Triangle", pag::TextRangeSelectorShape::Triangle),
		value("Round", pag::TextRangeSelectorShape::Round),
		value("Smooth", pag::TextRangeSelectorShape::Smooth)
     );

	registration::enumeration<pag::TextAnimatorTrackingType>("TextAnimatorTrackingType")
     (
		value("BeforeAndAfter", pag::TextAnimatorTrackingType::BeforeAndAfter),
		value("Before", pag::TextAnimatorTrackingType::Before),
		value("After", pag::TextAnimatorTrackingType::After)
     );

	registration::enumeration<pag::TextSelectorType>("TextSelectorType")
     (
		value("Range", pag::TextSelectorType::Range),
		value("Wiggly", pag::TextSelectorType::Wiggly),
		value("Expression", pag::TextSelectorType::Expression)
     );

	registration::enumeration<pag::ShapeType>("ShapeType")
     (
		value("Unknown", pag::ShapeType::Unknown),
		value("ShapeGroup", pag::ShapeType::ShapeGroup),
		value("Rectangle", pag::ShapeType::Rectangle),
		value("Ellipse", pag::ShapeType::Ellipse),
		value("PolyStar", pag::ShapeType::PolyStar),
		value("ShapePath", pag::ShapeType::ShapePath),
		value("Fill", pag::ShapeType::Fill),
		value("Stroke", pag::ShapeType::Stroke),
		value("GradientFill", pag::ShapeType::GradientFill),
		value("GradientStroke", pag::ShapeType::GradientStroke),
		value("MergePaths", pag::ShapeType::MergePaths),
		value("TrimPaths", pag::ShapeType::TrimPaths),
		value("Repeater", pag::ShapeType::Repeater),
		value("RoundCorners", pag::ShapeType::RoundCorners)
     );

	registration::enumeration<pag::PolyStarType>("PolyStarType")
     (
		value("Star", pag::PolyStarType::Star),
		value("Polygon", pag::PolyStarType::Polygon)
     );

	registration::enumeration<pag::CompositeOrder>("CompositeOrder")
     (
		value("BelowPreviousInSameGroup", pag::CompositeOrder::BelowPreviousInSameGroup),
		value("AbovePreviousInSameGroup", pag::CompositeOrder::AbovePreviousInSameGroup)
     );

	registration::enumeration<pag::MergePathsMode>("MergePathsMode")
     (
		value("Merge", pag::MergePathsMode::Merge),
		value("Add", pag::MergePathsMode::Add),
		value("Subtract", pag::MergePathsMode::Subtract),
		value("Intersect", pag::MergePathsMode::Intersect),
		value("ExcludeIntersections", pag::MergePathsMode::ExcludeIntersections)
     );

	registration::enumeration<pag::TrimPathsType>("TrimPathsType")
     (
		value("Simultaneously", pag::TrimPathsType::Simultaneously),
		value("Individually", pag::TrimPathsType::Individually)
     );

	registration::enumeration<pag::RepeaterOrder>("RepeaterOrder")
     (
		value("Below", pag::RepeaterOrder::Below),
		value("Above", pag::RepeaterOrder::Above)
     );

	registration::enumeration<pag::IrisShapeType>("IrisShapeType")
     (
		value("FastRectangle", pag::IrisShapeType::FastRectangle),
		value("Triangle", pag::IrisShapeType::Triangle),
		value("Square", pag::IrisShapeType::Square),
		value("Pentagon", pag::IrisShapeType::Pentagon),
		value("Hexagon", pag::IrisShapeType::Hexagon),
		value("Heptagon", pag::IrisShapeType::Heptagon),
		value("Octagon", pag::IrisShapeType::Octagon),
		value("Nonagon", pag::IrisShapeType::Nonagon),
		value("Decagon", pag::IrisShapeType::Decagon)
     );

	registration::enumeration<pag::TrackMatteType>("TrackMatteType")
     (
		value("None", pag::TrackMatteType::None),
		value("Alpha", pag::TrackMatteType::Alpha),
		value("AlphaInverted", pag::TrackMatteType::AlphaInverted),
		value("Luma", pag::TrackMatteType::Luma),
		value("LumaInverted", pag::TrackMatteType::LumaInverted)
     );

	registration::enumeration<pag::CachePolicy>("CachePolicy")
     (
		value("Auto", pag::CachePolicy::Auto),
		value("Enable", pag::CachePolicy::Enable),
		value("Disable", pag::CachePolicy::Disable)
     );

	registration::enumeration<pag::CompositionType>("CompositionType")
     (
		value("Unknown", pag::CompositionType::Unknown),
		value("Vector", pag::CompositionType::Vector),
		value("Bitmap", pag::CompositionType::Bitmap),
		value("Video", pag::CompositionType::Video)
     );

}
