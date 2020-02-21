#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_dviewer.h"

class dviewer : public QMainWindow
{
	Q_OBJECT

public:
	dviewer(QWidget *parent = Q_NULLPTR);

private:
	Ui::dviewerClass ui;
};
