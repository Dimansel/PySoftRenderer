typedef struct {
	double **vertices;
	int **faces;
	double **vertex_normals;
	double **face_normals;
	int vertices_len;
	int faces_len;
	double *pos;
	int *color;
	int shader;
} Model;

typedef struct {
	int width, height, fov;
	double near, far, aspect, h, w;
	double x, y, z;
	double yaw, pitch;
} Camera;

double to_radians(double angle);
void mult_quat(double *q1, double *q2);
void rotate(double *v, double yaw, double pitch);
double *project(Camera *cam, double *w);
void render(int *framebuffer, double *zbuffer, Camera *cam, Model *model, double *lightPos);
char is_outside(double *v, Camera *cam);
char point_in_triangle(double *pt, double *vt1, double *vt2, double *vt3);
double sign(double *p1, double *p2, double *p3);
double area(double *vt1, double *vt2, double *vt3);
double dot(double *v1, double *v2);
void normalize(double *vec);
int rgbToInt(double r, double g, double b);
int clrToInt(int *c);
double min(double a, double b);
double max(double a, double b);