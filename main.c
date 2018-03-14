#include "rt.h"

#ifndef __APPLE__
# define quit_key 113
#else
# define quit_key 12
#endif


#define XDIM 1024
#define YDIM 1024
#define SPP_PER_DEVICE 500

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
#define H_FOV M_PI_2 * 70.0 / 90.0
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

void add_box(Face **faces, cl_float3 min, cl_float3 max)
{
	Face *box = calloc(14, sizeof(Face));

	//manually (ugh) populate vertices
	cl_float3 LDF = min;
	cl_float3 LDB = (cl_float3){min.x, min.y, max.z};
	cl_float3 RDF = (cl_float3){max.x, min.y, min.z};
	cl_float3 RDB = (cl_float3){max.x, min.y, max.z};
	cl_float3 RUB = max;
	cl_float3 LUB = (cl_float3){min.x, max.y, max.z};
	cl_float3 RUF = (cl_float3){max.x, max.y, min.z};
	cl_float3 LUF = (cl_float3){min.x, max.y, min.z};

	//front face - check
	box[0].verts[0] = LDF;
	box[0].verts[1] = RDF;
	box[0].verts[2] = RUF;
	box[1].verts[0] = LDF;
	box[1].verts[1] = RUF;
	box[1].verts[2] = LUF;

	//left face -check
	box[2].verts[0] = LDF;
	box[2].verts[1] = LUF;
	box[2].verts[2] = LDB;
	box[3].verts[0] = LUF;
	box[3].verts[1] = LUB;
	box[3].verts[2] = LDB;

	//right face - check
	box[4].verts[0] = RDB;
	box[4].verts[1] = RUF;
	box[4].verts[2] = RDF;
	box[5].verts[0] = RDB;
	box[5].verts[1] = RUB;
	box[5].verts[2] = RUF;

	//back face -good
	box[6].verts[0] = LDB;
	box[6].verts[1] = RUB;
	box[6].verts[2] = RDB;
	box[7].verts[0] = LDB;
	box[7].verts[1] = LUB;
	box[7].verts[2] = RUB;

	//down face -good
	box[8].verts[0] = LDF;
	box[8].verts[1] = RDB;
	box[8].verts[2] = RDF;
	box[9].verts[0] = LDF;
	box[9].verts[1] = LDB;
	box[9].verts[2] = RDB;

	//up face - check
	box[10].verts[0] = LUF;
	box[10].verts[1] = RUF;
	box[10].verts[2] = RUB;
	box[11].verts[0] = LUF;
	box[11].verts[1] = RUB;
	box[11].verts[2] = LUB;


	cl_float3 offset = (cl_float3){0.0f, -0.0001f, 0.0f};
	cl_float3 ceil_span = vec_sub(RUB, LUF);
	cl_float3 other_span = vec_sub(RUF, LUB);
	cl_float3 ceil_ctr = vec_add(offset, vec_add(LUF, vec_scale(ceil_span, 0.5f)));

	//light
	box[12].verts[0] = vec_sub(ceil_ctr, vec_scale(ceil_span, 0.2f)); //luf
	box[12].verts[1] = vec_add(ceil_ctr, vec_scale(other_span, 0.2f)); //ruf
	box[12].verts[2] = vec_add(ceil_ctr, vec_scale(ceil_span, 0.2f)); //rub
	box[13].verts[0] = vec_sub(ceil_ctr, vec_scale(ceil_span, 0.2f)); //luf
	box[13].verts[1] = vec_add(ceil_ctr, vec_scale(ceil_span, 0.2f)); //rub
	box[13].verts[2] = vec_sub(ceil_ctr, vec_scale(other_span, 0.2f)); //lub

	for (int i = 0; i < 14; i++)
	{
		if (i != 0)
			box[i - 1].next = &box[i];
		box[i].shape = 3;
		box[i].mat_ind = 1; //default wall material
		if (i == 12 || i == 13)
			box[i].mat_ind = 2; //lighted ceiling
		if (i == 6 || i == 7)
			box[i].mat_ind = 3; //other color
		if (i == 2 || i == 3)
			box[i].mat_ind = 4; // another color
		box[i].center = vec_scale(vec_add(vec_add(box[i].verts[0], box[i].verts[1]), box[i].verts[2]), 1.0 / 3.0);
		cl_float3 N = cross(vec_sub(box[i].verts[1], box[i].verts[0]), vec_sub(box[i].verts[2], box[i].verts[0]));
		box[i].norms[0] = N;
		box[i].norms[1] = N;
		box[i].norms[2] = N;
		box[i].tex[0] = (cl_float3){0.0f, 0.0f, 0.0f};
		box[i].tex[1] = (cl_float3){0.0f, 0.0f, 0.0f};
		box[i].tex[2] = (cl_float3){0.0f, 0.0f, 0.0f};
	}
	box[13].next = *faces;
	*faces = &box[0];
}

