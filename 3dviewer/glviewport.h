#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLTexture>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QSharedPointer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFramebufferObjectFormat>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

struct Vertex
{
	glm::vec3 Position;
	glm::vec3 Normal;
	glm::vec2 TexCoords;
	glm::vec3 Tangent;
	glm::vec3 BiTangent;
};

class GLViewport;
class MyMaterial
{
protected:
	GLViewport *parent;
	QList<QOpenGLTexture*> tex;
	QOpenGLShaderProgram *shader;
public:
	virtual ~MyMaterial() {}

	virtual void bind() = 0;
	QOpenGLShaderProgram* getShader();

	QString name;
};

class MyDefaultMaterial :public MyMaterial
{
	static const unsigned TEXNUM;
protected:
	MyDefaultMaterial() {}
	MyDefaultMaterial(glm::vec3 d, glm::vec3 s);

	bool _repeat, _mirrow;
public:
	~MyDefaultMaterial();

	void bind();

	void setDiffTexture(QString file);
	void setSpecTexture(QString file);
	void setNormalTexture(QString file);
	void setMetalTexture(QString file);
	void setRoughTexture(QString file);
	void setEmitTexture(QString file);
	void setAOTexture(QString file);
	bool repeat();
	bool mirrow();
	void setRepeatMirrow(bool r, bool m);

	static MyDefaultMaterial* createMaterial(GLViewport *pa, QOpenGLShaderProgram *s);
	static MyDefaultMaterial* createMaterial(GLViewport *pa, QOpenGLShaderProgram *s, glm::vec3 diff, glm::vec3 spec, float shine);
	static MyDefaultMaterial* createMaterial(GLViewport *pa, QOpenGLShaderProgram *s, glm::vec3 diff, glm::vec3 spec, float metal, float rough);

	glm::vec3 diffuse;
	glm::vec3 specular;
	glm::vec3 emission;
	glm::vec2 uvoffset;
	glm::vec2 uvscale;
	float shine;
	float rough;
	float metal;
	QList<QString> texpath;
};

class MyColorMaterial :public MyMaterial
{
	MyColorMaterial(glm::vec3 c) :color(c) {}
public:
	void bind();
	void bind(glm::vec3 c);

	static MyColorMaterial* createMaterial(GLViewport *pa, QOpenGLShaderProgram *s, glm::vec3 c);
	glm::vec3 color;
};

class MyObject
{
public:
	enum Type
	{
		Plane,
		Disk,
		Cube,
		Sphere,
		Cylinder,
		Prism,
		Cone,
		Pyramid,
		Import
	};
private:
	static unsigned idcount;
protected:
	glm::mat4 getMatrix();
	glm::mat4 getHPBMatrix();
	glm::vec3 calcTangent(Vertex &p1, Vertex &p2, Vertex &p3);
	glm::vec3 calcBitangent(Vertex &p1, Vertex &p2, Vertex &p3);
	glm::vec3 calcTangent(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec2 uv1, glm::vec2 uv2, glm::vec2 uv3);
	glm::vec3 calcBitangent(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec2 uv1, glm::vec2 uv2, glm::vec2 uv3);
	void allocateVertices(const void *data, int count);
	void allocateIndices(const void *data, int count);
	void setVertexAttrib();

	QOpenGLVertexArrayObject VAO;
	QOpenGLBuffer VBO, EBO;
	GLViewport *parent;
	MyMaterial *mat;
	unsigned id;
	Type T;

public:
	MyObject(GLViewport *pa);
	MyObject(GLViewport *pa, QString name);
	virtual ~MyObject() {}

	unsigned getID();
	Type type();
	void setMaterial(MyMaterial *m);
	MyMaterial* getMaterial();
	virtual void draw();
	virtual void addVertexAndIndices(std::vector<Vertex> &ver, std::vector<unsigned> &ind) {}

	glm::vec3 position;
	glm::vec3 rotation;
	glm::vec3 scale;
	QString name;
};

class MyPlane :public MyObject
{
	float width, height;
public:
	MyPlane(GLViewport *pa);
	MyPlane(GLViewport *pa, QString name);

