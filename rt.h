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
#include "qdbmp/qdbmp.h"

#ifdef __APPLE__
# include <OpenCL/cl.h>
# include "mac-mlx/mlx.h"
#endif

#ifndef __APPLE__
# include <CL/cl.h>
# include "linux-mlx/mlx.h"
#endif

#define UNIT_X (cl_float3){1, 0, 0}
#define UNIT_Y (cl_float3){0, 1, 0}
#define UNIT_Z (cl_float3){0, 0, 1}

#define DIM_IA		400
#define DIM_PT		1024

#define H_FOV M_PI_2 * 60.0 / 90.0

#define ERROR 1e-4

#define FREE 1

#define IA 0
#define PT 1

#define BLACK (cl_float3){0.0, 0.0, 0.0}
#define RED (cl_float3){1.0, 0.2, 0.2}
#define GREEN (cl_float3){0.2, 1.0, 0.2}
#define BLUE (cl_float3){0.2, 0.2, 1.0}
#define WHITE (cl_float3){1.0, 1.0, 1.0}

#define ORIGIN (cl_float3){0.0f, 0.0f, 0.0f}

#define GPU_MAT_DIFFUSE 1
#define GPU_MAT_SPECULAR 2
#define GPU_MAT_REFRACTIVE 3
#define GPU_MAT_NULL 4

#define GPU_SPHERE 1
#define GPU_TRIANGLE 3
#define GPU_QUAD 4

#define MOVE_SPEED	10
#define TURN_SPEED	M_PI / 30

enum type {SPHERE, PLANE, CYLINDER, TRIANGLE};
enum mat {MAT_DIFFUSE, MAT_SPECULAR, MAT_REFRACTIVE, MAT_NULL};

typedef struct s_property
{
	char name[512];
	uint32_t n;
	int	n_datatype;
	void *data;
	int datatype;
}				Property;

typedef struct s_elements
{
	char name[512];
	int total;
	int	property_total;
	Property *props;
}				Elements;

typedef struct s_file_info
{
	FILE *ptr;
	char name[512];
	int header_size;
	int	verts_size;
	int	faces_size;
}				File_info;

enum axis{
	X_AXIS,
	Y_AXIS,
	Z_AXIS
};

typedef struct s_3x3
{
	cl_float3 row1;
	cl_float3 row2;
	cl_float3 row3;
}				t_3x3;

typedef struct s_map
{
	int height;
	int width;
	cl_uchar *pixels;
}				Map;

typedef struct s_material
{
	char *friendly_name;
	cl_float3 Ns; //specular exponent
	float Ni; //index of refraction
	float d; //opacity
	float Tr; //transparency (1 - d)
	float roughness;
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

	char *map_Ks_path;
	Map *map_Ks;

	char *map_Ke_path;
	Map *map_Ke;
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

	cl_float3 center;
	struct s_face *next;
}				Face;

typedef struct s_Box
{
	cl_float3 min; //spatial bounds of box
	cl_float3 max; 
	cl_int start; //indices in Faces[] that this box contains
	cl_int end;
	cl_int children[8];
	cl_int children_count;
}				Box;

typedef struct s_gpu_bin
{
	cl_float minx;
	cl_float miny;
	cl_float minz;
	cl_int lind;
	cl_float maxx;
	cl_float maxy;
	cl_float maxz;
	cl_int rind;
}				gpu_bin;

typedef struct bvh_struct
{
	cl_float3 min; //spatial boundary
	cl_float3 max; //spatial boundary
	struct bvh_struct *left; 
	struct bvh_struct *right;
	gpu_bin *parent;
	Face *faces;
	int face_ind;
	int count;
	//maybe more?
	struct bvh_struct *next; //for tree building not for tracing
}				tree_box;

typedef struct s_AABB
{
	cl_float3 min;
	cl_float3 max;

	struct s_AABB *members;
	struct s_AABB *left;
	struct s_AABB *right;
	struct s_AABB *next;
	struct s_AABB *parent;

	int start_ind;
	int member_count;
	int flat_ind;

	Face *f;
}				AABB;

typedef struct s_new_scene
{
	Material *materials;
	int mat_count;
	Face *faces;
	int face_count;
	AABB *bins;
	int bin_count;
}				Scene;

typedef struct s_gpu_context
{
	cl_context *contexts;
	cl_command_queue *commands;
	cl_program *programs;
	cl_uint numPlatforms;
	cl_uint numDevices;
	cl_platform_id *platform;
	cl_device_id *device_ids;
}				gpu_context;

