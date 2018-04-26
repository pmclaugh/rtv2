#include "rt.h"

// typedef struct s_env
// {
// 	t_camera	cam;
// 	Scene		*scene;

// 	void		*mlx;
// 	t_mlx_data	*ia;
// 	t_mlx_data	*pt;
// 	t_key		key;
	
// 	int			mode;
// 	int			view;
// 	_Bool		show_fps;
// 	int			spp;
// 	int			samples;
// 	_Bool		render;
// 	float		eps;
// 	int			min_bounces;
// }				t_env;

// typedef struct s_new_scene
// {
// 	Material *materials;
// 	int mat_count;
// 	Face *faces;
// 	int face_count;
// 	AABB *bins;
// 	int bin_count;
// }				Scene;

enum path_type{
	FROM_CAMERA,
	FROM_LIGHT
};

typedef struct s_BRDF
{
	float (*eval)(cl_float3, cl_float3, cl_float3);
	cl_float3 (*sample)(cl_float3, cl_float3);
}				BRDF;

typedef struct s_vertex
{
	struct s_vertex *next;
	cl_float3 position;
	cl_float3 direction;
	cl_float3 normal;
	cl_float3 mask;
	cl_float3 color;
	BRDF *brdf;
}				Vertex;

typedef struct s_path
{
	Vertex *vertices;
	int vertex_count;
	enum path_type type;
}				Path;

Path *init_camera_paths(int count, t_camera cam)
{
	Path *camera_paths = calloc(count, sizeof(Path));
	for (int i = 0; i < count; i++)
	{
		camera_paths[i].type = FROM_CAMERA;
		camera_paths[i].vertices = new_camera_vertex(i, count, cam);
		camera_paths[i].vertex_count = 1;
	}
}

Path *init_light_paths(int count, t_light light)
{
	Path *camera_paths = calloc(count, sizeof(Path));
	for (int i = 0; i < count; i++)
	{
		camera_paths[i].type = FROM_LIGHT;
		camera_paths[i].vertices = new_light_vertex(light);
		camera_paths[i].vertex_count = 1;
	}
}

#define LIGHT_PATH_COUNT 1000
#define CAMERA_PATH_COUNT DIM_PT * DIM_PT

cl_float3 *bidirectional(t_env *env)
{
	Scene *S = env->scene;
	cl_float3 output = calloc(DIM_PT * DIM_PT, sizeof(cl_float3));
	
	Path *camera_paths = init_camera_paths(CAMERA_PATH_COUNT, env->cam);
	Path *light_paths = init_light_paths(LIGHT_PATH_COUNT, env->light); //need to make a light struct that's like t_camera

	for (int i = 0; i < LIGHT_PATH_COUNT; i++)
		propagate(light_paths[i]);
	for (int i = 0; i < CAMERA_PATH_COUNT; i++)
		propagate(camera_paths[i]);
}