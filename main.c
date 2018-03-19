#include "rt.h"

#ifndef __APPLE__
# define quit_key 113
#else
#endif

void	sum_color(cl_float3 *a, cl_float3 *b, int size)

#define XDIM 1024
#define YDIM 1024
#define SPP_PER_DEVICE 420

typedef struct s_param
{
	for (int i = 0; i < size; i++)
	{
		a[i].x += b[i].x;
		a[i].y += b[i].y;
		a[i].z += b[i].z;
	}
}


//THIS DOESNT WORK RIGHT, JUST FOR TESTING
#define H_FOV M_PI_2 * 70.0 / 90.0
void		path_tracer(t_env *env)
{
	env->samples += 1;
	if (env->samples > env->spp)
	{
		env->samples = 0;
		env->render = 0;
		for (int i = 0; i < DIM_PT * DIM_PT; i++)
		{
			env->pt->total_clr[i].x = 0;
			env->pt->total_clr[i].y = 0;
			env->pt->total_clr[i].z = 0;
		}
		return ;
	}
	set_camera(&env->cam, DIM_PT);
	cl_float3 *pix = gpu_render(env->scene, env->cam, DIM_PT, DIM_PT, 1);
	sum_color(env->pt->total_clr, pix, DIM_PT * DIM_PT);
	alt_composite(env->pt, DIM_PT * DIM_PT, env->samples);
	draw_pixels(env->pt, DIM_PT, DIM_PT);
	mlx_put_image_to_window(env->mlx, env->pt->win, env->pt->img, 0, 0);
	mlx_key_hook(env->pt->win, exit_hook, env);
}

void		init_mlx_data(t_env *env)
{
	env->mlx = mlx_init();

	env->pt = malloc(sizeof(t_mlx_data));
	env->pt->bpp = 0;
	env->pt->size_line = 0;
	env->pt->endian = 0;
	env->pt->win = mlx_new_window(env->mlx, DIM_PT, DIM_PT, "CLIVE - Path Tracer");
	env->pt->img = mlx_new_image(env->mlx, DIM_PT, DIM_PT);
	env->pt->imgbuff = mlx_get_data_addr(env->pt->img, &env->pt->bpp, &env->pt->size_line, &env->pt->endian);
	env->pt->bpp /= 8;
	env->pt->pixels = malloc(sizeof(cl_double3) * (DIM_PT * DIM_PT));
	env->pt->total_clr = malloc(sizeof(cl_double3) * (DIM_PT * DIM_PT));
	for (int i = 0; i < DIM_PT * DIM_PT; i++)
	{
		env->pt->total_clr[i].x = 0;
		env->pt->total_clr[i].y = 0;
		env->pt->total_clr[i].z = 0;
	}
	
	env->ia = malloc(sizeof(t_mlx_data));
	env->ia->bpp = 0;
	env->ia->size_line = 0;
	env->ia->endian = 0;
	env->ia->win = mlx_new_window(env->mlx, DIM_IA, DIM_IA, "CLIVE - Interactive Mode");
	env->ia->img = mlx_new_image(env->mlx, DIM_IA, DIM_IA);
	env->ia->imgbuff = mlx_get_data_addr(env->ia->img, &env->ia->bpp, &env->ia->size_line, &env->ia->endian);
	env->ia->bpp /= 8;
	env->ia->pixels = malloc(sizeof(cl_double3) * ((DIM_IA) * (DIM_IA)));
	env->ia->total_clr = NULL;
}

t_env		*init_env(void)
{
	t_env	*env = malloc(sizeof(t_env));
	env->cam = init_camera();
	//load camera settings from config file and import scene
	load_config(env);
	set_camera(&env->cam, (float)DIM_IA);
	env->mode = 1;
	env->show_fps = 0;
	env->key.w = 0;
	env->key.a = 0;
	env->key.s = 0;
	env->key.d = 0;
	env->key.uarr = 0;
	env->key.darr = 0;
	env->key.larr = 0;
	env->key.rarr = 0;
	env->key.space = 0;
	env->key.shift = 0;
	env->samples = 0;
	env->render = 0;
	return env;
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

	min = (cl_float3){-0.25, 0.05, -0.35};
	max = (cl_float3){0.25, 0.35, 0.35};

	add_box(&ply, min, max);

	Scene *scene = calloc(1, sizeof(Scene));
	scene->materials = calloc(5, sizeof(Material));
	//imported model
	scene->materials[0].Kd = (cl_float3){0.8f, 0.8f, 0.8f};
	scene->materials[0].Ka = (cl_float3){0.9f, 0.4f, 0.7f};
	scene->materials[0].Ke = (cl_float3){0.0f, 0.0f, 0.0f};
	scene->materials[0].Ns = (cl_float3){0.0f, 0.0f, 0.0f};
	
	//walls
	scene->materials[1].Kd = (cl_float3){0.6f, 0.6f, 0.6f};
	scene->materials[1].Ka = (cl_float3){0.0f, 0.0f, 0.0f};
	scene->materials[1].Ke = (cl_float3){0.0f, 0.0f, 0.0f};
	scene->materials[1].Ns = (cl_float3){1.0f, 0.0f, 0.0f};

	//light
	scene->materials[2].Kd = (cl_float3){0.8f, 0.8f, 0.8f};
	scene->materials[2].Ka = (cl_float3){0.0f, 0.0f, 0.0f};
	scene->materials[2].Ke = (cl_float3){1.0f, 1.0f, 1.0f};
	scene->materials[2].Ns = (cl_float3){1.0f, 0.0f, 0.0f};

	//optional alternate wall
	scene->materials[3].Kd = (cl_float3){0.6f, 0.6f, 0.6f};
	scene->materials[3].Ka = (cl_float3){0.0f, 0.0f, 0.0f};
	scene->materials[3].Ke = (cl_float3){0.0f, 0.0f, 0.0f};
	scene->materials[3].Ns = (cl_float3){1.0f, 0.0f, 0.0f};

	//optional alternate wall
	scene->materials[4].Kd = (cl_float3){0.6f, 0.6f, 0.6f};
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

int main(int ac, char **av)
{
	srand(time(NULL));
	// scene_from_stl("iona.stl");
	
	Scene *scene = scene_from_ply("objects/ply/dragon.ply");

  	//Initialize environment with scene and intial configurations
	t_env	*env = init_env();

	//LL is best for this bvh. don't want to rearrange import for now, will do later
	Face *face_list = NULL;
	for (int i = 0; i < env->scene->face_count; i++)
	{
		Face *f = calloc(1, sizeof(Face));
		memcpy(f, &env->scene->faces[i], sizeof(Face));
		f->next = face_list;
		face_list = f;
	}
	free(env->scene->faces);

	//Build BVH
  
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

	// cam.center = (cl_float3){-0.28, 0.13, 0.0}; //dragon facing
	// cam.normal = (cl_float3){1.0, 0.0, 0.0};

	cam.center = (cl_float3){0.0, 0.12, -0.25}; //trogdor
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
