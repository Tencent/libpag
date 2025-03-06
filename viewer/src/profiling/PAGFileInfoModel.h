#ifndef PROFILING_PAG_FILEINFO_MODEL_H_
#define PROFILING_PAG_FILEINFO_MODEL_H_

#include <QList>
#include <QString>
#include <QAbstractListModel>
#include <pag/pag.h>

class PAGFileInfo
{
public:
  PAGFileInfo(const QString& name = "", const QString& value = "", const QString& ext = "");

  auto getExt() const -> QString;
  auto getName() const -> QString;
  auto getValue() const -> QString;

  auto setExt(const QString& str) -> void;
  auto setName(const QString& str) -> void;
  auto setValue(const QString& str) -> void;

private:
  QString ext;
  QString name;
  QString value;
};

class PAGFileInfoModel : public QAbstractListModel {
  Q_OBJECT
 public:
  enum PAGFileInfoRoles {
    NameRole = Qt::UserRole + 1,
    ValueRole,
    ExtRole
  };

  explicit PAGFileInfoModel(QObject* parent = nullptr);

  Q_SLOT void updateFileInfo(const std::shared_ptr<pag::PAGFile>& pagFile, std::string filePath);

  auto data(const QModelIndex& index, int role) const -> QVariant override;
  auto rowCount(const QModelIndex& parent = QModelIndex()) const -> int override;

 protected:
  auto roleNames() const -> QHash<int, QByteArray> override;

 private:
  auto updateDisplayFileInfo(const PAGFileInfo& displayInfo) -> void;

  QList<PAGFileInfo> fileInfos;
};

#endif // PROFILING_PAG_FILEINFO_MODEL_H_