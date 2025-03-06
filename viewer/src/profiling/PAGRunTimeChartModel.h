#ifndef PROFILING_PAG_RUNTIME_CHART_MODEL_H_
#define PROFILING_PAG_RUNTIME_CHART_MODEL_H_

#include <QObject>
#include <QQmlListProperty>
#include <pag/file.h>

class PAGChartData : public QObject {
  Q_OBJECT
 public:
  PAGChartData(int imageValue, int renderValue, int presentValue);

  Q_PROPERTY(int imageValue   READ getImageValue)
  Q_PROPERTY(int renderValue  READ getRenderValue)
  Q_PROPERTY(int presentValue READ getPresentValue)

  auto getImageValue() const -> int;
  auto getRenderValue() const -> int;
  auto getPresentValue() const -> int;

 public:
  int imageValue;
  int renderValue;
  int presentValue;
  int sum;
};

class PAGRunTimeChartModel : public QObject {
  Q_OBJECT
 public:
  PAGRunTimeChartModel();
  ~PAGRunTimeChartModel() override;

  Q_PROPERTY(int size                             READ getSize)
  Q_PROPERTY(int index                            READ getIndex)
  Q_PROPERTY(int maxValue                         READ getMaxValue)
  Q_PROPERTY(QQmlListProperty<PAGChartData> items READ getItems NOTIFY itemsChange)

  auto setIndex(int index) -> void;

  auto getSize() const -> int;
  auto getIndex() const -> int;
  auto getItems() -> QQmlListProperty<PAGChartData>;
  auto getMaxValue() const -> int;
  auto getColumnItem(int index) const -> PAGChartData*;

  Q_SIGNAL void itemsChange();

  Q_SLOT void addColumnItem(PAGChartData *data);

  auto resetFile(std::shared_ptr<pag::File> file) -> void;
  auto resetColumns(PAGRunTimeChartModel *model) -> void;
  auto clearColumns(bool notify = true) -> void;
  auto updateColumnItem(PAGChartData *data, int index) -> void;

private:
  static auto clearColumns(QQmlListProperty<PAGChartData>* list) -> void;
  static auto getColumnItem(QQmlListProperty<PAGChartData>* list, qsizetype i) -> PAGChartData*;
  static auto getColumnCount(QQmlListProperty<PAGChartData>* list) -> qsizetype;
  static auto appendColumnItem(QQmlListProperty<PAGChartData>* list, PAGChartData* m) -> void;

private:
  int index = 0;
  int maxValue = 0;
  QVector<PAGChartData *> items;
};

#endif // PROFILING_PAG_RUNTIME_CHART_MODEL_H_