#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <structmember.h>

#include "glm/glm.hpp"

#define v_xyz(obj) &obj.x, &obj.y, &obj.z

float hill_slope(float x) {
    return cosf(x * 3.14159265f) * 0.5f + 0.5f;
}

float cliff_slope(float x) {
    return tanhf(9.0f - x * 4.0f * 3.14159265f) * 0.5f + 0.5f;
}

typedef float (* Slope)(float);

struct TerrainVertex {
    glm::vec3 vert;
    glm::vec3 norm;

    void normalize(float dx, float dy) {
        norm = glm::normalize(glm::vec3(dx, dy, 2.0f));
    }
};

struct Terrain {
    PyObject_HEAD
    int size_x;
    int size_y;
    float base_x;
    float base_y;
    float scale;
    TerrainVertex * data;
    PyObject * mem;

    glm::vec3 & vertex(int ix, int iy) {
        return data[iy * size_x + ix].vert;
    }

    glm::vec3 & normal(int ix, int iy) {
        return data[iy * size_x + ix].norm;
    }

    void normalize(int ix, int iy) {
        int ax = glm::clamp(ix - 1, 0, size_x - 1);
        int bx = glm::clamp(ix + 1, 0, size_x - 1);
        int ay = glm::clamp(iy - 1, 0, size_y - 1);
        int by = glm::clamp(iy + 1, 0, size_y - 1);
        float dx = data[iy * size_x + ax].vert.z - data[iy * size_x + bx].vert.z;
        float dy = data[ay * size_x + ix].vert.z - data[by * size_x + ix].vert.z;
        data[iy * size_x + ix].normalize(dx / scale / (bx - ax), dy / scale / (by - ay));
    }
};

PyTypeObject * Terrain_type;

Terrain * meth_terrain(PyObject * self, PyObject * args, PyObject * kwargs) {
    static char * keywords[] = {"size", "center", "scale", NULL};

    int size_x, size_y;
    float center_x = 0.0f;
    float center_y = 0.0f;
    float scale = 1.0f;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "(ii)|(ff)f", keywords, &size_x, &size_y, &center_x, &center_y, &scale)) {
        return 0;
    }

    Terrain * terrain = (Terrain *)PyObject_New(Terrain, Terrain_type);

    terrain->size_x = size_x;
    terrain->size_y = size_y;
    terrain->base_x = center_x - (size_x - 1) * scale / 2.0f;
    terrain->base_y = center_y - (size_y - 1) * scale / 2.0f;
    terrain->scale = scale;

    int size = sizeof(TerrainVertex) * size_x * size_y;

    terrain->data = (TerrainVertex *)malloc(size);
    terrain->mem = PyMemoryView_FromMemory((char *)terrain->data, size, PyBUF_WRITE);

    for (int iy = 0; iy < size_y; ++iy) {
        for (int ix = 0; ix < size_x; ++ix) {
            terrain->vertex(ix, iy) = glm::vec3(terrain->base_x + ix * scale, terrain->base_y + iy * scale, 0.0f);
            terrain->normal(ix, iy) = glm::vec3(0.0f, 0.0f, 1.0f);
        }
    }

    return terrain;
}

PyObject * Terrain_meth_build_index_buffer(Terrain * self, PyObject * args, PyObject * kwargs) {
    static char * keywords[] = {"start_x", "start_y", "size_x", "size_y", NULL};

    int start_x = 0;
    int start_y = 0;
    int size_x = self->size_x;
    int size_y = self->size_y;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|iiii", keywords, &start_x, &start_y, &size_x, &size_y)) {
        return 0;
    }

    int size = sizeof(int) * ((size_x * 2 + 1) * (size_y - 1) - 1);
    PyObject * res = PyBytes_FromStringAndSize(NULL, size);
    int * ptr = (int *)PyBytes_AS_STRING(res);

    for (int iy = start_y + 1; iy < start_y + size_y; ++iy) {
        for (int ix = start_x; ix < start_x + size_x; ++ix) {
            *ptr++ = iy * self->size_x + ix;
            *ptr++ = (iy - 1) * self->size_x + ix;
        }
        if (iy != start_y + size_y - 1) {
            *ptr++ = -1;
        }
    }

    return res;
}

