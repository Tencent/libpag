#include "PAGRunTimeChartModel.h"

PAGChartData::PAGChartData(int imageValue, int renderValue, int presentValue)
  : imageValue(imageValue), renderValue(renderValue), presentValue(presentValue){
  sum = imageValue + renderValue + presentValue;
}

auto PAGChartData::getImageValue() const -> int {
  return imageValue;
}

auto PAGChartData::getRenderValue() const -> int {
  return renderValue;
}

auto PAGChartData::getPresentValue() const -> int {
  return presentValue;
}

PAGRunTimeChartModel::PAGRunTimeChartModel() = default;

PAGRunTimeChartModel::~PAGRunTimeChartModel() {
  clearColumns(false);
}

auto PAGRunTimeChartModel::setIndex(int index) -> void {
  this->index = index;
}

auto PAGRunTimeChartModel::getSize() const -> int {
  return static_cast<int>(items.size());
}

auto PAGRunTimeChartModel::getIndex() const -> int {
  return index;
}

auto PAGRunTimeChartModel::getItems() -> QQmlListProperty<PAGChartData> {
   return {this, this, &PAGRunTimeChartModel::appendColumnItem, &PAGRunTimeChartModel::getColumnCount,
     &PAGRunTimeChartModel::getColumnItem,&PAGRunTimeChartModel::clearColumns};
}

auto PAGRunTimeChartModel::getMaxValue() const -> int {
  return maxValue;
}

auto PAGRunTimeChartModel::getColumnItem(int index) const -> PAGChartData* {
  return items.at(index);
}

auto PAGRunTimeChartModel::addColumnItem(PAGChartData *data) -> void {
  auto* tmp = new PAGChartData(data->imageValue, data->renderValue, data->presentValue);
  items.append(tmp);
  maxValue = std::max(tmp->sum, maxValue);
  index = static_cast<int>(items.size());
  Q_EMIT itemsChange();
}

auto PAGRunTimeChartModel::resetFile(std::shared_ptr<pag::File> file) -> void {
  clearColumns(true);
  PAGChartData model(2, 2, 2);
  addColumnItem(&model);
}

auto PAGRunTimeChartModel::resetColumns(PAGRunTimeChartModel *model) -> void {
  if (this != model) {
    clearColumns(false);
  }
  for (auto const& item : model->items) {
    auto* tmp = new PAGChartData(item->imageValue, item->renderValue, item->presentValue);
    items.append(tmp);
  }
  index = model->index;
  maxValue = model->maxValue;
  Q_EMIT itemsChange();
}

auto PAGRunTimeChartModel::clearColumns(bool notify) -> void {
  index = 0;
  maxValue = 0;
  QVector<PAGChartData *> vector;
  vector.swap(items);
  for (int i = 0; i < vector.count(); ++i) {
    delete vector[i];
  }
  vector.clear();

  if (notify) {
    Q_EMIT itemsChange();
  }
}

auto PAGRunTimeChartModel::updateColumnItem(PAGChartData *data, int index) -> void {
  while (items.size() <= index) {
    auto* tmp = new PAGChartData(0, 0, 0);
    items.append(tmp);
  }
  items[index]->imageValue = data->imageValue;
  items[index]->renderValue = data->renderValue;
  items[index]->presentValue = data->presentValue;
  maxValue = 0;
  // TODO Improve the code
  for (auto const& item : items) {
    int64_t sum = item->presentValue + item->renderValue + item->imageValue;
    maxValue = maxValue < sum ? sum : maxValue;
  }
  this->index = index;
  Q_EMIT itemsChange();
}

auto PAGRunTimeChartModel::clearColumns(QQmlListProperty<PAGChartData>* list) -> void {
  reinterpret_cast<PAGRunTimeChartModel* >(list->data)->clearColumns();
}

auto PAGRunTimeChartModel::getColumnItem(QQmlListProperty<PAGChartData>* list, qsizetype i) -> PAGChartData* {
  return reinterpret_cast<PAGRunTimeChartModel* >(list->data)->getColumnItem(static_cast<int>(i));
}

auto PAGRunTimeChartModel::getColumnCount(QQmlListProperty<PAGChartData>* list) -> qsizetype {
  return reinterpret_cast<PAGRunTimeChartModel* >(list->data)->getSize();
}

auto PAGRunTimeChartModel::appendColumnItem(QQmlListProperty<PAGChartData>* list, PAGChartData* m) -> void {
  reinterpret_cast<PAGRunTimeChartModel* >(list->data)->addColumnItem(m);
}