typedef struct s_gpu_mat
{
	cl_float3 Kd;
	cl_float3 Ks;
	cl_float3 Ke;
	
	cl_float Ni;
	cl_float Tr;
	cl_float roughness;

	cl_int diff_ind;
	cl_int diff_h;
	cl_int diff_w;

	cl_int spec_ind;
	cl_int spec_h;
	cl_int spec_w;

	cl_int emiss_ind;
	cl_int emiss_h;
	cl_int emiss_w;

	cl_int bump_ind;
	cl_int bump_h;
	cl_int bump_w;

	cl_int trans_ind;
	cl_int trans_h;
	cl_int trans_w;
}				gpu_mat;

typedef struct s_gpu_scene
{
	cl_float3 *V;
	cl_float3 *T;
	cl_float3 *N;
	cl_int *M;
	cl_float3 *TN;
	cl_float3 *BTN;
	cl_uint tri_count;

	gpu_bin *bins;
	cl_uint bin_count;

	cl_uchar *tex;
	cl_uint tex_size;

	gpu_mat *mats;
	cl_uint mat_count;

	cl_uint *seeds;
	cl_uint seed_count;
}				gpu_scene;

typedef struct s_file_edits
{
	float scale;
	cl_float3 translate;
	cl_float3 rotate;
	cl_float3 Kd;		//default diffuse constant (used if no map)
	cl_float3 Ks;		//default spec constant
	cl_float3 Ke;		//default emission constant
	char *map_Kd_path;	//diffuse map (ie texture)
	char *map_Ks_path;	//spec map
	char *map_Ke_path;	//emit map
	float ior;			//index of refraction
	float roughness;	//between 0 and 1
	float transparency;	//between 0 and 1, 1 fully transparent
}				File_edits;

typedef struct	s_key
{
	_Bool		w;
	_Bool		a;
	_Bool		s;
	_Bool		d;
	_Bool		larr;
	_Bool		rarr;
	_Bool		darr;
	_Bool		uarr;
	_Bool		space;
	_Bool		shift;
}				t_key;

typedef struct	s_camera
{
	cl_float3	pos;
	cl_float3	dir;
	float		width;
	float		height;
	float		dist;
	float		angle_x;
	float		angle_y;

	cl_float3	focus;
	cl_float3	origin;
	cl_float3	d_x;
	cl_float3	d_y;
}				t_camera;

typedef struct s_split
{
	AABB *left;
	AABB *left_flex;
	AABB *right;
	AABB *right_flex;
	int left_count;
	int right_count;
	int both_count;

	enum axis ax;
	float ratio;
}				Split;

typedef struct	s_render_token {
	cl_event *done;
	int device_count;
	cl_float3 **outputs;
	int **counts;
}				render_token;

//bvh_util.c
#define INF (cl_float3){FLT_MAX, FLT_MAX, FLT_MAX}
#define NEG_INF (cl_float3){-1.0f * FLT_MAX, -1.0f * FLT_MAX, -1.0f * FLT_MAX}
#define SPLIT_TEST_NUM 32
#define LEAF_THRESHOLD 4
#define BOOST_DEPTH 11
#define ALPHA 0.01f
#define SPLIT_SPOT ((float)i + 1.0f) / (SPLIT_TEST_NUM + 1.0f) 

cl_float3 center(const AABB *box);
void push(AABB **stack, AABB *box);
AABB *pop(AABB **stack);
AABB *empty_box();
AABB *box_from_points(cl_float3 *points, int pt_count);
AABB *box_from_face(Face *face);
void flex_box(AABB *box, AABB *added);
AABB *box_from_boxes(AABB *boxes);
AABB *dupe_box(AABB* box);
int point_in_box(cl_float3 point, AABB *box);
int box_in_box(AABB *box, AABB *in);
int all_in(AABB *box, AABB *in);
void print_box(AABB *box);
void print_face(Face *f);
int edge_clip(cl_float3 A, cl_float3 B, AABB *clipping, cl_float3 *points, int *pt_count, int *res_a, int *res_b);
void clip_box(AABB *box, AABB *bound);
Split *new_split(AABB *box, enum axis a, float ratio);
void free_split(Split *split);
void print_split(Split *split);
float SA(AABB *box);
float SAH(Split *split, AABB *parent);
Split *pick_best(Split **splits, AABB *parent);
int x_sort(const void *arg1, const void *arg2);
int y_sort(const void *arg1, const void *arg2);
int z_sort(const void *arg1, const void *arg2);
float SA_overlap(Split *split);

Face *stl_import(char *stl_file);

