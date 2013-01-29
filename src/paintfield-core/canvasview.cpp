#include <QtGui>

#include "drawutil.h"
#include "tool.h"
#include "layerrenderer.h"
#include "debug.h"
#include "canvascontroller.h"
#include "appcontroller.h"
#include "abstractcanvasviewportcontroller.h"
#include "canvasviewportcontrollersoftware.h"
#include "canvasviewportcontrollergl.h"

#include "canvasview.h"

namespace PaintField
{

using namespace Malachite;


class CanvasRenderer : public LayerRenderer
{
public:
	CanvasRenderer() : LayerRenderer() {}
	
	void setTool(Tool *tool) { _tool = tool; }
	
protected:
	
	void drawLayer(SurfacePainter *painter, const Layer *layer)
	{
		if (_tool && _tool->customDrawLayers().contains(layer))
			_tool->drawLayer(painter, layer);
		else
			LayerRenderer::drawLayer(painter, layer);
	}
	
private:
	
	Tool *_tool = 0;
};

struct Navigation
{
	double scale = 1, rotation = 0;
	QPoint translation;
};

class CanvasView::Data
{
public:
	
	CanvasController *canvas = 0;
	ScopedQObjectPointer<Tool> tool;
	
	double mousePressure = 0;
	QRect repaintRect;
	QRect prevCustomCursorRect;
	Malachite::Vec2D customCursorPos;
	bool tabletActive = false;
	
	KeyTracker keyTracker;
	
	QCursor toolCursor;
	
	QKeySequence scaleKeys, rotationKeys, translationKeys;
	
	bool isScalingByDrag = false, isRotatingByDrag = false, isTranslatingByDrag = false;
	QPoint navigationOrigin;
	
	Navigation nav, backupNav, memorizedNav;
	bool mirror = false;
	
	QSize sceneSize;
	QPoint viewCenter;
	
	QTransform transformToScene, transformFromScene;
	
