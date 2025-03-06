#include "PAGFileInfoModel.h"
#include <pag/file.h>
#include "utils/string.h"

PAGFileInfo::PAGFileInfo(const QString& name, const QString& value, const QString& ext) : name(name), value(value), ext(ext) {

}

auto PAGFileInfo::getExt() const -> QString  {
  return ext;
}

auto PAGFileInfo::getName() const -> QString  {
  return name;
}

auto PAGFileInfo::getValue() const -> QString  {
  return value;
}

auto PAGFileInfo::setExt(const QString& str) -> void {
  ext = str;
}

auto PAGFileInfo::setName(const QString& str) -> void {
  name = str;
}

auto PAGFileInfo::setValue(const QString& str) -> void {
  value = str;
}

PAGFileInfoModel::PAGFileInfoModel(QObject* parent) : QAbstractListModel(parent) {
  beginInsertRows(QModelIndex(), rowCount(), rowCount());
  fileInfos << PAGFileInfo("Duration", "", "s");
  fileInfos << PAGFileInfo("FrameRate", "", "FPS");
  fileInfos << PAGFileInfo("Width");
  fileInfos << PAGFileInfo("Height");
  fileInfos << PAGFileInfo("Graphics", "", "");
  fileInfos << PAGFileInfo("Videos");
  fileInfos << PAGFileInfo("Layers");
  fileInfos << PAGFileInfo("SDK Version");
  endInsertRows();
}

auto PAGFileInfoModel::updateFileInfo(const std::shared_ptr<pag::PAGFile>& pagFile, std::string filePath) -> void {
  const auto& file = pagFile;
  beginResetModel();
  updateDisplayFileInfo(PAGFileInfo("Duration", Utils::toQString(file->duration() / 1000000.0)));
  updateDisplayFileInfo(PAGFileInfo("FrameRate", Utils::toQString(file->frameRate())));
  updateDisplayFileInfo(PAGFileInfo("Width", Utils::toQString(file->width())));
  updateDisplayFileInfo(PAGFileInfo("Height", Utils::toQString(file->height())));
  auto memorySize = pag::CalculateGraphicsMemory(file->getFile());
  updateDisplayFileInfo(PAGFileInfo("Graphics", Utils::getMemorySizeNumString(memorySize), Utils::getMemorySizeUnit(memorySize)));
  updateDisplayFileInfo(PAGFileInfo("Videos", Utils::toQString(file->numVideos())));
  updateDisplayFileInfo(PAGFileInfo("Layers", Utils::toQString(file->getFile()->numLayers())));
  auto version = Utils::tagCodeToVersion(file->tagLevel());
  updateDisplayFileInfo(PAGFileInfo("SDK Version", version.c_str()));
  endResetModel();
}

auto PAGFileInfoModel::data(const QModelIndex& index, int role) const -> QVariant {
  if ((index.row() < 0) || (index.row() >= fileInfos.count())) {
    return {};
  }

  const PAGFileInfo &fileInfo = fileInfos[index.row()];
  if (role == NameRole) {
    return fileInfo.getName();
  } else if (role == ValueRole) {
    return fileInfo.getValue();
  } else if (role == ExtRole) {
    return fileInfo.getExt();
  }

  return {};
}

auto PAGFileInfoModel::rowCount(const QModelIndex& parent) const -> int {
  Q_UNUSED(parent);
  return static_cast<int>(fileInfos.count());
}

auto PAGFileInfoModel::roleNames() const -> QHash<int, QByteArray> {
  QHash<int, QByteArray> roles;
  roles[NameRole] = "name";
  roles[ValueRole] = "value";
  roles[ExtRole] = "ext";
  return roles;
}

auto PAGFileInfoModel::updateDisplayFileInfo(const PAGFileInfo& displayInfo) -> void {
  for (auto& fileInfo : fileInfos) {
    if (fileInfo.getName().compare(displayInfo.getName()) == 0) {
      fileInfo.setValue(displayInfo.getValue());
      if (!displayInfo.getExt().isEmpty()) {
        fileInfo.setExt(displayInfo.getExt());
      }
      break;
    }
  }
}