#include "glviewport.h"
#include "stb_image.h"
#include <QFile>
#include <QDebug>
#include <QTime>
#include <QTimer>
#include <QImage>
#include <QOpenGLPixelTransferOptions>
#include <QOpenGLFramebufferObject>

GLViewport::GLViewport(QWidget *parent)
	: QOpenGLWidget(parent), QOpenGLFunctions_3_3_Core()
{
	env_intensity = 0;
	cam = new MyCamera();
	glm::vec4 worldpos(cam->anchor, 1);
	worldpos = cam->getProj() * cam->getView() * worldpos;
	worldpos /= worldpos.w;
	oldDepth = worldpos.z / 2 + 0.5;
	clearcolor = glm::vec4(0.4);
	QSurfaceFormat format;
	format.setDepthBufferSize(24);
	format.setStencilBufferSize(8);
	format.setVersion(3, 3);
	format.setSamples(8);
	//qInfo() <<"samples: "<< format.samples();
	format.setProfile(QSurfaceFormat::CoreProfile);
	setFormat(format);
	/*auto pTimer = new QTimer(this);
	connect(pTimer, &QTimer::timeout, this, QOverload<>::of(&GLViewport::update));
	pTimer->start(1000 / 60.0);*/
}

GLViewport::~GLViewport()
{
	delete cam;
	delete colormtl;
	delete defaultmtl;
	delete ushader;
	delete colorshader;
	delete env_diff;
	delete env_spec;
	delete env;
	delete brdf_lut;
	for (int i = 0; i < objlist.size(); i++)
		if(objlist[i])
			delete objlist[i];
	for (int i = 0; i < lightlist.size(); i++)
		if(lightlist[i])
			delete lightlist[i];
	for (int i = 0; i < materiallist.size(); i++)
		if(materiallist[i])
			delete materiallist[i];
}

void GLViewport::setBackground(glm::vec4 color)
{
	clearcolor = color;
}

bool GLViewport::saveImage(QString file)
{
	return grabFramebuffer().save(file);
}

void GLViewport::renderCube()
{
	const static float skyboxVertices[] = {
		// positions          
		-1.0f,  1.0f, -1.0f,
		-1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f, -1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,

		-1.0f, -1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f,
		-1.0f, -1.0f,  1.0f,

		-1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f, -1.0f,
		 1.0f,  1.0f,  1.0f,
		 1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f,  1.0f,
		-1.0f,  1.0f, -1.0f,

		-1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f, -1.0f,
		 1.0f, -1.0f, -1.0f,
		-1.0f, -1.0f,  1.0f,
		 1.0f, -1.0f,  1.0f
	};
	QOpenGLVertexArrayObject vao;
	QOpenGLBuffer vbo;
	vao.create();
	vbo.create();
	vao.bind();
	vbo.bind();
	vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
	vbo.allocate(skyboxVertices, sizeof(skyboxVertices));
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
	glEnableVertexAttribArray(0);
	glDrawArrays(GL_TRIANGLES, 0, 36);
}

void GLViewport::convolutionDiff(unsigned size)
{
	if (env_diff)
		delete env_diff;
	env_diff = new QOpenGLTexture(QOpenGLTexture::TargetCubeMap);
	env_diff->create();
	env_diff->setSize(size, size);
	env_diff->setFormat(QOpenGLTexture::RGB32F);
	env_diff->allocateStorage();
	env_diff->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
	env_diff->setWrapMode(QOpenGLTexture::ClampToEdge);

	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	glm::mat4 captureViews[] =
	{
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};
	glm::vec3 camup[] = 
	{
		glm::vec3(0.0f, -1.0f,  0.0f),
		glm::vec3(0.0f, -1.0f,  0.0f),
		glm::vec3(0.0f,  0.0f,  1.0f),
		glm::vec3(0.0f,  0.0f, -1.0f),
		glm::vec3(0.0f, -1.0f,  0.0f),
		glm::vec3(0.0f, -1.0f,  0.0f)
	};

	QOpenGLShaderProgram shader;
	if (!shader.addShaderFromSourceFile(QOpenGLShader::Vertex, ":/GLViewport/HDRVS"))
		qCritical() << shader.log();
	if (!shader.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/GLViewport/convolutionFS"))
		qCritical() << shader.log();
	if (!shader.link())
		qCritical() << shader.log();

	QOpenGLFramebufferObject fbo(size, size);
	fbo.bind();
	shader.bind();
	shader.setUniformValue("environmentMap", 0);
	glUniformMatrix4fv(glGetUniformLocation(shader.programId(), "projection"), 1, GL_FALSE, glm::value_ptr(captureProjection));
	env->bind(0);
	glViewport(0, 0, size, size);
	for (unsigned int i = 0; i < 6; ++i)
	{
		shader.setUniformValue("camup", camup[i].x, camup[i].y, camup[i].z);
		glUniformMatrix4fv(glGetUniformLocation(shader.programId(), "view"), 1, GL_FALSE, glm::value_ptr(captureViews[i]));
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, env_diff->textureId(), 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		renderCube(); // renders a 1x1 cube
	}
	fbo.release();
	glViewport(0, 0, this->width(), this->height());
}

void GLViewport::convolutionSpec(unsigned size)
{
	if (env_spec)
		delete env_spec;
	env_spec = new QOpenGLTexture(QOpenGLTexture::TargetCubeMap);
	env_spec->create();
	env_spec->setSize(size, size);
	env_spec->setMipLevels(env_spec->maximumMipLevels());
	env_spec->setFormat(QOpenGLTexture::RGB32F);
	env_spec->allocateStorage();
	env_spec->setMinMagFilters(QOpenGLTexture::LinearMipMapLinear, QOpenGLTexture::Linear);
	env_spec->setWrapMode(QOpenGLTexture::ClampToEdge);
	qInfo() << env_spec->mipLevels();
	env_spec->generateMipMaps();

	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	glm::mat4 captureViews[] =
	{
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};

	QOpenGLShaderProgram shader;
	if (!shader.addShaderFromSourceFile(QOpenGLShader::Vertex, ":/GLViewport/HDRVS"))
		qCritical() << shader.log();
	if (!shader.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/GLViewport/convolutionspecFS"))
		qCritical() << shader.log();
	if (!shader.link())
		qCritical() << shader.log();

	shader.bind();
	shader.setUniformValue("environmentMap", 0);
	glUniformMatrix4fv(glGetUniformLocation(shader.programId(), "projection"), 1, GL_FALSE, glm::value_ptr(captureProjection));
	env->bind(0);
	unsigned int maxMipLevels = 6;
	for (unsigned int mip = 0; mip < maxMipLevels; mip++)
	{
		// reisze framebuffer according to mip-level size.
		unsigned int mipSize = size * std::pow(0.5, mip);
		QOpenGLFramebufferObject fbo(mipSize, mipSize);
		fbo.bind();
		glViewport(0, 0, mipSize, mipSize);

		float roughness = (float)mip / (float)(maxMipLevels - 1);
		shader.setUniformValue("roughness", roughness);
		for (unsigned int i = 0; i < 6; ++i)
		{
			glUniformMatrix4fv(glGetUniformLocation(shader.programId(), "view"), 1, GL_FALSE, glm::value_ptr(captureViews[i]));
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
				GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, env_spec->textureId(), mip);

			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			renderCube();
		}
		fbo.release();
	}
	glViewport(0, 0, this->width(), this->height());
}

void GLViewport::genCubemap(unsigned size, float exposure = 0.0)
{
	QFile file(":/GLViewport/env");
	file.open(QFile::ReadOnly);
	QByteArray buf = file.readAll();
	stbi_set_flip_vertically_on_load(true);
	int width, height, nrComponents;
	float *data = stbi_loadf_from_memory((const unsigned char*)buf.constData(), buf.size(), &width, &height, &nrComponents, 0);
	unsigned int hdrTexture;
	if (data)
	{
		glGenTextures(1, &hdrTexture);
		glBindTexture(GL_TEXTURE_2D, hdrTexture);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB32F, width, height, 0, GL_RGB, GL_FLOAT, data);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
	}
	else
	{
		qWarning() << "Failed to load HDR image.";
	}
	if (env)
		delete env;
	env = new QOpenGLTexture(QOpenGLTexture::TargetCubeMap);
	env->create();
	env->setSize(size, size);
	env->setMipLevels(env->maximumMipLevels());
	env->setFormat(QOpenGLTexture::RGB32F);
	env->allocateStorage();
	env->setMinMagFilters(QOpenGLTexture::LinearMipMapLinear, QOpenGLTexture::Linear);
	env->setWrapMode(QOpenGLTexture::ClampToEdge);

	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	glm::mat4 captureViews[] =
	{
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
	   glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};

	QOpenGLShaderProgram shader;
	if (!shader.addShaderFromSourceFile(QOpenGLShader::Vertex, ":/GLViewport/HDRVS"))
		qCritical() << shader.log();
	if (!shader.addShaderFromSourceFile(QOpenGLShader::Fragment, ":/GLViewport/HDRFS"))
		qCritical() << shader.log();
	if (!shader.link())
		qCritical() << shader.log();

	QOpenGLFramebufferObject fbo(size, size);
	fbo.bind();
	shader.bind();
	shader.setUniformValue("equirectangularMap", 0);
	shader.setUniformValue("exposure", exposure);
	glUniformMatrix4fv(glGetUniformLocation(shader.programId(), "projection"), 1, GL_FALSE, glm::value_ptr(captureProjection));
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, hdrTexture);
	glViewport(0, 0, size, size);
	for (unsigned int i = 0; i < 6; ++i)
	{
		glUniformMatrix4fv(glGetUniformLocation(shader.programId(), "view"), 1, GL_FALSE, glm::value_ptr(captureViews[i]));
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, env->textureId(), 0);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		renderCube(); // renders a 1x1 cube
	}
	fbo.release();
	env->generateMipMaps();
	glViewport(0, 0, this->width(), this->height());
}

