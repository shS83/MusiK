"""Biolume predator/firefly simulation. Positions use viewport-height units."""
import numpy as np


class Biolume:
    def __init__(self, aspect):
        self.rng = np.random.default_rng()
        angle = 2 * np.pi * (np.arange(8) + .3) / 8
        self.cells = np.column_stack((.34 * aspect * np.cos(angle), .36 * np.sin(angle),
                                      self.rng.uniform(.048, .078, 8), np.full(8, .13)))
        self.velocity = self.rng.uniform(-.08, .08, (8, 2))
        self.cooldown = np.zeros(8)
        self.colors = np.array([(.12,.47,1), (.55,.15,1), (.08,.72,.94), (.75,.18,.85)] * 2)
        self.flies = np.column_stack((self.edges(48, aspect), self.rng.uniform(0, 2*np.pi, 48), np.ones(48)))
        self.flight = self.rng.uniform(-.04, .04, (48, 2))
        self.owner = np.full(48, -1)
        self.age = np.zeros(48)
        self.glows = np.zeros((16, 4))
        self.time = 0.

    def edges(self, count, aspect):
        points = self.rng.uniform(-.55, .55, (count, 2)) * [aspect, 1.]
        side = self.rng.integers(4, size=count)
        points[side < 2, 0] = np.where(side[side < 2] == 0, -1, 1) * (.5*aspect+.07)
        points[side >= 2, 1] = np.where(side[side >= 2] == 2, -.57, .57)
        return points

    def glow(self, xy):
        slots = np.flatnonzero(self.glows[:, 3] <= 0)
        if len(slots):
            self.glows[slots[0]] = (*xy, 0, 1)

    def step(self, dt, aspect, bass, treble, pulse):
        dt = min(.035, dt)
        self.time += dt
        self.glows[:, 3] = np.maximum(0, self.glows[:, 3] - dt*.75)
        cells, flies = self.cells, self.flies
        free = self.owner < 0
        delta = flies[None, :, :2] - cells[:, None, :2]
        distance = np.linalg.norm(delta, axis=2)
        target = delta[np.arange(8), np.argmin(np.where(free, distance, np.inf), axis=1)]
        target /= np.maximum(1e-8, np.linalg.norm(target, axis=1, keepdims=True))
        wiggle = np.column_stack((np.sin(self.time*.91 + np.arange(8)*2.8),
                                  np.cos(self.time*.77 + np.arange(8)*1.9)))
        self.velocity += (target*(.14+.07*bass+.045*pulse)*free.any() + wiggle*.028-self.velocity)*dt*1.7
        cells[:, :2] += self.velocity*dt
        bounds = [max(.001, aspect*.5-.035), .46]
        self.velocity[np.abs(cells[:, :2]) > bounds] *= -1
        cells[:, :2] = np.clip(cells[:, :2], -np.array(bounds), bounds)
        cells[:, 3] = np.maximum(.06, cells[:, 3]-dt*.025)
        self.cooldown = np.maximum(0, self.cooldown-dt)
        delta = flies[None, :, :2] - cells[:, None, :2]
        distance = np.linalg.norm(delta, axis=2)
        avoid = (delta*((distance < .30)/(distance**2+.009))[:, :, None]).sum(axis=0)
        heading = np.column_stack((np.cos(flies[:, 2]), np.sin(flies[:, 2])))
        desired = heading*.04 + avoid*.005
        desired /= np.maximum(1e-8, np.linalg.norm(desired, axis=1, keepdims=True))
        self.flight += (desired*(.075+.035*treble)-self.flight)*dt*3
        flies[free, :2] += self.flight[free]*dt
        flies[free, 2] += dt*.7*np.sin(self.time*2+np.flatnonzero(free)*1.71)
        nearest = distance.argmin(axis=0)
        caught = free & (distance.min(axis=0) < cells[nearest, 2]*.75)
        for i in np.flatnonzero(caught):
            c = nearest[i]
            self.owner[i] = c
            self.age[i] = 0
            cells[c, 2:] = np.minimum([.155, 1.5], cells[c, 2:]+[.0019, .16])
            self.glow(flies[i, :2])
        captive = self.owner >= 0
        self.age[captive] += dt
        host = cells[self.owner[captive]]
        flies[captive, 2] += dt*(4+4*treble)
        angle = flies[captive, 2]
        radius = np.maximum(.004, host[:, 2]*(.65-.55*self.age[captive]))
        flies[captive, :2] = host[:, :2] + radius[:, None]*np.column_stack((np.cos(angle), np.sin(angle)))
        flies[captive, 3] = -1
        respawn = (captive & (self.age > 1.25)) | (free & (np.abs(flies[:, :2]) > [aspect*.5+.17,.67]).any(axis=1))
        flies[respawn, :2] = self.edges(respawn.sum(), aspect)
        flies[respawn, 3] = 1
        self.owner[respawn] = -1
        for a,b in zip(*np.triu_indices(8,1)):
            if self.cooldown[a] or self.cooldown[b] or np.linalg.norm(cells[a,:2]-cells[b,:2]) > .70*(cells[a,2]+cells[b,2]):
                continue
            big, small = (a,b) if cells[a,2]>=cells[b,2] else (b,a)
            areas = cells[[big,small],2]**2
            self.colors[big] = np.average(self.colors[[big,small]],axis=0,weights=areas)
            cells[big,2:] = min(.17,np.sqrt(areas@[1.,.65])), min(1.5,cells[big,3]+.7*cells[small,3])
            self.glow(cells[big,:2])
            cells[small] = (*self.edges(1,aspect)[0], self.rng.uniform(.045,.065), .14)
            self.cooldown[[big,small]] = [2.5,3.]
            self.owner[self.owner==small] = big

    def upload(self, program):
        for name, data in (("u_cells",self.cells), ("u_flies",self.flies),
                           ("u_cellColors",self.colors), ("u_glows",self.glows)):
            program[name].write(data.astype("f4").tobytes())

