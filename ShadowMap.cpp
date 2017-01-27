#include "ShadowMap.h"

#include <QtGlobal>

#include <QDebug>
#include <QFile>
#include <QImage>
#include <QTime>

#include <QVector2D>
#include <QVector3D>
#include <QMatrix4x4>

#include <cmath>
#include <cstring>

MyWindow::~MyWindow()
{
    if (mProgram != 0) delete mProgram;
}

MyWindow::MyWindow()
    : mProgram(0), currentTimeMs(0), currentTimeS(0), tPrev(0), angle(M_PI / 4.0f), shadowMapWidth(512), shadowMapHeight(512)
{
    setSurfaceType(QWindow::OpenGLSurface);
    setFlags(Qt::Window | Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);

    QSurfaceFormat format;
    format.setDepthBufferSize(24);
    format.setMajorVersion(4);
    format.setMinorVersion(3);
    format.setSamples(4);
    format.setProfile(QSurfaceFormat::CoreProfile);
    setFormat(format);
    create();

    resize(800, 600);

    mContext = new QOpenGLContext(this);
    mContext->setFormat(format);
    mContext->create();

    mContext->makeCurrent( this );

    mFuncs = mContext->versionFunctions<QOpenGLFunctions_4_3_Core>();
    if ( !mFuncs )
    {
        qWarning( "Could not obtain OpenGL versions object" );
        exit( 1 );
    }
    if (mFuncs->initializeOpenGLFunctions() == GL_FALSE)
    {
        qWarning( "Could not initialize core open GL functions" );
        exit( 1 );
    }

    initializeOpenGLFunctions();

    QTimer *repaintTimer = new QTimer(this);
    connect(repaintTimer, &QTimer::timeout, this, &MyWindow::render);
    repaintTimer->start(1000/60);

    QTimer *elapsedTimer = new QTimer(this);
    connect(elapsedTimer, &QTimer::timeout, this, &MyWindow::modCurTime);
    elapsedTimer->start(1);       
}

void MyWindow::modCurTime()
{
    currentTimeMs++;
    currentTimeS=currentTimeMs/1000.0f;
}

void MyWindow::initialize()
{
    CreateVertexBuffer();
    setupFBO();

    initShaders();
    pass1Index = mFuncs->glGetSubroutineIndex(mProgram->programId(), GL_FRAGMENT_SHADER, "recordDepth");
    pass2Index = mFuncs->glGetSubroutineIndex(mProgram->programId(), GL_FRAGMENT_SHADER, "shadeWithShadow");

    initMatrices();

    //mRotationMatrixLocation = mProgram->uniformLocation("RotationMatrix");

    lightFrustum = new Frustum(Projection::PERSPECTIVE);
    float c = 1.65f;
    QVector3D lightPos(0.0f,c * 5.25f, c * 7.5f);  // World coords
    lightFrustum->orient( lightPos, QVector3D(0.0f, 0.0f, 0.0f), QVector3D(0.0f,1.0f,0.0f));
    lightFrustum->setPerspective( 50.0f, 1.0f, 1.0f, 25.0f);
    LightPV = shadowBias * lightFrustum->getProjectionMatrix() * lightFrustum->getViewMatrix();

    glFrontFace(GL_CCW);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.0, 1.0);
}

void MyWindow::setupFBO()
{
    GLfloat border[] = {1.0f, 0.0f,0.0f,0.0f };
    // The depth buffer texture
    GLuint depthTex;
    glGenTextures(1, &depthTex);
    glBindTexture(GL_TEXTURE_2D, depthTex);
    mFuncs->glTexStorage2D(GL_TEXTURE_2D, 1, GL_DEPTH_COMPONENT24, shadowMapWidth, shadowMapHeight);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LESS);

    // Assign the depth buffer texture to texture channel 0
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthTex);

    // Create and set up the FBO
    glGenFramebuffers(1, &shadowFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                           GL_TEXTURE_2D, depthTex, 0);

    GLenum drawBuffers[] = {GL_NONE};
    mFuncs->glDrawBuffers(1, drawBuffers);

    GLenum result = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if( result == GL_FRAMEBUFFER_COMPLETE) {
        printf("Framebuffer is complete.\n");
    } else {
        printf("Framebuffer is not complete.\n");
    }

    glBindFramebuffer(GL_FRAMEBUFFER,0);
}