	void setSize(float w, float h);
	void addVertexAndIndices(std::vector<Vertex> &vertices, std::vector<unsigned> &indices)
	{
		float w = width;
		float h = height;
		unsigned start = vertices.size();
		Vertex v;
		v.Position = glm::vec4(w / 2, 0, h / 2, 1);
		v.Normal = glm::vec3(0, 1, 0);
		v.TexCoords = glm::vec2(1, 1);
		vertices.push_back(v);
		v.Position = glm::vec4(w / 2, 0, -h / 2, 1);
		v.Normal = glm::vec3(0, 1, 0);
		v.TexCoords = glm::vec2(0, 1);
		vertices.push_back(v);
		v.Position = glm::vec4(-w / 2, 0, -h / 2, 1);
		v.Normal = glm::vec3(0, 1, 0);
		v.TexCoords = glm::vec2(0, 0);
		vertices.push_back(v);
		v.Position = glm::vec4(-w / 2, 0, h / 2, 1);
		v.Normal = glm::vec3(0, 1, 0);
		v.TexCoords = glm::vec2(1, 0);
		vertices.push_back(v);
		indices.push_back(start + 0);
		indices.push_back(start + 1);
		indices.push_back(start + 2);
		indices.push_back(start + 2);
		indices.push_back(start + 3);
		indices.push_back(start + 0);
		glm::mat4 trans = getMatrix();
		for (int i = start; i < vertices.size(); i++)
			vertices[i].Position = trans * glm::vec4(vertices[i].Position, 1);
	}
	glm::vec2 getSize();
};

class MyDisk :public MyObject
{
	float radius;
	unsigned fragment;

	void genDisk(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float r, unsigned f);
public:
	MyDisk(GLViewport * pa);
	MyDisk(GLViewport * pa, QString name);

	void setSize(float r, unsigned f);
	void addVertexAndIndices(std::vector<Vertex> &ver, std::vector<unsigned> &ind)
	{
		unsigned start = ver.size();
		genDisk(ver, ind, radius, fragment);
		glm::mat4 trans = getMatrix();
		for (int i = start; i < ver.size(); i++)
			ver[i].Position = trans * glm::vec4(ver[i].Position, 1);
	}
	float getSize();
	unsigned getFragment();
};

class MyCube :public MyObject
{
	glm::vec3 size;
public:
	MyCube(GLViewport *pa);
	MyCube(GLViewport *pa, QString name);

	void setSize(float x, float y, float z);

	void addVertexAndIndices(std::vector<Vertex> &ver, std::vector<unsigned> &ind)
	{
		unsigned start = ver.size();
		glm::vec3 vertices[8] =
		{
			glm::vec3(size.x / 2,size.y / 2,size.z / 2),
			glm::vec3(size.x / 2,size.y / 2,-size.z / 2),
			glm::vec3(-size.x / 2,size.y / 2,-size.z / 2),
			glm::vec3(-size.x / 2,size.y / 2,size.z / 2),
			glm::vec3(size.x / 2,-size.y / 2,size.z / 2),
			glm::vec3(size.x / 2,-size.y / 2,-size.z / 2),
			glm::vec3(-size.x / 2,-size.y / 2,-size.z / 2),
			glm::vec3(-size.x / 2,-size.y / 2,size.z / 2)
		};
		unsigned face[6][4] =
		{
			{ 0,1,2,3 },
			{ 7,6,5,4 },
			{ 3,7,4,0 },
			{ 2,1,5,6 },
			{ 0,4,5,1 },
			{ 2,6,7,3 }
		};
		glm::vec3 norm[6] =
		{
			glm::vec3(0.0f,1.0f,0.0f),
			glm::vec3(0.0f,-1.0f,0.0f),
			glm::vec3(0.0f,0.0f,1.0f),
			glm::vec3(0.0f,0.0f,-1.0f),
			glm::vec3(1.0f,0.0f,0.0f),
			glm::vec3(-1.0f,0.0f,0.0f)
		};
		glm::vec2 texcoord[4] =
		{
			glm::vec2(0,0),
			glm::vec2(0,1),
			glm::vec2(1,1),
			glm::vec2(1,0)
		};
		Vertex v;
		for (int i = 0; i < 6; i++)
		{
			for (int j = 0; j < 4; j++)
			{
				v.Position = vertices[face[i][j]];
				v.Normal = norm[i];
				v.TexCoords = texcoord[j];
				ver.push_back(v);
			}
			ind.push_back(start + i * 4 + 0);
			ind.push_back(start + i * 4 + 1);
			ind.push_back(start + i * 4 + 2);
			ind.push_back(start + i * 4 + 2);
			ind.push_back(start + i * 4 + 3);
			ind.push_back(start + i * 4 + 0);
		}
		glm::mat4 trans = getMatrix();
		for (int i = start; i < ver.size(); i++)
			ver[i].Position = trans * glm::vec4(ver[i].Position, 1);
	}
	glm::vec3 getSize();
};