	AbstractCanvasViewportController *viewportController = 0;
};

CanvasView::CanvasView(CanvasController *canvas, QWidget *parent) :
    QWidget(parent),
    d(new Data)
{
	d->canvas = canvas;
	d->sceneSize = canvas->document()->size();
	
	// setup viewport
	{
		d->viewportController = new CanvasViewportControllerGL(this);
		connect(d->viewportController, SIGNAL(ready()), this, SLOT(onViewportReady()));
		
		auto layout = new QVBoxLayout;
		layout->addWidget(d->viewportController->view());
		d->viewportController->view()->setMouseTracking(true);
		layout->setContentsMargins(0, 0, 0 ,0);
		setLayout(layout);
		
		d->viewCenter = QPoint(width() / 2, height() / 2);
	}
	
	connect(layerModel(), SIGNAL(tilesUpdated(QPointSet)),
	        this, SLOT(updateTiles(QPointSet)));
	
	connect(appController()->app(), SIGNAL(tabletActiveChanged(bool)),
	        this, SLOT(onTabletActiveChanged(bool)));
	onTabletActiveChanged(appController()->app()->isTabletActive());
	
	// setup key bindings
	{
		auto keyBindingHash = appController()->settingsManager()->keyBindingHash();
		
		d->translationKeys = keyBindingHash["paintfield.canvas.dragTranslation"];
		d->scaleKeys = keyBindingHash["paintfield.canvas.dragScale"];
		d->rotationKeys = keyBindingHash["paintfield.canvas.dragRotation"];
	}
	
	// set widget properties
	{
		setMouseTracking(true);
	}
}

CanvasView::~CanvasView()
{
	while (qApp->overrideCursor())
		qApp->restoreOverrideCursor();
	
	delete d;
}

double CanvasView::scale() const
{
	return d->nav.scale;
}

double CanvasView::rotation() const
{
	return d->nav.rotation;
}

QPoint CanvasView::translation() const
{
	return d->nav.translation;
}

QPoint CanvasView::viewCenter() const
{
	return d->viewCenter;
}

void CanvasView::setScale(double value)
{
	if (d->nav.scale != value)
	{
		d->nav.scale = value;
		emit scaleChanged(value);
	}
}

void CanvasView::setRotation(double value)
{
	if (d->nav.rotation != value)
	{
		d->nav.rotation = value;
		emit rotationChanged(value);
	}
}

void CanvasView::setTranslation(const QPoint &value)
{
	if (d->nav.translation != value)
	{
		d->nav.translation = value;
		emit translationChanged(value);
	}
}

void CanvasView::memorizeNavigation()
{
	d->memorizedNav = d->nav;
}

void CanvasView::restoreNavigation()
{
	d->nav = d->memorizedNav;
}

QTransform CanvasView::transformToScene() const
{
	return d->transformToScene;
}

QTransform CanvasView::transformFromScene() const
{
	return d->transformFromScene;
}

void CanvasView::updateTransforms()
{
	QTransform transform;
	
	PAINTFIELD_DEBUG << d->sceneSize;
	PAINTFIELD_DEBUG << viewCenter();
	
	QPoint sceneOffset = QPoint(d->sceneSize.width(), d->sceneSize.height()) / 2;
	QPoint viewOffset = viewCenter() + translation();
	
	transform.translate(-sceneOffset.x(), -sceneOffset.y());
	transform.scale(scale(), scale());
	transform.rotate(rotation());
	transform.translate(viewOffset.x(), viewOffset.y());
	
	d->transformFromScene = transform;
	d->transformToScene = transform.inverted();
	d->viewportController->setTransform(transform);
	
	update();
}

CanvasController *CanvasView::controller()
{
	return d->canvas;
}

Document *CanvasView::document()
{
	return d->canvas->document();
}

LayerModel *CanvasView::layerModel()
{
	return d->canvas->layerModel();
}

void CanvasView::setTool(Tool *tool)
{
	d->tool.reset(tool);
	
	if (tool)
	{
		connect(tool, SIGNAL(requestUpdate(QPointSet)), this, SLOT(updateTiles(QPointSet)));
		connect(tool, SIGNAL(requestUpdate(QHash<QPoint,QRect>)), this, SLOT(updateTiles(QHash<QPoint,QRect>)));
		
		if (tool->isCustomCursorEnabled())
		{
			d->toolCursor = QCursor(Qt::BlankCursor);
			setCursor(QCursor(Qt::BlankCursor));
		}
		else
		{
			connect(tool, SIGNAL(cursorChanged(QCursor)), this, SLOT(onToolCursorChanged(QCursor)));
			d->toolCursor = tool->cursor();
			setCursor(tool->cursor());
		}
	}
}

void CanvasView::updateTiles(const QPointSet &keys, const QHash<QPoint, QRect> &rects)
{
	CanvasRenderer renderer;
	renderer.setTool(d->tool.data());
	
	Surface surface = renderer.renderToSurface(layerModel()->rootLayer()->children(), keys, rects);
	
	QPointSet renderKeys = rects.isEmpty() ? keys : rects.keys().toSet();

	for (const QPoint &key : renderKeys)
	{
		QRect rect;
		
		if (!rects.isEmpty())
		{
			rect = rects.value(key);
			if (rect.isEmpty())
				continue;
		}
		else
		{
			rect = QRect(0, 0, Surface::TileSize, Surface::TileSize);
		}
		
		Image image(rect.size());
		image.fill(Color::fromRgbValue(1,1,1).toPixel());
		
		if (surface.contains(key))
		{
			Painter painter(&image);
			painter.drawTransformedImage(-rect.topLeft(), surface.tileForKey(key));
		}
		
		d->viewportController->updateTile(key, image, rect.topLeft());
	}
	
	d->viewportController->update();
}

void CanvasView::onClicked()
{
	setFocus();
	controller()->workspace()->setCurrentCanvas(controller());
}

void CanvasView::onToolCursorChanged(const QCursor &cursor)
{
	setCursor(cursor);
}

void CanvasView::onTabletActiveChanged(bool active)
{
	d->tabletActive = active;
}

void CanvasView::onViewportReady()
{
	d->viewportController->setDocumentSize(d->sceneSize);
	updateTransforms();
	updateTiles(layerModel()->document()->tileKeys());
}

void CanvasView::keyPressEvent(QKeyEvent *event)
{
	if (d->tool)
		d->tool->toolEvent(event);
	
	PAINTFIELD_DEBUG << "pressed:" << event->key() << "modifiers" << event->modifiers();
	d->keyTracker.keyPressed(event->key());
}

void CanvasView::keyReleaseEvent(QKeyEvent *event)
{
	if (d->tool)
		d->tool->toolEvent(event);
	
	PAINTFIELD_DEBUG << "released:" << event->key() << "modifiers" << event->modifiers();
	d->keyTracker.keyReleased(event->key());
}

void CanvasView::enterEvent(QEvent *e)
{
	super::enterEvent(e);
	setFocus();
	qApp->setOverrideCursor(d->toolCursor);
}

void CanvasView::leaveEvent(QEvent *e)
{
	super::leaveEvent(e);
	
	while (qApp->overrideCursor())
		qApp->restoreOverrideCursor();
}

void CanvasView::mouseDoubleClickEvent(QMouseEvent *event)
{
	if (d->tool)
		event->setAccepted(sendCanvasMouseEvent(event));
}

void CanvasView::mousePressEvent(QMouseEvent *event)
{
	onClicked();
	
	if (event->button() == Qt::LeftButton)
	{
		if (tryBeginDragNavigation(event->pos()))
		{
			event->accept();
			return;
		}
	}
	
	if (d->tool)
		event->setAccepted(sendCanvasTabletEvent(event) || sendCanvasMouseEvent(event));
}

void CanvasView::mouseMoveEvent(QMouseEvent *event)
{
	if (continueDragNavigation(event->pos()))
	{
		event->accept();
		return;
	}
	
	if (d->tool)
		event->setAccepted(sendCanvasTabletEvent(event) || sendCanvasMouseEvent(event));
}

void CanvasView::mouseReleaseEvent(QMouseEvent *event)
{
	endDragNavigation();
	
	if (d->tool)
		event->setAccepted(sendCanvasTabletEvent(event) || sendCanvasMouseEvent(event));
}

void CanvasView::tabletEvent(QTabletEvent *event)
{
	auto toNewEventType = [](QEvent::Type type)
	{
		switch (type)
		{
			default:
			case QEvent::TabletMove:
				return PaintField::EventWidgetTabletMove;
			case QEvent::TabletPress:
				return PaintField::EventWidgetTabletPress;
			case QEvent::TabletRelease:
				return PaintField::EventWidgetTabletRelease;
		}
	};
	
	switch (event->type())
	{
		case QEvent::TabletPress:
			
			onClicked();
			if (tryBeginDragNavigation(event->pos()))
			{
				event->accept();
				return;
			}
			break;
			
		case QEvent::TabletMove:
			
			if (continueDragNavigation(event->pos()))
			{
				event->accept();
				return;
			}
			break;
			
		case QEvent::TabletRelease:
			
			endDragNavigation();
			break;
			
		default:
			break;
	}
	
	TabletInputData data(event->hiResGlobalPos(), event->pressure(), event->rotation(), event->tangentialPressure(), Vec2D(event->xTilt(), event->yTilt()));
	WidgetTabletEvent widgetTabletEvent(toNewEventType(event->type()), event->globalPos(), event->pos(), data, event->modifiers());
	
	customTabletEvent(&widgetTabletEvent);
	event->setAccepted(widgetTabletEvent.isAccepted());
}

void CanvasView::customTabletEvent(WidgetTabletEvent *event)
{
	
	switch (int(event->type()))
	{
		case EventWidgetTabletPress:
			
			onClicked();
			if (tryBeginDragNavigation(event->posInt))
			{
				event->accept();
				return;
			}
			break;
			
		case EventWidgetTabletMove:
			
			if (continueDragNavigation(event->posInt))
			{
				event->accept();
				return;
			}
			break;
			
		case EventWidgetTabletRelease:
			
			endDragNavigation();
			break;
			
		default:
			break;
	}
	
	if (d->tool)
	{
		event->setAccepted(sendCanvasTabletEvent(event));
	}
}

void CanvasView::resizeEvent(QResizeEvent *)
{
	d->viewCenter = QPoint(width() / 2, height() / 2);
	updateTransforms();
}

bool CanvasView::event(QEvent *event)
{
	switch ((int)event->type())
	{
		case EventWidgetTabletMove:
		case EventWidgetTabletPress:
		case EventWidgetTabletRelease:
			customTabletEvent(static_cast<WidgetTabletEvent *>(event));
			return event->isAccepted();
		default:
			return QWidget::event(event);
	}
}

bool CanvasView::sendCanvasMouseEvent(QMouseEvent *event)
{
	auto toCanvasEventType = [](QEvent::Type type)
	{
		switch (type)
		{
			default:
			case QEvent::MouseMove:
				return EventCanvasMouseMove;
			case QEvent::MouseButtonPress:
				return EventCanvasMousePress;
			case QEvent::MouseButtonRelease:
				return EventCanvasMouseRelease;
			case QEvent::MouseButtonDblClick:
				return EventCanvasMouseDoubleClick;
		}
	};
	
	CanvasMouseEvent canvasEvent(toCanvasEventType(event->type()), event->globalPos(), event->posF() * transformToScene(), event->modifiers());
	d->tool->toolEvent(&canvasEvent);
	
	return canvasEvent.isAccepted();
}

bool CanvasView::sendCanvasTabletEvent(WidgetTabletEvent *event)
{
	TabletInputData data = event->globalData;
	Vec2D globalPos = data.pos;
	data.pos += Vec2D(event->posInt - event->globalPosInt);
	data.pos *= transformToScene();
	
	auto toCanvasEventType = [](int type)
	{
		switch (type)
		{
			default:
			case EventWidgetTabletMove:
				return EventCanvasTabletMove;
			case EventWidgetTabletPress:
				return EventCanvasTabletPress;
			case EventWidgetTabletRelease:
				return EventCanvasTabletRelease;
		}
	};
	
	CanvasTabletEvent canvasEvent(toCanvasEventType(event->type()), globalPos, event->globalPosInt, data, event->modifiers());
	d->tool->toolEvent(&canvasEvent);
	
	return canvasEvent.isAccepted();
}

bool CanvasView::sendCanvasTabletEvent(QMouseEvent *mouseEvent)
{
	auto toCanvasEventType = [](QEvent::Type type)
	{
		switch (type)
		{
			default:
			case QEvent::MouseMove:
				return EventCanvasTabletMove;
			case QEvent::MouseButtonPress:
				return EventCanvasTabletPress;
			case QEvent::MouseButtonRelease:
				return EventCanvasTabletRelease;
		}
	};
	
	int type = toCanvasEventType(mouseEvent->type());
	
	if (type == EventCanvasTabletPress)
		d->mousePressure = 1.0;
	if (type == EventCanvasTabletRelease)
		d->mousePressure = 0.0;
	
	TabletInputData data(mouseEvent->posF() * transformToScene(), d->mousePressure, 0, 0, Vec2D(0));
	CanvasTabletEvent tabletEvent(type, mouseEvent->globalPos(), mouseEvent->globalPos(), data, mouseEvent->modifiers());
	d->tool->toolEvent(&tabletEvent);
	return tabletEvent.isAccepted();
}

void CanvasView::addCustomCursorRectToRepaintRect()
{
	if (d->tool->isCustomCursorEnabled())
	{
		addRepaintRect(d->prevCustomCursorRect);
		
		auto rect = d->tool->customCursorRect(d->customCursorPos);
		d->prevCustomCursorRect = rect;
		
		addRepaintRect(rect);
	}
}

void CanvasView::addRepaintRect(const QRect &rect)
{
	d->repaintRect |= rect;
}

void CanvasView::repaintDesignatedRect()
{
	if (d->repaintRect.isValid())
	{
		//PAINTFIELD_DEBUG << "repainting" << _repaintRect;
		repaint(d->repaintRect);
		d->repaintRect = QRect();
	}
}

bool CanvasView::tryBeginDragNavigation(const QPoint &pos)
{
	if (d->keyTracker.match(d->scaleKeys))
	{
		beginDragScaling(pos);
		return true;
	}
	if (d->keyTracker.match(d->rotationKeys))
	{
		beginDragRotation(pos);
		return true;
	}
	 if (d->keyTracker.match(d->translationKeys))
	{
		beginDragTranslation(pos);
		return true;
	}
	
	return false;
}

bool CanvasView::continueDragNavigation(const QPoint &pos)
{
	if (d->isTranslatingByDrag)
	{
		continueDragTranslation(pos);
		return true;
	}
	if (d->isScalingByDrag)
	{
		continueDragScaling(pos);
		return true;
	}
	if (d->isRotatingByDrag)
	{
		continueDragRotation(pos);
		return true;
	}
	
	return false;
}

void CanvasView::endDragNavigation()
{
	endDragTranslation();
	endDragScaling();
	endDragRotation();
}

void CanvasView::beginDragTranslation(const QPoint &pos)
{
	qApp->setOverrideCursor(Qt::ClosedHandCursor);
	
	d->isTranslatingByDrag = true;
	d->navigationOrigin = pos;
	d->backupNav = d->nav;
}

void CanvasView::continueDragTranslation(const QPoint &pos)
{
	setTranslation(d->backupNav.translation + (pos - d->navigationOrigin));
}

void CanvasView::endDragTranslation()
{
	qApp->restoreOverrideCursor();
	d->isTranslatingByDrag = false;
}

void CanvasView::beginDragScaling(const QPoint &pos)
{
	qApp->setOverrideCursor(Qt::SizeVerCursor);
	
	d->isScalingByDrag = true;
	d->navigationOrigin = pos;
	d->backupNav = d->nav;
}

void CanvasView::continueDragScaling(const QPoint &pos)
{
	auto delta = pos - d->navigationOrigin;
	
	constexpr double divisor = 100;
	
	double scaleRatio = exp2(-delta.y() / divisor);
	double scale = d->backupNav.scale * scaleRatio;
	
	auto navigationOffset = d->navigationOrigin - viewCenter();
	
	auto translation = (d->backupNav.translation - navigationOffset) * scaleRatio + navigationOffset;
	
	setScale(scale);
	setTranslation(translation);
}

void CanvasView::endDragScaling()
{
	qApp->restoreOverrideCursor();
	d->isScalingByDrag = false;
}

void CanvasView::beginDragRotation(const QPoint &pos)
{
	qApp->setOverrideCursor(Qt::ClosedHandCursor);
	
	d->isRotatingByDrag = true;
	d->navigationOrigin = pos;
	d->backupNav = d->nav;
}

void CanvasView::continueDragRotation(const QPoint &pos)
{
	auto originalDelta = d->navigationOrigin - viewCenter();
	auto delta = pos - viewCenter();
	if (delta != QPoint())
	{
		auto originalRotation = -atan2(originalDelta.x(), originalDelta.y()) / M_PI * 180.0;
		auto deltaRotation = -atan2(delta.x(), delta.y()) / M_PI * 180.0;
		
		auto navigationOffset = d->navigationOrigin - viewCenter();
		
		auto rotation = d->backupNav.rotation + deltaRotation - originalRotation;
		
		QTransform transform;
		transform.rotate(rotation);
		
		auto translation = (d->backupNav.translation - navigationOffset) * transform + navigationOffset;
		
		setRotation(rotation);
		setTranslation(translation);
	}
}

void CanvasView::endDragRotation()
{
	qApp->restoreOverrideCursor();
	
	d->isRotatingByDrag = false;
}


}
