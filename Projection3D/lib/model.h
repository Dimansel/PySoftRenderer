void calculate_face_normals(int **faces, double **vertices, double **face_normals, int faces_len, int vertices_len);
void calculate_vertex_normals(int **faces, double **vertices, double **face_normals, double **vertex_normals, int **adj_faces, int faces_len, int vertices_len, int adj_len);
int in_face_index(int *face, int i);