class MyCylinder :public MyObject
{
	float radius;
	float height;
	unsigned fragment;

	void genDisk(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float r, float h, unsigned f);
	virtual void genSide(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float r, float h, unsigned f);
protected:
	MyCylinder(GLViewport *pa, int flag) :MyObject(pa) {}
	MyCylinder(GLViewport *pa, QString name, int flag) :MyObject(pa, name) {}
public:
	MyCylinder(GLViewport *pa);
	MyCylinder(GLViewport *pa, QString name);

	void setSize(float r, float h, unsigned f);
	void addVertexAndIndices(std::vector<Vertex> &ver, std::vector<unsigned> &ind)
	{
		unsigned start = ver.size();
		genDisk(ver, ind, radius, height / 2, fragment);
		genDisk(ver, ind, radius, -height / 2, fragment);
		genSide(ver, ind, radius, height / 2, fragment);
		glm::mat4 trans = getMatrix();
		for (int i = start; i < ver.size(); i++)
			ver[i].Position = trans * glm::vec4(ver[i].Position, 1);
	}
	glm::vec2 getSize();
	unsigned getFragment();
};

class MyPrism :public MyCylinder
{
	void genSide(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float r, float h, unsigned f);
public:
	MyPrism(GLViewport *pa);
	MyPrism(GLViewport *pa, QString name);
};

class MySphere :public MyObject
{
	float radius;
	unsigned fragment;

	void genSphere(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float r, unsigned f);
public:
	MySphere(GLViewport *pa);
	MySphere(GLViewport *pa, QString name);

	void setSize(float r, unsigned f);
	void addVertexAndIndices(std::vector<Vertex> &ver, std::vector<unsigned> &ind)
	{
		unsigned start = ver.size();
		genSphere(ver, ind, radius, fragment);
		glm::mat4 trans = getMatrix();
		for (int i = start; i < ver.size(); i++)
			ver[i].Position = trans * glm::vec4(ver[i].Position, 1);
	}
	float getSize();
	unsigned getFragment();
};

class MyCone :public MyObject
{
	float rtop,rbottom;
	float height;
	unsigned fragment;

	void genDisk(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float r, float h, unsigned f);
	virtual void genSide(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float r1, float r2, float h, unsigned f);
protected:
	MyCone(GLViewport *pa, int flag) :MyObject(pa) {}
	MyCone(GLViewport *pa, QString name, int flag) :MyObject(pa, name) {}
public:
	MyCone(GLViewport *pa);
	MyCone(GLViewport *pa, QString name);

	void setSize(float r1, float r2, float h, unsigned f);
	void addVertexAndIndices(std::vector<Vertex> &ver, std::vector<unsigned> &ind)
	{
		unsigned start = ver.size();
		if (rtop > 0.0f)
			genDisk(ver, ind, rtop, height / 2, fragment);
		if (rbottom > 0.0f)
			genDisk(ver, ind, rbottom, -height / 2, fragment);
		genSide(ver, ind, rtop, rbottom, height / 2, fragment);
		glm::mat4 trans = getMatrix();
		for (int i = start; i < ver.size(); i++)
			ver[i].Position = trans * glm::vec4(ver[i].Position, 1);
	}
	glm::vec3 getSize();
	unsigned getFragment();
};

class MyPyramid :public MyCone
{
	void genSide(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float r1, float r2, float h, unsigned f);
public:
	MyPyramid(GLViewport *pa);
	MyPyramid(GLViewport *pa, QString name);
};

class MyGrid :public MyObject
{
	float width;
	float height;
	unsigned col;
	unsigned row;

	void genGrid(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float w, float h, unsigned c, unsigned r);
public:
	MyGrid(GLViewport *pa);
	MyGrid(GLViewport *pa, QString name);

	void draw();
	void setSize(float w, float h, unsigned c, unsigned r);
	glm::vec2 getSize();
};

class MyImport : public MyObject
{
	std::vector<Vertex> ver;
	std::vector<unsigned> ind;
public:
	MyImport(GLViewport *pa) :MyObject(pa) { T = Import; }
	MyImport(GLViewport *pa, QString name) :MyObject(pa, name) { T = Import; }

	std::vector<Vertex>& getVertexArray()
	{
		return ver;
	}
	std::vector<unsigned>& getIndicesArray()
	{
		return ind;
	}
	void updateData()
	{
		VAO.bind();
		VBO.bind();
		EBO.bind();
		allocateVertices(&ver[0], ver.size() * sizeof(Vertex));
		allocateIndices(&ind[0], ind.size() * sizeof(unsigned));
		setVertexAttrib();
	}
	void addVertexAndIndices(std::vector<Vertex> &vertex, std::vector<unsigned> &index)
	{
		unsigned start = vertex.size();
		for (int i = 0; i < ver.size(); i++)
			vertex.push_back(ver[i]);
		for (int i = 0; i < ind.size(); i++)
			index.push_back(start + ind[i]);
	}
};

