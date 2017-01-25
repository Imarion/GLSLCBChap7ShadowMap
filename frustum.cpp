#include "frustum.h"
#include <limits>
#include <cstdio>

#include <QVector3D>
#include <QVector4D>
#include <QMatrix4x4>

#include <QtMath>

Frustum::Frustum(Projection::ProjType t) : type(t)
{
    //int elem[12 * 2];

    this->orient( QVector3D(0.0f,0.0f,1.0f), QVector3D(0.0f,0.0f,0.0f), QVector3D(0.0f,1.0f,0.0f) );
    if( type == Projection::ORTHO ) {
        this->setOrthoBounds(-1.0f,1.0f,-1.0f,1.0f,-1.0f,1.0f);
    } else {
        this->setPerspective(50.0f, 1.0f, 0.5f, 100.0f);
/*
        int idx = 0;
        // The outer edges
        elem[idx++] = 0; elem[idx++] = 5;
        elem[idx++] = 0; elem[idx++] = 6;
        elem[idx++] = 0; elem[idx++] = 7;
        elem[idx++] = 0; elem[idx++] = 8;
        // The near plane
        elem[idx++] = 1; elem[idx++] = 2;
        elem[idx++] = 2; elem[idx++] = 3;
        elem[idx++] = 3; elem[idx++] = 4;
        elem[idx++] = 4; elem[idx++] = 1;
        // The far plane
        elem[idx++] = 5; elem[idx++] = 6;
        elem[idx++] = 6; elem[idx++] = 7;
        elem[idx++] = 7; elem[idx++] = 8;
        elem[idx++] = 8; elem[idx++] = 5;
        */
    }
/*
    glBindVertexArray(0);
    glGenBuffers(2, handle);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 24 * sizeof(int), elem, GL_STATIC_DRAW);
*/
}

void Frustum::orient( const QVector3D &pos, const QVector3D& a, const QVector3D& u )
{
    this->origin = pos;
    this->at = a;
    this->up = u;
}

void Frustum::setOrthoBounds( float xmin, float xmax, float ymin, float ymax,
                     float nearDist, float farDist )
{
    this->xmin = xmin;
    this->xmax = xmax;
    this->ymin = ymin;
    this->ymax= ymax;
    this->mNear = nearDist;
	this->mFar = farDist;
}

void Frustum::setPerspective( float fovy, float ar, float nearDist, float farDist )
{
    this->fovy = fovy;
    this->ar = ar;
    this->mNear = nearDist;
    this->mFar = farDist;
}

void Frustum::enclose( const Frustum & other )
{
    QVector3D n = QVector3D(other.origin - other.at).normalized();
    QVector3D u = QVector3D(QVector3D::crossProduct(other.up, n)).normalized();
    QVector3D v = QVector3D(QVector3D::crossProduct(n, u)).normalized();
    if( type == Projection::PERSPECTIVE )
        this->orient( origin, other.getCenter(), up );
    QMatrix4x4 m = this->getViewMatrix();

    QVector3D p[8];

    // Get 8 points that define the frustum
    if( other.type == Projection::PERSPECTIVE ) {
        float dy = other.mNear * tanf( qDegreesToRadians(other.fovy) / 2.0f );
        float dx = other.ar * dy;
        QVector3D c = other.origin - n * other.mNear;
        p[0] = c + u * dx + v * dy;
        p[1] = c - u * dx + v * dy;
        p[2] = c - u * dx - v * dy;
        p[3] = c + u * dx - v * dy;
        dy = other.mFar * tanf( qDegreesToRadians(other.fovy) / 2.0f );
        dx = other.ar * dy;
        c = other.origin - n * other.mFar;
        p[4] = c + u * dx + v * dy;
        p[5] = c - u * dx + v * dy;
        p[6] = c - u * dx - v * dy;
        p[7] = c + u * dx - v * dy;
    } else {
        QVector3D c = other.origin - n * other.mNear;
        p[0] = c + u * other.xmax + v * other.ymax;
        p[1] = c + u * other.xmax + v * other.ymin;
        p[2] = c + u * other.xmin + v * other.ymax;
        p[3] = c + u * other.xmin + v * other.ymin;
        c = other.origin - n * other.mFar;
        p[4] = c + u * other.xmax + v * other.ymax;
        p[5] = c + u * other.xmax + v * other.ymin;
        p[6] = c + u * other.xmin + v * other.ymax;
        p[7] = c + u * other.xmin + v * other.ymin;
    }

    // Adjust frustum to contain
    if( type == Projection::PERSPECTIVE ) {
        fovy = 0.0f;
        mFar = 0.0f;
        mNear = std::numeric_limits<float>::max();
        float maxHorizAngle = 0.0f;
        for( int i = 0; i < 8; i++) {
            // Convert to local space
            QVector4D pt = m * QVector4D(p[i],1.0f);

            if( pt.z() < 0.0f ) {
                float d = -pt.z();
                float angle = atanf( fabs(pt.x()) / d );
                if( angle > maxHorizAngle ) maxHorizAngle = angle;
                angle = qRadiansToDegrees( atanf( fabs(pt.y()) / d ) );
                if( angle * 2.0f > fovy ) fovy = angle * 2.0f;
                if( mNear > d ) mNear = d;
                if( mFar < d ) mFar = d;
            }
        }
        float h = ( mNear * tanf( qDegreesToRadians(fovy)/ 2.0f) ) * 2.0f;
        float w = ( mNear * tanf( maxHorizAngle ) ) * 2.0f;
        ar = w / h;
    } else {
        xmin = ymin = mNear = std::numeric_limits<float>::max();
        xmax = ymax = mFar = std::numeric_limits<float>::min();
        for( int i = 0; i < 8; i++) {
            // Convert to local space
            QVector4D pt = m * QVector4D(p[i],1.0f);
            if( xmin > pt.x() ) xmin = pt.x();
            if( xmax < pt.x() ) xmax = pt.x();
            if( ymin > pt.y() ) ymin = pt.y();
            if( ymax < pt.y() ) ymax = pt.y();
            if( mNear > -pt.z() ) mNear = -pt.z();
            if( mFar < -pt.z() ) mFar = -pt.z();
        }
    }

}

