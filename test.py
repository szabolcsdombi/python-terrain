import os

import numpy as np
from moderngl_window import WindowConfig, run_window_config
from pyrr import Matrix44

import moderngl
import terrain


class CrateExample(WindowConfig):
    title = "Crate"

    def __init__(self, **kwargs):
        super().__init__(**kwargs)

        self.prog = self.ctx.program(
            vertex_shader='''
                #version 330

                uniform mat4 Mvp;

                in vec3 in_vert;
                in vec3 in_norm;

                out vec3 v_vert;
                out vec3 v_norm;

                void main() {
                    gl_Position = Mvp * vec4(in_vert, 1.0);
                    v_vert = in_vert;
                    v_norm = normalize(in_norm);
                }
            ''',
            fragment_shader='''
                #version 330

                uniform vec3 Light;

                in vec3 v_vert;
                in vec3 v_norm;

                out vec4 f_color;

                void main() {
                    float lum = clamp(dot(normalize(Light - v_vert), v_norm), 0.0, 1.0) * 0.8 + 0.2;
                    f_color = vec4(lum, lum, lum, 1.0);
                }
            ''',
        )

        self.mvp = self.prog['Mvp']
        self.light = self.prog['Light']

        self.terrain = terrain.terrain(size=(100, 100), center=(0.0, 0.0), scale=0.1)
        # self.terrain = terrain.terrain(size=(100, 100), center=(0.0, 0.0), scale=1.0)
        # self.terrain = terrain.terrain(size=(4, 4), center=(0.0, 0.0), scale=2.0)
        for i in range(200):
            x, y = np.random.uniform(0.0, 1.0, 2)
            radius = np.random.uniform(15.0, 30.0)
            height = np.random.uniform(-0.1, 0.1) * radius
            slope = np.random.choice(['hill', 'cliff'])
            self.terrain.bump(x, y, radius, height, slope, normalize=False)
        self.terrain.normalize()

        self.vbo = self.ctx.buffer(self.terrain.mem)
        self.ibo = self.ctx.buffer(self.terrain.build_index_buffer())
        self.vao = self.ctx.simple_vertex_array(self.prog, self.vbo, 'in_vert', 'in_norm', index_buffer=self.ibo)

        line = np.array([
            [0.0, 0.0, 3.0],
            [2.0, 3.0, -4.0],
        ], 'f4')

        self.vbo1 = self.ctx.buffer(line.tobytes())
        self.vao1 = self.ctx.simple_vertex_array(self.prog, self.vbo1, 'in_vert')

        self.point = np.array([
            [0.0, 0.0, 0.0],
            [0.0, 0.0, 0.0],
        ], 'f4')

        vert, norm = np.array(self.terrain.raycast(line[0], line[1]))
        self.point[:] = [vert, vert + norm * 0.1]
        print(vert, norm)

        self.vbo2 = self.ctx.buffer(self.point.tobytes())
        self.vao2 = self.ctx.simple_vertex_array(self.prog, self.vbo2, 'in_vert')

    def render(self, time, frame_time):
        angle = time
        self.ctx.clear(1.0, 1.0, 1.0)
        self.ctx.enable(moderngl.DEPTH_TEST | moderngl.CULL_FACE)

        camera_pos = self.point[0] + (np.cos(angle) * 1.0, np.sin(angle) * 1.0, 1.0)

        proj = Matrix44.perspective_projection(45.0, self.aspect_ratio, 0.1, 1000.0)
        lookat = Matrix44.look_at(
            camera_pos,
            self.point[0],
            (0.0, 0.0, 1.0),
        )

        self.mvp.write((proj * lookat).astype('f4').tobytes())
        self.light.value = tuple(camera_pos)
        self.ctx.wireframe = True
        self.vao.render(moderngl.TRIANGLE_STRIP)
        self.vao1.render(moderngl.LINES)
        self.ctx.point_size = 5.0
        self.ctx.disable(moderngl.DEPTH_TEST | moderngl.CULL_FACE)
        self.vao2.render(moderngl.POINTS)



if __name__ == '__main__':
    run_window_config(CrateExample)
