#include "PAGRunTimeDataModel.h"

PAGRunTimeData::PAGRunTimeData(int avg, int max, int current, const QString& name, const QString& colorCode)
  : avg(avg), max(max), current(current), name(name), colorCode(colorCode) {

}

auto PAGRunTimeData::getAvg() const -> int {
  return avg;
}

auto PAGRunTimeData::getMax() const -> int {
  return max;
}

auto PAGRunTimeData::getName() const -> QString {
  return name;
}

auto PAGRunTimeData::getCurrent() const -> int {
  return current;
}

auto PAGRunTimeData::getColorCode() const -> QString {
  return colorCode;
}

PAGRunTimeDataModel::PAGRunTimeDataModel(QObject* parent) : QAbstractListModel(parent) {

}

auto PAGRunTimeDataModel::data(const QModelIndex& index, int role) const -> QVariant {
  if ((index.row() < 0) || (index.row() >= items.count())) {
    return {};
  }

  const auto &item = items.at(index.row());
  switch (role) {
    case NameRole: {
      return item.getName();
    }
    case ColorCodeRole: {
      return item.getColorCode();
    }
    case CurrentRole: {
      return item.getCurrent();
    }
    case AvgRole: {
      return item.getAvg();
    }
    case MaxRole: {
      return item.getMax();
    }
    default:
      return {};
  }
}

auto PAGRunTimeDataModel::rowCount(const QModelIndex& parent) const -> int {
  return static_cast<int>(items.count());
}

auto PAGRunTimeDataModel::updateRunTimeData(const PAGRunTimeData &render, const PAGRunTimeData &present, const PAGRunTimeData &decode) -> void {
  beginResetModel();
  items.clear();
  items << render;
  items << decode;
  items << present;
  endResetModel();
}

auto PAGRunTimeDataModel::roleNames() const -> QHash<int, QByteArray> {
  QHash<int, QByteArray> roles;
  roles[NameRole] = "name";
  roles[ColorCodeRole] = "colorCode";
  roles[CurrentRole] = "current";
  roles[AvgRole] = "avg";
  roles[MaxRole] = "max";
  return roles;
}
