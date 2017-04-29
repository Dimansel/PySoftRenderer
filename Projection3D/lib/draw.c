#include "draw.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#define PI 3.14159265358979323846
#define EPS 1e-13

double to_radians(double angle) {
    return angle*PI/180;
}

void mult_quat(double *q1, double *q2) {
    double a = q1[0]*q2[3] + q2[0]*q1[3] + q1[1]*q2[2] - q2[1]*q1[2];
    double b = q1[1]*q2[3] + q2[1]*q1[3] + q2[0]*q1[2] - q1[0]*q2[2];
    double c = q1[2]*q2[3] + q2[2]*q1[3] + q1[0]*q2[1] - q2[0]*q1[1];
    double d = -(q1[0]*q2[0] + q1[1]*q2[1] + q1[2]*q2[2] - q1[3]*q2[3]);
    q1[0] = a;
    q1[1] = b;
    q1[2] = c;
    q1[3] = d;
}

void rotate(double *v, double yaw, double pitch) {
    double q1[4] = {0, -sin(to_radians(yaw/2)), 0, cos(to_radians(yaw/2))};
    double r1[4] = {0, -q1[1], 0, q1[3]};
    double q2[4] = {-sin(to_radians(pitch/2)), 0, 0, cos(to_radians(pitch/2))};
    double r2[4] = {-q2[0], 0, 0, q2[3]};
    double qv[4] = {v[0], v[1], v[2], 0};
    mult_quat(q2, q1);
    mult_quat(q2, qv);
    mult_quat(q2, r1);
    mult_quat(q2, r2);
    v[0] = q2[0];
    v[1] = q2[1];
    v[2] = q2[2];
}

double *project(Camera *cam, double *w) {
    double v[3] = {w[0] - cam->x, w[1] - cam->y, w[2] - cam->z};
    rotate(v, cam->yaw, cam->pitch);

    if (v[2] <= cam->near || v[2] > cam->far) return NULL;
    double xn = cam->h * v[0] / v[2];
    double yn = cam->w * v[1] / v[2];

    double *vp = malloc(3 * sizeof(double));
    vp[0] = (1 + xn) * cam->width / 2;
    vp[1] = (1 - yn) * cam->height / 2;
    vp[2] = v[2];

    return vp;
}

