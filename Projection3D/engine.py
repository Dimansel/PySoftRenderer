from PIL import Image
from math import sqrt, acos, sin, cos, tan, pi
from ctypes import Structure, c_int, c_double, c_uint64, POINTER
import numpy as np
import os, re, time, ctypes

TO_RAD = pi/180

LIB_PATH = os.path.join(os.path.dirname(__file__), 'lib/')
lib_model = ctypes.cdll.LoadLibrary(LIB_PATH + 'model.so')
lib_draw = ctypes.cdll.LoadLibrary(LIB_PATH + 'draw.so')

_pp = np.ctypeslib.ndpointer(dtype = np.uintp, ndim = 1, flags = 'C')
_p_d = np.ctypeslib.ndpointer(dtype = np.double, ndim = 1, flags = 'C_CONTIGUOUS')
_p_i = np.ctypeslib.ndpointer(dtype = np.int32, ndim = 1, flags = 'C_CONTIGUOUS')

def exec_time(s):
    def __(func):
        def _(*args, **kwargs):
            if __name__ == '__main__':
                t = time.time()
                result = func(*args, **kwargs)
                print(s, time.time() - t)
                return result
            else:
                return func(*args, **kwargs)
        return _
    return __

def double_pointer(arr):
    '''Returns an array of pointers to subarrays of 2d numpy array'''

    return (arr.ctypes.data + np.arange(arr.shape[0])*arr.strides[0]).astype(np.uintp)

class Camera(Structure):
    '''Describes a "camera" in 3d space. Stores necessary information'''

    _fields_ = [('width', c_int),
                ('height', c_int),
                ('fov', c_int),
                ('near', c_double),
                ('far', c_double),
                ('aspect', c_double),
                ('h', c_double),
                ('w', c_double),
                ('x', c_double),
                ('y', c_double),
                ('z', c_double),
                ('yaw', c_double),
                ('pitch', c_double)]

    def __init__(self, width, height, fov, near, far):
        aspect = width/height
        h = 1/tan(fov*pi/360)
        super().__init__(width, height, fov, near, far, aspect, h, aspect*h, 0, 0, 0, 0, 0)

    def process_mouse(self, dx, dy):
        dx *= 0.16
        dy *= 0.16

        if self.yaw + dx >= 360:
            self.yaw += dx - 360
        elif self.yaw + dx < 0:
            self.yaw = 360 - self.yaw + dx
        else:
            self.yaw += dx

        if self.pitch + dy < -90:
            self.pitch = -90;
        elif self.pitch + dy > 90:
            self.pitch = 90
        else:
            self.pitch += dy

    def process_keyboard(self, step, key_codes):
        key_up = 25 in key_codes
        key_down = 39 in key_codes
        key_left = 38 in key_codes
        key_right = 40 in key_codes
        fly_up = 65 in key_codes
        fly_down = 50 in key_codes

        yaw = self.yaw * TO_RAD
        sin_yaw = step * sin(yaw)
        cos_yaw = step * cos(yaw)

        if key_up and not key_down:
            self.x += sin_yaw
            self.z += cos_yaw
        elif key_down and not key_up:
            self.x -= sin_yaw
            self.z -= cos_yaw
        if key_left and not key_right:
            self.x -= cos_yaw
            self.z += sin_yaw
        elif key_right and not key_left:
            self.x += cos_yaw
            self.z -= sin_yaw
        if fly_up and not fly_down:
            self.y += step
        elif fly_down and not fly_up:
            self.y -= step

