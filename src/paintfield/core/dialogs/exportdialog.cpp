#include "../widgets/widgetgroup.h"

#include "exportdialog.h"
#include "ui_exportdialog.h"

namespace PaintField
{

ExportDialog::ExportDialog(QWidget *parent) :
	QDialog(parent),
	ui(new Ui::PaintField_ExportDialog),
	_group(new WidgetGroup(this))
{
	setWindowTitle(tr("Export"));
	
	ui->setupUi(this);
	
	_group->addWidget(ui->qualityLabel);
	_group->addWidget(ui->horizontalSlider);
	_group->addWidget(ui->spinBox);
	_group->addWidget(ui->percentLabel);
	
	connect(ui->spinBox, SIGNAL(valueChanged(int)), this, SLOT(onQualityValueChanged(int)));
	
	_comboBoxHash.insert("png", tr("PNG"));
	_comboBoxHash.insert("jpg", tr("JPEG"));
	_comboBoxHash.insert("bmp", tr("Windows Bitmap"));
	
	ui->comboBox->addItems(_comboBoxHash.values());
	
	connect(ui->comboBox, SIGNAL(activated(int)), this, SLOT(onComboBoxActivated(int)));
	
	ui->comboBox->setCurrentIndex(0);
	onComboBoxActivated(0);
	ui->spinBox->setValue(90);
}

ExportDialog::~ExportDialog()
{
	delete ui;
}

QString ExportDialog::currentText() const
{
	return ui->comboBox->currentText();
}

void ExportDialog::onComboBoxActivated(int index)
{
	_format = _comboBoxHash.keys().at(index);
	
	_group->setVisible(_format == "jpg");
}

void ExportDialog::onQualityValueChanged(int quality)
{
	_quality = quality;
}

}
