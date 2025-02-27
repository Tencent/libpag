#include "PAGQuickItemTypes.h"

TextureNode::TextureNode(QQuickWindow* window) : window(window) {
  // Our texture node must have a texture, so use the default 0 texture.
  QImage image(1, 1, QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  texture = window->createTextureFromImage(image);
  setTexture(texture);
  setFiltering(QSGTexture::Linear);
}

TextureNode::~TextureNode() {
  delete texture;
}