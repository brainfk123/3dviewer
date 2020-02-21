#include  "mainwindow.h"
#include <QPalette>
#include <QBrush>
#include <QLinearGradient>
#include <QColor>
#include <QColorDialog>
#include <QFileDialog>
#include <QTime>
#include <QDate>
#include <QStandardPaths>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent), ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	setupAction();
	setupMaterialPanel();
	setupObjectPanel();
	ui->splitter->setSizes(QList<int>{20000, 10000});
	ui->splitter_2->setSizes(QList<int>{20000, 10000});
	addCam();
}
MainWindow::~MainWindow()
{
	delete ui;
}

void MainWindow::setupAction()
{
	connect(ui->action_cube, &QAction::triggered, [=](bool) {
		addCube();
		ui->viewport->update();
	});
	connect(ui->action_sphere, &QAction::triggered, [=](bool) {
		addSphere();
		ui->viewport->update();
	});
	connect(ui->action_plane, &QAction::triggered, [=](bool) {
		addPlane();
		ui->viewport->update();
	});
	connect(ui->action_disk, &QAction::triggered, [=](bool) {
		addDisk();
		ui->viewport->update();
	});
	connect(ui->action_cylinder, &QAction::triggered, [=](bool) {
		addCylinder();
		ui->viewport->update();
	});
	connect(ui->action_prism, &QAction::triggered, [=](bool) {
		addPrism();
		ui->viewport->update();
	});
	connect(ui->action_cone, &QAction::triggered, [=](bool) {
		addCone();
		ui->viewport->update();
	});
	connect(ui->action_pyramid, &QAction::triggered, [=](bool) {
		addPyramid();
		ui->viewport->update();
	});
	connect(ui->action_directlight, &QAction::triggered, [=](bool) {
		addDirectionLight();
		ui->viewport->update();
	});
	connect(ui->action_spotlight, &QAction::triggered, [=](bool) {
		addSpotLight();
		ui->viewport->update();
	});
	connect(ui->action_material, &QAction::triggered, [=](bool) {
		addMaterial();
		ui->viewport->update();
	});
	connect(ui->action_save, &QAction::triggered, [=](bool) {
		QString name = "3DViewer_" + QDate::currentDate().toString("yyyy-MM-dd") + "_" + QTime::currentTime().toString("HH-mm-ss") + ".png";
		QString path = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
		QString file = QFileDialog::getSaveFileName(this, u8"保存截图", path + "/" + name, "PNG (*.png);;JPEG (*.jpg *.jpeg);;Bitmap (*.bmp)");
		//qInfo() << file;
		if (!file.isEmpty())
			ui->viewport->saveImage(file);
	});
	connect(ui->action_import, &QAction::triggered, [=](bool) {
		Assimp::Importer t_importer;
		std::string szOut;
		t_importer.GetExtensionList(szOut);
		QString t_assimp = u8"3D模型 (" + QString::fromStdString(szOut) + ")";
		QString file = QFileDialog::getOpenFileName(this, u8"导入", "./", t_assimp+u8";;全部文件 (*.*)");
		if (!file.isEmpty())
		{
			importFile(file);
			ui->viewport->update();
		}
	});
	connect(ui->action_export, &QAction::triggered, [=](bool) {
		QString file = QFileDialog::getSaveFileName(this, u8"导出", "./", u8"Wavefront Object (*.obj)");
		if (!file.isEmpty())
			exportFile(file);
	});
	connect(ui->action_exit, &QAction::triggered, [=](bool) {
		if (!(QMessageBox::information(this, u8"退出", u8"退出程序？", u8"确定", u8"取消")))
		{
			close();
		}
	});
}

void MainWindow::processMesh(aiMesh *mesh, const aiScene *scene, MyImport *obj)
{
	std::vector<Vertex> &vertices = obj->getVertexArray();
	std::vector<unsigned> &indices = obj->getIndicesArray();
	unsigned start = vertices.size();
	for (unsigned i = 0; i < mesh->mNumVertices; i++)
	{
		Vertex vertex;
		vertex.Position = glm::vec3(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z);
		if (mesh->mNormals)
			vertex.Normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
		else
			vertex.Normal = glm::vec3(0,1,0);
		if (mesh->mTangents)
			vertex.Tangent = glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z);
		else
			vertex.Tangent = glm::vec3(1,0,0);
		if (mesh->mBitangents)
			vertex.BiTangent = glm::vec3(mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z);
		else
			vertex.BiTangent = glm::vec3(0,0,1);
		if (mesh->mTextureCoords[0])
			vertex.TexCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
		else
			vertex.TexCoords = glm::vec2(0.0f, 0.0f);
		vertices.push_back(vertex);
	}
	for (unsigned i = 0; i < mesh->mNumFaces; i++)
	{
		aiFace face = mesh->mFaces[i];
		for (unsigned j = 0; j < face.mNumIndices; j++)
			indices.push_back(start + face.mIndices[j]);
	}
}

void MainWindow::processNode(aiNode *node, const aiScene *scene, std::vector<MyImport*> &objects)
{
	for (unsigned int i = 0; i < node->mNumChildren; i++)
	{
		processNode(node->mChildren[i], scene, objects);
	}
	if (node->mNumMeshes == 0)return;
	MyImport *obj = new MyImport(ui->viewport);
	if (node->mName.length != 0)
		obj->name = node->mName.C_Str();
	else
		obj->name = u8"未命名物体";
	for (unsigned int i = 0; i < node->mNumMeshes; i++)
	{
		aiMesh *mesh = scene->mMeshes[node->mMeshes[i]];
		processMesh(mesh, scene, obj);
	}
	obj->updateData();
	objects.push_back(obj);
}

void MainWindow::loadModel(std::string path, std::vector<MyImport*> &objects)
{
	Assimp::Importer import;
	const aiScene *scene = import.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals | aiProcess_CalcTangentSpace);

	if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
	{
		qWarning() << "ERROR::ASSIMP::" << import.GetErrorString();
		return;
	}

	processNode(scene->mRootNode, scene, objects);
}

void MainWindow::importFile(QString file)
{
	std::vector<MyImport*> objects;
	loadModel(file.toStdString(), objects);
	for (int i = 0; i < objects.size(); i++)
	{
		objects[i]->setMaterial(ui->viewport->defaultmtl);
		objtype.append(Object);
		objindex.append(ui->viewport->objlist.size());
		ui->viewport->objlist.append(objects[i]);
		ui->objectlist->addItem(objects[i]->name);
		ui->objectlist->setCurrentRow(ui->objectlist->count() - 1);
	}
}

void MainWindow::exportFile(QString file)
{
	Assimp::Exporter exporter;
	
	aiScene scene;

	scene.mRootNode = new aiNode();
	scene.mMaterials = new aiMaterial*[1];
	scene.mMaterials[0] = new aiMaterial();
	scene.mNumMaterials = 1;
	scene.mNumTextures = 0;
	scene.mRootNode->mNumMeshes = 0;

	int objnum = 0;
	for (int i = 0; i < ui->viewport->objlist.size(); i++)
		if (ui->viewport->objlist[i])
			objnum++;

	scene.mMeshes = new aiMesh*[objnum];
	scene.mNumMeshes = objnum;
	aiNode **t_node_list = new aiNode*[objnum];

	int objindex = 0;
	for (int i = 0; i < ui->viewport->objlist.size(); i++)
		if (ui->viewport->objlist[i])
		{
			auto pMesh = scene.mMeshes[objindex] = new aiMesh();

			t_node_list[objindex] = new aiNode();
			t_node_list[objindex]->mMeshes = new unsigned int[1];
			t_node_list[objindex]->mMeshes[0] = objindex;
			t_node_list[objindex]->mNumMeshes = 1;
			t_node_list[objindex]->mNumChildren = 0;
			t_node_list[objindex]->mName = ui->viewport->objlist[i]->name.toStdString();

			std::vector<Vertex> ver;
			std::vector<unsigned> ind;
			ui->viewport->objlist[i]->addVertexAndIndices(ver, ind);
			pMesh->mVertices = new aiVector3D[ver.size()];
			pMesh->mNumVertices = ver.size();
			pMesh->mNormals = new aiVector3D[ver.size()];
			pMesh->mTextureCoords[0] = new aiVector3D[ver.size()];
			pMesh->mNumUVComponents[0] = 2;
			pMesh->mMaterialIndex = 0;
			for (int j = 0; j < ver.size(); j++)
			{
				pMesh->mVertices[j] = aiVector3D(ver[j].Position.x, ver[j].Position.y, ver[j].Position.z);
				pMesh->mNormals[j] = aiVector3D(ver[j].Normal.x, ver[j].Normal.y, ver[j].Normal.z);
				pMesh->mTextureCoords[0][j] = aiVector3D(ver[j].TexCoords.x, ver[j].TexCoords.y, 0);
			}
			pMesh->mFaces = new aiFace[ind.size() / 3];
			pMesh->mNumFaces = ind.size() / 3;
			for (int j = 0; j < ind.size() / 3; j++)
			{
				aiFace& face = pMesh->mFaces[j];
				face.mIndices = new unsigned int[3];
				face.mNumIndices = 3;
				face.mIndices[0] = ind[3 * j];
				face.mIndices[1] = ind[3 * j + 1];
				face.mIndices[2] = ind[3 * j + 2];
			}
			objindex++;
		}

	scene.mRootNode->addChildren(objnum, t_node_list);
	/*
	for (int i = 0; i < exporter.GetExportFormatCount(); i++)
	{
		const aiExportFormatDesc *t_format_desc = exporter.GetExportFormatDescription(i);
		qInfo() << t_format_desc->id << t_format_desc->description << t_format_desc->fileExtension;
	}
	*/
	//qInfo() << file;
	qInfo() << exporter.Export(&scene, "objnomtl", file.toStdString(), aiProcess_Triangulate);
}

