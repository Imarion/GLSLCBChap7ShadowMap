#ifndef FRUSTUM_H
#define FRUSTUM_H

#include <QVector3D>
#include <QMatrix4x4>

namespace Projection {
    enum ProjType {
        PERSPECTIVE, ORTHO
    };
}

class Frustum
{
private:
    Projection::ProjType type;

    QVector3D origin;
    QVector3D at;
    QVector3D up;

    float mNear;
    float mFar;
    float xmin, xmax, ymin, ymax;
    float fovy, ar;

    QVector3D view, proj;
    int       handle[2];

public:
    Frustum( Projection::ProjType type );

    void orient( const QVector3D &pos, const QVector3D& a, const QVector3D& u );
    void setOrthoBounds( float xmin, float xmax, float ymin, float ymax,
                         float , float  );
    void setPerspective( float , float , float , float  );
    void enclose( const Frustum & );

    QMatrix4x4 getViewMatrix() const;
    QMatrix4x4 getProjectionMatrix() const;
    QVector3D getOrigin() const;
    QVector3D getCenter() const;

    void printInfo() const;
    void render() const;
};

#endif // FRUSTUM_H
