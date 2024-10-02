from phi.torch.flow import *
from tqdm.notebook import trange
import matplotlib.pyplot as plt

##### Variables ############################
len_x = 10 # domain length
len_y = 10 # domain width

res_x = 50 # resolution grid x
res_y = 50 # resolution grid y

obstacles_ = {
    'box1': [(2, 5), (8, 9)], 
    #'chair': [(3, 4), (7.5, 8.5)],
}

ac_ = {
    'ac1': [(2, 3), (0, 1)],
}

t_ambient = 30
t_room = 25
t_ac = 20
inflow = vec(x=0, y=10)

viscosity = .001
conductivity = 1.

############################################
domain = Box(x=len_x, y=len_y)

acs = { 
    name: Box(x=ac_[name][0], y=ac_[name][1])
    for name in ac_
}

obstacles = {
    name: Box(x=obstacles_[name][0], y=obstacles_[name][1])
    for name in obstacles_
}
objects_dict = {**obstacles, **acs}

mesh = geom.build_mesh(domain, x=res_x, y=res_y, obstacles=objects_dict)

# @jit.compile
def step(v, p, t, dt=1.):
    v = advect.semi_lagrangian(v, v, dt)
    v, p = fluid.make_incompressible(v, solve=Solve(x0=p))

    t = advect.semi_lagrangian(t, v, dt)
    t = diffuse.implicit(t, conductivity, dt, correct_skew=False)
    return v, p, t

boundary_v_obstacles = {
    name: 0 for name in obstacles_
}
boundary_v_acs = {
    name: inflow for name in ac_
}
boundary_v = {
    'x': 0, 'y': 0, **boundary_v_obstacles, **boundary_v_acs
}
v0 = Field(mesh, tensor(vec(x=0, y=0)), boundary_v)

boundary_t_obstacles = {
    name: ZERO_GRADIENT for name in obstacles_
}
boundary_t_acs = {
    name: t_ac for name in ac_
}
boundary_t= {
    'x': t_ambient, 'y': t_ambient,
    **boundary_t_obstacles, **boundary_t_acs
}
t0 = Field(mesh, tensor(t_room), boundary_t)


v_sol, p_sol, t_sol = iterate(
    step, batch(time=50), v0, None, t0, dt=0.1, range=trange)

plot(t_sol, *obstacles, *acs, animate='time', overlay='args')
plt.show()