void GLViewport::initializeGL()
{
	initializeOpenGLFunctions();
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
	//glEnable(GL_TEXTURE_CUBE_MAP_EXT);

	ushader = new QOpenGLShaderProgram();
	if (!ushader->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/GLViewport/DefaultVS"))
		qCritical() << ushader->log();
	if (!ushader->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/GLViewport/DefaultFS"))
		qCritical() << ushader->log();
	if (!ushader->link())
		qCritical() << ushader->log();

	colorshader = new QOpenGLShaderProgram();
	if (!colorshader->addShaderFromSourceFile(QOpenGLShader::Vertex, ":/GLViewport/GridVS"))
		qCritical() << colorshader->log();
	if (!colorshader->addShaderFromSourceFile(QOpenGLShader::Fragment, ":/GLViewport/GridFS"))
		qCritical() << colorshader->log();
	if (!colorshader->link())
		qCritical() << colorshader->log();

	genCubemap(1024);
	convolutionDiff(32);
	convolutionSpec(256);
	/*delete env;
	env = nullptr;*/
	brdf_lut=new QOpenGLTexture(QOpenGLTexture::Target2D);
	brdf_lut->create();
	brdf_lut->setMinMagFilters(QOpenGLTexture::Linear, QOpenGLTexture::Linear);
	brdf_lut->setWrapMode(QOpenGLTexture::ClampToEdge);
	brdf_lut->setData(QImage(":/GLViewport/lut"),QOpenGLTexture::DontGenerateMipMaps);

	colormtl = MyColorMaterial::createMaterial(this, colorshader, glm::vec3(0.2));
	defaultmtl = MyDefaultMaterial::createMaterial(this, ushader, glm::vec3(0.22, 0.25, 0.28), glm::vec3(1), 0, 0.2);

	grid = new MyGrid(this, "grid");
	grid->setMaterial(colormtl);

	defaultlight = DirectLight::createLight(this);
	defaultlight->rotation = glm::vec3(30, 0, -20);

	cam = new MyCamera();
}

void GLViewport::resizeGL(int w, int h)
{
	glViewport(0, 0, w, h);
	cam->ratio = (float)w / h;
}

void GLViewport::paintGL()
{
	if (mFBO)
	{
		delete mFBO;
		mFBO = nullptr;
	}
	glClearColor(clearcolor.r, clearcolor.g, clearcolor.b, 1.0);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	for (int i = 0; i < objlist.size(); i++)
		if(objlist[i])
			objlist[i]->draw();
	glDepthMask(GL_FALSE);
	grid->draw();
	for (int i = 0; i < lightlist.size(); i++)
		if(lightlist[i])
			lightlist[i]->draw(colormtl);
	//defaultlight->draw(colormtl);
	glDepthMask(GL_TRUE);
	calcFPS();
}

void GLViewport::mousePressEvent(QMouseEvent * e)
{
	if (button == Qt::NoButton && (e->button() == Qt::LeftButton || e->button() == Qt::RightButton))
	{
		mouseX = e->x();
		mouseY = e->y();
		button = e->button();
		setAnchor(mouseX, mouseY);
	}
}

void GLViewport::mouseMoveEvent(QMouseEvent * e)
{
	int moveX = e->x() - mouseX;
	int moveY = e->y() - mouseY;
	if (button == Qt::RightButton)
	{
		rotateCam(moveX, moveY);
		//qInfo() << "left down.";
		update();
		mouseX += moveX;
		mouseY += moveY;
	}
	else if (button == Qt::LeftButton)
	{
		moveCam(e->x(), e->y());
		//qInfo() << "left down.";
		update();
	}
}

void GLViewport::mouseReleaseEvent(QMouseEvent * e)
{
	if (e->button() == button)
	{
		button = Qt::NoButton;
		mouseX = e->x();
		mouseY = e->y();
	}
}

void GLViewport::wheelEvent(QWheelEvent * e)
{
	mouseX = e->x();
	mouseY = e->y();
	setAnchor(mouseX, mouseY);
	zoomCam(mouseX, mouseY, e->angleDelta().y()>0);
	update();
}

double GLViewport::calcFPS()
{
	static int framecount=0;
	static int lasttime=QTime::currentTime().msecsSinceStartOfDay();
	double fps;
	framecount++;
	if (QTime::currentTime().msecsSinceStartOfDay() - lasttime >= 1000)
	{
		fps=framecount * 1000.0 / (QTime::currentTime().msecsSinceStartOfDay() - lasttime);
		if (fps > 1.0)
			qInfo() << "fps: " << fps;
		lasttime = QTime::currentTime().msecsSinceStartOfDay();
		framecount = 0;
	}
	return fps > 1.0 ? fps : 1.0;
}

void GLViewport::setAnchor(int x, int y)
{
	float mouseDepth;
	makeCurrent();
	if (!mFBO)
	{
		QOpenGLFramebufferObjectFormat format;
		format.setSamples(0);
		format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
		mFBO = new QOpenGLFramebufferObject(size(), format);

		GLuint id1 = defaultFramebufferObject();
		GLuint id2 = mFBO->handle();
		glBindFramebuffer(GL_READ_FRAMEBUFFER, id1);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, id2);
		context()->extraFunctions()->glBlitFramebuffer(0, 0, width(), height(), 0, 0, mFBO->width(), mFBO->height(), GL_DEPTH_BUFFER_BIT, GL_NEAREST);
	}
	mFBO->bind();
	glReadPixels(x, height() - y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &mouseDepth);
	//qInfo() << glGetError();
	mFBO->release();
	if (mouseDepth < 1.0)
		oldDepth = mouseDepth;
	glm::vec4 worldpos;
	worldpos.x = 2.0*x / width() - 1.0;
	worldpos.y = 2.0*(height() - y) / height() - 1.0;
	worldpos.z = 2.0*oldDepth - 1.0;
	worldpos.w = 1.0;
	worldpos = glm::inverse(cam->getProj()*cam->getView())* worldpos;
	worldpos /= worldpos.w;
	cam->anchor = worldpos;
	//qInfo() << "left button: (" << mouseX << ", " << mouseY << ").";
	//qInfo() << "worldpos: (" << worldpos.x << worldpos.y << worldpos.z << ").";
}

void GLViewport::rotateCam(float x, float y)
{
	//qInfo()<<"movement:" << x << y;
	glm::mat4 mat(1.0f);
	glm::vec4 c_pos = glm::vec4(cam->pos - cam->anchor, 1);
	glm::vec4 t_pos = glm::vec4(cam->target - cam->anchor, 1);
	glm::vec3 dir = glm::normalize(cam->target - cam->pos);
	glm::vec3 yaxis = glm::dot(glm::vec3(0, 1, 0), cam->up) >= 0 ? glm::vec3(0, 1, 0) : glm::vec3(0, -1, 0);
	defaultlight->rotation.x += (yaxis.y > 0 ? -x : x) / 3;
	cam->pos = glm::vec3(glm::rotate(mat, glm::radians(-x / 3), yaxis)*c_pos) + cam->anchor;
	cam->target = glm::vec3(glm::rotate(mat, glm::radians(-x / 3), yaxis)*t_pos) + cam->anchor;
	c_pos = glm::vec4(cam->pos - cam->anchor, 1);
	t_pos = glm::vec4(cam->target - cam->anchor, 1);
	dir = glm::normalize(cam->target - cam->pos);
	glm::vec3 xaxis = glm::normalize(glm::cross(dir, yaxis));
	cam->pos = glm::vec3(glm::rotate(mat, glm::radians(-y / 3), xaxis)*c_pos) + cam->anchor;
	cam->target = glm::vec3(glm::rotate(mat, glm::radians(-y / 3), xaxis)*t_pos) + cam->anchor;
	dir = glm::normalize(cam->target - cam->pos);
	cam->up = glm::normalize(glm::cross(xaxis, dir));
	defaultlight->rotation.z += -y / 3;
}

void GLViewport::moveCam(float x, float y)
{
	glm::vec4 worldpos;
	worldpos.x = 2.0*(x) / width() - 1;
	worldpos.y = 2.0*(height() - y) / height() - 1;
	worldpos.z = 2.0*oldDepth - 1.0;
	worldpos.w = 1.0;
	worldpos = glm::inverse(cam->getProj()*cam->getView()) * worldpos;
	worldpos /= worldpos.w;
	glm::vec3 dist = glm::vec3(worldpos) - cam->anchor;
	cam->pos -= dist;
	cam->target -= dist;
}

void GLViewport::zoomCam(float x, float y, bool direction)
{
	if (cam->perspective)
	{
		glm::vec3 pos = cam->pos - cam->anchor;
		if (direction)
		{
			if (oldDepth < 0.3)
				return;
			pos *= -0.2f;
		}
		else
		{
			if (oldDepth > 0.999995)
				return;
			pos *= 0.25f;
		}
		cam->pos += pos;
		cam->target += pos;
		glm::vec4 worldpos(cam->anchor, 1);
		worldpos = cam->getProj() * cam->getView() * worldpos;
		worldpos /= worldpos.w;
		oldDepth = worldpos.z / 2 + 0.5;
	}
	else
	{
		glm::vec2 move;
		move.x = x / width() - 0.5f;
		move.y = (height() - y) / height() - 0.5f;
		if (direction)
		{
			if (cam->height < 1)
				return;
			move *= 0.2f;
		}
		else
		{
			if (cam->height > 999999)
				return;
			move *= -0.25f;
		}
		move *= glm::vec2(cam->height*cam->ratio, cam->height);
		//qInfo() << move.x << move.y;
		glm::vec3 yaxis = glm::normalize(cam->up);
		glm::vec3 xaxis = glm::normalize(glm::cross(cam->target - cam->pos, yaxis));
		cam->pos += yaxis * move.y + xaxis * move.x;
		cam->target += yaxis * move.y + xaxis * move.x;
		if (direction)
			cam->height *= 0.8f;
		else
			cam->height *= 1.25f;
	}
	//qInfo() << oldDepth;
}

void GLViewport::switchCamMode(bool mode)
{
	cam->perspective = mode;
	glm::vec4 worldpos(cam->anchor, 1);
	worldpos = cam->getProj() * cam->getView() * worldpos;
	worldpos /= worldpos.w;
	oldDepth = worldpos.z / 2 + 0.5;
	//qInfo() << oldDepth;
}

MyDefaultMaterial::MyDefaultMaterial(glm::vec3 d, glm::vec3 s)
{
	diffuse = d;
	specular = s;
}

MyDefaultMaterial::~MyDefaultMaterial()
{
	for (int i = 0; i < TEXNUM; i++)
		if (tex[i])
			delete tex[i];
}

void MyDefaultMaterial::bind()
{
	shader->bind();
	shader->setUniformValue("c_diff", diffuse.r, diffuse.g, diffuse.b);
	shader->setUniformValue("c_spec", specular.r, specular.g, specular.b);
	shader->setUniformValue("c_emission", emission.r, emission.g, emission.b);
	shader->setUniformValue("c_metal", metal);
	shader->setUniformValue("c_rough", rough);
	shader->setUniformValue("c_env_intesity", parent->env_intensity);
	shader->setUniformValue("uv_offset", uvoffset.x, uvoffset.y);
	shader->setUniformValue("uv_scale", uvscale.x, uvscale.y);
	if (tex[0])
	{
		tex[0]->bind(0);
		shader->setUniformValue("t_diff", 0);
		shader->setUniformValue("use_difftex", 1);
	}
	else
	{
		shader->setUniformValue("c_diff", diffuse.r, diffuse.g, diffuse.b);
		shader->setUniformValue("use_difftex", 0);
	}
	if (tex[1])
	{
		tex[1]->bind(1);
		shader->setUniformValue("t_spec", 1);
		shader->setUniformValue("use_spectex", 1);
	}
	else
	{
		shader->setUniformValue("use_spectex", 0);
	}
	if (tex[2])
	{
		tex[2]->bind(2);
		shader->setUniformValue("t_metal", 2);
		shader->setUniformValue("use_metaltex", 1);
	}
	else
	{
		shader->setUniformValue("use_metaltex", 0);
	}
	if (tex[3])
	{
		tex[3]->bind(3);
		shader->setUniformValue("t_rough", 3);
		shader->setUniformValue("use_roughtex", 1);
	}
	else
	{
		shader->setUniformValue("use_roughtex", 0);
	}
	if (tex[4])
	{
		tex[4]->bind(4);
		shader->setUniformValue("t_normal", 4);
		shader->setUniformValue("use_normaltex", 1);
	}
	else
	{
		shader->setUniformValue("use_normaltex", 0);
	}
	if (tex[5])
	{
		tex[5]->bind(5);
		shader->setUniformValue("t_emission", 5);
		shader->setUniformValue("use_normaltex", 1);
	}
	else
	{
		shader->setUniformValue("use_emissiontex", 0);
	}
	if (tex[6])
	{
		tex[6]->bind(6);
		shader->setUniformValue("t_ao", 6);
		shader->setUniformValue("use_aotex", 1);
	}
	else
	{
		shader->setUniformValue("use_aotex", 0);
	}
	parent->env_diff->bind(7);
	shader->setUniformValue("t_env_diff", 7);
	parent->env_spec->bind(8);
	shader->setUniformValue("t_env_spec", 8);
	parent->brdf_lut->bind(9);
	shader->setUniformValue("t_lut", 9);
	parent->glUniformMatrix4fv(parent->glGetUniformLocation(shader->programId(), "m_view"), 1, GL_FALSE, glm::value_ptr(parent->cam->getView()));
	parent->glUniformMatrix4fv(parent->glGetUniformLocation(shader->programId(), "m_proj"), 1, GL_FALSE, glm::value_ptr(parent->cam->getProj()));
	parent->glUniform3fv(parent->glGetUniformLocation(shader->programId(), "v_pos"), 1, glm::value_ptr(parent->cam->getPos()));
	int lnum = 0;
	for (int i = 0; i < parent->lightlist.size(); i++)
		if (parent->lightlist[i])
		{
			parent->lightlist[i]->setLight(shader, lnum);
			lnum++;
		}
	parent->glUniform1i(parent->glGetUniformLocation(shader->programId(), "l_num"), lnum);
}

void MyDefaultMaterial::setDiffTexture(QString file)
{
	if (tex[0])
	{
		delete tex[0];
		tex[0] = nullptr;
	}
	if (file.isEmpty())
		return;
	tex[0] = new QOpenGLTexture(QOpenGLTexture::Target2D);
	tex[0]->create();
	tex[0]->setData(QImage(file));
	tex[0]->setMinMagFilters(QOpenGLTexture::LinearMipMapLinear, QOpenGLTexture::Linear);
	tex[0]->setMaximumAnisotropy(16);
	if (_repeat)
		if (_mirrow)
			tex[0]->setWrapMode(QOpenGLTexture::MirroredRepeat);
		else
			tex[0]->setWrapMode(QOpenGLTexture::Repeat);
	else
		tex[0]->setWrapMode(QOpenGLTexture::ClampToBorder);
}

void MyDefaultMaterial::setSpecTexture(QString file)
{
	if (tex[1])
	{
		delete tex[1];
		tex[1] = nullptr;
	}
	if (file.isEmpty())
		return;
	tex[1] = new QOpenGLTexture(QOpenGLTexture::Target2D);
	tex[1]->create();
	tex[1]->setData(QImage(file));
	tex[1]->setMinMagFilters(QOpenGLTexture::LinearMipMapLinear, QOpenGLTexture::Linear);
	tex[1]->setMaximumAnisotropy(16);
	if (_repeat)
		if (_mirrow)
			tex[1]->setWrapMode(QOpenGLTexture::MirroredRepeat);
		else
			tex[1]->setWrapMode(QOpenGLTexture::Repeat);
	else
		tex[1]->setWrapMode(QOpenGLTexture::ClampToBorder);
}

void MyDefaultMaterial::setNormalTexture(QString file)
{
	if (tex[4])
	{
		delete tex[4];
		tex[4] = nullptr;
	}
	if (file.isEmpty())
		return;
	tex[4] = new QOpenGLTexture(QOpenGLTexture::Target2D);
	tex[4]->create();
	tex[4]->setData(QImage(file));
	tex[4]->setMinMagFilters(QOpenGLTexture::LinearMipMapLinear, QOpenGLTexture::Linear);
	tex[4]->setMaximumAnisotropy(16);
	if (_repeat)
		if (_mirrow)
			tex[4]->setWrapMode(QOpenGLTexture::MirroredRepeat);
		else
			tex[4]->setWrapMode(QOpenGLTexture::Repeat);
	else
		tex[4]->setWrapMode(QOpenGLTexture::ClampToBorder);
}

void MyDefaultMaterial::setMetalTexture(QString file)
{
	if (tex[2])
	{
		delete tex[2];
		tex[2] = nullptr;
	}
	if (file.isEmpty())
		return;
	tex[2] = new QOpenGLTexture(QOpenGLTexture::Target2D);
	tex[2]->create();
	tex[2]->setData(QImage(file));
	tex[2]->setMinMagFilters(QOpenGLTexture::LinearMipMapLinear, QOpenGLTexture::Linear);
	tex[2]->setMaximumAnisotropy(16);
	if (_repeat)
		if (_mirrow)
			tex[2]->setWrapMode(QOpenGLTexture::MirroredRepeat);
		else
			tex[2]->setWrapMode(QOpenGLTexture::Repeat);
	else
		tex[2]->setWrapMode(QOpenGLTexture::ClampToBorder);
}

void MyDefaultMaterial::setRoughTexture(QString file)
{
	if (tex[3])
	{
		delete tex[3];
		tex[3] = nullptr;
	}
	if (file.isEmpty())
		return;
	tex[3] = new QOpenGLTexture(QOpenGLTexture::Target2D);
	tex[3]->create();
	tex[3]->setData(QImage(file));
	tex[3]->setMinMagFilters(QOpenGLTexture::LinearMipMapLinear, QOpenGLTexture::Linear);
	tex[3]->setMaximumAnisotropy(16);
	if (_repeat)
		if (_mirrow)
			tex[3]->setWrapMode(QOpenGLTexture::MirroredRepeat);
		else
			tex[3]->setWrapMode(QOpenGLTexture::Repeat);
	else
		tex[3]->setWrapMode(QOpenGLTexture::ClampToBorder);
}

void MyDefaultMaterial::setEmitTexture(QString file)
{
	if (tex[5])
	{
		delete tex[5];
		tex[5] = nullptr;
	}
	if (file.isEmpty())
		return;
	tex[5] = new QOpenGLTexture(QOpenGLTexture::Target2D);
	tex[5]->create();
	tex[5]->setData(QImage(file));
	tex[5]->setMinMagFilters(QOpenGLTexture::LinearMipMapLinear, QOpenGLTexture::Linear);
	tex[5]->setMaximumAnisotropy(16);
	if (_repeat)
		if (_mirrow)
			tex[5]->setWrapMode(QOpenGLTexture::MirroredRepeat);
		else
			tex[5]->setWrapMode(QOpenGLTexture::Repeat);
	else
		tex[5]->setWrapMode(QOpenGLTexture::ClampToBorder);
}

void MyDefaultMaterial::setAOTexture(QString file)
{
	if (tex[6])
	{
		delete tex[6];
		tex[6] = nullptr;
	}
	if (file.isEmpty())
		return;
	tex[6] = new QOpenGLTexture(QOpenGLTexture::Target2D);
	tex[6]->create();
	tex[6]->setData(QImage(file));
	tex[6]->setMinMagFilters(QOpenGLTexture::LinearMipMapLinear, QOpenGLTexture::Linear);
	tex[6]->setMaximumAnisotropy(16);
	if (_repeat)
		if (_mirrow)
			tex[6]->setWrapMode(QOpenGLTexture::MirroredRepeat);
		else
			tex[6]->setWrapMode(QOpenGLTexture::Repeat);
	else
		tex[6]->setWrapMode(QOpenGLTexture::ClampToBorder);
}

bool MyDefaultMaterial::repeat()
{
	return _repeat;
}

bool MyDefaultMaterial::mirrow()
{
	return _mirrow;
}

QOpenGLShaderProgram * MyMaterial::getShader()
{
	return shader;
}

void MyDefaultMaterial::setRepeatMirrow(bool r, bool m)
{
	for (int i = 0; i < TEXNUM; i++)
		if(tex[i])
			if (r)
			{
				if (m)
				{
					tex[i]->setWrapMode(QOpenGLTexture::MirroredRepeat);
				}
				else
				{
					tex[i]->setWrapMode(QOpenGLTexture::Repeat);
				}
			}
			else
			{
				tex[i]->setWrapMode(QOpenGLTexture::ClampToBorder);
			}
	
	_repeat = r;
	_mirrow = m;
}

const unsigned MyDefaultMaterial::TEXNUM = 7;
MyDefaultMaterial * MyDefaultMaterial::createMaterial(GLViewport * pa, QOpenGLShaderProgram * s)
{
	MyDefaultMaterial* ret = new MyDefaultMaterial(glm::vec3(0.8),glm::vec3(1));
	ret->parent = pa;
	ret->shader = s;
	ret->shine = 64;
	ret->metal = 0;
	ret->rough = 0.2;
	ret->emission = glm::vec3(0);
	for (int i = 0; i < TEXNUM; i++)
	{
		ret->tex.append(nullptr);
		ret->texpath.append("");
	}
	ret->uvoffset = glm::vec2(0);
	ret->uvscale = glm::vec2(1);
	ret->_repeat = true;
	ret->_mirrow = false;
	return ret;
}

MyDefaultMaterial * MyDefaultMaterial::createMaterial(GLViewport * pa, QOpenGLShaderProgram * s, glm::vec3 diff, glm::vec3 spec, float shine)
{
	MyDefaultMaterial* ret = createMaterial(pa, s);
	ret->diffuse = diff;
	ret->specular = spec;
	ret->shine = shine;
	return ret;
}

MyDefaultMaterial * MyDefaultMaterial::createMaterial(GLViewport * pa, QOpenGLShaderProgram * s, glm::vec3 diff, glm::vec3 spec, float metal, float rough)
{
	MyDefaultMaterial* ret = createMaterial(pa, s);
	ret->diffuse = diff;
	ret->specular = spec;
	ret->metal = metal;
	ret->rough = rough;
	return ret;
}

glm::mat4 MyObject::getMatrix()
{
	glm::mat4 mat(1.0f);
	mat = glm::translate(mat, position);
	mat *= getHPBMatrix();
	mat = glm::scale(mat, scale);
	return mat;
}

glm::mat4 MyObject::getHPBMatrix()
{
	glm::mat4 mat;
	glm::mat4 ret(1.0);
	glm::vec3 H = glm::vec3(0, 1, 0);
	glm::vec3 P = glm::vec3(0, 0, 1);
	glm::vec3 B = glm::vec3(1, 0, 0);
	mat = glm::rotate(glm::mat4(1.0), glm::radians(rotation.x), H);
	P = mat * glm::vec4(P, 0);
	B = mat * glm::vec4(B, 0);
	ret = mat * ret;
	mat = glm::rotate(glm::mat4(1.0), glm::radians(rotation.y), P);
	B = mat * glm::vec4(B, 0);
	ret = mat * ret;
	mat = glm::rotate(glm::mat4(1.0), glm::radians(rotation.z), B);
	ret = mat * ret;
	return ret;
}

glm::vec3 MyObject::calcTangent(Vertex & p1, Vertex & p2, Vertex & p3)
{
	return calcTangent(p1.Position, p2.Position, p3.Position, p1.TexCoords, p2.TexCoords, p3.TexCoords);
}

glm::vec3 MyObject::calcBitangent(Vertex & p1, Vertex & p2, Vertex & p3)
{
	return calcBitangent(p1.Position, p2.Position, p3.Position, p1.TexCoords, p2.TexCoords, p3.TexCoords);
}

glm::vec3 MyObject::calcTangent(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec2 uv1, glm::vec2 uv2, glm::vec2 uv3)
{
	glm::vec3 tangent;
	glm::vec3 edge1 = p2 - p1;
	glm::vec3 edge2 = p3 - p1;
	glm::vec2 deltaUV1 = uv2 - uv1;
	glm::vec2 deltaUV2 = uv3 - uv1;

	float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
	tangent.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
	tangent.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
	tangent.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
	tangent = glm::normalize(tangent);
	return tangent;
}

glm::vec3 MyObject::calcBitangent(glm::vec3 p1, glm::vec3 p2, glm::vec3 p3, glm::vec2 uv1, glm::vec2 uv2, glm::vec2 uv3)
{
	glm::vec3 bitangent;
	glm::vec3 edge1 = p2 - p1;
	glm::vec3 edge2 = p3 - p1;
	glm::vec2 deltaUV1 = uv2 - uv1;
	glm::vec2 deltaUV2 = uv3 - uv1;

	float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
	bitangent.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
	bitangent.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
	bitangent.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
	bitangent = glm::normalize(bitangent);
	return bitangent;
}

unsigned MyObject::idcount = 0;
MyObject::MyObject(GLViewport * pa):
	VBO(QOpenGLBuffer::VertexBuffer),EBO(QOpenGLBuffer::IndexBuffer)
{
	id = idcount++;
	parent = pa;
	mat = nullptr;
	position = glm::vec3(0);
	rotation = glm::vec3(0, 0, 0);
	scale = glm::vec3(1);
	parent->makeCurrent();
	VAO.create();
	VBO.create();
	EBO.create();
	qInfo() << VAO.objectId() << VBO.bufferId() << EBO.bufferId();
}

MyObject::MyObject(GLViewport * pa, QString n) :
	MyObject(pa)
{
	name = n;
}

unsigned MyObject::getID()
{
	return id;
}

MyObject::Type MyObject::type()
{
	return T;
}

void MyObject::setMaterial(MyMaterial * m)
{
	mat = m;
}

MyMaterial * MyObject::getMaterial()
{
	return mat;
}

void MyObject::allocateVertices(const void * data, int count)
{
	parent->makeCurrent();
	VAO.bind();
	VBO.bind();
	VBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
	VBO.allocate(data, count);
}

void MyObject::allocateIndices(const void * data, int count)
{
	parent->makeCurrent();
	VAO.bind();
	EBO.bind();
	EBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
	EBO.allocate(data, count);
}

void MyObject::setVertexAttrib()
{
	parent->makeCurrent();
	VAO.bind();
	parent->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
	parent->glEnableVertexAttribArray(0);
	parent->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
	parent->glEnableVertexAttribArray(1);
	parent->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
	parent->glEnableVertexAttribArray(2);
	parent->glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Tangent));
	parent->glEnableVertexAttribArray(3);
	parent->glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, BiTangent));
	parent->glEnableVertexAttribArray(4);
}

