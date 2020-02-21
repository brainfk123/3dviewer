#include "dviewer.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	dviewer w;
	w.show();
	return a.exec();
}
