#include "rt.h"

#ifndef __APPLE__
# define quit_key 113
#else
# define quit_key 12
#endif


#define XDIM 512
#define YDIM 512
#define SPP_PER_DEVICE 2000

typedef struct s_param
{
	void *mlx;
	void *win;
	void *img;
	int x;
	int y;

	Scene *scene;
	t_camera cam;
	cl_float3 *pixels;
	int samplecount;
}				t_param;

//THIS DOESNT WORK RIGHT, JUST FOR TESTING
#define H_FOV M_PI_2 * 60.0 / 90.0
void init_camera(t_camera *camera, int xres, int yres)
{
	//printf("init camera %d %d\n", xres, yres);
	//determine a focus point in front of the view-plane
	//such that the edge-focus-edge vertex has angle H_FOV

	camera->normal = unit_vec(camera->normal);

	float d = (camera->width / 2.0) / tan(H_FOV / 2.0);
	camera->focus = vec_add(camera->center, vec_scale(camera->normal, d));

	//now i need unit vectors on the plane
	t_3x3 camera_matrix = rotation_matrix(UNIT_Z, camera->normal);
	cl_float3 camera_x, camera_y;
	camera_x = mat_vec_mult(camera_matrix, UNIT_X);
	camera_y = mat_vec_mult(camera_matrix, UNIT_Y);

	//pixel deltas
	camera->d_x = vec_scale(camera_x, camera->width / (float)xres);
	camera->d_y = vec_scale(camera_y, camera->height / (float)yres);

	//start at bottom corner (the plane's "origin")
	camera->origin = vec_sub(camera->center, vec_scale(camera_x, camera->width / 2.0));
	camera->origin = vec_sub(camera->origin, vec_scale(camera_y, camera->height / 2.0));
}

int key_hook(int keycode, void *param)
{
	t_param *p = (t_param *)param;
	if (keycode == quit_key)
	{
		mlx_destroy_image(p->mlx, p->img);
		mlx_destroy_window(p->mlx, p->win);
		exit(0);
	}
	return (1);
}

// typedef struct s_new_scene
// {
// 	Material *materials;
// 	int mat_count;
// 	Face *faces;
// 	int face_count;
// 	AABB *bins;
// 	int bin_count;
// }				Scene;

// typedef struct s_material
// {
// 	char *friendly_name;
// 	float Ns; //specular exponent
// 	float Ni; //index of refraction
// 	float d; //opacity
// 	float Tr; //transparency (1 - d)
// 	cl_float3 Tf; //transmission filter (ie color)
// 	int illum; //flag for illumination model, raster only
// 	cl_float3 Ka; //ambient mask
// 	cl_float3 Kd; //diffuse mask
// 	cl_float3 Ks; //specular mask
// 	cl_float3 Ke; //emission mask

// 	char *map_Ka_path;
// 	Map *map_Ka;

// 	char *map_Kd_path;
// 	Map *map_Kd;

// 	char *map_bump_path;
// 	Map *map_bump;

// 	char *map_d_path;
// 	Map *map_d;

// 	char *map_Ks_path;
// 	Map *map_Ks;
// }				Material;

Scene *scene_from_ply(char *filename)
{
	Face *ply = ply_import(filename);
	int box_count, ref_count;

	Scene *scene = calloc(1, sizeof(Scene));
	scene->materials = calloc(1, sizeof(Material));
	scene->materials->Kd = (cl_float3){0.2f, 0.2f, 0.6f};
	scene->materials->Ka = (cl_float3){0.8f, 0.8f, 0.8f};
	scene->mat_count = 1;
	scene->bins = sbvh(ply, &box_count, &ref_count);
	study_tree(scene->bins, 10000);
	scene->face_count = ref_count;
	scene->bin_count = box_count;
	flatten_faces(scene);

	return scene;
}