void MyObject::draw()
{
	if (mat)
	{
		mat->bind();
		QOpenGLShaderProgram *shader = mat->getShader();
		glm::mat4 trans = getMatrix();
		glm::mat4 inv = glm::transpose(glm::inverse(trans));
		parent->glUniformMatrix4fv(parent->glGetUniformLocation(shader->programId(), "m_trans"), 1, GL_FALSE, glm::value_ptr(trans));
		parent->glUniformMatrix4fv(parent->glGetUniformLocation(shader->programId(), "m_invtrans"), 1, GL_FALSE, glm::value_ptr(inv));
	}
	VAO.bind();
	VBO.bind();
	EBO.bind();
	parent->glDrawElements(GL_TRIANGLES, EBO.size() / sizeof(unsigned), GL_UNSIGNED_INT, 0);
}

DirectLight::DirectLight(GLViewport * pa) :MyLight(pa), l_color(1), l_intensity(1)
{
	T = Direction;
	std::vector<Vertex> ver;
	genLines(ver);
	allocateVertices(&ver[0], ver.size() * sizeof(Vertex));
	setVertexAttrib();
}

void DirectLight::genLines(std::vector<Vertex>& ver)
{
	Vertex v;
	v.Normal = glm::vec3(1, 0, 0);
	v.TexCoords = glm::vec2(0);
	v.Position = glm::vec3(20, 0, 0);
	ver.push_back(v);
	v.Position = glm::vec3(-20, 0, 0);
	ver.push_back(v);
	v.Position = glm::vec3(0, 20, 0);
	ver.push_back(v);
	v.Position = glm::vec3(0, -20, 0);
	ver.push_back(v);
	v.Position = glm::vec3(0, 0, 20);
	ver.push_back(v);
	v.Position = glm::vec3(0, 0, -200);
	ver.push_back(v);
	float x = 10 * glm::sqrt(2);
	v.Position = glm::vec3(x, x, 0);
	ver.push_back(v);
	v.Position = glm::vec3(-x, -x, 0);
	ver.push_back(v);
	v.Position = glm::vec3(-x, x, 0);
	ver.push_back(v);
	v.Position = glm::vec3(x, -x, 0);
	ver.push_back(v);
	v.Position = glm::vec3(x, 0, x);
	ver.push_back(v);
	v.Position = glm::vec3(-x, 0, -x);
	ver.push_back(v);
	v.Position = glm::vec3(-x, 0, x);
	ver.push_back(v);
	v.Position = glm::vec3(x, 0, -x);
	ver.push_back(v);
	v.Position = glm::vec3(0, x, x);
	ver.push_back(v);
	v.Position = glm::vec3(0, -x, -x);
	ver.push_back(v);
	v.Position = glm::vec3(0, -x, x);
	ver.push_back(v);
	v.Position = glm::vec3(0, x, -x);
	ver.push_back(v);
}