void MainWindow::addCam()
{
	objtype.append(Camera);
	objindex.append(0);
	ui->objectlist->addItem(u8"摄像机");
	ui->objectlist->setCurrentRow(0);
}

void MainWindow::addDirectionLight()
{
	DirectLight *light = DirectLight::createLight(ui->viewport);
	light->name = u8"方向光";
	objtype.append(Light);
	objindex.append(ui->viewport->lightlist.size());
	ui->viewport->lightlist.append(light);
	ui->objectlist->addItem(u8"方向光");
	ui->objectlist->setCurrentRow(ui->objectlist->count() - 1);
}

void MainWindow::addSpotLight()
{
	SpotLight *light = SpotLight::createLight(ui->viewport);
	light->name = u8"点光源";
	objtype.append(Light);
	objindex.append(ui->viewport->lightlist.size());
	ui->viewport->lightlist.append(light);
	ui->objectlist->addItem(u8"点光源");
	ui->objectlist->setCurrentRow(ui->objectlist->count() - 1);
}

void MainWindow::addCube()
{
	MyCube *cube = new MyCube(ui->viewport, u8"立方体");
	cube->setMaterial(ui->viewport->defaultmtl);
	objtype.append(Object);
	objindex.append(ui->viewport->objlist.size());
	ui->viewport->objlist.append(cube);
	ui->objectlist->addItem(u8"立方体");
	ui->objectlist->setCurrentRow(ui->objectlist->count() - 1);
}

void MainWindow::addSphere()
{
	MySphere *sphere = new MySphere(ui->viewport, u8"球体");
	sphere->setMaterial(ui->viewport->defaultmtl);
	objtype.append(Object);
	objindex.append(ui->viewport->objlist.size());
	ui->viewport->objlist.append(sphere);
	ui->objectlist->addItem(u8"球体");
	ui->objectlist->setCurrentRow(ui->objectlist->count() - 1);
}

void MainWindow::addPlane()
{
	MyPlane *plane = new MyPlane(ui->viewport, u8"平面");
	plane->setMaterial(ui->viewport->defaultmtl);
	objtype.append(Object);
	objindex.append(ui->viewport->objlist.size());
	ui->viewport->objlist.append(plane);
	ui->objectlist->addItem(u8"平面");
	ui->objectlist->setCurrentRow(ui->objectlist->count() - 1);
}

void MainWindow::addDisk()
{
	MyDisk *disk = new MyDisk(ui->viewport, u8"圆盘");
	disk->setMaterial(ui->viewport->defaultmtl);
	objtype.append(Object);
	objindex.append(ui->viewport->objlist.size());
	ui->viewport->objlist.append(disk);
	ui->objectlist->addItem(u8"圆盘");
	ui->objectlist->setCurrentRow(ui->objectlist->count() - 1);
}

void MainWindow::addCylinder()
{
	MyCylinder *cylinder = new MyCylinder(ui->viewport, u8"圆柱");
	cylinder->setMaterial(ui->viewport->defaultmtl);
	objtype.append(Object);
	objindex.append(ui->viewport->objlist.size());
	ui->viewport->objlist.append(cylinder);
	ui->objectlist->addItem(u8"圆柱");
	ui->objectlist->setCurrentRow(ui->objectlist->count() - 1);
}

void MainWindow::addPrism()
{
	MyPrism *prism = new MyPrism(ui->viewport, u8"棱柱");
	prism->setMaterial(ui->viewport->defaultmtl);
	objtype.append(Object);
	objindex.append(ui->viewport->objlist.size());
	ui->viewport->objlist.append(prism);
	ui->objectlist->addItem(u8"棱柱");
	ui->objectlist->setCurrentRow(ui->objectlist->count() - 1);
}

void MainWindow::addCone()
{
	MyCone *cone = new MyCone(ui->viewport, u8"圆锥");
	cone->setMaterial(ui->viewport->defaultmtl);
	objtype.append(Object);
	objindex.append(ui->viewport->objlist.size());
	ui->viewport->objlist.append(cone);
	ui->objectlist->addItem(u8"圆锥");
	ui->objectlist->setCurrentRow(ui->objectlist->count() - 1);
}

void MainWindow::addPyramid()
{
	MyPyramid *pyramid = new MyPyramid(ui->viewport, u8"棱锥");
	pyramid->setMaterial(ui->viewport->defaultmtl);
	objtype.append(Object);
	objindex.append(ui->viewport->objlist.size());
	ui->viewport->objlist.append(pyramid);
	ui->objectlist->addItem(u8"棱锥");
	ui->objectlist->setCurrentRow(ui->objectlist->count() - 1);
}

void MainWindow::addMaterial()
{
	MyDefaultMaterial *mtl = MyDefaultMaterial::createMaterial(ui->viewport, ui->viewport->ushader, glm::vec3(0.8), glm::vec3(1), 0, 0.2);
	mtl->name = u8"新材质";
	mtlindex.append(ui->viewport->materiallist.size());
	ui->viewport->materiallist.append(mtl);
	ui->materiallist->addItem(u8"新材质");
	ui->materiallist->setCurrentRow(ui->materiallist->count() - 1);
}

void MainWindow::setupMaterialPanel()
{
	setupMtlList();
	ui->mtlname_panel->hide();
	connect(ui->applymtl, &QPushButton::clicked, [=](bool) {
		int idx = ui->objectlist->currentRow();
		int mtl = ui->materiallist->currentRow();
		MyObject *obj = getCurObject();
		if (obj)
		{
			obj->setMaterial(ui->viewport->materiallist[mtlindex[mtl]]);
			ui->viewport->update();
		}
	});
	connect(ui->deletemtl, &QPushButton::clicked, [=](bool) {
		int idx = ui->materiallist->currentRow();
		if (idx >= 0)
		{
			MyDefaultMaterial *mtl = ui->viewport->materiallist[mtlindex[idx]];
			for (int i = 0; i < objindex.size(); i++)
			{
				if (objtype[i] == Object)
				{
					if (ui->viewport->objlist[objindex[i]]->getMaterial() == mtl)
					{
						ui->viewport->objlist[objindex[i]]->setMaterial(ui->viewport->defaultmtl);
					}
				}
			}
			delete ui->materiallist->takeItem(idx);
			ui->viewport->materiallist[mtlindex[idx]] = nullptr;
			mtlindex.removeAt(idx);
			delete mtl;
			ui->viewport->update();
		}
	});
	connect(ui->mtl_name, &QLineEdit::editingFinished, [=]() {
		QString text = ui->mtl_name->text();
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (text.isEmpty())
		{
			if (mtl)
				ui->mtl_name->setText(mtl->name);
		}
		else
		{
			if (mtl)
			{
				mtl->name = text;
				ui->materiallist->currentItem()->setText(text);
			}
		}
	});
	setupDiffPanel();
	setupSpecPanel();
	setupUVPanel();
	setupMetalPanel();
	setupRoughPanel();
	setupNormalPanel();
	setupEmitPanel();
	setupAOPanel();
	setupEnvPanel();
}

MyDefaultMaterial * MainWindow::getCurrentMtl()
{
	int row = ui->materiallist->currentRow();
	if (row >= 0)
		return ui->viewport->materiallist[mtlindex[row]];
	return nullptr;
}

