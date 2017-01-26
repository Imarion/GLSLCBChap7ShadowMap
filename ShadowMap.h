#include <QWindow>
#include <QTimer>
#include <QString>
#include <QKeyEvent>

#include <QVector3D>
#include <QMatrix4x4>

#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLFunctions_4_3_Core>

#include <QOpenGLShaderProgram>

#include "teapot.h"
#include "vboplane.h"
#include "torus.h"
#include "frustum.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define ToRadian(x) ((x) * M_PI / 180.0f)
#define ToDegree(x) ((x) * 180.0f / M_PI)
#define TwoPI (float)(2 * M_PI)

//class MyWindow : public QWindow, protected QOpenGLFunctions_3_3_Core
class MyWindow : public QWindow, protected QOpenGLFunctions
{
    Q_OBJECT

public:
    explicit MyWindow();
    ~MyWindow();
    virtual void keyPressEvent( QKeyEvent *keyEvent );    

private slots:
    void render();

private:    
    void initialize();
    void setupFBO();
    void modCurTime();

    void initShaders();
    void CreateVertexBuffer();    
    void initMatrices();
    void drawscene();
    void renderScene();

    void PrepareTexture(GLenum TextureTarget, const QString& FileName, GLuint& TexObject, bool flip);

protected:
    void resizeEvent(QResizeEvent *);

private:
    QOpenGLContext *mContext;
    QOpenGLFunctions_4_3_Core *mFuncs;

    QOpenGLShaderProgram *mProgram;

    QTimer mRepaintTimer;
    double currentTimeMs;
    double currentTimeS;
    bool   mUpdateSize;
    float  tPrev, angle;
    int    shadowMapWidth, shadowMapHeight;

    GLuint mVAOTeapot, mVAOPlane, mVAOTorus, mVBO, mIBO, shadowFBO;
    GLuint mPositionBufferHandle, mColorBufferHandle;
    GLuint mRotationMatrixLocation;
    GLuint pass1Index, pass2Index;

    Teapot   *mTeapot;
    VBOPlane *mPlane;
    Torus    *mTorus;

    QVector3D  worldLight;
    Frustum    *lightFrustum;
    QMatrix4x4 ModelMatrixTeapot, ModelMatrixPlane[3], ModelMatrixTorus, ViewMatrix, ProjectionMatrix;
    QMatrix4x4 shadowBias, LightPV, ViewMatrixLight, ProjectionMatrixLight;

    //debug
    void printMatrix(const QMatrix4x4& mat);
};