void DirectLight::setLight(QOpenGLShaderProgram * shader, unsigned index)
{
	shader->setUniformValue((std::string("l[") + std::to_string(index) + "].color").c_str(), l_color.r, l_color.g, l_color.b);
	glm::vec4 dir = getHPBMatrix() * glm::vec4(0, 0, 1, 0);
	//qInfo() << dir.x << dir.y << dir.z;
	shader->setUniformValue((std::string("l[") + std::to_string(index) + "].pos").c_str(), dir.x, dir.y, dir.z, 0.0f);
	shader->setUniformValue((std::string("l[") + std::to_string(index) + "].intensity").c_str(), l_intensity);
}

void DirectLight::draw(MyColorMaterial* mtl)
{
	glm::vec3 c = l_color * l_intensity;
	mtl->bind(c);
	QOpenGLShaderProgram *shader = mtl->getShader();
	parent->glUniformMatrix4fv(parent->glGetUniformLocation(shader->programId(), "m_trans"), 1, GL_FALSE, glm::value_ptr(getMatrix()));
	VAO.bind();
	VBO.bind();
	parent->glDrawArrays(GL_LINES, 0, VBO.size() / sizeof(Vertex));
}

DirectLight * DirectLight::createLight(GLViewport * pa)
{
	return new DirectLight(pa);
}

glm::mat4 MyLight::getHPBMatrix()
{
	glm::mat4 mat;
	glm::mat4 ret(1.0f);
	glm::vec3 H = glm::vec3(0, 1, 0);
	glm::vec3 P = glm::vec3(0, 0, 1);
	glm::vec3 B = glm::vec3(1, 0, 0);
	mat = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.x), H);
	P = mat * glm::vec4(P, 0);
	B = mat * glm::vec4(B, 0);
	ret = mat * ret;
	mat = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.y), P);
	B = mat * glm::vec4(B, 0);
	ret = mat * ret;
	mat = glm::rotate(glm::mat4(1.0f), glm::radians(rotation.z), B);
	ret = mat * ret;
	return ret;
}

glm::mat4 MyLight::getMatrix()
{
	glm::mat4 mat(1.0f);
	mat = glm::translate(mat, position);
	mat *= getHPBMatrix();
	return mat;
}