void MainWindow::setupMtlList()
{
	connect(ui->materiallist, &QListWidget::currentRowChanged, [=](int row) {
		if (row >= 0 && row < ui->materiallist->count())
		{
			MyDefaultMaterial *mtl = ui->viewport->materiallist[mtlindex[row]];
			ui->mtl_name->setText(mtl->name);
			ui->diffR->setValue(mtl->diffuse.r * 255);
			ui->diffG->setValue(mtl->diffuse.g * 255);
			ui->diffB->setValue(mtl->diffuse.b * 255);
			ui->difftexpath->setText(mtl->texpath[0]);
			ui->specR->setValue(mtl->specular.r * 255);
			ui->specG->setValue(mtl->specular.g * 255);
			ui->specB->setValue(mtl->specular.b * 255);
			ui->spectexpath->setText(mtl->texpath[1]);
			ui->repeatcheck->setChecked(mtl->repeat());
			ui->mirrowcheck->setEnabled(mtl->repeat());
			ui->mirrowcheck->setChecked(mtl->mirrow());
			ui->uoffset->setValue(mtl->uvoffset.x);
			ui->voffset->setValue(mtl->uvoffset.y);
			ui->uscale->setValue(mtl->uvscale.x);
			ui->vscale->setValue(mtl->uvscale.y);
			ui->metal->setValue(mtl->metal);
			ui->metaltexpath->setText(mtl->texpath[2]);
			ui->rough->setValue(mtl->rough);
			ui->roughtexpath->setText(mtl->texpath[3]);
			ui->normaltexpath->setText(mtl->texpath[4]);
			ui->emitR->setValue(mtl->emission.r * 255);
			ui->emitG->setValue(mtl->emission.g * 255);
			ui->emitB->setValue(mtl->emission.b * 255);
			ui->emittexpath->setText(mtl->texpath[5]);
			ui->aotexpath->setText(mtl->texpath[6]);
			ui->mtlname_panel->show();
			ui->diffpanel->show();
			ui->specpanel->show();
			ui->uvpanel->show();
			ui->metalpanel->show();
			ui->roughpanel->show();
			ui->normalpanel->show();
			ui->emitpanel->show();
			ui->aopanel->show();
		}
		else
		{
			ui->mtlname_panel->hide();
			ui->diffpanel->hide();
			ui->specpanel->hide();
			ui->uvpanel->hide();
			ui->metalpanel->hide();
			ui->roughpanel->hide();
			ui->normalpanel->hide();
			ui->emitpanel->hide();
			ui->aopanel->hide();
		}
	});
}

void MainWindow::setupDiffPanel()
{
	ui->diffpanel->hide();
	//ui->diffpanel->setDisabled(true);
	connect(ui->diffRslider, &QSlider::valueChanged, [=](int value) {
		ui->diffR->setValue(value);
		QString style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(%1,0,%2),stop:1 rgb(%1,255,%2));margin:-5px 0;}";
		ui->diffGslider->setStyleSheet(style.arg(ui->diffR->value()).arg(ui->diffB->value()));
		style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(%1,%2,0),stop:1 rgb(%1,%2,255));margin:-5px 0;}";
		ui->diffBslider->setStyleSheet(style.arg(ui->diffR->value()).arg(ui->diffG->value()));
	});
	connect(ui->diffR, QOverload<int>::of(&QSpinBox::valueChanged), [=](int v) {
		ui->diffRslider->setValue(v);
		ui->diffbutton->setStyleSheet(QString("border:2px solid #333333;border-radius:0;background-color:rgb(%1,%2,%3);width:50px;height:25px;").arg(v).arg(ui->diffG->value()).arg(ui->diffB->value()));
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (mtl)
		{
			mtl->diffuse.r = v / 255.0;
			ui->viewport->update();
		}
	});
	connect(ui->diffGslider, &QSlider::valueChanged, [=](int value) {
		ui->diffG->setValue(value);
		QString style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(0,%1,%2),stop:1 rgb(255,%1,%2));margin:-5px 0;}";
		ui->diffRslider->setStyleSheet(style.arg(ui->diffG->value()).arg(ui->diffB->value()));
		style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(%1,%2,0),stop:1 rgb(%1,%2,255));margin:-5px 0;}";
		ui->diffBslider->setStyleSheet(style.arg(ui->diffR->value()).arg(ui->diffG->value()));
	});
	connect(ui->diffG, QOverload<int>::of(&QSpinBox::valueChanged), [=](int v) {
		ui->diffGslider->setValue(v);
		ui->diffbutton->setStyleSheet(QString("border:2px solid #333333;border-radius:0;background-color:rgb(%1,%2,%3);width:50px;height:25px;").arg(ui->diffR->value()).arg(v).arg(ui->diffB->value()));
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (mtl)
		{
			mtl->diffuse.g = v / 255.0;
			ui->viewport->update();
		}
	});
	connect(ui->diffBslider, &QSlider::valueChanged, [=](int value) {
		ui->diffB->setValue(value);
		QString style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(0,%1,%2),stop:1 rgb(255,%1,%2));margin:-5px 0;}";
		ui->diffRslider->setStyleSheet(style.arg(ui->diffG->value()).arg(ui->diffB->value()));
		style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(%1,0,%2),stop:1 rgb(%1,255,%2));margin:-5px 0;}";
		ui->diffGslider->setStyleSheet(style.arg(ui->diffR->value()).arg(ui->diffB->value()));
	});
	connect(ui->diffB, QOverload<int>::of(&QSpinBox::valueChanged), [=](int v) {
		ui->diffBslider->setValue(v);
		ui->diffbutton->setStyleSheet(QString("border:2px solid #333333;border-radius:0;background-color:rgb(%1,%2,%3);width:50px;height:25px;").arg(ui->diffR->value()).arg(ui->diffG->value()).arg(v));
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (mtl)
		{
			mtl->diffuse.b = v / 255.0;
			ui->viewport->update();
		}
	});
	connect(ui->diffbutton, &QPushButton::clicked, [=](bool) {
		QColor color = QColorDialog::getColor(QColor(ui->diffR->value(), ui->diffG->value(), ui->diffB->value()), this, u8"漫反射");
		if (color.isValid())
		{
			ui->diffbutton->setStyleSheet(QString("border:2px solid #333333;border-radius:0;background-color:%1;width:50px;height:25px;").arg(color.name()));
			ui->diffR->setValue(color.red());
			ui->diffG->setValue(color.green());
			ui->diffB->setValue(color.blue());
		}
	});
	connect(ui->difftexbutton, &QPushButton::clicked, [=](bool) {
		QString file = QFileDialog::getOpenFileName(this, u8"打开文件");
		if (!file.isEmpty() && QFileInfo(file).exists() && QFileInfo(file).isFile())
		{
			ui->difftexpath->setText(file);
			diffTexChanged(file);
		}
	});
	connect(ui->difftexpath, &QLineEdit::editingFinished, [=]() {
		QString file = ui->difftexpath->text();
		if (file.isEmpty() || !file.isEmpty() && QFileInfo(file).exists() && QFileInfo(file).isFile())
		{
			ui->difftexpath->setText(file);
			diffTexChanged(file);
		}
		else
		{
			MyDefaultMaterial *mtl = getCurrentMtl();
			if (mtl)
				ui->difftexpath->setText(mtl->texpath[0]);
			else
				ui->difftexpath->setText("");
		}
	});
}

void MainWindow::diffTexChanged(QString file)
{
	int row = ui->materiallist->currentRow();
	if (row >= 0)
	{
		MyDefaultMaterial *mtl = ui->viewport->materiallist[mtlindex[row]];
		mtl->setDiffTexture(file);
		mtl->texpath[0] = file;
		ui->viewport->update();
	}
}

