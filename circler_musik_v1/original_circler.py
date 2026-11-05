from array import array
import math

import moderngl
import pygame as pg

WIDTH = 1280
HEIGHT = 720
SPAWN_INTERVAL = 0.3  # Sekunteja uusien tetraedrien välillä.
HUE_STEP = 12.0  # Sävyn muutos asteina per tetraedri.
COLOR_CYCLES_PER_ORBIT = 1  # Sateenkaaren kierroksia yhtä spiraalin kierrosta kohti.

VERTEX_SHADER = """
#version 330
in vec3 in_position;
in vec3 in_normal;
uniform vec2 viewport_size;
uniform vec2 center;
uniform float size;
uniform vec3 rotation;
out vec3 normal;

void main() {
    vec3 c = cos(rotation);
    vec3 s = sin(rotation);
    mat3 rx = mat3(1, 0, 0, 0, c.x, s.x, 0, -s.x, c.x);
    mat3 ry = mat3(c.y, 0, -s.y, 0, 1, 0, s.y, 0, c.y);
    mat3 rz = mat3(c.z, s.z, 0, -s.z, c.z, 0, 0, 0, 1);
    mat3 model_rotation = rz * ry * rx;
    vec3 position = model_rotation * in_position * size;
    normal = model_rotation * in_normal;

    // Ortografinen projektio säilyttää spiraalin mitat pikseleinä.
    vec2 pixel = center + vec2(position.x, -position.y);
    gl_Position = vec4(
        2.0 * pixel.x / viewport_size.x - 1.0,
        1.0 - 2.0 * pixel.y / viewport_size.y,
        -position.z / 1000.0,
        1.0
    );
}
"""

FRAGMENT_SHADER = """
#version 330
in vec3 normal;
uniform vec3 color;
out vec4 frag_color;

void main() {
    vec3 n = normalize(normal);
    vec3 light = normalize(vec3(-0.5, 0.8, 1.0));
    float diffuse = max(dot(n, light), 0.0);
    vec3 halfway = normalize(light + vec3(0.0, 0.0, 1.0));
    float specular = pow(max(dot(n, halfway), 0.0), 32.0);
    frag_color = vec4(color * (0.25 + 0.75 * diffuse) + 0.2 * specular, 1.0);
}
"""


def tetrahedron_vertices():
    # Neljä yksikköpallolla olevaa kärkeä, kaikki särmät yhtä pitkiä.
    vertices = [
        pg.Vector3(1, 1, 1).normalize(),
        pg.Vector3(1, -1, -1).normalize(),
        pg.Vector3(-1, 1, -1).normalize(),
        pg.Vector3(-1, -1, 1).normalize(),
    ]
    data = array("f")
    for indices in ((0, 1, 2), (0, 3, 1), (0, 2, 3), (1, 3, 2)):
        a, b, c = (vertices[i] for i in indices)
        normal = (b - a).cross(c - a).normalize()
        if normal.dot(a) < 0:
            b, c = c, b
            normal = -normal
        # Jokaisella tahkolla on oma normaali: reunat pysyvät terävinä.
        for vertex in (a, b, c):
            data.extend(vertex)
            data.extend(normal)
    return data.tobytes()


class Tetrahedron:
    def __init__(self, hue=0.0):
        self.start_hue = hue % 360
        self.color = pg.Color(255, 255, 255)
        self.color.hsva = (self.start_hue, 100, 100, 100)
        self.size = 1.0  # Keskipisteen etäisyys kärkeen pikseleinä.
        self.max_size = 50.0
        self.size_speed = 6.0
        self.rotation = 0.0  # Radiaaneja; shader pyörittää kaikkia kolmea akselia.
        self.rotation_speed = math.radians(60.0)
        self.radius = 0.0
        self.angle = 0.0
        self.angular_speed = 2.0
        self.growth_speed = 20.0

    def update(self, dt):
        self.size = min(self.max_size, self.size + self.size_speed * dt)
        self.rotation = (self.rotation + self.rotation_speed * dt) % (20 * math.tau)
        self.angle += self.angular_speed * dt
        self.radius += self.growth_speed * dt
        hue = (self.start_hue + math.degrees(self.angle) * COLOR_CYCLES_PER_ORBIT) % 360
        self.color.hsva = (hue, 100, 100, 100)

    def position(self, width, height):
        return (
            width / 2 + math.cos(self.angle) * self.radius,
            height / 2 + math.sin(self.angle) * self.radius,
        )

    def is_visible(self, width, height):
        x, y = self.position(width, height)
        return -self.size < x < width + self.size and -self.size < y < height + self.size