typedef struct	s_mlx_data
{
	void		*win;
	void		*img;
	char		*imgbuff;
	int			bpp;
	int			size_line;
	int			endian;
	cl_double3	*pixels;
	cl_double3	*total_clr;
}				t_mlx_data;

typedef struct s_env
{
	t_camera	cam;
	Scene		*scene;

	void		*mlx;
	t_mlx_data	*ia;
	t_mlx_data	*pt;
	t_key		key;
	
	int			mode;
	int			view;
	_Bool		show_fps;
	int			spp;
	int			samples;
	_Bool		render;
}				t_env;

AABB *sbvh(Face *faces, int *box_count, int *ref_count);
void study_tree(AABB *tree, int ray_count);
void flatten_faces(Scene *scene);
gpu_bin *flatten_bvh(Scene *scene);
float area(AABB *box);
cl_double3 *composite(cl_float3 **outputs, int numDevices, int resolution, cl_int **counts);
cl_double3 *debug_composite(cl_float3 **outputs, int numDevices, int resolution, int **counts);
int depth(AABB *box);

Face *ply_import(char *ply_file, File_edits edit_info, int *face_count);
Face *object_flatten(Face *faces, int *face_count);

////Old stuff
void draw_pixels(t_mlx_data *data, int xres, int yres);

void load_config(t_env *env);
Scene *scene_from_obj(char *rel_path, char *filename, File_edits edit_info);
Scene *scene_from_ply(char *rel_path, char *filename, File_edits edit_info);

void	alt_composite(t_mlx_data *data, int resolution, unsigned int samples);
cl_float3 *gpu_render(Scene *scene, t_camera cam, int xdim, int ydim, unsigned int samples, int first);
//Scene *scene_from_obj(char *rel_path, char *filename);
//cl_double3 *gpu_render(Scene *scene, t_camera cam, int xdim, int ydim, int SPP);

void old_bvh(Scene *S);
Box *bvh_obj(Face *Faces, int start, int end, int *boxcount);
void gpu_ready_bvh(Scene *S, int *counts, int obj_count);

uint64_t splitBy3(const unsigned int a);
uint64_t mortonEncode_magicbits(const unsigned int x, const unsigned int y, const unsigned int z);
uint64_t morton64(float x, float y, float z);

//vector helpers
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
cl_float3	vec_rotate_xy(const cl_float3 a, const float angle);
cl_float3	vec_rotate_yz(const cl_float3 a, const float angle);
cl_float3	vec_rotate_xz(const cl_float3 a, const float angle);
cl_float3	vec_rotate_xyz(const cl_float3 a, const float angle_x, const float angle_y);
cl_float3 vec_rev(cl_float3 v);
void vec_rot(const cl_float3 rotate, cl_float3 *V);
void	clr_avg(cl_double3 *a, cl_double3 *b, int samples, int size);

//utility functions
char *strtrim(char const *s);
char *move_str(char *big, char *little, int flag);
char *itoa(int n);
int countwords(const char *str, char c);
cl_float3 get_vec(const char *line);
void print_vec(const cl_float3 vec);
void print_3x3(const t_3x3 mat);
void print_clf3(cl_float3 v);
float max3(float a, float b, float c);
float min3(float a, float b, float c);


//key_command.c
int		exit_hook(int key, t_env *env);
int		key_press(int key, t_env *env);
int		key_release(int key, t_env *env);
int		forever_loop(t_env *env);

//read.c
char			read_char(FILE *fp, const int file_endian, const int machine_endian);
unsigned char	read_uchar(FILE *fp, const int file_endian, const int machine_endian);
short			read_short(FILE *fp, const int file_endian, const int machine_endian);
unsigned short	read_ushort(FILE *fp, const int file_endian, const int machine_endian);
int				read_int(FILE *fp, const int file_endian, const int machine_endian);
unsigned int	read_uint(FILE *fp, const int file_endian, const int machine_endian);
float			read_float(FILE *fp, const int file_endian, const int machine_endian);
double			read_double(FILE *fp, const int file_endian, const int machine_endian);

//get_face.c
int get_face_elements(char *line, int *va, int *vta, int *vna, int *vb, int *vtb, int *vnb, int *vc, int *vtc, int *vnc, int *vd, int *vtd, int *vnd);

//interactive.c
void	interactive(t_env *env);

//camera.c
void		set_camera(t_camera *cam, float win_dim);
t_camera	init_camera(void);


//main.c
void		path_tracer(t_env *env);
void		init_mlx_data(t_env *env);
t_env		*init_env(void);