void MainWindow::setupSpecPanel()
{
	ui->specpanel->hide();
	//ui->specpanel->setDisabled(true);
	connect(ui->specRslider, &QSlider::valueChanged, [=](int value) {
		ui->specR->setValue(value);
		QString style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(%1,0,%2),stop:1 rgb(%1,255,%2));margin:-5px 0;}";
		ui->specGslider->setStyleSheet(style.arg(ui->specR->value()).arg(ui->specB->value()));
		style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(%1,%2,0),stop:1 rgb(%1,%2,255));margin:-5px 0;}";
		ui->specBslider->setStyleSheet(style.arg(ui->specR->value()).arg(ui->specG->value()));
	});
	connect(ui->specR, QOverload<int>::of(&QSpinBox::valueChanged), [=](int v) {
		ui->specRslider->setValue(v);
		ui->specbutton->setStyleSheet(QString("border:2px solid #333333;border-radius:0;background-color:rgb(%1,%2,%3);width:50px;height:25px;").arg(v).arg(ui->specG->value()).arg(ui->specB->value()));
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (mtl)
		{
			mtl->specular.r = v / 255.0;
			ui->viewport->update();
		}
	});
	connect(ui->specGslider, &QSlider::valueChanged, [=](int value) {
		ui->specG->setValue(value);
		QString style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(0,%1,%2),stop:1 rgb(255,%1,%2));margin:-5px 0;}";
		ui->specRslider->setStyleSheet(style.arg(ui->specG->value()).arg(ui->specB->value()));
		style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(%1,%2,0),stop:1 rgb(%1,%2,255));margin:-5px 0;}";
		ui->specBslider->setStyleSheet(style.arg(ui->specR->value()).arg(ui->specG->value()));
	});
	connect(ui->specG, QOverload<int>::of(&QSpinBox::valueChanged), [=](int v) {
		ui->specGslider->setValue(v);
		ui->specbutton->setStyleSheet(QString("border:2px solid #333333;border-radius:0;background-color:rgb(%1,%2,%3);width:50px;height:25px;").arg(ui->specR->value()).arg(v).arg(ui->specB->value()));
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (mtl)
		{
			mtl->specular.g = v / 255.0;
			ui->viewport->update();
		}
	});
	connect(ui->specBslider, &QSlider::valueChanged, [=](int value) {
		ui->specB->setValue(value);
		QString style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(0,%1,%2),stop:1 rgb(255,%1,%2));margin:-5px 0;}";
		ui->specRslider->setStyleSheet(style.arg(ui->specG->value()).arg(ui->specB->value()));
		style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(%1,0,%2),stop:1 rgb(%1,255,%2));margin:-5px 0;}";
		ui->specGslider->setStyleSheet(style.arg(ui->specR->value()).arg(ui->specB->value()));
	});
	connect(ui->specB, QOverload<int>::of(&QSpinBox::valueChanged), [=](int v) {
		ui->specBslider->setValue(v);
		ui->specbutton->setStyleSheet(QString("border:2px solid #333333;border-radius:0;background-color:rgb(%1,%2,%3);width:50px;height:25px;").arg(ui->specR->value()).arg(ui->specG->value()).arg(v));
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (mtl)
		{
			mtl->specular.b = v / 255.0;
			ui->viewport->update();
		}
	});
	connect(ui->specbutton, &QPushButton::clicked, [=](bool) {
		QColor color = QColorDialog::getColor(QColor(ui->specR->value(), ui->specG->value(), ui->specB->value()), this, u8"漫反射");
		if (color.isValid())
		{
			ui->specbutton->setStyleSheet(QString("border:2px solid #333333;border-radius:0;background-color:%1;width:50px;height:25px;").arg(color.name()));
			ui->specR->setValue(color.red());
			ui->specG->setValue(color.green());
			ui->specB->setValue(color.blue());
		}
	});
	connect(ui->spectexbutton, &QPushButton::clicked, [=](bool) {
		QString file = QFileDialog::getOpenFileName(this, u8"打开文件");
		if (!file.isEmpty() && QFileInfo(file).exists() && QFileInfo(file).isFile())
		{
			ui->spectexpath->setText(file);
			specTexChanged(file);
		}
	});
	connect(ui->spectexpath, &QLineEdit::editingFinished, [=]() {
		QString file = ui->spectexpath->text();
		if (file.isEmpty() || !file.isEmpty() && QFileInfo(file).exists() && QFileInfo(file).isFile())
		{
			ui->spectexpath->setText(file);
			specTexChanged(file);
		}
		else
		{
			MyDefaultMaterial *mtl = getCurrentMtl();
			if (mtl)
				ui->spectexpath->setText(mtl->texpath[1]);
			else
				ui->spectexpath->setText("");
		}
	});
}

void MainWindow::specTexChanged(QString file)
{
	int row = ui->materiallist->currentRow();
	if (row >= 0)
	{
		MyDefaultMaterial *mtl = ui->viewport->materiallist[mtlindex[row]];
		mtl->setSpecTexture(file);
		mtl->texpath[1] = file;
		ui->viewport->update();
	}
}

void MainWindow::setupUVPanel()
{
	ui->uvpanel->hide();
	connect(ui->repeatcheck, &QCheckBox::stateChanged, [=](int state) {
		if (state == Qt::Checked)
		{
			ui->mirrowcheck->setEnabled(true);
		}
		else if (state == Qt::Unchecked)
		{
			ui->mirrowcheck->setDisabled(true);
		}
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (mtl)
		{
			mtl->setRepeatMirrow(state == Qt::Checked, mtl->mirrow());
			ui->viewport->update();
		}
	});
	connect(ui->mirrowcheck, &QCheckBox::stateChanged, [=](int state) {
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (mtl)
		{
			mtl->setRepeatMirrow(mtl->repeat(), state == Qt::Checked);
			ui->viewport->update();
		}
	});
	connect(ui->uoffset, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (mtl)
		{
			mtl->uvoffset.x = v;
			ui->viewport->update();
		}
	});
	connect(ui->voffset, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (mtl)
		{
			mtl->uvoffset.y = v;
			ui->viewport->update();
		}
	});
	connect(ui->uscale, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (mtl)
		{
			mtl->uvscale.x = v;
			ui->viewport->update();
		}
	});
	connect(ui->vscale, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (mtl)
		{
			mtl->uvscale.y = v;
			ui->viewport->update();
		}
	});
}

void MainWindow::setupMetalPanel()
{
	ui->metalpanel->hide();
	connect(ui->metal, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		ui->slider_metal->setValue(v * 100);
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (mtl)
		{
			mtl->metal = v;
			ui->viewport->update();
		}
	});
	connect(ui->slider_metal, &QAbstractSlider::sliderMoved, [=](int v) {
		ui->metal->setValue(v / 100.0);
	});
	connect(ui->metaltexbutton, &QPushButton::clicked, [=](bool) {
		QString file = QFileDialog::getOpenFileName(this, u8"打开文件");
		if (!file.isEmpty() && QFileInfo(file).exists() && QFileInfo(file).isFile())
		{
			ui->metaltexpath->setText(file);
			metalTexChanged(file);
		}
	});
	connect(ui->metaltexpath, &QLineEdit::editingFinished, [=]() {
		QString file = ui->spectexpath->text();
		if (file.isEmpty() || !file.isEmpty() && QFileInfo(file).exists() && QFileInfo(file).isFile())
		{
			ui->metaltexpath->setText(file);
			metalTexChanged(file);
		}
		else
		{
			MyDefaultMaterial *mtl = getCurrentMtl();
			if (mtl)
				ui->metaltexpath->setText(mtl->texpath[2]);
			else
				ui->metaltexpath->setText("");
		}
	});
}

void MainWindow::metalTexChanged(QString file)
{
	int row = ui->materiallist->currentRow();
	if (row >= 0)
	{
		MyDefaultMaterial *mtl = ui->viewport->materiallist[mtlindex[row]];
		mtl->setMetalTexture(file);
		mtl->texpath[2] = file;
		ui->viewport->update();
	}
}

void MainWindow::setupRoughPanel()
{
	ui->roughpanel->hide();
	connect(ui->rough, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		ui->slider_rough->setValue(v * 100);
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (mtl)
		{
			mtl->rough = v;
			ui->viewport->update();
		}
	});
	connect(ui->slider_rough, &QAbstractSlider::sliderMoved, [=](int v) {
		ui->rough->setValue(v / 100.0);
	});
	connect(ui->roughtexbutton, &QPushButton::clicked, [=](bool) {
		QString file = QFileDialog::getOpenFileName(this, u8"打开文件");
		if (!file.isEmpty() && QFileInfo(file).exists() && QFileInfo(file).isFile())
		{
			ui->roughtexpath->setText(file);
			roughTexChanged(file);
		}
	});
	connect(ui->roughtexpath, &QLineEdit::editingFinished, [=]() {
		QString file = ui->spectexpath->text();
		if (file.isEmpty() || !file.isEmpty() && QFileInfo(file).exists() && QFileInfo(file).isFile())
		{
			ui->roughtexpath->setText(file);
			roughTexChanged(file);
		}
		else
		{
			MyDefaultMaterial *mtl = getCurrentMtl();
			if (mtl)
				ui->roughtexpath->setText(mtl->texpath[3]);
			else
				ui->roughtexpath->setText("");
		}
	});
}

void MainWindow::roughTexChanged(QString file)
{
	int row = ui->materiallist->currentRow();
	if (row >= 0)
	{
		MyDefaultMaterial *mtl = ui->viewport->materiallist[mtlindex[row]];
		mtl->setRoughTexture(file);
		mtl->texpath[3] = file;
		ui->viewport->update();
	}
}

void MainWindow::setupNormalPanel()
{
	ui->normalpanel->hide();
	connect(ui->normaltexbutton, &QPushButton::clicked, [=](bool) {
		QString file = QFileDialog::getOpenFileName(this, u8"打开文件");
		if (!file.isEmpty() && QFileInfo(file).exists() && QFileInfo(file).isFile())
		{
			ui->normaltexpath->setText(file);
			normalTexChanged(file);
		}
	});
	connect(ui->normaltexpath, &QLineEdit::editingFinished, [=]() {
		QString file = ui->spectexpath->text();
		if (file.isEmpty() || !file.isEmpty() && QFileInfo(file).exists() && QFileInfo(file).isFile())
		{
			ui->normaltexpath->setText(file);
			normalTexChanged(file);
		}
		else
		{
			MyDefaultMaterial *mtl = getCurrentMtl();
			if (mtl)
				ui->normaltexpath->setText(mtl->texpath[4]);
			else
				ui->normaltexpath->setText("");
		}
	});
}

