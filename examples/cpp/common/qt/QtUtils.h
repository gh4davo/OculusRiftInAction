/************************************************************************************

 Authors     :   Bradley Austin Davis <bdavis@saintandreas.org>
 Copyright   :   Copyright Brad Davis. All Rights reserved.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

 http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.

 ************************************************************************************/

#pragma once

#include <QtWidgets>
#include <QOpenGLWidget>
#include <QPixmap>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFramebufferObject>
#include <QOffscreenSurface>
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QQuickRenderControl>


namespace oria { namespace qt {
  inline vec2 toGlm(const QSize & size) {
    return vec2(size.width(), size.height());
  }

  inline vec2 toGlm(const QPointF & pt) {
    return vec2(pt.x(), pt.y());
  }

  inline QSize sizeFromGlm(const vec2 & size) {
    return QSize(size.x, size.y);
  }

  inline QPointF pointFromGlm(const vec2 & pt) {
    return QPointF(pt.x, pt.y);
  }

  template<typename T> 
  T toQtType(Resource res) {
    T result;
    size_t size = Resources::getResourceSize(res);
    result.resize(size);
    Resources::getResourceData(res, result.data());
    return result;
  }

  inline QByteArray toByteArray(Resource res) {
    return toQtType<QByteArray>(res);
  }

  inline QString toString(Resource res) {
    QByteArray data = toByteArray(res);
    return QString::fromUtf8(data.data(), data.size());
  }

  inline QImage loadImageResource(Resource res) {
    QImage image;
    image.loadFromData(toByteArray(res));
    return image;
  }

  inline QPixmap loadXpmResource(Resource res) {
    QString cursorXpmStr = oria::qt::toString(res);
    QStringList list = cursorXpmStr.split(QRegExp("\\n|\\r\\n|\\r"));
    std::vector<QByteArray> bv;
    std::vector<const char*> v;
    foreach(QString line, list) {
      bv.push_back(line.toLocal8Bit());
      v.push_back(*bv.rbegin());
    }
    QPixmap result = QPixmap(&v[0]);
    return result;
  }


} } // namespaces
/**
 * Forwards mouse and keyboard input from the specified widget to the
 * graphics view, allowing the user to click on one widget (like an
 * OpenGl window with controls rendered inside it) and have the resulting
 * click reflected on the scene displayed in this view.
 */
class ForwardingGraphicsView : public QGraphicsView {
  QWidget * filterTarget{ nullptr };

public:
  ForwardingGraphicsView(QWidget * filterTarget = nullptr);

  void install(QWidget * filterTarget);
  void remove(QWidget * filterTarget);

protected:
  void forwardMouseEvent(QMouseEvent * event);
  void forwardKeyEvent(QKeyEvent * event);
  void resizeEvent(QResizeEvent *event);
  bool eventFilter(QObject *object, QEvent *event);
};


class PaintlessOpenGLWidget : public QOpenGLWidget {
protected:
  bool event(QEvent * e) {
    if (QEvent::Paint == e->type()) {
      return true;
    }
    return QOpenGLWidget::event(e);
  }

public:
  explicit PaintlessOpenGLWidget() : QOpenGLWidget() {
  }
};


class DelegatingOpengGLWindow : public PaintlessOpenGLWidget {
  typedef std::function<void()> Callback;
  typedef std::function<void(int, int)> ResizeCallback;


  Callback paintCallback;
  Callback initCallback;
  ResizeCallback resizeCallback;

protected:
  void initializeGL() {
    initCallback();
  }

  void resizeGL(int w, int h) {
    resizeCallback(w, h);
  }

  void paintGL() {
    paintCallback();
    update();
  }

public:
  explicit DelegatingOpengGLWindow(
    Callback paint, Callback init = []{}, ResizeCallback resize = [](int, int){}) :
    PaintlessOpenGLWidget(), paintCallback(paint), initCallback(init), resizeCallback(resize) {
  }
};


class OffscreenUiWindow : public QObject {
  //Q_OBJECT

  QOpenGLContext *m_context{ nullptr };
  QOffscreenSurface *m_offscreenSurface{ nullptr };
  QQuickRenderControl *m_renderControl{ nullptr };
  QQuickWindow *m_quickWindow{ nullptr };
  QQuickItem *m_rootItem{ nullptr };
  QTimer m_updateTimer;

protected:
  QOpenGLFramebufferObject *m_fbo{ nullptr };
  QQmlEngine *m_qmlEngine{ nullptr };
  QQmlComponent *m_qmlComponent{ nullptr };

public:
  OffscreenUiWindow(const QSize & size, QOpenGLContext * sharedContext);

  virtual void setupScene();

  virtual void loadSceneComponents() = 0;

  void createFbo();

  void destroyFbo();

  void updateSizes();

  void requestUpdate();

  void resizeEvent(QResizeEvent *);

  virtual void updateQuick();
};

class LambdaThread : public QThread {
  Lambda f;

  void run() {
    f();
  }

public:
  LambdaThread() {}

  template <typename F>
  LambdaThread(F f) : f(f) {}

  template <typename F>
  void setLambda(F f) { this->f = f; }
};


#ifdef OS_WIN
#define QT_APP_WITH_ARGS(AppClass) \
  int argc = 1; \
  char ** argv = &lpCmdLine;  \
  AppClass app(argc, argv);
#else
#define QT_APP_WITH_ARGS(AppClass) AppClass app(argc, argv);
#endif

#define RUN_QT_APP(AppClass) \
MAIN_DECL { \
  try { \
    qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", "."); \
    QT_APP_WITH_ARGS(AppClass); \
    return app.exec(); \
  } catch (std::exception & error) { \
    SAY_ERR(error.what()); \
  } catch (const std::string & error) { \
    SAY_ERR(error.c_str()); \
  } \
  return -1; \
}