class DirectLight;
class AmbientLight;
class MyLight
{
public:
	enum Type
	{
		Direction,
		Spot,
		Ambient
	};
protected:
	glm::mat4 getHPBMatrix();
	glm::mat4 getMatrix();
	void allocateVertices(const void *data, int count);
	void setVertexAttrib();

	QOpenGLVertexArrayObject VAO;
	QOpenGLBuffer VBO;

	GLViewport *parent;
	Type T;
public:
	MyLight(GLViewport *pa);
	virtual ~MyLight();

	Type type();
	virtual void setLight(QOpenGLShaderProgram* shader, unsigned index) = 0;
	virtual void draw(MyColorMaterial* mtl) {}

	glm::vec3 position;
	glm::vec3 rotation;
	QString name;
};

class DirectLight :public MyLight
{
	DirectLight(GLViewport *pa);
	void genLines(std::vector<Vertex>& ver);
public:
	void setLight(QOpenGLShaderProgram* shader, unsigned index);
	void draw(MyColorMaterial* mtl);

	static DirectLight* createLight(GLViewport *pa);

	glm::vec3 l_color;
	float l_intensity;
};

class SpotLight :public MyLight
{
	SpotLight(GLViewport *pa);
	void genLines(std::vector<Vertex>& ver);
public:
	void setLight(QOpenGLShaderProgram* shader, unsigned index);
	void draw(MyColorMaterial* mtl);

	static SpotLight* createLight(GLViewport *pa);

	glm::vec3 l_color;
	float l_intensity;
};

class AmbientLight :public MyLight
{
	AmbientLight(GLViewport *pa);
public:
	void setLight(QOpenGLShaderProgram* shader, unsigned index);

	static AmbientLight* createLight(GLViewport *pa);

	glm::vec3 l_color;
};

class MyCamera
{
	friend class GLViewport;
	glm::vec3 pos;
	glm::vec3 target;
	glm::vec3 anchor;
	glm::vec3 up;
	float height;
public:
	MyCamera() :pos(600, 300, 600), target(0), anchor(0), up(0, 1, 0), height(600), fov(30) {}

	glm::mat4 getProj();
	glm::mat4 getView();
	glm::vec3 getPos();

	float fov;
	float ratio;
	bool perspective = true;
};

class GLViewport : public QOpenGLWidget, public QOpenGLFunctions_3_3_Core
{
	Q_OBJECT

public:
	GLViewport(QWidget *parent = Q_NULLPTR);
	~GLViewport();

	void setBackground(glm::vec4 color);
	bool saveImage(QString file);
	void switchCamMode(bool mode);

	QOpenGLTexture *env_spec = nullptr;
	QOpenGLTexture *env_diff = nullptr;
	QOpenGLTexture *brdf_lut = nullptr;
	QOpenGLTexture *env = nullptr;
	MyCamera *cam;
	MyGrid* grid;
	MyDefaultMaterial *defaultmtl;
	DirectLight *defaultlight;
	QOpenGLShaderProgram *ushader;
	QOpenGLShaderProgram *colorshader;
	QList<MyObject*> objlist;
	QList<MyLight*> lightlist;
	QList<MyDefaultMaterial*> materiallist;
	float env_intensity;
protected:
	void initializeGL();
	void resizeGL(int w, int h);
	void paintGL();

	void mousePressEvent(QMouseEvent *e);
	void mouseMoveEvent(QMouseEvent *e);
	void mouseReleaseEvent(QMouseEvent *e);
	void wheelEvent(QWheelEvent *e);
private:
	double calcFPS();
	void setAnchor(int x, int y);
	void rotateCam(float x, float y);
	void moveCam(float x, float y);
	void zoomCam(float x, float y, bool direction);
	void genCubemap(unsigned size, float exposure);
	void renderCube();
	void convolutionDiff(unsigned size);
	void convolutionSpec(unsigned size);

	MyColorMaterial *colormtl;
	QOpenGLFramebufferObject* mFBO = nullptr;
	Qt::MouseButton button = Qt::NoButton;

	glm::vec3 clearcolor;
	int mouseX, mouseY;
	float oldDepth;
	bool ismoved;
};