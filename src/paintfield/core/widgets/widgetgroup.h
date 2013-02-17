#ifndef FSWIDGETGROUP_H
#define FSWIDGETGROUP_H

#include <QObject>

namespace PaintField
{

class WidgetGroup : public QObject
{
	Q_OBJECT
public:
	explicit WidgetGroup(QObject *parent = 0) : QObject(parent) {}
	
	void addWidget(QWidget *widget) { _widgets << widget; }
	
	template <class Widget>
	void addWidgets(const QList<Widget *> &widgets)
	{
		_widgets.reserve(_widgets.size() + widgets.size());
		for (Widget *widget : widgets)
			_widgets << widget;
	}
	
signals:
	
public slots:
	
	void setVisible(bool visible);
	void setEnabled(bool enabled);
	
private:
	
	QList<QWidget *> _widgets;
};

}

#endif // FSWIDGETGROUP_H