Scene *scene_from_ply(char *filename)
{
	Face *ply = ply_import(filename);
	int box_count, ref_count;

	cl_float3 min, max;

	min = (cl_float3){-2.0, -2.0, -3.0};
	max = (cl_float3){2.0, 2.0, 3.0};

	add_box(&ply, min, max);

	Scene *scene = calloc(1, sizeof(Scene));
	scene->materials = calloc(5, sizeof(Material));
	//imported model
	scene->materials[0].Kd = (cl_float3){0.8f, 0.8f, 0.8f};
	scene->materials[0].Ka = (cl_float3){0.8f, 0.8f, 0.8f};
	scene->materials[0].Ke = (cl_float3){0.0f, 0.0f, 0.0f};
	scene->materials[0].Ns = (cl_float3){0.0f, 0.0f, 0.0f};
	
	//walls
	scene->materials[1].Kd = (cl_float3){0.3f, 0.3f, 0.8f};
	scene->materials[1].Ka = (cl_float3){0.0f, 0.0f, 0.0f};
	scene->materials[1].Ke = (cl_float3){0.0f, 0.0f, 0.0f};
	scene->materials[1].Ns = (cl_float3){1.0f, 0.0f, 0.0f};

	//light
	scene->materials[2].Kd = (cl_float3){0.8f, 0.8f, 0.8f};
	scene->materials[2].Ka = (cl_float3){0.0f, 0.0f, 0.0f};
	scene->materials[2].Ke = (cl_float3){1.0f, 1.0f, 1.0f};
	scene->materials[2].Ns = (cl_float3){1.0f, 0.0f, 0.0f};

	//optional alternate wall
	scene->materials[3].Kd = (cl_float3){0.3f, 0.8f, 0.3f};
	scene->materials[3].Ka = (cl_float3){0.0f, 0.0f, 0.0f};
	scene->materials[3].Ke = (cl_float3){0.0f, 0.0f, 0.0f};
	scene->materials[3].Ns = (cl_float3){1.0f, 0.0f, 0.0f};

	//optional alternate wall
	scene->materials[4].Kd = (cl_float3){0.8f, 0.3f, 0.3f};
	scene->materials[4].Ka = (cl_float3){0.0f, 0.0f, 0.0f};
	scene->materials[4].Ke = (cl_float3){0.0f, 0.0f, 0.0f};
	scene->materials[4].Ns = (cl_float3){1.0f, 0.0f, 0.0f};

	scene->mat_count = 5;
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
	scene->materials = calloc(3, sizeof(Material));
	scene->materials[0].Kd = (cl_float3){0.2f, 0.2f, 0.7f};
	scene->materials[0].Ka = (cl_float3){0.4f, 0.4f, 0.4f};
	scene->materials[0].Ke = (cl_float3){0.0f, 0.0f, 0.0f};
	
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
	
	Scene *scene = scene_from_ply("objects/ply/oct.ply");

	// Scene *scene = scene_from_obj("objects/sponza/", "sponza.obj");

	// // LL is best for this bvh. don't want to rearrange import for now, will do later
	// Face *face_list = NULL;
	// for (int i = 0; i < scene->face_count; i++)
	// {
	// 	Face *f = calloc(1, sizeof(Face));
	// 	memcpy(f, &scene->faces[i], sizeof(Face));
	// 	f->next = face_list;
	// 	face_list = f;
	// }
	// free(scene->faces);

	// int box_count, ref_count;
	// AABB *tree = sbvh(face_list, &box_count, &ref_count);
	// printf("finished with %d boxes\n", box_count);
	// study_tree(tree, 100000);


	// scene->bins = tree;
	// scene->bin_count = box_count;
	// scene->face_count = ref_count;
	// printf("about to flatten\n");
	// flatten_faces(scene);

	//return 0;

	t_camera cam;
	//cam.center = (cl_float3){-400.0, 50.0, -220.0}; //reference vase view (1,0,0)
	//cam.center = (cl_float3){-320.0, 60.0, -220.0}; // vase view (1,0,0)
	//cam.center = (cl_float3){-540.0, 150.0, 380.0}; //weird wall-hole (0,0,1)
	//cam.center = (cl_float3){-800.0, 450.0, 0.0}; //standard high perspective on curtain
	// cam.center = (cl_float3){100.0, 450.0, 0.0}; //standard high perspective on curtain
	// cam.center = (cl_float3){-800.0, 600.0, 350.0}; //upstairs left
	//cam.center = (cl_float3){-300.0, 30.0, 400.0}; //down left
	//cam.center = (cl_float3){600.0, 150.0, -95.0}; //lion
	// cam.center = (cl_float3){780.0, 650.0, -35.0}; //lion
	//cam.center = (cl_float3){-250.0, 100.0, 0.0};
	//cam.center = (cl_float3){-3000.0, 2200.0, 0.0}; //outside

	// cam.center = (cl_float3){-0.3, 0.1, 0.0}; //bunn
	// cam.normal = (cl_float3){1.0, 0.0, 0.0};

	// cam.center = (cl_float3){0.3, 0.15, 0.0}; //dragon facing
	// cam.normal = (cl_float3){-1.0, 0.0, 0.0};


	cam.center = (cl_float3){0.0, 0.4, -2.9}; //trogdor
	cam.normal = (cl_float3){0.0, 0.0, 1.0};

	cam.normal = unit_vec(cam.normal);
	cam.width = 0.1f * (float)XDIM / (float)YDIM;
	cam.height = 0.1f;

	init_camera(&cam, XDIM, YDIM);
	//printf("%lu\n", sizeof(gpu_bin));

	cl_double3 *pixels = gpu_render(scene, cam, XDIM, YDIM, SPP_PER_DEVICE);

	char *filename;
	if (ac == 1)
		asprintf(&filename, "out.ppm");
	else
		filename = strdup(av[1]);
	FILE *f = fopen(filename, "w");
	fprintf(f, "P3\n%d %d\n%d\n ",XDIM,YDIM,255);
	for (int row=0; row<YDIM; row++)
	{
		for (int col=0;col<XDIM;col++) {
			fprintf(f,"%d %d %d ", (int)(pixels[row * XDIM + (XDIM - col)].x * 255), (int)(pixels[row * XDIM + (XDIM - col)].y * 255), (int)(pixels[row * XDIM + (XDIM - col)].z * 255));
		}
		fprintf(f, "\n");
	}
	fprintf(f, "\n");
	free(filename);
	fclose(f);


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