Scene *scene_from_stl(char *filename)
{
	Face *ply = stl_import(filename);
	int box_count, ref_count;

	Scene *scene = calloc(1, sizeof(Scene));
	scene->materials = calloc(1, sizeof(Material));
	scene->materials->Kd = (cl_float3){0.2f, 0.2f, 0.7f};
	scene->materials->Ka = (cl_float3){0.4f, 0.4f, 0.4f};
	scene->mat_count = 1;
	scene->bins = sbvh(ply, &box_count, &ref_count);
	study_tree(scene->bins, 10000);
	scene->face_count = ref_count;
	scene->bin_count = box_count;
	flatten_faces(scene);
	return scene;
}

int main(int ac, char **av)
{
	srand(time(NULL));
	// scene_from_stl("iona.stl");
	
	// return 0;
	Scene *scene = scene_from_ply("objects/ply/bunny.ply");
	//return 0;
	// Scene *sponza = scene_from_obj("objects/sponza/", "sponza.obj");

	// // LL is best for this bvh. don't want to rearrange import for now, will do later
	// Face *face_list = NULL;
	// for (int i = 0; i < sponza->face_count; i++)
	// {
	// 	Face *f = calloc(1, sizeof(Face));
	// 	memcpy(f, &sponza->faces[i], sizeof(Face));
	// 	f->next = face_list;
	// 	face_list = f;
	// }
	// free(sponza->faces);

	// int box_count, ref_count;
	// AABB *tree = sbvh(face_list, &box_count, &ref_count);
	// printf("finished with %d boxes\n", box_count);
	// study_tree(tree, 100000);


	// sponza->bins = tree;
	// sponza->bin_count = box_count;
	// sponza->face_count = ref_count;
	// printf("about to flatten\n");
	// flatten_faces(sponza);

	//return 0;

	t_camera cam;
	//cam.center = (cl_float3){-400.0, 50.0, -220.0}; //reference vase view (1,0,0)
	//cam.center = (cl_float3){-320.0, 60.0, -220.0}; // vase view (1,0,0)
	//cam.center = (cl_float3){-540.0, 150.0, 380.0}; //weird wall-hole (0,0,1)
	//cam.center = (cl_float3){-800.0, 450.0, 0.0}; //standard high perspective on curtain
	//cam.center = (cl_float3){-800.0, 450.0, 0.0}; //standard high perspective on curtain
	//cam.center = (cl_float3){-800.0, 600.0, 350.0}; //upstairs left
	//cam.center = (cl_float3){0.0, 30.0, 400.0}; //down left
	//cam.center = (cl_float3){800.0, 150.0, -35.0}; //lion
	//cam.center = (cl_float3){780.0, 650.0, -35.0}; //lion
	//cam.center = (cl_float3){-250.0, 100.0, 0.0};
	//cam.center = (cl_float3){-3000.0, 2200.0, 0.0}; //outside

	cam.center = (cl_float3){-0.3, 0.1, 0.0}; //bunn
	cam.normal = (cl_float3){1.0, 0.0, 0.0};

	// cam.center = (cl_float3){0.0, 0.12, -0.35}; //trogdor
	// cam.normal = (cl_float3){0.0, 0.0, 1.0};

	cam.normal = unit_vec(cam.normal);
	cam.width = 0.1f * (float)XDIM / (float)YDIM;
	cam.height = 0.1f;

	init_camera(&cam, XDIM, YDIM);
	//printf("%lu\n", sizeof(gpu_bin));

	cl_double3 *pixels = gpu_render(scene, cam, XDIM, YDIM, SPP_PER_DEVICE);

	void *mlx = mlx_init();
	void *win = mlx_new_window(mlx, XDIM, YDIM, "RTV1");
	void *img = mlx_new_image(mlx, XDIM, YDIM);
	draw_pixels(img, XDIM, YDIM, pixels);
	
	mlx_put_image_to_window(mlx, win, img, 0, 0);

	t_param *param = calloc(1, sizeof(t_param));
	*param = (t_param){mlx, win, img, XDIM, YDIM, scene, cam, NULL, 0};
	
	mlx_key_hook(win, key_hook, param);
	mlx_loop(mlx);

	free(pixels);

	printf("all done\n");
}