void MyLight::allocateVertices(const void * data, int count)
{
	parent->makeCurrent();
	VAO.bind();
	VBO.bind();
	VBO.setUsagePattern(QOpenGLBuffer::StaticDraw);
	VBO.allocate(data, count);
}

void MyLight::setVertexAttrib()
{
	parent->makeCurrent();
	VAO.bind();
	parent->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
	parent->glEnableVertexAttribArray(0);
	parent->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));
	parent->glEnableVertexAttribArray(1);
	parent->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));
	parent->glEnableVertexAttribArray(2);
}

MyLight::MyLight(GLViewport * pa) :
	position(0), rotation(0), VBO(QOpenGLBuffer::VertexBuffer)
{
	parent = pa;
	parent->makeCurrent();
	VAO.create();
	VBO.create();
	qInfo() << VAO.objectId() << VBO.bufferId();
}

MyLight::~MyLight() {}

MyLight::Type MyLight::type()
{
	return T;
}

AmbientLight::AmbientLight(GLViewport * pa) :MyLight(pa), l_color(0)
{
	T = Ambient;
}

void AmbientLight::setLight(QOpenGLShaderProgram * shader, unsigned index)
{
	shader->setUniformValue("l_ambient", l_color.r, l_color.g, l_color.b);
	shader->setUniformValue((std::string("l[") + std::to_string(index) + "].color").c_str(), 0.0f, 0.0f, 0.0f);
	shader->setUniformValue((std::string("l[") + std::to_string(index) + "].pos").c_str(), 1.0f, 0.0f, 0.0f, 0.0f);
}

AmbientLight * AmbientLight::createLight(GLViewport * pa)
{
	return new AmbientLight(pa);
}

MyPlane::MyPlane(GLViewport * pa) :MyObject(pa)
{
	setSize(400, 400);
	T = Plane;
}

MyPlane::MyPlane(GLViewport * pa, QString name) : MyObject(pa, name)
{
	setSize(400, 400);
	T = Plane;
}

void MyPlane::setSize(float w, float h)
{
	width = w;
	height = h;
	std::vector<Vertex> vertices;
	std::vector<unsigned> indices;
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
	indices.push_back(0);
	indices.push_back(1);
	indices.push_back(2);
	indices.push_back(2);
	indices.push_back(3);
	indices.push_back(0);
	vertices[0].Tangent = calcTangent(vertices[0], vertices[1], vertices[2]);
	vertices[0].BiTangent = calcBitangent(vertices[0], vertices[1], vertices[2]);
	vertices[1].Tangent = calcTangent(vertices[1], vertices[0], vertices[2]);
	vertices[1].BiTangent = calcBitangent(vertices[1], vertices[0], vertices[2]);
	vertices[2].Tangent = calcTangent(vertices[2], vertices[1], vertices[0]);
	vertices[2].BiTangent = calcBitangent(vertices[2], vertices[1], vertices[0]);
	vertices[3].Tangent = calcTangent(vertices[3], vertices[2], vertices[0]);
	vertices[3].BiTangent = calcBitangent(vertices[3], vertices[2], vertices[0]);
	allocateVertices(&vertices[0], vertices.size() * sizeof(Vertex));
	allocateIndices(&indices[0], indices.size() * sizeof(unsigned));
	setVertexAttrib();
}

glm::vec2 MyPlane::getSize()
{
	return glm::vec2(width, height);
}

glm::mat4 MyCamera::getProj()
{
	if (perspective)
	{
		return glm::perspective(glm::radians(fov), ratio, 1.0f, 100000.0f);
	}
	else
	{
		return glm::ortho(-height * ratio / 2, height *ratio / 2, -height / 2, height / 2, 1.0f, 100000.0f);
	}
}

glm::mat4 MyCamera::getView()
{
	glm::vec3 yaxis = glm::dot(glm::vec3(0, 1, 0), up) >= 0 ? glm::vec3(0, 1, 0) : glm::vec3(0, -1, 0);
	glm::vec3 dir = glm::normalize(pos - target);
	return perspective ? glm::lookAt(pos, target, yaxis) : glm::lookAt(target + 50000.0f * dir, target, yaxis);
}

glm::vec3 MyCamera::getPos()
{
	return pos;
}

MyCube::MyCube(GLViewport * pa) :MyObject(pa)
{ 
	setSize(200, 200, 200);
	T = Cube;
}

MyCube::MyCube(GLViewport * pa, QString name) :MyObject(pa, name)
{
	setSize(200, 200, 200);
	T = Cube;
}

void MyCube::setSize(float x, float y, float z)
{
	size = glm::vec3(x, y, z);
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
	std::vector<Vertex> ver;
	std::vector<unsigned> ind;
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
		ind.push_back(i * 4 + 0);
		ind.push_back(i * 4 + 1);
		ind.push_back(i * 4 + 2);
		ind.push_back(i * 4 + 2);
		ind.push_back(i * 4 + 3);
		ind.push_back(i * 4 + 0);
		ver[i * 4 + 0].Tangent = calcTangent(ver[i * 4 + 0], ver[i * 4 + 1], ver[i * 4 + 2]);
		ver[i * 4 + 0].BiTangent = calcBitangent(ver[i * 4 + 0], ver[i * 4 + 1], ver[i * 4 + 2]);
		ver[i * 4 + 1].Tangent = calcTangent(ver[i * 4 + 1], ver[i * 4 + 0], ver[i * 4 + 2]);
		ver[i * 4 + 1].BiTangent = calcBitangent(ver[i * 4 + 1], ver[i * 4 + 0], ver[i * 4 + 2]);
		ver[i * 4 + 2].Tangent = calcTangent(ver[i * 4 + 2], ver[i * 4 + 1], ver[i * 4 + 0]);
		ver[i * 4 + 2].BiTangent = calcBitangent(ver[i * 4 + 2], ver[i * 4 + 1], ver[i * 4 + 0]);
		ver[i * 4 + 3].Tangent = calcTangent(ver[i * 4 + 3], ver[i * 4 + 0], ver[i * 4 + 2]);
		ver[i * 4 + 3].BiTangent = calcBitangent(ver[i * 4 + 3], ver[i * 4 + 0], ver[i * 4 + 2]);
	}
	allocateVertices(&ver[0], ver.size() * sizeof(Vertex));
	allocateIndices(&ind[0], ind.size() * sizeof(unsigned));
	setVertexAttrib();
}

glm::vec3 MyCube::getSize()
{
	return size;
}

void MyCylinder::genDisk(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float r, float h, unsigned f)
{
	Vertex v;
	unsigned start = ver.size();
	v.Normal = glm::vec3(0, h > 0 ? 1 : -1, 0);
	v.Position = glm::vec3(0, h, 0);
	v.TexCoords = glm::vec2(0.5, 0.5);
	ver.push_back(v);
	for (int i = 0; i < f; i++)
	{
		v.Position = glm::vec3(r*glm::sin(glm::radians(360.0 / f * i)), h, r*glm::cos(glm::radians(360.0 / f * i)));
		v.TexCoords = glm::vec2((glm::sin(glm::radians(360.0 / f * i)) + 1) / 2, (glm::cos(glm::radians(360.0 / f * i)) + 1) / 2);
		ver.push_back(v);
	}
	for (int i = 1; i < f; i++)
	{
		ind.push_back(start + i);
		ind.push_back(start + i + 1);
		ind.push_back(start);
		ver[start].Tangent = calcTangent(ver[start], ver[start + i], ver[start + i + 1]);
		ver[start].BiTangent = calcBitangent(ver[start], ver[start + i], ver[start + i + 1]);
		ver[start + i].Tangent = calcTangent(ver[start + i], ver[start], ver[start + i + 1]);
		ver[start + i].BiTangent = calcBitangent(ver[start + i], ver[start], ver[start + i + 1]);
		ver[start + i + 1].Tangent = calcTangent(ver[start + i + 1], ver[start + i], ver[start]);
		ver[start + i + 1].BiTangent = calcBitangent(ver[start + i + 1], ver[start + i], ver[start]);
	}
	ind.push_back(start + f);
	ind.push_back(start + 1);
	ind.push_back(start);
}

void MyCylinder::genSide(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float r, float h, unsigned f)
{
	Vertex v;
	unsigned start = ver.size();
	for (int i = 0; i <= f; i++)
	{
		v.Normal = glm::vec3(glm::sin(glm::radians(360.0 / f * i)), 0, glm::cos(glm::radians(360.0 / f * i)));
		v.Position = glm::vec3(r*glm::sin(glm::radians(360.0 / f * i)), h, r*glm::cos(glm::radians(360.0 / f * i)));
		v.TexCoords = glm::vec2(1.0 * i / f, 0);
		ver.push_back(v);
		v.Position = glm::vec3(r*glm::sin(glm::radians(360.0 / f * i)), -h, r*glm::cos(glm::radians(360.0 / f * i)));
		v.TexCoords = glm::vec2(1.0 * i / f, 1);
		ver.push_back(v);
	}
	for (int i = 0; i < f; i++)
	{
		ind.push_back(start + i * 2);
		ind.push_back(start + i * 2 + 1);
		ind.push_back(start + i * 2 + 3);
		ind.push_back(start + i * 2 + 3);
		ind.push_back(start + i * 2 + 2);
		ind.push_back(start + i * 2);
		ver[start + i * 2].Tangent = calcTangent(ver[start + i * 2], ver[start + i * 2 + 1], ver[start + i * 2 + 3]);
		ver[start + i * 2].BiTangent = calcBitangent(ver[start + i * 2], ver[start + i * 2 + 1], ver[start + i * 2 + 3]);
		ver[start + i * 2 + 1].Tangent = calcTangent(ver[start + i * 2 + 1], ver[start + i * 2], ver[start + i * 2 + 3]);
		ver[start + i * 2 + 1].BiTangent = calcBitangent(ver[start + i * 2 + 1], ver[start + i * 2], ver[start + i * 2 + 3]);
		ver[start + i * 2 + 2].Tangent = calcTangent(ver[start + i * 2 + 2], ver[start + i * 2 + 3], ver[start + i * 2]);
		ver[start + i * 2 + 2].BiTangent = calcBitangent(ver[start + i * 2 + 2], ver[start + i * 2 + 3], ver[start + i * 2]);
		ver[start + i * 2 + 3].Tangent = calcTangent(ver[start + i * 2 + 3], ver[start + i * 2 + 1], ver[start + i * 2]);
		ver[start + i * 2 + 3].BiTangent = calcBitangent(ver[start + i * 2 + 3], ver[start + i * 2 + 1], ver[start + i * 2]);
	}
}

