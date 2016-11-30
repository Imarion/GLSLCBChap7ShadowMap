#ifndef TORUS_H
#define TORUS_H

#define PI 3.1415926536
#define TWOPI 2*PI

class Torus
{
private:
    int nFaces, rings, sides;

    // Vertices
    float *v;
    int nVerts;

    // Normals
    float *n;

    // Tex coords
    float *tex;

    // Elements
    unsigned int *el;

    void generateVerts(float * , float * ,float *, unsigned int *, float , float);

public:
    Torus(float, float, int, int);
    ~Torus();

    float *getv();
    int    getnVerts();
    float *getn();
    float *gettex();
    unsigned int *getel();

    int    getnFaces();
};

#endif // TORUS_H