void render(int *framebuffer, double *zbuffer, Camera *cam, Model *model, double *lightPos) {
    int i, color;
    int mcolor = clrToInt(model->color);

    //flat shading
    int shaded;

    //gouraud shading
    int shaded1[3];
    int shaded2[3];
    int shaded3[3];

    for (i = 0; i < model->faces_len; ++i) {
        int *f = model->faces[i];
        if (f[3] == -1 || f[4] == -1 || f[5] == -1 || f[6] == -1)
            continue;

        double *ww1 = model->vertices[f[0]-1];
        double *ww2 = model->vertices[f[1]-1];
        double *ww3 = model->vertices[f[2]-1];

        double w1[3] = {ww1[0]+model->pos[0], ww1[1]+model->pos[1], ww1[2]+model->pos[2]};
        double w2[3] = {ww2[0]+model->pos[0], ww2[1]+model->pos[1], ww2[2]+model->pos[2]};
        double w3[3] = {ww3[0]+model->pos[0], ww3[1]+model->pos[1], ww3[2]+model->pos[2]};

        double *v1 = project(cam, w1);
        double *v2 = project(cam, w2);
        double *v3 = project(cam, w3);

        if (!v1 || !v2 || !v3)
            continue;
        if (is_outside(v1, cam) && is_outside(v2, cam) && is_outside(v3, cam))
            continue;

        double det = (v2[0]-v1[0])*(v3[1]-v1[1])-(v3[0]-v1[0])*(v2[1]-v1[1]);
        if (det < 0)
            continue;

        double *vn1 = model->vertex_normals[f[3]-1];
        double *vn2 = model->vertex_normals[f[4]-1];
        double *vn3 = model->vertex_normals[f[5]-1];
        double *fn = model->face_normals[f[6]-1];

        //initializing shader
        if (model->shader == 1) {
            double face_center[3] = {(w1[0]+w2[0]+w3[0])/3, (w1[1]+w2[1]+w3[1])/3, (w1[2]+w2[2]+w3[2])/3};
            double light_vec[3] = {lightPos[0] - face_center[0], lightPos[1] - face_center[1], lightPos[2] - face_center[2]};
            normalize(light_vec);
            double cos = max(0, dot(fn, light_vec));
            shaded = rgbToInt(cos*model->color[0], cos*model->color[1], cos*model->color[2]);
        } else if (model->shader == 2) {
            double light_vec1[3] = {lightPos[0]-w1[0], lightPos[1]-w1[1], lightPos[2]-w1[2]};
            double light_vec2[3] = {lightPos[0]-w2[0], lightPos[1]-w2[1], lightPos[2]-w2[2]};
            double light_vec3[3] = {lightPos[0]-w3[0], lightPos[1]-w3[1], lightPos[2]-w3[2]};
            normalize(light_vec1);
            normalize(light_vec2);
            normalize(light_vec3);
            double cos1 = max(0, dot(vn1, light_vec1));
            double cos2 = max(0, dot(vn2, light_vec2));
            double cos3 = max(0, dot(vn3, light_vec3));
            int h;
            for (h = 0; h < 3; ++h) {
                shaded1[h] = (int)(cos1*model->color[h]);
                shaded2[h] = (int)(cos2*model->color[h]);
                shaded3[h] = (int)(cos3*model->color[h]);
            }
        }

        int xmin = (int)max(0, min(v1[0], min(v2[0], v3[0])));
        int xmax = (int)min(cam->width-1, max(v1[0], max(v2[0], v3[0])));
        int ymin = (int)max(0, min(v1[1], min(v2[1], v3[1])));
        int ymax = (int)min(cam->height-1, max(v1[1], max(v2[1], v3[1])));

        int y, x;
        //inefficient triangle rasterization algorithm
        for (y = ymin; y <= ymax; ++y) {
            for (x = xmin; x <= xmax; ++x) {
                double p[2] = {x, y};
                if (point_in_triangle(p, v1, v2, v3)) {
                    //calculating coefficients of barycentric interpolation
                    double a = area(v1, v2, v3);
                    double w_1 = area(v2, v3, p) / (a*v1[2]);
                    double w_2 = area(v3, v1, p) / (a*v2[2]);
                    double w_3 = area(v1, v2, p) / (a*v3[2]);

                    //interpolating Z coordinate
                    double z = 1/(w_1 + w_2 + w_3);

                    if (z < zbuffer[x+y*cam->width]) {
                        zbuffer[x+y*cam->width] = z;

                        //applying shader
                        if (model->shader == 0) {
                            color = mcolor;
                        } else if (model->shader == 1) {
                            color = shaded;
                        } else if (model->shader == 2) {
                            //interpolating color for an arbitrary point of current triangle
                            //using 3 calculated colors of triangle's vertices
                            double g1 = shaded1[0]*w_1+shaded2[0]*w_2+shaded3[0]*w_3;
                            double g2 = shaded1[1]*w_1+shaded2[1]*w_2+shaded3[1]*w_3;
                            double g3 = shaded1[2]*w_1+shaded2[2]*w_2+shaded3[2]*w_3;
                            color = rgbToInt(z*g1, z*g2, z*g3);
                        } else if (model->shader == 3) {
                            //interpolating vertex normal for an arbitrary point of current triangle
                            //using 3 calculated normals of triangle's vertices
                            double g1 = vn1[0]*w_1+vn2[0]*w_2+vn3[0]*w_3;
                            double g2 = vn1[1]*w_1+vn2[1]*w_2+vn3[1]*w_3;
                            double g3 = vn1[2]*w_1+vn2[2]*w_2+vn3[2]*w_3;
                            double iV[3] = {z*g1, z*g2, z*g3};

                            //interpolating point in space to get light vector
                            double point[3] = {(w1[0]*w_1+w2[0]*w_2+w3[0]*w_3)*z, (w1[1]*w_1+w2[1]*w_2+w3[1]*w_3)*z, (w1[2]*w_1+w2[2]*w_2+w3[2]*w_3)*z};
                            normalize(iV);
                            double light_vec[3] = {lightPos[0]-point[0], lightPos[1]-point[1], lightPos[2]-point[2]};
                            normalize(light_vec);
                            
                            double cos = max(0, dot(iV, light_vec));
                            color = rgbToInt((int)(cos*model->color[0]), (int)(cos*model->color[1]), (int)(cos*model->color[2]));
                        }

                        framebuffer[x+y*cam->width] = color;
                    }
                }
            }
        }

        free(v1);
        free(v2);
        free(v3);
    }
}

char is_outside(double *v, Camera *cam) {
    return (v[0] < 0 || v[0] >= cam->width || v[1] < 0 || v[1] >= cam->height);
}

char point_in_triangle(double *pt, double *vt1, double *vt2, double *vt3) {
    char b1, b2, b3;

    b1 = sign(pt, vt1, vt2) < 0;
    b2 = sign(pt, vt2, vt3) < 0;
    b3 = sign(pt, vt3, vt1) < 0;

    return (b1 == b2) && (b2 == b3);
}

double sign(double *p1, double *p2, double *p3) {
    return (p1[0] - p3[0]) * (p2[1] - p3[1]) - (p2[0] - p3[0]) * (p1[1] - p3[1]);
}

double area(double *vt1, double *vt2, double *vt3) {
    return (vt3[0] - vt1[0]) * (vt2[1] - vt1[1]) - (vt3[1] - vt1[1]) * (vt2[0] - vt1[0]);
}

double dot(double *v1, double *v2) {
    return v1[0]*v2[0] + v1[1]*v2[1] + v1[2]*v2[2];
}

void normalize(double *vec) {
    double len = sqrt(vec[0]*vec[0] + vec[1]*vec[1] + vec[2]*vec[2]);
    if (len < EPS)
        return;
    int k;
    for (k = 0; k < 3; ++k)
        vec[k] /= len;
}

int rgbToInt(double r, double g, double b) {
    return ((255 * 256 + (int)b) * 256 + (int)g) * 256 + (int)r;
}

int clrToInt(int *c) {
    return ((255 * 256 + c[2]) * 256 + c[1]) * 256 + c[0];
}

double min(double a, double b) {
    if (a < b)
        return a;
    return b;
}

double max(double a, double b) {
    if (a > b)
        return a;
    return b;
}