MyCylinder::MyCylinder(GLViewport * pa) :MyObject(pa)
{
	setSize(50, 200, 36);
	T = Cylinder;
}

MyCylinder::MyCylinder(GLViewport * pa, QString name) :MyObject(pa, name)
{
	setSize(50, 200, 36);
	T = Cylinder;
}

void MyCylinder::setSize(float r, float h, unsigned f)
{
	radius = r >= 0 ? r : 0;
	height = h >= 0 ? h : 0;
	fragment = f >= 3 ? f : 3;
	std::vector<Vertex> ver;
	std::vector<unsigned> ind;
	genDisk(ver, ind, radius, height / 2, fragment);
	genDisk(ver, ind, radius, -height / 2, fragment);
	genSide(ver, ind, radius, height / 2, fragment);
	allocateVertices(&ver[0], ver.size() * sizeof(Vertex));
	allocateIndices(&ind[0], ind.size() * sizeof(unsigned));
	setVertexAttrib();
}

glm::vec2 MyCylinder::getSize()
{
	return glm::vec2(radius, height);
}

unsigned MyCylinder::getFragment()
{
	return fragment;
}

void MyGrid::genGrid(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float w, float h, unsigned c, unsigned r)
{
	Vertex v;
	unsigned start = ver.size();
	for (int i = 0; i < c; i++)
	{
		v.Position = glm::vec3(-w / 2 + w * i / (c - 1), 0, -h / 2);
		v.Normal = glm::vec3(0, 1, 0);
		v.TexCoords = glm::vec2(i / (c - 1), 0);
		ver.push_back(v);
		v.Position = glm::vec3(-w / 2 + w * i / (c - 1), 0, h / 2);
		v.Normal = glm::vec3(0, 1, 0);
		v.TexCoords = glm::vec2(i / (c - 1), 1);
		ver.push_back(v);
	}
	for (int i = 0; i < c; i++)
	{
		ind.push_back(start + i * 2);
		ind.push_back(start + i * 2 + 1);
	}
	start = ver.size();
	for (int i = 0; i < r; i++)
	{
		v.Position = glm::vec3(-w / 2, 0, -h / 2 + h * i / (r - 1));
		v.Normal = glm::vec3(0, 1, 0);
		v.TexCoords = glm::vec2(0, i / (r - 1));
		ver.push_back(v);
		v.Position = glm::vec3(w / 2, 0, -h / 2 + h * i / (r - 1));
		v.Normal = glm::vec3(0, 1, 0);
		v.TexCoords = glm::vec2(1, i / (r - 1));
		ver.push_back(v);
	}
	for (int i = 0; i < r; i++)
	{
		ind.push_back(start + i * 2);
		ind.push_back(start + i * 2 + 1);
	}
}

MyGrid::MyGrid(GLViewport * pa) :MyObject(pa) { setSize(10000, 10000, 100, 100); }

MyGrid::MyGrid(GLViewport * pa, QString name) : MyObject(pa, name) { setSize(10000, 10000, 100, 100); }

void MyGrid::draw()
{
	if (mat)
	{
		mat->bind();
		QOpenGLShaderProgram *shader = mat->getShader();
		parent->glUniformMatrix4fv(parent->glGetUniformLocation(shader->programId(), "m_trans"), 1, GL_FALSE, glm::value_ptr(getMatrix()));
	}
	VAO.bind();
	//qInfo() << objname;
	parent->glDrawElements(GL_LINES, EBO.size() / sizeof(unsigned), GL_UNSIGNED_INT, 0);
}

void MyGrid::setSize(float w, float h, unsigned c, unsigned r)
{
	width = w;
	height = h;
	col = c >= 2 ? c : 2;
	row = r >= 2 ? r : 2;
	std::vector<Vertex> ver;
	std::vector<unsigned> ind;
	genGrid(ver, ind, width, height, col, row);
	allocateVertices(&ver[0], ver.size() * sizeof(Vertex));
	allocateIndices(&ind[0], ind.size() * sizeof(unsigned));
	setVertexAttrib();
}

glm::vec2 MyGrid::getSize()
{
	return glm::vec2(width, height);
}

void MyColorMaterial::bind()
{
	shader->bind();
	shader->setUniformValue("color", color.r, color.g, color.b);
	parent->glUniformMatrix4fv(parent->glGetUniformLocation(shader->programId(), "m_view"), 1, GL_FALSE, glm::value_ptr(parent->cam->getView()));
	parent->glUniformMatrix4fv(parent->glGetUniformLocation(shader->programId(), "m_proj"), 1, GL_FALSE, glm::value_ptr(parent->cam->getProj()));
}

void MyColorMaterial::bind(glm::vec3 c)
{
	shader->bind();
	shader->setUniformValue("color", c.r, c.g, c.b);
	parent->glUniformMatrix4fv(parent->glGetUniformLocation(shader->programId(), "m_view"), 1, GL_FALSE, glm::value_ptr(parent->cam->getView()));
	parent->glUniformMatrix4fv(parent->glGetUniformLocation(shader->programId(), "m_proj"), 1, GL_FALSE, glm::value_ptr(parent->cam->getProj()));
}

MyColorMaterial * MyColorMaterial::createMaterial(GLViewport * pa, QOpenGLShaderProgram * s, glm::vec3 c)
{
	MyColorMaterial *ret = new MyColorMaterial(c);
	ret->parent = pa;
	ret->shader = s;
	return ret;
}

void MyPrism::genSide(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float r, float h, unsigned f)
{
	Vertex v;
	unsigned start = ver.size();
	for (int i = 0; i < f; i++)
	{
		v.Normal = glm::vec3(glm::sin(glm::radians(360.0 * ((2.0*i + 1) / (2.0*f)))), 0, glm::cos(glm::radians(360.0 * ((2.0*i + 1) / (2.0*f)))));
		v.Position = glm::vec3(r*glm::sin(glm::radians(360.0 / f * i)), h, r*glm::cos(glm::radians(360.0 / f * i)));
		v.TexCoords = glm::vec2(1.0 * i / f, 0);
		ver.push_back(v);
		v.Position = glm::vec3(r*glm::sin(glm::radians(360.0 / f * i)), -h, r*glm::cos(glm::radians(360.0 / f * i)));
		v.TexCoords = glm::vec2(1.0 * i / f, 1);
		ver.push_back(v);
		v.Position = glm::vec3(r*glm::sin(glm::radians(360.0 / f * (i + 1))), -h, r*glm::cos(glm::radians(360.0 / f * (i + 1))));
		v.TexCoords = glm::vec2(1.0 * (i + 1) / f, 1);
		ver.push_back(v);
		v.Position = glm::vec3(r*glm::sin(glm::radians(360.0 / f * (i + 1))), h, r*glm::cos(glm::radians(360.0 / f * (i + 1))));
		v.TexCoords = glm::vec2(1.0 * (i + 1) / f, 0);
		ver.push_back(v);
		ind.push_back(start + 4 * i + 0);
		ind.push_back(start + 4 * i + 1);
		ind.push_back(start + 4 * i + 2);
		ind.push_back(start + 4 * i + 2);
		ind.push_back(start + 4 * i + 3);
		ind.push_back(start + 4 * i + 0);
		ver[start + i * 4 + 0].Tangent = calcTangent(ver[start + i * 4 + 0], ver[start + i * 4 + 1], ver[start + i * 4 + 2]);
		ver[start + i * 4 + 0].BiTangent = calcBitangent(ver[start + i * 4 + 0], ver[start + i * 4 + 1], ver[start + i * 4 + 2]);
		ver[start + i * 4 + 1].Tangent = calcTangent(ver[start + i * 4 + 1], ver[start + i * 4 + 0], ver[start + i * 4 + 2]);
		ver[start + i * 4 + 1].BiTangent = calcBitangent(ver[start + i * 4 + 1], ver[start + i * 4 + 0], ver[start + i * 4 + 2]);
		ver[start + i * 4 + 2].Tangent = calcTangent(ver[start + i * 4 + 2], ver[start + i * 4 + 1], ver[start + i * 4 + 0]);
		ver[start + i * 4 + 2].BiTangent = calcBitangent(ver[start + i * 4 + 2], ver[start + i * 4 + 1], ver[start + i * 4 + 0]);
		ver[start + i * 4 + 3].Tangent = calcTangent(ver[start + i * 4 + 3], ver[start + i * 4 + 0], ver[start + i * 4 + 2]);
		ver[start + i * 4 + 3].BiTangent = calcBitangent(ver[start + i * 4 + 3], ver[start + i * 4 + 0], ver[start + i * 4 + 2]);
	}
}

MyPrism::MyPrism(GLViewport * pa) :MyCylinder(pa, 0)
{
	setSize(50, 200, 6);
	T = Prism;
}

MyPrism::MyPrism(GLViewport * pa, QString name) :MyCylinder(pa, name, 0)
{
	setSize(50, 200, 6);
	T = Prism;
}

