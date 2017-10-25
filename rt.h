#pragma once

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include <time.h>
#include <pthread.h>

#include "mlx/mlx.h"

#define UNIT_X (t_float3){1, 0, 0}
#define UNIT_Y (t_float3){0, 1, 0}
#define UNIT_Z (t_float3){0, 0, 1}

#define ERROR 1e-5

#define BLACK (t_float3){0.0, 0.0, 0.0}
#define RED (t_float3){1.0, 0.0, 0.0}
#define GREEN (t_float3){0.0, 1.0, 0.0}
#define BLUE (t_float3){0.0, 0.0, 1.0}
#define WHITE (t_float3){1.0, 1.0, 1.0}

enum type {SPHERE, PLANE, CYLINDER, TRIANGLE};

typedef struct s_float3
{
	float x;
	float y;
	float z;
}				t_float3;

typedef struct s_3x3
{
	t_float3 row1;
	t_float3 row2;
	t_float3 row3;
}				t_3x3;

typedef struct s_camera
{
	//keep this and turn it into t_camera
	t_float3 center;
	t_float3 normal;
	float width;
	float height;

	t_float3 focus;
	t_float3 origin;
	t_float3 d_x;
	t_float3 d_y;
}				t_camera;

typedef struct s_object
{
	enum type shape;
	t_float3 position;
	t_float3 normal;
	t_float3 corner;
	t_float3 color;
	float emission;
	struct s_object *next;
}				t_object;

typedef struct s_ray
{
	t_float3 origin;
	t_float3 direction;
	t_float3 inv_dir;
}				t_ray;

typedef struct s_box
{
	t_float3 max;
	t_float3 min;
	t_float3 mid;
	uint64_t morton;
	struct s_box *children;
	int children_count;
	t_object *object;
}				t_box;

typedef struct s_scene
{
	t_camera camera;
	t_object *objects;
	t_box *bvh;
	t_box *bvh_debug;
	int box_count;
}				t_scene;

typedef struct s_import
{
	t_object *head;
	t_object *tail;
	int obj_count;
	t_float3 min;
	t_float3 max;
}				t_import;

typedef struct s_halton
{
	float value;
	float inv_base;
}				t_halton;

int hit(t_ray *ray, const t_scene *scene, int do_shade);
void draw_pixels(void *img, int xres, int yres, t_float3 *pixels);

void print_vec(const t_float3 vec);
void print_3x3(const t_3x3 mat);
void print_ray(const t_ray ray);
void print_object(const t_object *object);

float vec_mag(const t_float3 vec);
t_float3 unit_vec(const t_float3 vec);
float dot(const t_float3 a, const t_float3 b);
t_float3 cross(const t_float3 a, const t_float3 b);
t_float3 vec_sub(const t_float3 a, const t_float3 b);
t_float3 vec_add(const t_float3 a, const t_float3 b);
t_float3 vec_scale(const t_float3 vec, const float scalar);
t_float3 mat_vec_mult(const t_3x3 mat, const t_float3 vec);
t_float3 angle_axis_rot(const float angle, const t_float3 axis, const t_float3 vec);
t_3x3 rotation_matrix(const t_float3 a, const t_float3 b);
t_float3 bounce(const t_float3 in, const t_float3 norm);
float dist(const t_float3 a, const t_float3 b);
t_float3 vec_rev(t_float3 v);
t_float3 vec_inv(const t_float3 v);
int vec_equ(const t_float3 a, const t_float3 b);
void orthonormal(t_float3 a, t_float3 *b, t_float3 *c);
t_float3 vec_had(const t_float3 a, const t_float3 b);

int intersect_object(const t_ray *ray, const t_object *object, float *d);
t_float3 norm_object(const t_object *object, const t_ray *ray);

void new_sphere(t_scene *scene, float x, float y, float z, float r, t_float3 color);
void new_plane(t_scene *scene, t_float3 center, t_float3 normal, t_float3 corner, t_float3 color);
void new_cylinder(t_scene *scene, t_float3 center, t_float3 radius, t_float3 extent, t_float3 color);
void new_triangle(t_scene *scene, t_float3 center, t_float3 normal, t_float3 corner, t_float3 color);


int intersect_triangle(const t_ray *ray, const t_object *triangle, float *d);

void make_bvh(t_scene *scene);
void hit_nearest(const t_ray *ray, const t_box *box, t_object **hit, float *d);
void hit_nearest_debug(const t_ray *ray, const t_box *box, t_object **hit, float *d);

t_import load_file(int ac, char **av);
void unit_scale(t_import import, t_float3 offset, float rescale);

float max3(float a, float b, float c);
float min3(float a, float b, float c);

t_float3 *simple_render(const t_scene *scene, const int xres, const int yres);
void init_camera(t_camera *camera, int xres, int yres);

float rand_unit();
t_float3 hemisphere(float u1, float u2);
t_float3 better_hemisphere(float u1, float u2);
float next_hal(t_halton *hal);
t_halton setup_halton(int i, int base);