void MainWindow::normalTexChanged(QString file)
{
	int row = ui->materiallist->currentRow();
	if (row >= 0)
	{
		MyDefaultMaterial *mtl = ui->viewport->materiallist[mtlindex[row]];
		mtl->setNormalTexture(file);
		mtl->texpath[4] = file;
		ui->viewport->update();
	}
}

void MainWindow::setupEmitPanel()
{
	ui->emitpanel->hide();
	//ui->emitpanel->setDisabled(true);
	connect(ui->emitRslider, &QSlider::valueChanged, [=](int value) {
		ui->emitR->setValue(value);
		QString style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(%1,0,%2),stop:1 rgb(%1,255,%2));margin:-5px 0;}";
		ui->emitGslider->setStyleSheet(style.arg(ui->emitR->value()).arg(ui->emitB->value()));
		style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(%1,%2,0),stop:1 rgb(%1,%2,255));margin:-5px 0;}";
		ui->emitBslider->setStyleSheet(style.arg(ui->emitR->value()).arg(ui->emitG->value()));
	});
	connect(ui->emitR, QOverload<int>::of(&QSpinBox::valueChanged), [=](int v) {
		ui->emitRslider->setValue(v);
		ui->emitbutton->setStyleSheet(QString("border:2px solid #333333;border-radius:0;background-color:rgb(%1,%2,%3);width:50px;height:25px;").arg(v).arg(ui->emitG->value()).arg(ui->emitB->value()));
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (mtl)
		{
			mtl->emission.r = v / 255.0;
			ui->viewport->update();
		}
	});
	connect(ui->emitGslider, &QSlider::valueChanged, [=](int value) {
		ui->emitG->setValue(value);
		QString style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(0,%1,%2),stop:1 rgb(255,%1,%2));margin:-5px 0;}";
		ui->emitRslider->setStyleSheet(style.arg(ui->emitG->value()).arg(ui->emitB->value()));
		style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(%1,%2,0),stop:1 rgb(%1,%2,255));margin:-5px 0;}";
		ui->emitBslider->setStyleSheet(style.arg(ui->emitR->value()).arg(ui->emitG->value()));
	});
	connect(ui->emitG, QOverload<int>::of(&QSpinBox::valueChanged), [=](int v) {
		ui->emitGslider->setValue(v);
		ui->emitbutton->setStyleSheet(QString("border:2px solid #333333;border-radius:0;background-color:rgb(%1,%2,%3);width:50px;height:25px;").arg(ui->emitR->value()).arg(v).arg(ui->emitB->value()));
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (mtl)
		{
			mtl->emission.g = v / 255.0;
			ui->viewport->update();
		}
	});
	connect(ui->emitBslider, &QSlider::valueChanged, [=](int value) {
		ui->emitB->setValue(value);
		QString style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(0,%1,%2),stop:1 rgb(255,%1,%2));margin:-5px 0;}";
		ui->emitRslider->setStyleSheet(style.arg(ui->emitG->value()).arg(ui->emitB->value()));
		style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(%1,0,%2),stop:1 rgb(%1,255,%2));margin:-5px 0;}";
		ui->emitGslider->setStyleSheet(style.arg(ui->emitR->value()).arg(ui->emitB->value()));
	});
	connect(ui->emitB, QOverload<int>::of(&QSpinBox::valueChanged), [=](int v) {
		ui->emitBslider->setValue(v);
		ui->emitbutton->setStyleSheet(QString("border:2px solid #333333;border-radius:0;background-color:rgb(%1,%2,%3);width:50px;height:25px;").arg(ui->emitR->value()).arg(ui->emitG->value()).arg(v));
		MyDefaultMaterial *mtl = getCurrentMtl();
		if (mtl)
		{
			mtl->emission.b = v / 255.0;
			ui->viewport->update();
		}
	});
	connect(ui->emitbutton, &QPushButton::clicked, [=](bool) {
		QColor color = QColorDialog::getColor(QColor(ui->emitR->value(), ui->emitG->value(), ui->emitB->value()), this, u8"漫反射");
		if (color.isValid())
		{
			ui->emitbutton->setStyleSheet(QString("border:2px solid #333333;border-radius:0;background-color:%1;width:50px;height:25px;").arg(color.name()));
			ui->emitR->setValue(color.red());
			ui->emitG->setValue(color.green());
			ui->emitB->setValue(color.blue());
		}
	});
	connect(ui->emittexbutton, &QPushButton::clicked, [=](bool) {
		QString file = QFileDialog::getOpenFileName(this, u8"打开文件");
		if (!file.isEmpty() && QFileInfo(file).exists() && QFileInfo(file).isFile())
		{
			ui->emittexpath->setText(file);
			emitTexChanged(file);
		}
	});
	connect(ui->emittexpath, &QLineEdit::editingFinished, [=]() {
		QString file = ui->emittexpath->text();
		if (file.isEmpty() || !file.isEmpty() && QFileInfo(file).exists() && QFileInfo(file).isFile())
		{
			ui->emittexpath->setText(file);
			emitTexChanged(file);
		}
		else
		{
			MyDefaultMaterial *mtl = getCurrentMtl();
			if (mtl)
				ui->emittexpath->setText(mtl->texpath[5]);
			else
				ui->emittexpath->setText("");
		}
	});
}

void MainWindow::emitTexChanged(QString file)
{
	int row = ui->materiallist->currentRow();
	if (row >= 0)
	{
		MyDefaultMaterial *mtl = ui->viewport->materiallist[mtlindex[row]];
		mtl->setEmitTexture(file);
		mtl->texpath[5] = file;
		ui->viewport->update();
	}
}

void MainWindow::setupAOPanel()
{
	ui->aopanel->hide();
	connect(ui->aotexbutton, &QPushButton::clicked, [=](bool) {
		QString file = QFileDialog::getOpenFileName(this, u8"打开文件");
		if (!file.isEmpty() && QFileInfo(file).exists() && QFileInfo(file).isFile())
		{
			ui->aotexpath->setText(file);
			aoTexChanged(file);
		}
	});
	connect(ui->aotexpath, &QLineEdit::editingFinished, [=]() {
		QString file = ui->spectexpath->text();
		if (file.isEmpty() || !file.isEmpty() && QFileInfo(file).exists() && QFileInfo(file).isFile())
		{
			ui->aotexpath->setText(file);
			aoTexChanged(file);
		}
		else
		{
			MyDefaultMaterial *mtl = getCurrentMtl();
			if (mtl)
				ui->aotexpath->setText(mtl->texpath[6]);
			else
				ui->aotexpath->setText("");
		}
	});
}

void MainWindow::aoTexChanged(QString file)
{
	int row = ui->materiallist->currentRow();
	if (row >= 0)
	{
		MyDefaultMaterial *mtl = ui->viewport->materiallist[mtlindex[row]];
		mtl->setAOTexture(file);
		mtl->texpath[6] = file;
		ui->viewport->update();
	}
}

void MainWindow::setupEnvPanel()
{
	connect(ui->env, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		ui->viewport->env_intensity = v;
		ui->slider_env->setValue(glm::clamp(v, -5.0, 5.0) * 10 + 50);
		ui->viewport->update();
	});
	connect(ui->slider_env, &QAbstractSlider::sliderMoved, [=](int v) {
		ui->env->setValue((v - 50.0) / 10);
	});
	ui->env->setValue(ui->viewport->env_intensity);
}

void MainWindow::setupObjectPanel()
{
	setupObjList();
	setupTransPanel();
	setupAttribPanel();
}

void MainWindow::camSelected()
{
	ui->frame_trans->setDisabled(true);
	ui->attribpanel->show();
	ui->frame_object->hide();
	ui->frame_light->hide();
	ui->frame_camera->show();
	ui->obj_name->setText(u8"摄像机");
	ui->obj_name->setDisabled(true);
	ui->deleteobj->setDisabled(true);
	ui->projmode->setCurrentIndex(ui->viewport->cam->perspective ? 0 : 1);
	ui->fov->setValue(ui->viewport->cam->fov);
}

void MainWindow::setAttribVisible(bool a1, bool a2, bool a3, bool a4, bool a5)
{
	ui->label_attrib1->setVisible(a1);
	ui->attrib1->setVisible(a1);
	ui->label_attrib2->setVisible(a2);
	ui->attrib2->setVisible(a2);
	ui->label_attrib3->setVisible(a3);
	ui->attrib3->setVisible(a3);
	ui->label_attrib4->setVisible(a4);
	ui->attrib4->setVisible(a4);
	ui->label_frag->setVisible(a5);
	ui->fragment->setVisible(a5);
}

void MainWindow::setAttribTitle(QString t1, QString t2, QString t3, QString t4)
{
	ui->label_attrib1->setText(t1);
	ui->label_attrib2->setText(t2);
	ui->label_attrib3->setText(t3);
	ui->label_attrib4->setText(t4);
}