void MySphere::genSphere(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float r, unsigned f)
{
	if (f % 2)
		f++;
	Vertex v;
	unsigned start = ver.size();
	int col = f;
	int row = f / 2;
	for (int i = 0; i <= row; i++)
	{
		float h = glm::cos(glm::radians(180.0 * i / row));
		float r2 = glm::sin(glm::radians(180.0 * i / row));
		for (int j = 0; j <= col; j++)
		{
			v.Position = glm::vec3(r * r2 * glm::sin(glm::radians(360.0 * j / col)), r * h, r * r2 * glm::cos(glm::radians(360.0 * j / col)));
			v.Normal = glm::vec3(r2 * glm::sin(glm::radians(360.0 * j / col)), h, r2 * glm::cos(glm::radians(360.0 * j / col)));
			v.TexCoords = glm::vec2(1.0 * j / col, 1.0 * i / row);
			ver.push_back(v);
		}
	}
	for (int i = 0; i < row; i++)
	{
		for (int j = 0; j < col; j++)
		{
			if (i == 0)
			{
				ind.push_back(start + i * (col + 1) + j);
				ind.push_back(start + (i + 1) * (col + 1) + j);
				ind.push_back(start + (i + 1) * (col + 1) + j + 1);
				ver[start + i * (col + 1) + j].Tangent = calcTangent(ver[start + i * (col + 1) + j], ver[start + (i + 1) * (col + 1) + j], ver[start + (i + 1) * (col + 1) + j + 1]);
				ver[start + i * (col + 1) + j].BiTangent = calcBitangent(ver[start + i * (col + 1) + j], ver[start + (i + 1) * (col + 1) + j], ver[start + (i + 1) * (col + 1) + j + 1]);
				ver[start + (i + 1) * (col + 1) + j].Tangent = calcTangent(ver[(i + 1) * (col + 1) + j], ver[start + i * (col + 1) + j], ver[start + (i + 1) * (col + 1) + j + 1]);
				ver[start + (i + 1) * (col + 1) + j].BiTangent = calcBitangent(ver[(i + 1) * (col + 1) + j], ver[start + i * (col + 1) + j], ver[start + (i + 1) * (col + 1) + j + 1]);
				ver[start + (i + 1) * (col + 1) + j + 1].Tangent = calcTangent(ver[start + (i + 1) * (col + 1) + j + 1], ver[start + (i + 1) * (col + 1) + j], ver[start + i * (col + 1) + j]);
				ver[start + (i + 1) * (col + 1) + j + 1].BiTangent = calcBitangent(ver[start + (i + 1) * (col + 1) + j + 1], ver[start + (i + 1) * (col + 1) + j], ver[start + i * (col + 1) + j]);
			}
			else if (i == row - 1)
			{
				ind.push_back(start + i * (col + 1) + j);
				ind.push_back(start + (i + 1) * (col + 1) + j);
				ind.push_back(start + i * (col + 1) + j + 1);
				ver[start + i * (col + 1) + j].Tangent = calcTangent(ver[start + i * (col + 1) + j], ver[start + (i + 1) * (col + 1) + j], ver[start + i * (col + 1) + j + 1]);
				ver[start + i * (col + 1) + j].BiTangent = calcBitangent(ver[start + i * (col + 1) + j], ver[start + (i + 1) * (col + 1) + j], ver[start + i * (col + 1) + j + 1]);
				ver[start + (i + 1) * (col + 1) + j].Tangent = calcTangent(ver[start + (i + 1) * (col + 1) + j], ver[start + i * (col + 1) + j], ver[start + i * (col + 1) + j + 1]);
				ver[start + (i + 1) * (col + 1) + j].BiTangent = calcBitangent(ver[start + (i + 1) * (col + 1) + j], ver[start + i * (col + 1) + j], ver[start + i * (col + 1) + j + 1]);
				ver[start + i * (col + 1) + j + 1].Tangent = calcTangent(ver[start + i * (col + 1) + j + 1], ver[start + (i + 1) * (col + 1) + j], ver[start + i * (col + 1) + j]);
				ver[start + i * (col + 1) + j + 1].BiTangent = calcBitangent(ver[start + i * (col + 1) + j + 1], ver[start + (i + 1) * (col + 1) + j], ver[start + i * (col + 1) + j]);
			}
			else
			{
				ind.push_back(start + i * (col + 1) + j);
				ind.push_back(start + (i + 1) * (col + 1) + j);
				ind.push_back(start + (i + 1) * (col + 1) + j + 1);
				ind.push_back(start + (i + 1) * (col + 1) + j + 1);
				ind.push_back(start + i * (col + 1) + j + 1);
				ind.push_back(start + i * (col + 1) + j);
				ver[start + i * (col + 1) + j].Tangent = calcTangent(ver[start + i * (col + 1) + j], ver[start + (i + 1) * (col + 1) + j], ver[start + (i + 1) * (col + 1) + j + 1]);
				ver[start + i * (col + 1) + j].BiTangent = calcBitangent(ver[start + i * (col + 1) + j], ver[start + (i + 1) * (col + 1) + j], ver[start + (i + 1) * (col + 1) + j + 1]);
				ver[start + (i + 1) * (col + 1) + j].Tangent = calcTangent(ver[(i + 1) * (col + 1) + j], ver[start + i * (col + 1) + j], ver[start + (i + 1) * (col + 1) + j + 1]);
				ver[start + (i + 1) * (col + 1) + j].BiTangent = calcBitangent(ver[(i + 1) * (col + 1) + j], ver[start + i * (col + 1) + j], ver[start + (i + 1) * (col + 1) + j + 1]);
				ver[start + (i + 1) * (col + 1) + j + 1].Tangent = calcTangent(ver[start + (i + 1) * (col + 1) + j + 1], ver[start + (i + 1) * (col + 1) + j], ver[start + i * (col + 1) + j]);
				ver[start + (i + 1) * (col + 1) + j + 1].BiTangent = calcBitangent(ver[start + (i + 1) * (col + 1) + j + 1], ver[start + (i + 1) * (col + 1) + j], ver[start + i * (col + 1) + j]);
				ver[start + i * (col + 1) + j + 1].Tangent = calcTangent(ver[start + i * (col + 1) + j + 1], ver[start + (i + 1) * (col + 1) + j], ver[start + i * (col + 1) + j]);
				ver[start + i * (col + 1) + j + 1].BiTangent = calcBitangent(ver[start + i * (col + 1) + j + 1], ver[start + (i + 1) * (col + 1) + j], ver[start + i * (col + 1) + j]);
			}
		}
	}
}

MySphere::MySphere(GLViewport * pa) :MyObject(pa)
{
	T = Sphere;
	setSize(100, 64);
}

MySphere::MySphere(GLViewport * pa, QString name) :MyObject(pa, name)
{
	T = Sphere;
	setSize(100, 64);
}

void MySphere::setSize(float r, unsigned f)
{
	radius = r >= 0 ? r : 0;
	fragment = f >= 3 ? f : 3;
	std::vector<Vertex> ver;
	std::vector<unsigned> ind;
	genSphere(ver, ind, radius, fragment);
	allocateVertices(&ver[0], ver.size() * sizeof(Vertex));
	allocateIndices(&ind[0], ind.size() * sizeof(unsigned));
	setVertexAttrib();
}

 float MySphere::getSize()
{
	return radius;
}

 unsigned MySphere::getFragment()
{
	return fragment;
}

void MyCone::genDisk(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float r, float h, unsigned f)
{
	Vertex v;
	unsigned start = ver.size();
	v.Normal = glm::vec3(0, h > 0 ? 1 : -1, 0);
	v.Position = glm::vec3(0, h, 0);
	v.TexCoords = glm::vec2(0.5, 0.5);
	ver.push_back(v);
	for (int i = 0; i < f; i++)
	{
		v.Position = glm::vec3(r*glm::sin(glm::radians(360.0 / f * i)), h, r*glm::cos(glm::radians(360.0 / f * i)));
		v.TexCoords = glm::vec2((glm::sin(glm::radians(360.0 / f * i)) + 1) / 2, (glm::cos(glm::radians(360.0 / f * i)) + 1) / 2);
		ver.push_back(v);
	}
	for (int i = 1; i < f; i++)
	{
		ind.push_back(start + i);
		ind.push_back(start + i + 1);
		ind.push_back(start);
		ver[start].Tangent = calcTangent(ver[start], ver[start + i], ver[start + i + 1]);
		ver[start].BiTangent = calcBitangent(ver[start], ver[start + i], ver[start + i + 1]);
		ver[start + i].Tangent = calcTangent(ver[start + i], ver[start], ver[start + i + 1]);
		ver[start + i].BiTangent = calcBitangent(ver[start + i], ver[start], ver[start + i + 1]);
		ver[start + i + 1].Tangent = calcTangent(ver[start + i + 1], ver[start + i], ver[start]);
		ver[start + i + 1].BiTangent = calcBitangent(ver[start + i + 1], ver[start + i], ver[start]);
	}
	ind.push_back(start + f);
	ind.push_back(start + 1);
	ind.push_back(start);
}

void MyCone::genSide(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float r1, float r2, float h, unsigned f)
{
	Vertex v;
	unsigned start = ver.size();
	for (int i = 0; i <= f; i++)
	{
		glm::vec3 tp = glm::vec3(r1*glm::sin(glm::radians(360.0 / f * i)), h, r1*glm::cos(glm::radians(360.0 / f * i)));
		glm::vec3 bp = glm::vec3(r2*glm::sin(glm::radians(360.0 / f * i)), -h, r2*glm::cos(glm::radians(360.0 / f * i)));
		glm::vec3 dir = glm::vec3(glm::sin(glm::radians(360.0 / f * i)), 0, glm::cos(glm::radians(360.0 / f * i)));
		glm::vec3 tangent = glm::cross(tp - bp, dir);
		v.Normal = glm::normalize(glm::cross(tangent, tp - bp));
		v.Position = tp;
		v.TexCoords = glm::vec2(1.0 * i / f, 0);
		ver.push_back(v);
		v.Position = bp;
		v.TexCoords = glm::vec2(1.0 * i / f, 1);
		ver.push_back(v);
	}
	for (int i = 0; i < f; i++)
	{
		ind.push_back(start + i * 2);
		ind.push_back(start + i * 2 + 1);
		ind.push_back(start + i * 2 + 3);
		ind.push_back(start + i * 2 + 3);
		ind.push_back(start + i * 2 + 2);
		ind.push_back(start + i * 2);
		ver[start + i * 2].Tangent = calcTangent(ver[start + i * 2], ver[start + i * 2 + 1], ver[start + i * 2 + 3]);
		ver[start + i * 2].BiTangent = calcBitangent(ver[start + i * 2], ver[start + i * 2 + 1], ver[start + i * 2 + 3]);
		ver[start + i * 2 + 1].Tangent = calcTangent(ver[start + i * 2 + 1], ver[start + i * 2], ver[start + i * 2 + 3]);
		ver[start + i * 2 + 1].BiTangent = calcBitangent(ver[start + i * 2 + 1], ver[start + i * 2], ver[start + i * 2 + 3]);
		ver[start + i * 2 + 2].Tangent = calcTangent(ver[start + i * 2 + 2], ver[start + i * 2 + 3], ver[start + i * 2]);
		ver[start + i * 2 + 2].BiTangent = calcBitangent(ver[start + i * 2 + 2], ver[start + i * 2 + 3], ver[start + i * 2]);
		ver[start + i * 2 + 3].Tangent = calcTangent(ver[start + i * 2 + 3], ver[start + i * 2 + 1], ver[start + i * 2]);
		ver[start + i * 2 + 3].BiTangent = calcBitangent(ver[start + i * 2 + 3], ver[start + i * 2 + 1], ver[start + i * 2]);
	}
}

MyCone::MyCone(GLViewport * pa) :MyObject(pa)
{
	T = Cone;
	setSize(0, 100, 200, 64);
}

MyCone::MyCone(GLViewport * pa, QString name) :MyObject(pa, name)
{
	T = Cone;
	setSize(0, 100, 200, 64);
}

