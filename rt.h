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
#include <OpenCL/cl.h>

#include "mlx/mlx.h"

#define xdim 512
#define ydim 512

#define UNIT_X (cl_float3){1, 0, 0}
#define UNIT_Y (cl_float3){0, 1, 0}
#define UNIT_Z (cl_float3){0, 0, 1}

#define ERROR 1e-5

#define BLACK (cl_float3){0.0, 0.0, 0.0}
#define RED (cl_float3){1.0, 0.2, 0.2}
#define GREEN (cl_float3){0.2, 1.0, 0.2}
#define BLUE (cl_float3){0.2, 0.2, 1.0}
#define WHITE (cl_float3){1.0, 1.0, 1.0}


#define GPU_MAT_DIFFUSE 1
#define GPU_MAT_SPECULAR 2
#define GPU_MAT_REFRACTIVE 3
#define GPU_MAT_NULL 4

#define GPU_SPHERE 1
#define GPU_TRIANGLE 3
#define GPU_QUAD 4

enum type {SPHERE, PLANE, CYLINDER, TRIANGLE};
enum mat {MAT_DIFFUSE, MAT_SPECULAR, MAT_REFRACTIVE, MAT_NULL};

typedef struct s_3x3
{
	cl_float3 row1;
	cl_float3 row2;
	cl_float3 row3;
}				t_3x3;

typedef struct s_camera
{
	//keep this and turn it into t_camera
	cl_float3 center;
	cl_float3 normal;
	float width;
	float height;

	cl_float3 focus;
	cl_float3 origin;
	cl_float3 d_x;
	cl_float3 d_y;
}				t_camera;

typedef struct s_object
{
	enum type shape;
	enum mat material;
	cl_float3 position;
	cl_float3 normal;
	cl_float3 corner;
	cl_float3 color;
	float emission;
	struct s_object *next;
}				t_object;

typedef struct s_ray
{
	cl_float3 origin;
	cl_float3 direction;
	cl_float3 inv_dir;
}				t_ray;

typedef struct s_box
{
	cl_float3 max;
	cl_float3 min;
	cl_float3 mid;
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
	cl_float3 min;
	cl_float3 max;
}				t_import;

typedef struct s_halton
{
	double value;
	double inv_base;
}				t_halton;

int hit(t_ray *ray, const t_scene *scene, int do_shade);
void draw_pixels(void *img, int xres, int yres, cl_float3 *pixels);

void print_vec(const cl_float3 vec);
void print_3x3(const t_3x3 mat);
void print_ray(const t_ray ray);
void print_object(const t_object *object);

float vec_mag(const cl_float3 vec);
cl_float3 unit_vec(const cl_float3 vec);
float dot(const cl_float3 a, const cl_float3 b);
cl_float3 cross(const cl_float3 a, const cl_float3 b);
cl_float3 vec_sub(const cl_float3 a, const cl_float3 b);
cl_float3 vec_add(const cl_float3 a, const cl_float3 b);
cl_float3 vec_scale(const cl_float3 vec, const float scalar);
cl_float3 mat_vec_mult(const t_3x3 mat, const cl_float3 vec);
cl_float3 angle_axis_rot(const float angle, const cl_float3 axis, const cl_float3 vec);
t_3x3 rotation_matrix(const cl_float3 a, const cl_float3 b);
cl_float3 bounce(const cl_float3 in, const cl_float3 norm);
float dist(const cl_float3 a, const cl_float3 b);
cl_float3 vec_rev(cl_float3 v);
cl_float3 vec_inv(const cl_float3 v);
int vec_equ(const cl_float3 a, const cl_float3 b);
void orthonormal(cl_float3 a, cl_float3 *b, cl_float3 *c);
cl_float3 vec_had(const cl_float3 a, const cl_float3 b);

int intersect_object(const t_ray *ray, const t_object *object, float *d);
cl_float3 norm_object(const t_object *object, const t_ray *ray);

void new_sphere(t_scene *scene, cl_float3 center, float r, cl_float3 color, enum mat material, float emission);
void new_plane(t_scene *scene, cl_float3 corner_A, cl_float3 corner_B, cl_float3 corner_C, cl_float3 color, enum mat material, float emission);
void new_cylinder(t_scene *scene, cl_float3 center, cl_float3 radius, cl_float3 extent, cl_float3 color);
void new_triangle(t_scene *scene, cl_float3 vertex0, cl_float3 vertex1, cl_float3 vertex2, cl_float3 color, enum mat material, float emission);

int intersect_triangle(const t_ray *ray, const t_object *triangle, float *d);

void make_bvh(t_scene *scene);
void hit_nearest(const t_ray *ray, const t_box *box, t_object **hit, float *d);
void hit_nearest_faster(const t_ray * const ray, t_box const * const box, t_object **hit, float *d);

t_import load_file(int ac, char **av, cl_float3 color, enum mat material, float emission);
void unit_scale(t_import import, cl_float3 offset, float rescale);

float max3(float a, float b, float c);
float min3(float a, float b, float c);

cl_float3 *simple_render(const t_scene *scene, const int xres, const int yres);
cl_float3 *mt_render(t_scene const *scene, const int xres, const int yres);
void init_camera(t_camera *camera, int xres, int yres);

float rand_unit(void);
cl_float3 hemisphere(float u1, float u2);
cl_float3 better_hemisphere(float u1, float u2);
double next_hal(t_halton *hal);
t_halton setup_halton(int i, int base);
float rand_unit_sin(void);


t_ray ray_from_camera(t_camera cam, float x, float y);






///New Obj Import///

typedef struct s_map
{
	int height;
	int width;
	int *pixels;
}				Map;

typedef struct s_material
{
	char *friendly_name;
	float Ns; //specular exponent
	float Ni; //index of refraction
	float d; //opacity
	float Tr; //transparency (1 - d)
	cl_float3 Tf; //transmission filter (ie color)
	int illum; //flag for illumination model, raster only
	cl_float3 Ka; //ambient mask
	cl_float3 Kd; //diffuse mask
	cl_float3 Ks; //specular mask
	cl_float3 Ke; //emission mask

	char *map_Ka_path;
	Map *map_Ka;

	char *map_Kd_path;
	Map *map_Kd;

	char *map_bump_path;
	Map *map_bump;

	char *map_d_path;
	Map *map_d;
}				Material;

typedef struct s_face
{
	cl_int shape;
	cl_int mat_type;
	cl_int mat_ind;
	cl_int smoothing;
	cl_float3 verts[4];
	cl_float3 norms[4];
	cl_float3 tex[4];
	cl_float3 N;
}				Face;

typedef struct compact_box
{
	cl_float3 min;
	cl_float3 max;
	cl_int key;
}				C_Box;

typedef struct s_Box
{
	cl_float3 min; //spatial bounds of box
	cl_float3 max; 
	cl_int start; //indices in Faces[] that this box contains
	cl_int end;
	cl_int children[8];
	cl_int children_count;
}				Box;

typedef struct s_new_scene
{
	Material *materials;
	int mat_count;
	Face *faces;
	int face_count;
	Box *boxes;
	int box_count;
	C_Box *c_boxes;
	int c_box_count;
}				Scene;

Scene *scene_from_obj(char *rel_path, char *filename);


cl_float3 *gpu_render(Scene *scene, t_camera cam);
void gpu_bvh(Scene *S);
Box *bvh_obj(Face *Faces, int start, int end, int *boxcount);
void gpu_ready_bvh(Scene *S, int *counts, int obj_count);
void print_clf3(cl_float3 v);