void MainWindow::objSelected(MyObject * obj)
{
	ui->attribpanel->show();
	ui->frame_object->show();
	ui->frame_light->hide();
	ui->frame_camera->hide();
	ui->frame_trans->setEnabled(true);
	ui->label_sx->setEnabled(true);
	ui->label_sy->setEnabled(true);
	ui->label_sz->setEnabled(true);
	ui->sx->setEnabled(true);
	ui->sy->setEnabled(true);
	ui->sz->setEnabled(true);
	ui->px->setValue(obj->position.x);
	ui->py->setValue(obj->position.y);
	ui->pz->setValue(obj->position.z);
	ui->sx->setValue(obj->scale.x);
	ui->sy->setValue(obj->scale.y);
	ui->sz->setValue(obj->scale.z);
	ui->rh->setValue(obj->rotation.x);
	ui->rp->setValue(obj->rotation.y);
	ui->rb->setValue(obj->rotation.z);
	ui->obj_name->setText(obj->name);
	ui->obj_name->setEnabled(true);
	ui->deleteobj->setEnabled(true);
	switch (obj->type())
	{
	case MyObject::Cube:
	{
		MyCube *o = dynamic_cast<MyCube*>(obj);
		setAttribVisible(true, true, true, false, false);
		setAttribTitle(u8"尺寸.X", u8"尺寸.Y", u8"尺寸.Z", "");
		glm::vec3 s = o->getSize();
		ui->attrib1->setValue(s.x);
		ui->attrib2->setValue(s.y);
		ui->attrib3->setValue(s.z);
		break;
	}
	case MyObject::Plane:
	{
		MyPlane *o = dynamic_cast<MyPlane*>(obj);
		setAttribVisible(true, true, false, false, false);
		setAttribTitle(u8"长度", u8"宽度", u8"", u8"");
		glm::vec2 s = o->getSize();
		ui->attrib1->setValue(s.x);
		ui->attrib2->setValue(s.y);
		break;
	}
	case MyObject::Disk:
	{
		MyDisk *o = dynamic_cast<MyDisk*>(obj);
		setAttribVisible(true, false, false, false, true);
		setAttribTitle(u8"半径", u8"", u8"", u8"");
		ui->attrib1->setValue(o->getSize());
		ui->fragment->setValue(o->getFragment());
		break;
	}
	case MyObject::Sphere:
	{
		MySphere *o = dynamic_cast<MySphere*>(obj);
		setAttribVisible(true, false, false, false, true);
		setAttribTitle(u8"半径", u8"", u8"", u8"");
		ui->attrib1->setValue(o->getSize());
		ui->fragment->setValue(o->getFragment());
		break;
	}
	case MyObject::Cylinder:
	{
		MyCylinder *o = dynamic_cast<MyCylinder*>(obj);
		setAttribVisible(true, true, false, false, true);
		setAttribTitle(u8"半径", u8"高度", u8"", u8"");
		glm::vec2 s = o->getSize();
		ui->attrib1->setValue(s.x);
		ui->attrib2->setValue(s.y);
		ui->fragment->setValue(o->getFragment());
		break;
	}
	case MyObject::Prism:
	{
		MyPrism *o = dynamic_cast<MyPrism*>(obj);
		setAttribVisible(true, true, false, false, true);
		setAttribTitle(u8"半径", u8"高度", u8"", u8"");
		glm::vec2 s = o->getSize();
		ui->attrib1->setValue(s.x);
		ui->attrib2->setValue(s.y);
		ui->fragment->setValue(o->getFragment());
		break;
	}
	case MyObject::Cone:
	{
		MyCone *o = dynamic_cast<MyCone*>(obj);
		setAttribVisible(true, true, true, false, true);
		setAttribTitle(u8"顶部半径", u8"底部半径", u8"高度", u8"");
		glm::vec3 s = o->getSize();
		ui->attrib1->setValue(s.x);
		ui->attrib2->setValue(s.y);
		ui->attrib3->setValue(s.z);
		ui->fragment->setValue(o->getFragment());
		break;
	}
	case MyObject::Pyramid:
	{
		MyPyramid *o = dynamic_cast<MyPyramid*>(obj);
		setAttribVisible(true, true, true, false, true);
		setAttribTitle(u8"顶部半径", u8"底部半径", u8"高度", u8"");
		glm::vec3 s = o->getSize();
		ui->attrib1->setValue(s.x);
		ui->attrib2->setValue(s.y);
		ui->attrib3->setValue(s.z);
		ui->fragment->setValue(o->getFragment());
		break;
	}
	case MyObject::Import:
	{
		setAttribVisible(false, false, false, false, false);
		break;
	}
	}
}

MyObject * MainWindow::getCurObject()
{
	MyObject *obj = nullptr;
	int row = ui->objectlist->currentRow();
	if (row >= 0)
	{
		if (objtype[row] == Object)
		{
			obj = ui->viewport->objlist[objindex[row]];
		}
	}
	return obj;
}

void MainWindow::lightSelected(MyLight * light)
{
	ui->frame_trans->setEnabled(true);
	ui->label_sx->setDisabled(true);
	ui->label_sy->setDisabled(true);
	ui->label_sz->setDisabled(true);
	ui->sx->setDisabled(true);
	ui->sy->setDisabled(true);
	ui->sz->setDisabled(true);
	ui->sx->setValue(1);
	ui->sy->setValue(1);
	ui->sz->setValue(1);
	ui->px->setValue(light->position.x);
	ui->py->setValue(light->position.y);
	ui->pz->setValue(light->position.z);
	ui->rh->setValue(light->rotation.x);
	ui->rp->setValue(light->rotation.y);
	ui->rb->setValue(light->rotation.z);
	ui->obj_name->setText(light->name);
	ui->obj_name->setEnabled(true);
	ui->deleteobj->setEnabled(true);
	QColor c = getLightColor(light);
	ui->lR->setValue(c.red());
	ui->lG->setValue(c.green());
	ui->lB->setValue(c.blue());
	ui->lintensity->setValue(getLightIntensity(light));
	ui->attribpanel->show();
	ui->frame_object->hide();
	ui->frame_light->show();
	ui->frame_camera->hide();
}

MyLight * MainWindow::getCurLight()
{
	MyLight *light = nullptr;
	int row = ui->objectlist->currentRow();
	if (row >= 0)
	{
		if (objtype[row] == Light)
		{
			light = ui->viewport->lightlist[objindex[row]];
		}
	}
	return light;
}

void MainWindow::setupObjList()
{
	connect(ui->objectlist, &QListWidget::currentRowChanged, [=](int row) {
		if (row >= 0 && row < ui->objectlist->count())
		{
			switch (objtype[row])
			{
				case Object:
				{
					MyObject *obj = ui->viewport->objlist[objindex[row]];
					objSelected(obj);
					break;
				}
				case Light:
				{
					MyLight *light = ui->viewport->lightlist[objindex[row]];
					lightSelected(light);
					break;
				}
				case Camera:
				{
					camSelected();
					break;
				}
			}
		}
		else
		{
			ui->attribpanel->hide();
		}
	});
}

void MainWindow::setupTransPanel()
{
	connect(ui->px, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		glm::vec3 pos = getCurrentPos();
		setCurrentPos(v, pos.y, pos.z);
		ui->viewport->update();
	});
	connect(ui->py, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		glm::vec3 pos = getCurrentPos();
		setCurrentPos(pos.x, v, pos.z);
		ui->viewport->update();
	});
	connect(ui->pz, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		glm::vec3 pos = getCurrentPos();
		setCurrentPos(pos.x, pos.y, v);
		ui->viewport->update();
	});
	connect(ui->sx, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		glm::vec3 scale = getCurrentScale();
		setCurrentScale(v, scale.y, scale.z);
		ui->viewport->update();
	});
	connect(ui->sy, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		glm::vec3 scale = getCurrentScale();
		setCurrentScale(scale.x, v, scale.z);
		ui->viewport->update();
	});
	connect(ui->sz, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		glm::vec3 scale = getCurrentScale();
		setCurrentScale(scale.x, scale.y,v);
		ui->viewport->update();
	});
	connect(ui->rh, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		glm::vec3 rotate = getCurrentRotation();
		setCurrentRotation(v, rotate.y, rotate.z);
		ui->viewport->update();
	});
	connect(ui->rp, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		glm::vec3 rotate = getCurrentRotation();
		setCurrentRotation(rotate.x, v, rotate.z);
		ui->viewport->update();
	});
	connect(ui->rb, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		glm::vec3 rotate = getCurrentRotation();
		setCurrentRotation(rotate.x, rotate.y, v);
		ui->viewport->update();
	});
}