void MyCone::setSize(float r1, float r2, float h, unsigned f)
{
	rtop = r1 >= 0 ? r1 : 0;
	rbottom = r2 >= 0 ? r2 : 0;
	height = h >= 0 ? h : 0;
	fragment = f >= 3 ? f : 3;
	std::vector<Vertex> ver;
	std::vector<unsigned> ind;
	if (rtop>0.0f)
		genDisk(ver, ind, rtop, height / 2, fragment);
	if (rbottom > 0.0f)
		genDisk(ver, ind, rbottom, -height / 2, fragment);
	genSide(ver, ind, rtop, rbottom, height / 2, fragment);
	allocateVertices(&ver[0], ver.size() * sizeof(Vertex));
	allocateIndices(&ind[0], ind.size() * sizeof(unsigned));
	setVertexAttrib();
}

glm::vec3 MyCone::getSize()
{
	return glm::vec3(rtop, rbottom, height);
}

unsigned MyCone::getFragment()
{
	return fragment;
}

void MyPyramid::genSide(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float r1, float r2, float h, unsigned f)
{
	Vertex v;
	unsigned start = ver.size();
	for (int i = 0; i <= f; i++)
	{
		glm::vec3 tp1 = glm::vec3(r1*glm::sin(glm::radians(360.0 / f * i)), h, r1*glm::cos(glm::radians(360.0 / f * i)));
		glm::vec3 bp1 = glm::vec3(r2*glm::sin(glm::radians(360.0 / f * i)), -h, r2*glm::cos(glm::radians(360.0 / f * i)));
		glm::vec3 tp2 = glm::vec3(r1*glm::sin(glm::radians(360.0 / f * (i + 1))), h, r1*glm::cos(glm::radians(360.0 / f * (i + 1))));
		glm::vec3 bp2 = glm::vec3(r2*glm::sin(glm::radians(360.0 / f * (i + 1))), -h, r2*glm::cos(glm::radians(360.0 / f * (i + 1))));
		if (r1 > 0.0)
			v.Normal = glm::normalize(glm::cross(bp1 - tp1, tp2 - tp1));
		else if (r2 > 0.0)
			v.Normal = glm::normalize(glm::cross(bp2 - bp1, tp1 - bp1));
		else
			v.Normal = glm::vec3(0, 1, 0);
		v.Position = tp1;
		v.TexCoords = glm::vec2(1.0 * i / f, 0);
		ver.push_back(v);
		v.Position = bp1;
		v.TexCoords = glm::vec2(1.0 * i / f, 1);
		ver.push_back(v);
		v.Position = bp2;
		v.TexCoords = glm::vec2(1.0 * (i + 1) / f, 1);
		ver.push_back(v);
		v.Position = tp2;
		v.TexCoords = glm::vec2(1.0 * (i + 1) / f, 0);
		ver.push_back(v);
		ind.push_back(start + 4 * i + 0);
		ind.push_back(start + 4 * i + 1);
		ind.push_back(start + 4 * i + 2);
		ind.push_back(start + 4 * i + 2);
		ind.push_back(start + 4 * i + 3);
		ind.push_back(start + 4 * i + 0);
		ver[start + i * 4 + 0].Tangent = calcTangent(ver[start + i * 4 + 0], ver[start + i * 4 + 1], ver[start + i * 4 + 2]);
		ver[start + i * 4 + 0].BiTangent = calcBitangent(ver[start + i * 4 + 0], ver[start + i * 4 + 1], ver[start + i * 4 + 2]);
		ver[start + i * 4 + 1].Tangent = calcTangent(ver[start + i * 4 + 1], ver[start + i * 4 + 0], ver[start + i * 4 + 2]);
		ver[start + i * 4 + 1].BiTangent = calcBitangent(ver[start + i * 4 + 1], ver[start + i * 4 + 0], ver[start + i * 4 + 2]);
		ver[start + i * 4 + 2].Tangent = calcTangent(ver[start + i * 4 + 2], ver[start + i * 4 + 1], ver[start + i * 4 + 0]);
		ver[start + i * 4 + 2].BiTangent = calcBitangent(ver[start + i * 4 + 2], ver[start + i * 4 + 1], ver[start + i * 4 + 0]);
		ver[start + i * 4 + 3].Tangent = calcTangent(ver[start + i * 4 + 3], ver[start + i * 4 + 0], ver[start + i * 4 + 2]);
		ver[start + i * 4 + 3].BiTangent = calcBitangent(ver[start + i * 4 + 3], ver[start + i * 4 + 0], ver[start + i * 4 + 2]);
	}
}

MyPyramid::MyPyramid(GLViewport * pa) :MyCone(pa, 0)
{
	T = Pyramid;
	setSize(0, 100, 200, 4);
}

MyPyramid::MyPyramid(GLViewport * pa, QString name) :MyCone(pa, name, 0)
{
	T = Pyramid;
	setSize(0, 100, 200, 4);
}

void MyDisk::genDisk(std::vector<Vertex>& ver, std::vector<unsigned>& ind, float r, unsigned f)
{
	Vertex v;
	unsigned start = ver.size();
	v.Normal = glm::vec3(0, 1, 0);
	v.Position = glm::vec3(0, 0, 0);
	v.TexCoords = glm::vec2(0.5, 0.5);
	ver.push_back(v);
	for (int i = 0; i < f; i++)
	{
		v.Position = glm::vec3(r*glm::sin(glm::radians(360.0 / f * i)), 0, r*glm::cos(glm::radians(360.0 / f * i)));
		v.TexCoords = glm::vec2((glm::sin(glm::radians(360.0 / f * i)) + 1) / 2, (glm::cos(glm::radians(360.0 / f * i)) + 1) / 2);
		ver.push_back(v);
	}
	for (int i = 1; i < f; i++)
	{
		ind.push_back(start + i);
		ind.push_back(start + i + 1);
		ind.push_back(start);
		ver[start].Tangent = calcTangent(ver[start], ver[start + i], ver[start + i + 1]);
		ver[start].BiTangent = calcBitangent(ver[start], ver[start + i], ver[start + i + 1]);
		ver[start + i].Tangent = calcTangent(ver[start + i], ver[start], ver[start + i + 1]);
		ver[start + i].BiTangent = calcBitangent(ver[start + i], ver[start], ver[start + i + 1]);
		ver[start + i + 1].Tangent = calcTangent(ver[start + i + 1], ver[start + i], ver[start]);
		ver[start + i + 1].BiTangent = calcBitangent(ver[start + i + 1], ver[start + i], ver[start]);
	}
	ind.push_back(start + f);
	ind.push_back(start + 1);
	ind.push_back(start);
}

MyDisk::MyDisk(GLViewport * pa) :MyObject(pa)
{
	setSize(200, 36);
	T = Disk;
}

MyDisk::MyDisk(GLViewport * pa, QString name) : MyObject(pa, name)
{
	setSize(200, 36);
	T = Disk;
}

void MyDisk::setSize(float r, unsigned f)
{
	radius = r >= 0 ? r : 0;
	fragment = f >= 3 ? f : 3;
	std::vector<Vertex> ver;
	std::vector<unsigned> ind;
	genDisk(ver, ind, radius, fragment);
	allocateVertices(&ver[0], ver.size() * sizeof(Vertex));
	allocateIndices(&ind[0], ind.size() * sizeof(unsigned));
	setVertexAttrib();
}

float MyDisk::getSize()
{
	return radius;
}

unsigned MyDisk::getFragment()
{
	return fragment;
}

SpotLight::SpotLight(GLViewport * pa) :MyLight(pa), l_color(1), l_intensity(1)
{
	T = Spot;
	std::vector<Vertex> ver;
	genLines(ver);
	allocateVertices(&ver[0], ver.size() * sizeof(Vertex));
	setVertexAttrib();
}

void SpotLight::genLines(std::vector<Vertex>& ver)
{
	Vertex v;
	v.Normal = glm::vec3(1, 0, 0);
	v.TexCoords = glm::vec2(0);
	v.Position = glm::vec3(20, 0, 0);
	ver.push_back(v);
	v.Position = glm::vec3(-20, 0, 0);
	ver.push_back(v);
	v.Position = glm::vec3(0, 20, 0);
	ver.push_back(v);
	v.Position = glm::vec3(0, -20, 0);
	ver.push_back(v);
	v.Position = glm::vec3(0, 0, 20);
	ver.push_back(v);
	v.Position = glm::vec3(0, 0, -20);
	ver.push_back(v);
	float x = 10 * glm::sqrt(2);
	v.Position = glm::vec3(x, x, 0);
	ver.push_back(v);
	v.Position = glm::vec3(-x, -x, 0);
	ver.push_back(v);
	v.Position = glm::vec3(-x, x, 0);
	ver.push_back(v);
	v.Position = glm::vec3(x, -x, 0);
	ver.push_back(v);
	v.Position = glm::vec3(x, 0, x);
	ver.push_back(v);
	v.Position = glm::vec3(-x, 0, -x);
	ver.push_back(v);
	v.Position = glm::vec3(-x, 0, x);
	ver.push_back(v);
	v.Position = glm::vec3(x, 0, -x);
	ver.push_back(v);
	v.Position = glm::vec3(0, x, x);
	ver.push_back(v);
	v.Position = glm::vec3(0, -x, -x);
	ver.push_back(v);
	v.Position = glm::vec3(0, -x, x);
	ver.push_back(v);
	v.Position = glm::vec3(0, x, -x);
	ver.push_back(v);
}

void SpotLight::setLight(QOpenGLShaderProgram * shader, unsigned index)
{
	shader->setUniformValue((std::string("l[") + std::to_string(index) + "].color").c_str(), l_color.r, l_color.g, l_color.b);
	shader->setUniformValue((std::string("l[") + std::to_string(index) + "].pos").c_str(), position.x, position.y, position.z, 1.0f);
	shader->setUniformValue((std::string("l[") + std::to_string(index) + "].intensity").c_str(), l_intensity);
}

void SpotLight::draw(MyColorMaterial * mtl)
{
	glm::vec3 c = l_color * l_intensity;
	mtl->bind(c);
	QOpenGLShaderProgram *shader = mtl->getShader();
	parent->glUniformMatrix4fv(parent->glGetUniformLocation(shader->programId(), "m_trans"), 1, GL_FALSE, glm::value_ptr(getMatrix()));
	VAO.bind();
	VBO.bind();
	parent->glDrawArrays(GL_LINES, 0, VBO.size() / sizeof(Vertex));
}

SpotLight * SpotLight::createLight(GLViewport * pa)
{
	return new SpotLight(pa);
}
