#ifndef RENDERING_PAGQUICKITEM_TYPES_H_
#define RENDERING_PAGQUICKITEM_TYPES_H_

#include <QSize>
#include <QQuickWindow>
#include <QSGSimpleTextureNode>
#include <QtGui/QOpenGLFunctions>

class PAGQuickItemProtocol {
 public:
  virtual ~PAGQuickItemProtocol() = default;
  virtual auto getFboSize() const -> QSize = 0;
  virtual auto getFramebufferId() const -> GLint = 0;
};

class TextureNode : public QObject, public QSGSimpleTextureNode {
  Q_OBJECT
 public:
  TextureNode(QQuickWindow* window);
  ~TextureNode() override;

 private:
  QSGTexture* texture{nullptr};
  QQuickWindow* window{nullptr};
};

#endif // RENDERING_PAGQUICKITEM_TYPES_H_