void MainWindow::setupAttribPanel()
{
	connect(ui->obj_name, &QLineEdit::editingFinished, [=]() {
		QString text = ui->obj_name->text();
		if (!text.isEmpty())
			setCurrentName(text);
		else
			ui->obj_name->setText(getCurrentName());
	});
	connect(ui->deleteobj, &QPushButton::clicked, [=](bool) {
		int idx = ui->objectlist->currentRow();
		if (idx >= 0)
		{
			if (objtype[idx] == Object)
			{
				MyObject *obj = ui->viewport->objlist[objindex[idx]];
				delete ui->objectlist->takeItem(idx);
				ui->viewport->objlist[objindex[idx]] = nullptr;
				objindex.removeAt(idx);
				objtype.removeAt(idx);
				delete obj;
				ui->viewport->update();
			}
			else if (objtype[idx] == Light)
			{
				MyLight *obj = ui->viewport->lightlist[objindex[idx]];
				delete ui->objectlist->takeItem(idx);
				ui->viewport->lightlist[objindex[idx]] = nullptr;
				objindex.removeAt(idx);
				objtype.removeAt(idx);
				delete obj;
				ui->viewport->update();
			}
		}
	});
	setupCamFrame();
	setupLightFrame();
	setupObjFrame();
}

void MainWindow::setupCamFrame()
{
	ui->frame_camera->hide();
	connect(ui->projmode, QOverload<int>::of(&QComboBox::currentIndexChanged), [=](int index) {
		ui->viewport->cam->perspective = index == 0;
		ui->viewport->update();
	});
	connect(ui->fov, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		ui->viewport->cam->fov = v;
		ui->viewport->update();
	});
}

void MainWindow::setupLightFrame()
{
	ui->frame_light->hide();
	connect(ui->lRslider, &QSlider::valueChanged, [=](int value) {
		ui->lR->setValue(value);
		QString style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(%1,0,%2),stop:1 rgb(%1,255,%2));margin:-5px 0;}";
		ui->lGslider->setStyleSheet(style.arg(ui->lR->value()).arg(ui->lB->value()));
		style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(%1,%2,0),stop:1 rgb(%1,%2,255));margin:-5px 0;}";
		ui->lBslider->setStyleSheet(style.arg(ui->lR->value()).arg(ui->lG->value()));
	});
	connect(ui->lR, QOverload<int>::of(&QSpinBox::valueChanged), [=](int v) {
		ui->lRslider->setValue(v);
		ui->lcolorbutton->setStyleSheet(QString("border:2px solid #333333;border-radius:0;background-color:rgb(%1,%2,%3);width:50px;height:25px;").arg(v).arg(ui->lG->value()).arg(ui->lB->value()));
		MyLight *l = getCurLight();
		if (l)
		{
			setLightColor(l, QColor(v, ui->lG->value(), ui->lB->value()));
			ui->viewport->update();
		}
	});
	connect(ui->lGslider, &QSlider::valueChanged, [=](int value) {
		ui->lG->setValue(value);
		QString style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(0,%1,%2),stop:1 rgb(255,%1,%2));margin:-5px 0;}";
		ui->lRslider->setStyleSheet(style.arg(ui->lG->value()).arg(ui->lB->value()));
		style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(%1,%2,0),stop:1 rgb(%1,%2,255));margin:-5px 0;}";
		ui->lBslider->setStyleSheet(style.arg(ui->lR->value()).arg(ui->lG->value()));
	});
	connect(ui->lG, QOverload<int>::of(&QSpinBox::valueChanged), [=](int v) {
		ui->lGslider->setValue(v);
		ui->lcolorbutton->setStyleSheet(QString("border:2px solid #333333;border-radius:0;background-color:rgb(%1,%2,%3);width:50px;height:25px;").arg(ui->lR->value()).arg(v).arg(ui->lB->value()));
		MyLight *l = getCurLight();
		if (l)
		{
			setLightColor(l, QColor(ui->lR->value(), v, ui->lB->value()));
			ui->viewport->update();
		}
	});
	connect(ui->lBslider, &QSlider::valueChanged, [=](int value) {
		ui->lB->setValue(value);
		QString style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(0,%1,%2),stop:1 rgb(255,%1,%2));margin:-5px 0;}";
		ui->lRslider->setStyleSheet(style.arg(ui->lG->value()).arg(ui->lB->value()));
		style = "QSlider::groove{border:1px solid #333333;height:15px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgb(%1,0,%2),stop:1 rgb(%1,255,%2));margin:-5px 0;}";
		ui->lGslider->setStyleSheet(style.arg(ui->lR->value()).arg(ui->lB->value()));
	});
	connect(ui->lB, QOverload<int>::of(&QSpinBox::valueChanged), [=](int v) {
		ui->lBslider->setValue(v);
		ui->lcolorbutton->setStyleSheet(QString("border:2px solid #333333;border-radius:0;background-color:rgb(%1,%2,%3);width:50px;height:25px;").arg(ui->lR->value()).arg(ui->lG->value()).arg(v));
		MyLight *l = getCurLight();
		if (l)
		{
			setLightColor(l, QColor(ui->lR->value(), ui->lG->value(), v));
			ui->viewport->update();
		}
	});
	connect(ui->lcolorbutton, &QPushButton::clicked, [=](bool) {
		QColor color = QColorDialog::getColor(QColor(ui->lR->value(), ui->lG->value(), ui->lB->value()), this, u8"颜色");
		if (color.isValid())
		{
			ui->lcolorbutton->setStyleSheet(QString("border:2px solid #333333;border-radius:0;background-color:%1;width:50px;height:25px;").arg(color.name()));
			ui->lR->setValue(color.red());
			ui->lG->setValue(color.green());
			ui->lB->setValue(color.blue());
		}
	});
	connect(ui->lintensity, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		ui->slider_lintensity->setValue(v > 1 ? 100 : v * 100);
		MyLight *l = getCurLight();
		if (l)
		{
			setLightIntensity(l, v);
			ui->viewport->update();
		}
	});
	connect(ui->slider_lintensity, &QAbstractSlider::sliderMoved, [=](int v) {
		ui->lintensity->setValue(v / 100.0);
	});
}

void MainWindow::setupObjFrame()
{
	ui->frame_object->hide();
	connect(ui->attrib1, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		MyObject *obj = getCurObject();
		if (obj)
		{
			setCurrentAttrib1(obj, v);
			ui->viewport->update();
		}
	});
	connect(ui->attrib2, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		MyObject *obj = getCurObject();
		if (obj)
		{
			setCurrentAttrib2(obj, v);
			ui->viewport->update();
		}
	});
	connect(ui->attrib3, QOverload<double>::of(&QDoubleSpinBox::valueChanged), [=](double v) {
		MyObject *obj = getCurObject();
		if (obj)
		{
			setCurrentAttrib3(obj, v);
			ui->viewport->update();
		}
	});
	connect(ui->fragment, QOverload<int>::of(&QSpinBox::valueChanged), [=](int v) {
		MyObject *obj = getCurObject();
		if (obj)
		{
			setCurrentAttrib5(obj, v);
			ui->viewport->update();
		}
	});
}

void MainWindow::setLightColor(MyLight * light, QColor c)
{
	switch (light->type())
	{
	case MyLight::Direction:
	{
		DirectLight *l = dynamic_cast<DirectLight*>(light);
		l->l_color.r = c.redF();
		l->l_color.g = c.greenF();
		l->l_color.b = c.blueF();
		break;
	}
	case MyLight::Spot:
	{
		SpotLight *l = dynamic_cast<SpotLight*>(light);
		l->l_color.r = c.redF();
		l->l_color.g = c.greenF();
		l->l_color.b = c.blueF();
		break;
	}
	}
}

QColor MainWindow::getLightColor(MyLight * light)
{
	switch (light->type())
	{
	case MyLight::Direction:
	{
		DirectLight *l = dynamic_cast<DirectLight*>(light);
		return QColor(l->l_color.r * 255, l->l_color.g * 255, l->l_color.b * 255);
	}
	case MyLight::Spot:
	{
		SpotLight *l = dynamic_cast<SpotLight*>(light);
		return QColor(l->l_color.r * 255, l->l_color.g * 255, l->l_color.b * 255);
	}
	}
	return QColor(0, 0, 0);
}

void MainWindow::setLightIntensity(MyLight * light, float I)
{
	switch (light->type())
	{
	case MyLight::Direction:
	{
		DirectLight *l = dynamic_cast<DirectLight*>(light);
		l->l_intensity = I;
		break;
	}
	case MyLight::Spot:
	{
		SpotLight *l = dynamic_cast<SpotLight*>(light);
		l->l_intensity = I;
		break;
	}
	}
}

float MainWindow::getLightIntensity(MyLight * light)
{
	switch (light->type())
	{
	case MyLight::Direction:
	{
		DirectLight *l = dynamic_cast<DirectLight*>(light);
		return l->l_intensity;
	}
	case MyLight::Spot:
	{
		SpotLight *l = dynamic_cast<SpotLight*>(light);
		return l->l_intensity;
	}
	}
	return 0;
}

