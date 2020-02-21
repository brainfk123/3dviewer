#pragma once
#include "ui_MainWindow.h"

#include <QMainWindow>
#include <QListWidget>
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE


class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);
	~MainWindow();

private:
	enum Type
	{
		Object,
		Light,
		Camera
	};
	Ui::MainWindow *ui;
	QList<Type> objtype;
	QList<unsigned> objindex;
	QList<unsigned> mtlindex;

	void setupAction();
	void addCam();
	void addDirectionLight();
	void addSpotLight();
	void addCube();
	void addSphere();
	void addPlane();
	void addDisk();
	void addCylinder();
	void addPrism();
	void addCone();
	void addPyramid();
	void addMaterial();
	void setupMaterialPanel();
	MyDefaultMaterial *getCurrentMtl();
	void setupMtlList();
	void setupDiffPanel();
	void diffTexChanged(QString file);
	void setupSpecPanel();
	void specTexChanged(QString file);
	void setupUVPanel();
	void setupMetalPanel();
	void metalTexChanged(QString file);
	void setupRoughPanel();
	void roughTexChanged(QString file);
	void setupNormalPanel();
	void normalTexChanged(QString file);
	void setupEmitPanel();
	void emitTexChanged(QString file);
	void setupAOPanel();
	void aoTexChanged(QString file);
	void setupEnvPanel();
	void setupObjectPanel();
	void camSelected();
	void setAttribVisible(bool a1, bool a2, bool a3, bool a4, bool a5);
	void setAttribTitle(QString t1, QString t2, QString t3, QString t4);
	void objSelected(MyObject *obj);
	MyObject* getCurObject();
	void lightSelected(MyLight *light);
	MyLight* getCurLight();
	void setupObjList();
	void setupTransPanel();
	void setupAttribPanel();
	void setupCamFrame();
	void setupLightFrame();
	void setupObjFrame();
	void setLightColor(MyLight *light, QColor c);
	QColor getLightColor(MyLight *light);
	void setLightIntensity(MyLight *light, float I);
	float getLightIntensity(MyLight *light);
	void setCurrentPos(float x, float y, float z);
	glm::vec3 getCurrentPos();
	void setCurrentScale(float x, float y, float z);
	glm::vec3 getCurrentScale();
	void setCurrentRotation(float x, float y, float z);
	glm::vec3 getCurrentRotation();
	void setCurrentName(QString name);
	QString getCurrentName();
	void setCurrentAttrib1(MyObject *obj, float v);
	void setCurrentAttrib2(MyObject *obj, float v);
	void setCurrentAttrib3(MyObject *obj, float v);
	void setCurrentAttrib5(MyObject *obj, unsigned v);
	void processMesh(aiMesh *mesh, const aiScene *scene, MyImport *obj);
	void processNode(aiNode *node, const aiScene *scene, std::vector<MyImport*> &objects);
	void loadModel(std::string path, std::vector<MyImport*> &objects);
	void importFile(QString file);
	void exportFile(QString file);
};