void MyWindow::CreateVertexBuffer()
{
    // *** Teapot
    mFuncs->glGenVertexArrays(1, &mVAOTeapot);
    mFuncs->glBindVertexArray(mVAOTeapot);

    QMatrix4x4 transform;    
    mTeapot = new Teapot(14, transform);

    // Create and populate the buffer objects
    unsigned int TeapotHandles[3];
    glGenBuffers(3, TeapotHandles);

    glBindBuffer(GL_ARRAY_BUFFER, TeapotHandles[0]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mTeapot->getnVerts()) * sizeof(float), mTeapot->getv(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, TeapotHandles[1]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mTeapot->getnVerts()) * sizeof(float), mTeapot->getn(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, TeapotHandles[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * mTeapot->getnFaces() * sizeof(unsigned int), mTeapot->getelems(), GL_STATIC_DRAW);

    // Setup the VAO
    // Vertex positions
    mFuncs->glBindVertexBuffer(0, TeapotHandles[0], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(0, 0);

    // Vertex normals
    mFuncs->glBindVertexBuffer(1, TeapotHandles[1], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(1, 1);

    // Indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, TeapotHandles[2]);

    mFuncs->glBindVertexArray(0);

    // *** Plane
    mFuncs->glGenVertexArrays(1, &mVAOPlane);
    mFuncs->glBindVertexArray(mVAOPlane);

    mPlane = new VBOPlane(40.0f, 40.0f, 2.0, 2.0);

    // Create and populate the buffer objects
    unsigned int PlaneHandles[3];
    glGenBuffers(3, PlaneHandles);

    glBindBuffer(GL_ARRAY_BUFFER, PlaneHandles[0]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mPlane->getnVerts()) * sizeof(float), mPlane->getv(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, PlaneHandles[1]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mPlane->getnVerts()) * sizeof(float), mPlane->getn(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, PlaneHandles[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * mPlane->getnFaces() * sizeof(unsigned int), mPlane->getelems(), GL_STATIC_DRAW);

    // Setup the VAO
    // Vertex positions
    mFuncs->glBindVertexBuffer(0, PlaneHandles[0], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(0, 0);

    // Vertex normals
    mFuncs->glBindVertexBuffer(1, PlaneHandles[1], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(1, 1);

    // Indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, PlaneHandles[2]);

    mFuncs->glBindVertexArray(0);

    // *** Torus
    mFuncs->glGenVertexArrays(1, &mVAOTorus);
    mFuncs->glBindVertexArray(mVAOTorus);

    mTorus = new Torus(0.7f * 2.0f, 0.3f * 2.0f, 50, 50);

    // Create and populate the buffer objects
    unsigned int TorusHandles[3];
    glGenBuffers(3, TorusHandles);

    glBindBuffer(GL_ARRAY_BUFFER, TorusHandles[0]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mTorus->getnVerts()) * sizeof(float), mTorus->getv(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, TorusHandles[1]);
    glBufferData(GL_ARRAY_BUFFER, (3 * mTorus->getnVerts()) * sizeof(float), mTorus->getn(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, TorusHandles[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 6 * mTorus->getnFaces() * sizeof(unsigned int), mTorus->getel(), GL_STATIC_DRAW);

    // Setup the VAO
    // Vertex positions
    mFuncs->glBindVertexBuffer(0, TorusHandles[0], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(0, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(0, 0);

    // Vertex normals
    mFuncs->glBindVertexBuffer(1, TorusHandles[1], 0, sizeof(GLfloat) * 3);
    mFuncs->glVertexAttribFormat(1, 3, GL_FLOAT, GL_FALSE, 0);
    mFuncs->glVertexAttribBinding(1, 1);

    // Indices
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, TorusHandles[2]);

    mFuncs->glBindVertexArray(0);

}

void MyWindow::initMatrices()
{
    ModelMatrixTeapot.rotate(  -90.0f, QVector3D(1.0f, 0.0f, 0.0f));

    ModelMatrixTorus.translate( 0.0f, 2.0f, 5.0f);
    ModelMatrixTorus.rotate(  -45.0f, QVector3D(1.0f, 0.0f, 0.0f));

    ModelMatrixPlane[1].translate(-5.0f, 5.0f, 0.0f);
    ModelMatrixPlane[1].rotate(-90.0f, QVector3D(0.0f, 0.0f, 1.0f));

    ModelMatrixPlane[2].translate( 0.0f, 5.0f, -5.0f);
    ModelMatrixPlane[2].rotate(-90.0f, QVector3D(1.0f, 0.0f, 0.0f));

    //ViewMatrix.lookAt(QVector3D(5.0f, 5.0f, 7.5f), QVector3D(0.0f,0.75f,0.0f), QVector3D(0.0f,1.0f,0.0f));

    // ! QT matrix is in row major
    shadowBias = QMatrix4x4(0.5f,0.0f,0.0f,0.5f,
                            0.0f,0.5f,0.0f,0.5f,
                            0.0f,0.0f,0.5f,0.5f,
                            0.0f,0.0f,0.0f,1.0f);

}

void MyWindow::resizeEvent(QResizeEvent *)
{
    mUpdateSize = true;

//    ProjectionMatrix.setToIdentity();
//    ProjectionMatrix.perspective(50.0f, (float)this->width()/(float)this->height(), 0.1f, 100.0f);
}

void MyWindow::render()
{
    if(!isVisible() || !isExposed())
        return;

    if (!mContext->makeCurrent(this))
        return;

    static bool initialized = false;
    if (!initialized) {
        initialize();
        initialized = true;
    }

    if (mUpdateSize) {
        glViewport(0, 0, size().width(), size().height());
        mUpdateSize = false;
    }

    //shadowMap();
    renderScene();
}

void MyWindow::renderScene()
{
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float deltaT = currentTimeS - tPrev;
    if(tPrev == 0.0f) deltaT = 0.0f;
    tPrev = currentTimeS;
    angle += 0.2f * deltaT;
    if (angle > TwoPI) angle -= TwoPI;

    float c = 1.0f;
    QVector3D cameraPos(c * 11.5f * cos(angle),c * 7.0f,c * 11.5f * sin(angle));

    //Pass 1 - render shadow map
    ViewMatrix.setToIdentity();
    ViewMatrix = lightFrustum->getViewMatrix();
    ProjectionMatrix.setToIdentity();
    ProjectionMatrix = lightFrustum->getProjectionMatrix();

    glBindFramebuffer(GL_FRAMEBUFFER, shadowFBO);
    glClear(GL_DEPTH_BUFFER_BIT);
    glViewport(0,0,shadowMapWidth,shadowMapHeight);
    mFuncs->glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &pass1Index);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);

    drawscene();

    //Pass 2 - actual render

    ViewMatrix.setToIdentity();
    ViewMatrix.lookAt(cameraPos,QVector3D(0.0f, 0.0f, 0.0f),QVector3D(0.0f,1.0f,0.0f));
    ProjectionMatrix.setToIdentity();
    ProjectionMatrix.perspective(50.0f, (float)this->width()/(float)this->height(), 0.1f, 100.0f);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0,0,this->width(), this->height());
    mFuncs->glUniformSubroutinesuiv( GL_FRAGMENT_SHADER, 1, &pass2Index);
    glDisable(GL_CULL_FACE);

    drawscene();

    mContext->swapBuffers(this);
}

void MyWindow::drawscene()
{
    QVector3D color(0.7f,0.5f,0.3f);

    // *** Draw teapot
    mFuncs->glBindVertexArray(mVAOTeapot);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    mProgram->bind();
    {        

        QMatrix4x4 mv1 = ViewMatrix * ModelMatrixTeapot;
        mProgram->setUniformValue("ModelViewMatrix", mv1);
        mProgram->setUniformValue("NormalMatrix", mv1.normalMatrix());
        mProgram->setUniformValue("MVP", ProjectionMatrix * mv1);

        mProgram->setUniformValue("ShadowMatrix", LightPV * ModelMatrixTeapot);
        mProgram->setUniformValue("ShadowMap", 0);

        mProgram->setUniformValue("Light.Position", ViewMatrix * QVector4D(lightFrustum->getOrigin(), 1.0f));
        mProgram->setUniformValue("Light.Intensity", QVector3D(0.85f, 0.85f, 0.85f));
        mProgram->setUniformValue("ViewNormalMatrix", ViewMatrix.normalMatrix());

        mProgram->setUniformValue("Material.Ka", color * 0.05f);
        mProgram->setUniformValue("Material.Kd", color);
        mProgram->setUniformValue("Material.Ks", 0.9f, 0.9f, 0.9f);
        mProgram->setUniformValue("Material.Shininess", 150.0f);

        glDrawElements(GL_TRIANGLES, 6 * mTeapot->getnFaces(), GL_UNSIGNED_INT, ((GLubyte *)NULL + (0)));

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    }
    mProgram->release();

    // *** Draw planes
    mFuncs->glBindVertexArray(mVAOPlane);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    mProgram->bind();
    {
        mProgram->setUniformValue("Light.Position", ViewMatrix * QVector4D(lightFrustum->getOrigin(), 1.0f));
        mProgram->setUniformValue("Light.Intensity", QVector3D(0.85f, 0.85f, 0.85f));
        mProgram->setUniformValue("ViewNormalMatrix", ViewMatrix.normalMatrix());

        mProgram->setUniformValue("Material.Ka", 0.05f, 0.05f, 0.05f);
        mProgram->setUniformValue("Material.Kd", 0.25f, 0.25f, 0.25f);
        mProgram->setUniformValue("Material.Ks", 0.0f,  0.0f,  0.0f);
        mProgram->setUniformValue("Material.Shininess", 1.0f);

        mProgram->setUniformValue("ShadowMap", 0);
        for (int i=0; i<3; i++)
        {
            QMatrix4x4 mv1 = ViewMatrix * ModelMatrixPlane[i];
            mProgram->setUniformValue("ModelViewMatrix", mv1);
            mProgram->setUniformValue("NormalMatrix", mv1.normalMatrix());
            mProgram->setUniformValue("MVP", ProjectionMatrix * mv1);

            mProgram->setUniformValue("ShadowMatrix", LightPV * ModelMatrixPlane[i]);

            glDrawElements(GL_TRIANGLES, 6 * mPlane->getnFaces(), GL_UNSIGNED_INT, ((GLubyte *)NULL + (0)));
        }

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    }
    mProgram->release();

    // *** Draw torus
    mFuncs->glBindVertexArray(mVAOTorus);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    mProgram->bind();
    {
        QMatrix4x4 mv1 = ViewMatrix * ModelMatrixTorus;
        mProgram->setUniformValue("ModelViewMatrix", mv1);
        mProgram->setUniformValue("NormalMatrix", mv1.normalMatrix());
        mProgram->setUniformValue("MVP", ProjectionMatrix * mv1);

        mProgram->setUniformValue("ShadowMatrix", LightPV * ModelMatrixTorus);
        mProgram->setUniformValue("ShadowMap", 0);

        mProgram->setUniformValue("Light.Position", ViewMatrix * QVector4D(lightFrustum->getOrigin(), 1.0f));
        mProgram->setUniformValue("Light.Intensity", QVector3D(0.85f, 0.85f, 0.85f));
        mProgram->setUniformValue("ViewNormalMatrix", ViewMatrix.normalMatrix());

        mProgram->setUniformValue("Material.Ka", color * 0.05f);
        mProgram->setUniformValue("Material.Kd", color);
        mProgram->setUniformValue("Material.Ks", 0.9f, 0.9f, 0.9f);
        mProgram->setUniformValue("Material.Shininess", 150.0f);

        glDrawElements(GL_TRIANGLES, 6 * mTorus->getnFaces(), GL_UNSIGNED_INT, ((GLubyte *)NULL + (0)));

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
    }
    mProgram->release();

}

void MyWindow::initShaders()
{
    QOpenGLShader vShader(QOpenGLShader::Vertex);
    QOpenGLShader fShader(QOpenGLShader::Fragment);    
    QFile         shaderFile;
    QByteArray    shaderSource;

    //Simple ADS
    shaderFile.setFileName(":/vshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "vertex compile: " << vShader.compileSourceCode(shaderSource);

    shaderFile.setFileName(":/fshader.txt");
    shaderFile.open(QIODevice::ReadOnly);
    shaderSource = shaderFile.readAll();
    shaderFile.close();
    qDebug() << "frag   compile: " << fShader.compileSourceCode(shaderSource);

    mProgram = new (QOpenGLShaderProgram);
    mProgram->addShader(&vShader);
    mProgram->addShader(&fShader);
    qDebug() << "shader link: " << mProgram->link();
}

void MyWindow::PrepareTexture(GLenum TextureTarget, const QString& FileName, GLuint& TexObject, bool flip)
{
    QImage TexImg;

    if (!TexImg.load(FileName)) qDebug() << "Erreur chargement texture";
    if (flip==true) TexImg=TexImg.mirrored();

    glGenTextures(1, &TexObject);
    glBindTexture(TextureTarget, TexObject);
    glTexImage2D(TextureTarget, 0, GL_RGB, TexImg.width(), TexImg.height(), 0, GL_BGRA, GL_UNSIGNED_BYTE, TexImg.bits());
    glTexParameterf(TextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(TextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void MyWindow::keyPressEvent(QKeyEvent *keyEvent)
{
    switch(keyEvent->key())
    {
        case Qt::Key_P:
            break;
        case Qt::Key_Up:
            break;
        case Qt::Key_Down:
            break;
        case Qt::Key_Left:
            break;
        case Qt::Key_Right:
            break;
        case Qt::Key_Delete:
            break;
        case Qt::Key_PageDown:
            break;
        case Qt::Key_Home:
            break;
        case Qt::Key_Z:
            break;
        case Qt::Key_Q:
            break;
        case Qt::Key_S:
            break;
        case Qt::Key_D:
            break;
        case Qt::Key_A:
            break;
        case Qt::Key_E:
            break;
        default:
            break;
    }
}

void MyWindow::printMatrix(const QMatrix4x4& mat)
{
    const float *locMat = mat.transposed().constData();

    for (int i=0; i<4; i++)
    {
        qDebug() << locMat[i*4] << " " << locMat[i*4+1] << " " << locMat[i*4+2] << " " << locMat[i*4+3];
    }
}