PyObject * Terrain_meth_raycast(Terrain * self, PyObject * args, PyObject * kwargs) {
    static char * keywords[] = {"from", "to", NULL};

    glm::vec3 from;
    glm::vec3 to;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "(fff)(fff)", keywords, v_xyz(from), v_xyz(to))) {
        return 0;
    }

    int ix = (int)((from.x - self->base_x) / self->scale);
    int iy = (int)((from.y - self->base_y) / self->scale);
    const glm::vec3 d = from - to;

    while (ix >= 0 && iy >= 0 && ix < self->size_x - 1 && iy < self->size_y - 1) {
        const glm::vec3 a = self->vertex(ix, iy);
        const glm::vec3 b = self->vertex(ix + 1, iy + 1) - a;
        const glm::vec3 g = from - a;

        for (int t = 0; t < 2; ++t) {
            const glm::vec3 c = self->vertex(ix + t, iy + 1 - t) - a;

            const float det = glm::determinant(glm::mat3(b, c, d));
            const float n = glm::determinant(glm::mat3(g, c, d)) / det;
            const float m = glm::determinant(glm::mat3(b, g, d)) / det;
            const float k = glm::determinant(glm::mat3(b, c, g)) / det;

            if (n >= 0.0f && m >= 0.0f && n + m <= 1.0f && k >= 0.0f) {
                const glm::vec3 vert = from - d * k;
                const glm::vec3 norm = glm::normalize(t ? glm::cross(c, b) : glm::cross(b, c));
                return Py_BuildValue("(fff)(fff)", vert.x, vert.y, vert.z, norm.x, norm.y, norm.z);
            }
        }

        if (!d.x && !d.y) {
            Py_RETURN_NONE;
        }

        if ((d.x > 0 ? (a.x - from.x) : (a.x + b.x - from.x)) / d.x > (d.y > 0 ? (a.y - from.y) : (a.y + b.y - from.y)) / d.y) {
            ix += d.x > 0.0f ? -1 : 1;
        } else {
            iy += d.y > 0.0f ? -1 : 1;
        }
    }

    Py_RETURN_NONE;
}

PyObject * Terrain_meth_bump(Terrain * self, PyObject * args, PyObject * kwargs) {
    static char * keywords[] = {"x", "y", "radius", "height", "slope", "normalize", NULL};

    float x, y;
    float radius;
    float height;
    const char * slope_name;
    int normalize = true;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "ffff|sp", keywords, &x, &y, &radius, &height, &slope_name, &normalize)) {
        return 0;
    }

    Slope slope = NULL;

    if (!strcmp(slope_name, "cliff")) {
        slope = cliff_slope;
    }

    if (!strcmp(slope_name, "hill")) {
        slope = hill_slope;
    }

    if (!slope) {
        PyErr_Format(PyExc_Exception, "invalid slope");
        return 0;
    }

    float tx = x * (self->size_x - 1);
    float ty = y * (self->size_y - 1);
    float th = height * self->scale;
    float rad2 = radius * radius;

    int start_x = glm::clamp((int)(tx - radius), 0, self->size_x);
    int start_y = glm::clamp((int)(ty - radius), 0, self->size_y);
    int end_x = glm::clamp((int)(tx + radius) + 1, 0, self->size_x);
    int end_y = glm::clamp((int)(ty + radius) + 1, 0, self->size_y);

    for (int iy = start_y; iy < end_y; ++iy) {
        for (int ix = start_x; ix < end_x; ++ix) {
            float d2 = (tx - ix) * (tx - ix) + (ty - iy) * (ty - iy);
            if (d2 > rad2) {
                continue;
            }
            self->vertex(ix, iy).z += slope(sqrtf(d2) / radius) * th;
        }
    }

    if (normalize) {
        for (int iy = start_y; iy < end_y; ++iy) {
            for (int ix = start_x; ix < end_x; ++ix) {
                float d2 = (tx - ix) * (tx - ix) + (ty - iy) * (ty - iy);
                if (d2 > rad2) {
                    continue;
                }
                self->normalize(ix, iy);
            }
        }
    }

    Py_RETURN_NONE;
}

PyObject * Terrain_meth_normalize(Terrain * self) {
    for (int iy = 0; iy < self->size_y; ++iy) {
        for (int ix = 0; ix < self->size_x; ++ix) {
            self->normalize(ix, iy);
        }
    }
    Py_RETURN_NONE;
}

void Terrain_tp_dealloc(Terrain * self) {
    Py_XDECREF(self->mem);
    if (self->data) {
        free(self->data);
    }
    Py_TYPE(self)->tp_free(self);
}

PyMethodDef Terrain_methods[] = {
    {"build_index_buffer", (PyCFunction)Terrain_meth_build_index_buffer, METH_VARARGS | METH_KEYWORDS, 0},
    {"raycast", (PyCFunction)Terrain_meth_raycast, METH_VARARGS | METH_KEYWORDS, 0},
    {"bump", (PyCFunction)Terrain_meth_bump, METH_VARARGS | METH_KEYWORDS, 0},
    {"normalize", (PyCFunction)Terrain_meth_normalize, METH_NOARGS, 0},
    {0},
};

PyMemberDef Terrain_members[] = {
    {"mem", T_OBJECT_EX, offsetof(Terrain, mem), READONLY, 0},
    {0},
};

PyType_Slot Terrain_slots[] = {
    {Py_tp_methods, Terrain_methods},
    {Py_tp_members, Terrain_members},
    {Py_tp_dealloc, Terrain_tp_dealloc},
    {0},
};

PyType_Spec Terrain_spec = {"terrain.Terrain", sizeof(Terrain), 0, Py_TPFLAGS_DEFAULT, Terrain_slots};

PyMethodDef module_methods[] = {
    {"terrain", (PyCFunction)meth_terrain, METH_VARARGS | METH_KEYWORDS, 0},
    {0},
};

PyModuleDef module_def = {PyModuleDef_HEAD_INIT, "terrain", 0, -1, module_methods, 0, 0, 0, 0};

extern "C" PyObject * PyInit_terrain() {
    PyObject * module = PyModule_Create(&module_def);
    Terrain_type = (PyTypeObject *)PyType_FromSpec(&Terrain_spec);
    return module;
}
