#include "model.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#define EPS 1e-13

void calculate_face_normals(int **faces, double **vertices, double **face_normals, int faces_len, int vertices_len) {
	int a;
	for (a = 0; a < faces_len; ++a) {
		int *f = faces[a];
		double *w1 = vertices[f[0]-1];
		double *w2 = vertices[f[1]-1];
		double *w3 = vertices[f[2]-1];

		double vec1[3] = {w2[0]-w1[0], w2[1]-w1[1], w2[2]-w1[2]};
		double vec2[3] = {w3[0]-w1[0], w3[1]-w1[1], w3[2]-w1[2]};

		double normal[3];
		normal[0] = vec1[1]*vec2[2]-vec1[2]*vec2[1];
		normal[1] = vec1[2]*vec2[0]-vec1[0]*vec2[2];
		normal[2] = vec1[0]*vec2[1]-vec1[1]*vec2[0];
		double len = sqrt(normal[0]*normal[0] + normal[1]*normal[1] + normal[2]*normal[2]);

		if (len >= EPS) {
			int k;
			for (k = 0; k < 3; ++k)
				face_normals[a][k] = normal[k]/len;
			f[6] = a+1;
		}
	}
}

void calculate_vertex_normals(int **faces, double **vertices, double **face_normals, double **vertex_normals, int **adj_faces, int faces_len, int vertices_len, int adj_len) {
	int a, k;
	for (a = 0; a < vertices_len; ++a) {
		double vNormal[3] = {0, 0, 0};

		int *adj_f = adj_faces[a];
		for (k = 0; k < adj_len; ++k) {
			int fi = adj_f[k];
			if (fi == 0)
				break;

			int *f = faces[fi-1];
			int vi = in_face_index(f, a+1);

			f[3+vi] = a+1;
			double *w1 = vertices[f[vi]-1];
			double *w2 = vertices[f[(vi+1)%3]-1];
			double *w3 = vertices[f[(vi+2)%3]-1];

			double vec1[3] = {w2[0]-w1[0], w2[1]-w1[1], w2[2]-w1[2]};
			double vec2[3] = {w3[0]-w1[0], w3[1]-w1[1], w3[2]-w1[2]};
			double len1 = sqrt(vec1[0]*vec1[0] + vec1[1]*vec1[1] + vec1[2]*vec1[2]);
			double len2 = sqrt(vec2[0]*vec2[0] + vec2[1]*vec2[1] + vec2[2]*vec2[2]);

			int l;
			for (l = 0; l < 3; ++l) {
				vec1[l] /= len1;
				vec2[l] /= len2;
			}
			double angle = acos(vec1[0]*vec2[0] + vec1[1]*vec2[1] + vec1[2]*vec2[2]);
			for (l = 0; l < 3; ++l) {
				vNormal[l] += face_normals[f[6]-1][l]*angle;
			}
		}
		
		double len = sqrt(vNormal[0]*vNormal[0] + vNormal[1]*vNormal[1] + vNormal[2]*vNormal[2]);
		if (len >= EPS)
			for (k = 0; k < 3; ++k)
				vertex_normals[a][k] = vNormal[k]/len;
	}
}

int in_face_index(int *face, int i) {
	int k;
	for (k = 0; k < 3; ++k)
		if (face[k] == i)
			return k;
	return -1;
}