void MainWindow::setCurrentPos(float x, float y, float z)
{
	int row = ui->objectlist->currentRow();
	if (row >= 0)
	{
		switch (objtype[row])
		{
		case Light:
		{
			MyLight *l = ui->viewport->lightlist[objindex[row]];
			l->position = glm::vec3(x, y, z);
			break;
		}
		case Object:
		{
			MyObject *obj = ui->viewport->objlist[objindex[row]];
			obj->position = glm::vec3(x, y, z);
			break;
		}
		}
	}
}

glm::vec3 MainWindow::getCurrentPos()
{
	int row = ui->objectlist->currentRow();
	if (row >= 0)
	{
		switch (objtype[row])
		{
		case Light:
		{
			MyLight *l = ui->viewport->lightlist[objindex[row]];
			return l->position;
		}
		case Object:
		{
			MyObject *obj = ui->viewport->objlist[objindex[row]];
			return obj->position;
		}
		}
	}
	return glm::vec3(0);
}

void MainWindow::setCurrentScale(float x, float y, float z)
{
	int row = ui->objectlist->currentRow();
	if (row >= 0)
	{
		switch (objtype[row])
		{
		case Object:
		{
			MyObject *obj = ui->viewport->objlist[objindex[row]];
			obj->scale = glm::vec3(x, y, z);
			break;
		}
		}
	}
}

glm::vec3 MainWindow::getCurrentScale()
{
	int row = ui->objectlist->currentRow();
	if (row >= 0)
	{
		switch (objtype[row])
		{
		case Object:
		{
			MyObject *obj = ui->viewport->objlist[objindex[row]];
			return obj->scale;
		}
		}
	}
	return glm::vec3(1);
}

void MainWindow::setCurrentRotation(float x, float y, float z)
{
	int row = ui->objectlist->currentRow();
	if (row >= 0)
	{
		switch (objtype[row])
		{
		case Light:
		{
			MyLight *l = ui->viewport->lightlist[objindex[row]];
			l->rotation = glm::vec3(x, y, z);
			break;
		}
		case Object:
		{
			MyObject *obj = ui->viewport->objlist[objindex[row]];
			obj->rotation = glm::vec3(x, y, z);
			break;
		}
		}
	}
}

glm::vec3 MainWindow::getCurrentRotation()
{
	int row = ui->objectlist->currentRow();
	if (row >= 0)
	{
		switch (objtype[row])
		{
		case Light:
		{
			MyLight *l = ui->viewport->lightlist[objindex[row]];
			return l->rotation;
		}
		case Object:
		{
			MyObject *obj = ui->viewport->objlist[objindex[row]];
			return obj->rotation;
		}
		}
	}
	return glm::vec3(0);
}

void MainWindow::setCurrentName(QString name)
{
	int row = ui->objectlist->currentRow();
	if (row >= 0)
	{
		switch (objtype[row])
		{
		case Light:
		{
			MyLight *l = ui->viewport->lightlist[objindex[row]];
			l->name = name;
			ui->objectlist->currentItem()->setText(name);
			break;
		}
		case Object:
		{
			MyObject *obj = ui->viewport->objlist[objindex[row]];
			obj->name = name;
			ui->objectlist->currentItem()->setText(name);
			break;
		}
		}
	}
}

QString MainWindow::getCurrentName()
{
	int row = ui->objectlist->currentRow();
	if (row >= 0)
	{
		switch (objtype[row])
		{
		case Light:
		{
			MyLight *l = ui->viewport->lightlist[objindex[row]];
			return l->name;
		}
		case Object:
		{
			MyObject *obj = ui->viewport->objlist[objindex[row]];
			return obj->name;
		}
		}
	}
	return "";
}

void MainWindow::setCurrentAttrib1(MyObject * obj, float v)
{
	switch (obj->type())
	{
	case MyObject::Cube:
	{
		MyCube *o = dynamic_cast<MyCube*>(obj);
		glm::vec3 s = o->getSize();
		o->setSize(v, s.y, s.z);
		break;
	}
	case MyObject::Plane:
	{
		MyPlane *o = dynamic_cast<MyPlane*>(obj);
		glm::vec2 s = o->getSize();
		o->setSize(v, s.y);
		break;
	}
	case MyObject::Disk:
	{
		MyDisk *o = dynamic_cast<MyDisk*>(obj);
		o->setSize(v, o->getFragment());
		break;
	}
	case MyObject::Sphere:
	{
		MySphere *o = dynamic_cast<MySphere*>(obj);
		o->setSize(v, o->getFragment());
		break;
	}
	case MyObject::Cylinder:
	{
		MyCylinder *o = dynamic_cast<MyCylinder*>(obj);
		glm::vec2 s = o->getSize();
		o->setSize(v, s.y, o->getFragment());
		break;
	}
	case MyObject::Prism:
	{
		MyPrism *o = dynamic_cast<MyPrism*>(obj);
		glm::vec2 s = o->getSize();
		o->setSize(v, s.y, o->getFragment());
		break;
	}
	case MyObject::Cone:
	{
		MyCone *o = dynamic_cast<MyCone*>(obj);
		glm::vec3 s = o->getSize();
		o->setSize(v, s.y, s.z, o->getFragment());
		break;
	}
	case MyObject::Pyramid:
	{
		MyPyramid *o = dynamic_cast<MyPyramid*>(obj);
		glm::vec3 s = o->getSize();
		o->setSize(v, s.y, s.z, o->getFragment());
		break;
	}
	}
}

void MainWindow::setCurrentAttrib2(MyObject * obj, float v)
{
	switch (obj->type())
	{
	case MyObject::Cube:
	{
		MyCube *o = dynamic_cast<MyCube*>(obj);
		glm::vec3 s = o->getSize();
		o->setSize(s.x, v, s.z);
		break;
	}
	case MyObject::Plane:
	{
		MyPlane *o = dynamic_cast<MyPlane*>(obj);
		glm::vec2 s = o->getSize();
		o->setSize(s.x, v);
		break;
	}
	case MyObject::Cylinder:
	{
		MyCylinder *o = dynamic_cast<MyCylinder*>(obj);
		glm::vec2 s = o->getSize();
		o->setSize(s.x, v, o->getFragment());
		break;
	}
	case MyObject::Prism:
	{
		MyPrism *o = dynamic_cast<MyPrism*>(obj);
		glm::vec2 s = o->getSize();
		o->setSize(s.x, v, o->getFragment());
		break;
	}
	case MyObject::Cone:
	{
		MyCone *o = dynamic_cast<MyCone*>(obj);
		glm::vec3 s = o->getSize();
		o->setSize(s.x, v, s.z, o->getFragment());
		break;
	}
	case MyObject::Pyramid:
	{
		MyPyramid *o = dynamic_cast<MyPyramid*>(obj);
		glm::vec3 s = o->getSize();
		o->setSize(s.x, v, s.z, o->getFragment());
		break;
	}
	}
}

void MainWindow::setCurrentAttrib3(MyObject * obj, float v)
{
	switch (obj->type())
	{
	case MyObject::Cube:
	{
		MyCube *o = dynamic_cast<MyCube*>(obj);
		glm::vec3 s = o->getSize();
		o->setSize(s.x, s.y, v);
		break;
	}
	case MyObject::Cone:
	{
		MyCone *o = dynamic_cast<MyCone*>(obj);
		glm::vec3 s = o->getSize();
		o->setSize(s.x, s.y, v, o->getFragment());
		break;
	}
	case MyObject::Pyramid:
	{
		MyPyramid *o = dynamic_cast<MyPyramid*>(obj);
		glm::vec3 s = o->getSize();
		o->setSize(s.x, s.y, v, o->getFragment());
		break;
	}
	}
}

void MainWindow::setCurrentAttrib5(MyObject * obj, unsigned v)
{
	switch (obj->type())
	{
	case MyObject::Disk:
	{
		MyDisk *o = dynamic_cast<MyDisk*>(obj);
		o->setSize(o->getSize(), v);
		break;
	}
	case MyObject::Sphere:
	{
		MySphere *o = dynamic_cast<MySphere*>(obj);
		o->setSize(o->getSize(), v);
		break;
	}
	case MyObject::Cylinder:
	{
		MyCylinder *o = dynamic_cast<MyCylinder*>(obj);
		glm::vec2 s = o->getSize();
		o->setSize(s.x, s.y, v);
		break;
	}
	case MyObject::Prism:
	{
		MyPrism *o = dynamic_cast<MyPrism*>(obj);
		glm::vec2 s = o->getSize();
		o->setSize(s.x, s.y, v);
		break;
	}
	case MyObject::Cone:
	{
		MyCone *o = dynamic_cast<MyCone*>(obj);
		glm::vec3 s = o->getSize();
		o->setSize(s.x, s.y, s.z, v);
		break;
	}
	case MyObject::Pyramid:
	{
		MyPyramid *o = dynamic_cast<MyPyramid*>(obj);
		glm::vec3 s = o->getSize();
		o->setSize(s.x, s.y, s.z, v);
		break;
	}
	}
}