class Renderer:
    def __init__(self, ctx):
        self.ctx = ctx
        self.program = ctx.program(vertex_shader=VERTEX_SHADER, fragment_shader=FRAGMENT_SHADER)
        self.vertices = ctx.buffer(tetrahedron_vertices())
        self.vao = ctx.vertex_array(
            self.program, [(self.vertices, "3f 3f", "in_position", "in_normal")]
        )
        ctx.enable(moderngl.DEPTH_TEST | moderngl.CULL_FACE)

    def draw(self, tetrahedra, width, height):
        self.ctx.viewport = (0, 0, width, height)
        self.ctx.clear(0.0, 0.0, 0.0, 1.0, depth=1.0)
        self.program["viewport_size"].value = (width, height)
        for tetrahedron in tetrahedra:
            self.program["center"].value = tetrahedron.position(width, height)
            self.program["size"].value = tetrahedron.size
            angle = tetrahedron.rotation
            self.program["rotation"].value = (0.55 * angle + 0.4, 0.7 * angle + 0.3, angle)
            self.program["color"].value = tuple(c / 255 for c in tetrahedron.color[:3])
            self.vao.render(moderngl.TRIANGLES)

    def release(self):
        self.vao.release()
        self.vertices.release()
        self.program.release()


def create_window():
    pg.display.gl_set_attribute(pg.GL_CONTEXT_MAJOR_VERSION, 3)
    pg.display.gl_set_attribute(pg.GL_CONTEXT_MINOR_VERSION, 3)
    pg.display.gl_set_attribute(pg.GL_CONTEXT_PROFILE_MASK, pg.GL_CONTEXT_PROFILE_CORE)
    pg.display.gl_set_attribute(pg.GL_DEPTH_SIZE, 24)
    flags = pg.OPENGL | pg.DOUBLEBUF | pg.RESIZABLE
    # Nelinkertainen reunojen pehmennys, jos näytönohjain tukee sitä.
    pg.display.gl_set_attribute(pg.GL_MULTISAMPLEBUFFERS, 1)
    pg.display.gl_set_attribute(pg.GL_MULTISAMPLESAMPLES, 4)
    try:
        pg.display.set_mode((WIDTH, HEIGHT), flags)
    except pg.error:
        pg.display.gl_set_attribute(pg.GL_MULTISAMPLEBUFFERS, 0)
        pg.display.gl_set_attribute(pg.GL_MULTISAMPLESAMPLES, 0)
        pg.display.set_mode((WIDTH, HEIGHT), flags)
    pg.display.set_caption("Circler — Tetrahedra")


def main():
    pg.init()
    ctx = None
    renderer = None
    try:
        create_window()
        ctx = moderngl.create_context(require=330)
        renderer = Renderer(ctx)
        clock = pg.time.Clock()
        spawn_timer = 0.0
        hue = 0.0
        tetrahedra = [Tetrahedron(hue)]
        running = True

        while running:
            dt = min(clock.tick(60) / 1000, 0.1)
            for event in pg.event.get():
                if event.type == pg.QUIT:
                    running = False
                if event.type == pg.KEYDOWN and event.key == pg.K_ESCAPE:
                    running = False
            if not running:
                break

            # Pygame päivittää OpenGL-ikkunan koon automaattisesti.
            width, height = pg.display.get_window_size()
            if width <= 0 or height <= 0:
                continue
            for tetrahedron in tetrahedra:
                tetrahedron.update(dt)
            tetrahedra = [t for t in tetrahedra if t.is_visible(width, height)]
            spawn_timer += dt
            if spawn_timer >= SPAWN_INTERVAL:
                spawn_timer %= SPAWN_INTERVAL
                hue = (hue + HUE_STEP) % 360
                tetrahedra.append(Tetrahedron(hue))

            renderer.draw(tetrahedra, width, height)
            pg.display.flip()
    finally:
        if renderer is not None:
            renderer.release()
        if ctx is not None:
            ctx.release()
        pg.quit()


if __name__ == "__main__":
    main()