class Model(Structure):
    '''
    Describes a model in 3d space. Contains arrays of vertices and faces
    Also contains color, position and shader value (0 - no shading, 1 - flat shading, 2 - gouraud shading, 3 - phong shading)
    On init builds array of adjacent faces for each vertex, calculates face and vertex normals with C function
    '''

    _fields_ = [('_vertices', POINTER(c_uint64)),
                ('_faces', POINTER(c_uint64)),
                ('_vertex_normals', POINTER(c_uint64)),
                ('_face_normals', POINTER(c_uint64)),
                ('_vertices_len', c_int),
                ('_faces_len', c_int),
                ('_pos', POINTER(c_double)),
                ('_color', POINTER(c_int)),
                ('shader', c_int)]

    @exec_time('Model init:')
    def __init__(self, vertices, faces):
        self.vertices = vertices
        self.faces = faces
        self.color = np.array([255, 255, 255], dtype = np.int32)
        self.position = np.zeros(3, dtype = np.double)
        
        self._define_funcs()

        self.face_normals = np.empty((len(faces), 3))
        self.vertex_normals = np.empty((len(vertices), 3))
        
        #This is the double pointers to the necessary 2d arrays
        self.f_pp = double_pointer(faces)
        self.v_pp = double_pointer(vertices)
        self.fn_pp = double_pointer(self.face_normals)
        self.vn_pp = double_pointer(self.vertex_normals)

        faces_len = len(faces)
        vertices_len = len(vertices)

        self._fn_wrapper(self.f_pp, self.v_pp, self.fn_pp, faces_len, vertices_len)
        self._find_adjacent_faces()
        self.af_pp = double_pointer(self.adj_faces)
        self._vn_wrapper(self.f_pp, self.v_pp, self.fn_pp, self.vn_pp, self.af_pp, faces_len, vertices_len, self.adj_faces.shape[1])

        super().__init__(self.v_pp.ctypes.data_as(POINTER(c_uint64)),
                         self.f_pp.ctypes.data_as(POINTER(c_uint64)),
                         self.vn_pp.ctypes.data_as(POINTER(c_uint64)),
                         self.fn_pp.ctypes.data_as(POINTER(c_uint64)),
                         vertices_len, faces_len,
                         self.position.ctypes.data_as(POINTER(c_double)),
                         self.color.ctypes.data_as(POINTER(c_int)),
                         0)

    @exec_time('Adjacent faces:')
    def _find_adjacent_faces(self):
        '''Builds a 2d array of adjacent faces for each vertex'''

        _adj_faces = [[] for _ in range(len(self.vertices))]
        for i in range(len(self.faces)):
            face = self.faces[i]
            if face[6] == -1:
                continue
            _adj_faces[face[0]-1].append(i+1)
            _adj_faces[face[1]-1].append(i+1)
            _adj_faces[face[2]-1].append(i+1)
        self.adj_faces = np.zeros((len(self.vertices), len(max(_adj_faces, key = len))), dtype = np.int32)
        for i, j in enumerate(_adj_faces):
            self.adj_faces[i][0:len(j)] = j

    @exec_time('Face normals:')
    def _fn_wrapper(self, a, b, c, d, e):
        self.calc_face_normals(a, b, c, d, e)

    @exec_time('Vertex normals:')
    def _vn_wrapper(self, a, b, c, d, e, f, g, h):
        self.calc_vertex_normals(a, b, c, d, e, f, g, h)

    def _define_funcs(self):
        '''Defines necessary C functions'''

        self.calc_face_normals = lib_model.calculate_face_normals
        self.calc_vertex_normals = lib_model.calculate_vertex_normals
        self._render = lib_draw.render

        self.calc_face_normals.argtypes = [_pp, _pp, _pp, c_int, c_int]
        self.calc_vertex_normals.argtypes = [_pp, _pp, _pp, _pp, _pp, c_int, c_int, c_int]
        self._render.argtypes = [_p_i, _p_d, POINTER(Camera), POINTER(Model), _p_d]

    @exec_time('Render:')
    def render(self, framebuffer, cam, lightPos):
        zbuffer = np.full(framebuffer.shape, cam.far, dtype = np.double)
        lightPos = np.array(lightPos, dtype = np.double)
        self._render(framebuffer, zbuffer, cam, self, lightPos)

P = r'(v|f) (.+?) (.+?) (.+)'
@exec_time('Total:')
def loadObj(filename):
    '''Creates and returns Model instance with obtained data from .obj file'''

    try:
        vertices = []
        faces = []
        for line in open(filename):
            p = re.search(P, line)
            if p:
                t = p.group(1)
                n1 = p.group(2)
                n2 = p.group(3)
                n3 = p.group(4)
                if t == 'v':
                    vertices.append([float(n1), float(n2), float(n3)])
                elif t == 'f':
                    #face is a list of the indexes of vertices (first
                    #three indexes of this list) that face contains of,
                    #indexes of it's vertex normals (second three
                    #indexes) and index of the face normal (the last element)
                    faces.append([int(n1), int(n2), int(n3), -1, -1, -1, -1])
        return Model(np.array(vertices), np.array(faces, dtype=np.int32))
    except IOError as e:
        print(e)

def main():
    cam = Camera(1200, 700, 70, 0, 100)
    cam.x = -3.258363728212743
    cam.y = 1.6
    cam.z = 1.7317174530617903
    cam.yaw = 46.56
    cam.pitch = 24.96
    lightPos = [cam.x, cam.y, cam.z]
    framebuffer = np.full(1200*700, -256**3, dtype = np.int32)
    m = loadObj('../Models/lamborghini.obj')
    m.position[2] = 4
    m.shader = 3
    m.render(framebuffer, cam, lightPos)
    img = Image.frombuffer('RGBA', (1200, 700), framebuffer, 'raw', 'RGBA', 0, 1)
    img.save(open('/home/topology/img001.png', 'wb'), 'png')

if __name__ == '__main__':
    main()