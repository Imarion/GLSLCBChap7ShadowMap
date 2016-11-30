#include <QtMath>

#include "torus.h"

Torus::~Torus()
{
    delete[] v;
    delete[] n;
    delete[] el;
    delete[] tex;
}

Torus::Torus(float outerRadius, float innerRadius, int nsides, int nrings) :
        rings(nrings), sides(nsides)
{
    nFaces  = sides * rings;
    nVerts  = sides * (rings+1);   // One extra ring to duplicate first ring

    // Verts
    v = new float[3 * nVerts];
    // Normals
    n = new float[3 * nVerts];
    // Tex coords
    tex = new float[2 * nVerts];
    // Elements
    el = new unsigned int[6 * nFaces];

    // Generate the vertex data
    generateVerts(v, n, tex, el, outerRadius, innerRadius);
}

float *Torus::getv()
{
    return v;
}

int Torus::getnVerts()
{
    return nVerts;
}

float *Torus::getn()
{
    return n;
}

float *Torus::gettex()
{
    return tex;
}

unsigned int *Torus::getel()
{
    return el;
}

int Torus::getnFaces()
{
    return nFaces;
}


void Torus::generateVerts(float * verts, float * norms, float * tex,
                             unsigned int * el,
                             float outerRadius, float innerRadius)
{
    float ringFactor = TWOPI / rings;
    float sideFactor = TWOPI / sides;
    int idx = 0, tidx = 0;
    for( int ring = 0; ring <= rings; ring++ ) {
        float u = ring * ringFactor;
        float cu = qCos(u);
        float su = qSin(u);
        for( int side = 0; side < sides; side++ ) {
            float v = side * sideFactor;
            float cv = cos(v);
            float sv = sin(v);
            float r = (outerRadius + innerRadius * cv);
            verts[idx] = r * cu;
            verts[idx + 1] = r * su;
            verts[idx + 2] = innerRadius * sv;
            norms[idx] = cv * cu * r;
            norms[idx + 1] = cv * su * r;
            norms[idx + 2] = sv * r;
            tex[tidx] = u / TWOPI;
            tex[tidx + 1] = v / TWOPI;
            tidx += 2;
            // Normalize
            float len = qSqrt( norms[idx] * norms[idx] +
                              norms[idx+1] * norms[idx+1] +
                              norms[idx+2] * norms[idx+2] );
            norms[idx] /= len;
            norms[idx+1] /= len;
            norms[idx+2] /= len;
            idx += 3;
        }
    }

    idx = 0;
    for( int ring = 0; ring < rings; ring++ ) {
        int ringStart = ring * sides;
        int nextRingStart = (ring + 1) * sides;
        for( int side = 0; side < sides; side++ ) {
            int nextSide = (side+1) % sides;
            // The quad
            el[idx] = (ringStart + side);
            el[idx+1] = (nextRingStart + side);
            el[idx+2] = (nextRingStart + nextSide);
            el[idx+3] = ringStart + side;
            el[idx+4] = nextRingStart + nextSide;
            el[idx+5] = (ringStart + nextSide);
            idx += 6;
        }
    }
}
