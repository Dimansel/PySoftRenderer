from Projection3D.engine import Camera, loadObj
from tkinter import *
from PIL import ImageTk, Image
import numpy as np
import os, time

class Screen(Label):
    def __init__(self, width, height, model_path, shader, master = None):
        Label.__init__(self, master)
        self.WIDTH = width
        self.HEIGHT = height
        self.title = self.master.title()
        self.model_path = model_path
        self.shader = shader
        self.cx = self.WIDTH//2
        self.cy = self.HEIGHT//2
        self.delay = 15
        self.fake_move = False
        self.mouse_grabbed = False
        self.fps = 0
        self.last_fps = 0
        self.pressed_keys = []
        self.allowed_keys = [38, 39, 40, 25, 50, 65]    #A, S, D, W, Shift, Space
    
    def init_scene(self):
        self.zero = -256**3
        self.cam = Camera(self.WIDTH, self.HEIGHT, 70, 0, 100)
        self.lightPos = [0, 0, 0]
        self.framebuffer = np.zeros(self.WIDTH*self.HEIGHT, dtype = np.int32)
        self.img = Image.frombuffer('RGBA', (self.WIDTH, self.HEIGHT), self.framebuffer, 'raw', 'RGBA', 0, 1)
        self.model = loadObj(self.model_path)
        self.model.shader = self.shader

    def render_scene(self):
        self.framebuffer.fill(self.zero)
        self.model.render(self.framebuffer, self.cam, self.lightPos)
        self.photo = ImageTk.PhotoImage(self.img)
        self.config(image = self.photo)

    def tick(self):
        self.cam.process_keyboard(0.15, self.pressed_keys)
        self.lightPos = [self.cam.x, self.cam.y, self.cam.z]
        self.render_scene()
        self.grab_mouse()
        if self.mouse_grabbed:
            self.update_fps()
            self.after(self.delay, self.tick)

    def update_fps(self):
        if time.process_time() - self.last_fps > 1:
            self.master.title(self.title + '. FPS: {}'.format(self.fps));
            self.fps = 0;
            self.last_fps += 1;
        self.fps += 1

    def grab_mouse(self):
        self.fake_move = True
        self.master.event_generate('<Motion>', warp = True, x = self.WIDTH//2, y = self.HEIGHT//2)

    def mouse_move(self, event):
        if self.fake_move or not self.mouse_grabbed:
            self.fake_move = False
            return
        self.fake_move = False
        self.cam.process_mouse(event.x - self.cx, event.y - self.cy)

    def key_press(self, event):
        if event.char == 'g':
            self.mouse_grabbed = not self.mouse_grabbed
            if self.mouse_grabbed:
                os.system('xset r off')
                self.config(cursor = '@none.cur black')
                self.grab_mouse()
                self.tick()
            else:
                os.system('xset r on')
                self.pressed_keys = []
                self.config(cursor = '')
        elif self.mouse_grabbed and event.keycode in self.allowed_keys:
            self.pressed_keys.append(event.keycode)

    def key_release(self, event):
        if self.mouse_grabbed and event.keycode in self.pressed_keys:
            self.pressed_keys.remove(event.keycode)

if __name__ == '__main__':
    root = Tk()
    w = 1000
    h = 700
    sw = root.winfo_screenwidth()
    sh = root.winfo_screenheight()
    x = (sw/2) - (w/2)
    y = (sh/2) - (h/2)

    root.geometry('{}x{}+{}+{}'.format(w, h, int(x), int(y)))
    root.title('Model Viewer')
    root.resizable(width = False, height = False)

    screen = Screen(w, h, 'Models/wt_teapot.obj', 3, root)
    screen.pack()

    screen.init_scene()
    screen.model.position[2] = 5
    screen.tick()

    root.bind('<Motion>', screen.mouse_move)
    root.bind('<KeyPress>', screen.key_press)
    root.bind('<KeyRelease>', screen.key_release)
    root.mainloop()
