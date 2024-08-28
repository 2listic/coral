from phi.torch.flow import *
from tqdm.notebook import trange

domain = Box(x=10, y=10, z=10)
boundary_v = {
    'x-': 20, 'x+': 0,
    'y': 0,
    'z': 0}
v0 = StaggeredGrid(0, boundary_v, domain, x=50, y=50, z=50)

boundary_t= {'x-': 0, 'x+': 0, 'y-': 0, 'y+': 0, 'z': 0}
t0 = CenteredGrid(0, boundary_t, domain, x=50, y=50, z=50)

heat_source_location = Box(x=(1, 2), y=(2, 3), z=(2, 3))
heat_source = 20 * resample(heat_source_location, to=t0, soft=True)

# @jit_compile
def step(v, p, t, dt=1.):
    v = advect.semi_lagrangian(v, v, dt)
    v, p = fluid.make_incompressible(v, [], Solve(x0=p))

    t = advect.semi_lagrangian(t, v, dt) + heat_source
    #t = 100 * resample(heat_source, t) + advect.semi_lagrangian(t, v, dt)
    t = diffuse.implicit(t, 1., dt) 
    return v, p, t

v_sol, p_sol, t_sol = iterate(
    step, batch(time=40), v0, None, t0, dt=0.01, range=trange)

v_sol_xy = v_sol[{'z': 2, 'vector': 'x, y'}]
v_sol_xz = v_sol[{'y': 2, 'vector': 'x, z'}]
v_sol_yz = v_sol[{'x': 1, 'vector': 'y, z'}]

t_sol_xy = t_sol[{'z': 2}]
t_sol_xz = t_sol[{'y': 2}]
t_sol_yz = t_sol[{'x': 1}]

plot(
    [t_sol_xy, t_sol_xz, t_sol_yz],
    ['XY', 'XZ', 'YZ'], animate='time')
import matplotlib.pyplot as plt
plt.show()