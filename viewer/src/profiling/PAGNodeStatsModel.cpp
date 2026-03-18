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

#include "PAGNodeStatsModel.h"
#include <algorithm>
#include <unordered_map>

namespace pag {

// Color palette for different node categories
static const std::vector<QString> NodeColors = {
    "#0096D8",  // Blue
    "#74AD59",  // Green
    "#DDB259",  // Yellow
    "#E06F42",  // Orange
    "#9B59B6",  // Purple
    "#3498DB",  // Light Blue
    "#E74C3C",  // Red
    "#1ABC9C",  // Teal
};

// Categorize node types for simpler display
static QString GetNodeCategory(pagx::NodeType type) {
  switch (type) {
    case pagx::NodeType::Layer:
    case pagx::NodeType::Composition:
      return "Layers";
    case pagx::NodeType::Group:
      return "Groups";
    case pagx::NodeType::Text:
    case pagx::NodeType::GlyphRun:
    case pagx::NodeType::Font:
    case pagx::NodeType::Glyph:
    case pagx::NodeType::TextModifier:
    case pagx::NodeType::TextPath:
    case pagx::NodeType::TextBox:
    case pagx::NodeType::RangeSelector:
      return "Text";
    case pagx::NodeType::Path:
    case pagx::NodeType::Rectangle:
    case pagx::NodeType::Ellipse:
    case pagx::NodeType::Polystar:
    case pagx::NodeType::PathData:
      return "Shapes";
    case pagx::NodeType::Fill:
    case pagx::NodeType::Stroke:
      return "Painters";
    case pagx::NodeType::Image:
    case pagx::NodeType::ImagePattern:
      return "Images";
    case pagx::NodeType::SolidColor:
    case pagx::NodeType::LinearGradient:
    case pagx::NodeType::RadialGradient:
    case pagx::NodeType::ConicGradient:
    case pagx::NodeType::DiamondGradient:
    case pagx::NodeType::ColorStop:
      return "Colors";
    case pagx::NodeType::TrimPath:
    case pagx::NodeType::RoundCorner:
    case pagx::NodeType::MergePath:
    case pagx::NodeType::Repeater:
      return "Modifiers";
    case pagx::NodeType::DropShadowStyle:
    case pagx::NodeType::InnerShadowStyle:
    case pagx::NodeType::BackgroundBlurStyle:
    case pagx::NodeType::BlurFilter:
    case pagx::NodeType::DropShadowFilter:
    case pagx::NodeType::InnerShadowFilter:
    case pagx::NodeType::BlendFilter:
    case pagx::NodeType::ColorMatrixFilter:
      return "Effects";
    default:
      return "Other";
  }
}

PAGNodeStatItem::PAGNodeStatItem(const QString& name, const QString& color, int count)
    : name(name), color(color), count(count) {
}

PAGNodeStatsModel::PAGNodeStatsModel() : QAbstractListModel(nullptr) {
}

PAGNodeStatsModel::PAGNodeStatsModel(QObject* parent) : QAbstractListModel(parent) {
}

QVariant PAGNodeStatsModel::data(const QModelIndex& index, int role) const {
  if ((index.row() < 0) || (index.row() >= static_cast<int>(statItems.size()))) {
    return {};
  }

  const PAGNodeStatItem& item = statItems[index.row()];
  auto statsRole = static_cast<PAGNodeStatsRoles>(role);
  if (statsRole == PAGNodeStatsRoles::NameRole) {
    return item.name;
  }
  if (statsRole == PAGNodeStatsRoles::ColorRole) {
    return item.color;
  }
  if (statsRole == PAGNodeStatsRoles::CountRole) {
    return item.count;
  }

  return {};
}

int PAGNodeStatsModel::rowCount(const QModelIndex& parent) const {
  Q_UNUSED(parent);
  return static_cast<int>(statItems.size());
}

int PAGNodeStatsModel::getTotalCount() const {
  return totalCount;
}

int PAGNodeStatsModel::getCount() const {
  return static_cast<int>(statItems.size());
}

void PAGNodeStatsModel::setPAGXDocument(std::shared_ptr<pagx::PAGXDocument> pagxDocument) {
  statItems.clear();
  totalCount = 0;

  if (pagxDocument == nullptr) {
    beginResetModel();
    endResetModel();
    Q_EMIT totalCountChanged();
    Q_EMIT countChanged();
    return;
  }

  // Count nodes by category
  std::unordered_map<QString, int> categoryCounts;
  for (const auto& node : pagxDocument->nodes) {
    QString category = GetNodeCategory(node->nodeType());
    categoryCounts[category]++;
    totalCount++;
  }

  // Convert to vector and sort by count (descending)
  std::vector<std::pair<QString, int>> sortedStats;
  sortedStats.reserve(categoryCounts.size());
  for (const auto& pair : categoryCounts) {
    sortedStats.emplace_back(pair.first, pair.second);
  }
  std::sort(sortedStats.begin(), sortedStats.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

  // Create stat items with colors
  size_t colorIndex = 0;
  for (const auto& pair : sortedStats) {
    QString color = NodeColors[colorIndex % NodeColors.size()];
    statItems.emplace_back(pair.first, color, pair.second);
    colorIndex++;
  }

  beginResetModel();
  endResetModel();
  Q_EMIT totalCountChanged();
  Q_EMIT countChanged();
}

QHash<int, QByteArray> PAGNodeStatsModel::roleNames() const {
  static QHash<int, QByteArray> roles = {
      {static_cast<int>(PAGNodeStatsRoles::NameRole), "name"},
      {static_cast<int>(PAGNodeStatsRoles::ColorRole), "colorCode"},
      {static_cast<int>(PAGNodeStatsRoles::CountRole), "count"}};
  return roles;
}

}  // namespace pag