QMatrix4x4 Frustum::getViewMatrix() const
{
    QMatrix4x4 temp;

    temp.lookAt(origin, at, up );

    return temp;
}

QMatrix4x4 Frustum::getProjectionMatrix() const
{
    QMatrix4x4 temp;

    if( type == Projection::PERSPECTIVE )
        temp.perspective( qDegreesToRadians(fovy), ar, mNear, mFar );
    else
        temp.ortho(xmin, xmax, ymin, ymax, mNear, mFar);

    return temp;
}

QVector3D Frustum::getOrigin() const
{
    return this->origin;
}

QVector3D Frustum::getCenter() const
{
    float dist = (mNear + mFar) / 2.0f;
    QVector3D r = QVector3D(at - origin).normalized();

    return origin + (r * dist);
}

void Frustum::printInfo() const
{
    if( type == Projection::PERSPECTIVE ) {
        printf("Perspective:  fovy = %f  ar = %f  near = %f  far = %f\n",
               fovy, ar, mNear, mFar);
    } else {
        printf("Orthographic: x(%f,%f) y(%f,%f) near = %f far = %f\n",
               xmin, xmax, ymin, ymax, mNear, mFar);
    }
    printf("   Origin = (%f, %f, %f)  at = (%f, %f, %f) up = (%f, %f, %f)\n",
           origin.x(), origin.y(), origin.z(), at.x(), at.y(), at.z(), up.x(), up.y(), up.z());
}

/*
void Frustum::render() const
{
    if( type == Projection::PERSPECTIVE ) {
        static float vert[9 * 3];
        static QVector3D p[8];

        vert[0] = origin.x;
        vert[1] = origin.y;
        vert[2] = origin.z;

        QVector3D n = glm::normalize(this->origin - this->at);
        QVector3D u = glm::normalize(glm::cross(this->up, n));
        QVector3D v = glm::normalize(glm::cross(n, u));

        float dy = mNear * tanf( glm::radians(fovy) / 2.0f );
        float dx = ar * dy;
        QVector3D c = origin - n * mNear;  // Center of near plane
        p[0] = c + u * dx + v * dy;
        p[1] = c - u * dx + v * dy;
        p[2] = c - u * dx - v * dy;
        p[3] = c + u * dx - v * dy;
        dy = mFar * tanf( glm::radians(fovy) / 2.0f );
        dx = ar * dy;
        c = origin - n * mFar;      // Center of far plane
        p[4] = c + u * dx + v * dy;
        p[5] = c - u * dx + v * dy;
        p[6] = c - u * dx - v * dy;
        p[7] = c + u * dx - v * dy;

        int idx = 3;
        for( int i = 0; i < 8 ; i++ ) {
            vert[idx++] = p[i].x;
            vert[idx++] = p[i].y;
            vert[idx++] = p[i].z;
        }

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, handle[0]);
        glBufferData(GL_ARRAY_BUFFER, 9 * 3 * sizeof(float), vert, GL_DYNAMIC_DRAW);
        glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, 0 );
        glEnableVertexAttribArray(0);  // Vertex position

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, handle[1]);
        glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
    }
}
*/
