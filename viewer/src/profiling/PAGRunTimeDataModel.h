#ifndef PROFILING_PAG_RUNTIME_DATA_MODEL_H_
#define PROFILING_PAG_RUNTIME_DATA_MODEL_H_

#include <QString>
#include <QObject>
#include <QAbstractListModel>

class PAGRunTimeData {
 public:
  PAGRunTimeData(int avg, int max, int current, const QString& name, const QString& colorCode);

  auto getAvg() const -> int;
  auto getMax() const -> int;
  auto getName() const -> QString;
  auto getCurrent() const -> int;
  auto getColorCode() const -> QString;

 public:
  int avg;
  int max;
  int current;
  QString name;
  QString colorCode;
};

class PAGRunTimeDataModel : public QAbstractListModel {
  Q_OBJECT
 public:
  enum PAGRunTimeDataRoles {
    NameRole = Qt::UserRole + 1,
    ColorCodeRole,
    CurrentRole,
    AvgRole,
    MaxRole
  };

  explicit PAGRunTimeDataModel(QObject* parent = nullptr);

  auto data(const QModelIndex& index, int role) const -> QVariant override;
  auto rowCount(const QModelIndex& parent) const -> int override;
  auto updateRunTimeData(const PAGRunTimeData &render, const PAGRunTimeData &present, const PAGRunTimeData &decode) -> void;

 protected:
  auto roleNames() const -> QHash<int, QByteArray> override;

 private:
  QList<PAGRunTimeData> items;
};

#endif // PROFILING_PAG_RUNTIME_DATA_MODEL_H_