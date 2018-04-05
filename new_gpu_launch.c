#include "rt.h"
#include <fcntl.h>

char *load_cl_file(char *file)
{
	int fd = open(file, O_RDONLY);
	char *source = calloc(sizeof(char),30000); // real kernel is about 8k characters, whatever
	read(fd, source, 30000);
	close(fd);
	return source;
}

void reseed(gpu_scene *scene)
{
	cl_uint *h_seeds = calloc(scene->seed_count, sizeof(cl_uint));
	for (int i = 0; i < scene->seed_count; i++)
		h_seeds[i] = rand();
	free(scene->seeds);
	scene->seeds = h_seeds;
}

int nonzero(const cl_float3 v)
{
	if (v.x == 0.0f && v.y == 0.0f && v.z == 0.0f)
		return 0;
	else
		return 1;
}

gpu_scene *prep_scene(Scene *s, gpu_context *CL, int xdim, int ydim)
{
	//SEEDS
	cl_uint *h_seeds = calloc(xdim * ydim * 2 * CL->numDevices * CL->numPlatforms, sizeof(cl_uint));
	for (int i = 0; i < xdim * ydim * 2 * CL->numDevices; i++)
		h_seeds[i] = rand();


	//TEXTURES
	cl_int tex_size = 0;
	for (int i = 0; i < s->mat_count; i++)
	{
		if (s->materials[i].map_Kd)
			tex_size += s->materials[i].map_Kd->height * s->materials[i].map_Kd->width * 3;
		if (s->materials[i].map_Ks)
			tex_size += s->materials[i].map_Ks->height * s->materials[i].map_Ks->width * 3;
		if (s->materials[i].map_Ke)
			tex_size += s->materials[i].map_Ke->height * s->materials[i].map_Ke->width * 3;
		if (s->materials[i].map_bump)
			tex_size += s->materials[i].map_bump->height * s->materials[i].map_bump->width * 3;
		if (s->materials[i].map_d)
			tex_size += s->materials[i].map_d->height * s->materials[i].map_d->width * 3;
	}

	cl_uchar *h_tex = calloc(sizeof(cl_uchar), tex_size);
	tex_size = 0;

	gpu_mat *simple_mats = calloc(s->mat_count, sizeof(gpu_mat));
	for (int i = 0; i < s->mat_count; i++)
	{
		simple_mats[i].Kd = s->materials[i].Kd;
		simple_mats[i].Ks = s->materials[i].Ks;
		simple_mats[i].Ke = s->materials[i].Ke;
		simple_mats[i].Ni = s->materials[i].Ni;
		simple_mats[i].Tr = s->materials[i].Tr;
		simple_mats[i].roughness = s->materials[i].roughness;
		printf("gpu mat %d will be a=%.2f, Ni=%.1f, Tr=%.1f\n", i, simple_mats[i].roughness, simple_mats[i].Ni, simple_mats[i].Tr);

		if (s->materials[i].map_Kd)
		{
			memcpy(&h_tex[tex_size], s->materials[i].map_Kd->pixels, s->materials[i].map_Kd->height * s->materials[i].map_Kd->width * 3);
			simple_mats[i].diff_ind = tex_size;
			simple_mats[i].diff_h = s->materials[i].map_Kd->height;
			simple_mats[i].diff_w = s->materials[i].map_Kd->width;
			tex_size += s->materials[i].map_Kd->height * s->materials[i].map_Kd->width * 3;
		}

		if (s->materials[i].map_Ks)
		{
			memcpy(&h_tex[tex_size], s->materials[i].map_Ks->pixels, s->materials[i].map_Ks->height * s->materials[i].map_Ks->width * 3);
			simple_mats[i].spec_ind = tex_size;
			simple_mats[i].spec_h = s->materials[i].map_Ks->height;
			simple_mats[i].spec_w = s->materials[i].map_Ks->width;
			tex_size += s->materials[i].map_Ks->height * s->materials[i].map_Ks->width * 3;
		}

		if (s->materials[i].map_Ke)
		{
			memcpy(&h_tex[tex_size], s->materials[i].map_Ke->pixels, s->materials[i].map_Ke->height * s->materials[i].map_Ke->width * 3);
			simple_mats[i].emiss_ind = tex_size;
			simple_mats[i].emiss_h = s->materials[i].map_Ke->height;
			simple_mats[i].emiss_w = s->materials[i].map_Ke->width;
			tex_size += s->materials[i].map_Ke->height * s->materials[i].map_Ke->width * 3;
		}

		if (s->materials[i].map_bump)
		{
			memcpy(&h_tex[tex_size], s->materials[i].map_bump->pixels, s->materials[i].map_bump->height * s->materials[i].map_bump->width * 3);
			simple_mats[i].bump_ind = tex_size;
			simple_mats[i].bump_h = s->materials[i].map_bump->height;
			simple_mats[i].bump_w = s->materials[i].map_bump->width;
			tex_size += s->materials[i].map_bump->height * s->materials[i].map_bump->width * 3;
		}

		if (s->materials[i].map_d)
		{
			memcpy(&h_tex[tex_size], s->materials[i].map_d->pixels, s->materials[i].map_d->height * s->materials[i].map_d->width * 3);
			simple_mats[i].trans_ind = tex_size;
			simple_mats[i].trans_h = s->materials[i].map_d->height;
			simple_mats[i].trans_w = s->materials[i].map_d->width;
			tex_size += s->materials[i].map_d->height * s->materials[i].map_d->width * 3;
		}
	}


	//FACES
	cl_float3 *V, *T, *N;
	V = calloc(s->face_count * 3, sizeof(cl_float3));
	T = calloc(s->face_count * 3, sizeof(cl_float3));
	N = calloc(s->face_count * 3, sizeof(cl_float3));
	cl_int *M;
	M = calloc(s->face_count, sizeof(cl_int));
	cl_float3 *TN, *BTN;
	TN = calloc(s->face_count, sizeof(cl_float3));
	BTN = calloc(s->face_count, sizeof(cl_float3));

	unsigned int light_count = 0;

	for (int i = 0; i < s->face_count; i++)
	{
		Face f = s->faces[i];

		V[i * 3] = f.verts[0];
		V[i * 3 + 1] = f.verts[1];
		V[i * 3 + 2] = f.verts[2];

		T[i * 3] = f.tex[0];
		T[i * 3 + 1] = f.tex[1];
		T[i * 3 + 2] = f.tex[2];

		N[i * 3] = f.norms[0];
		N[i * 3 + 1] = f.norms[1];
		N[i * 3 + 2] = f.norms[2];

		M[i] = f.mat_ind;
		if (simple_mats[f.mat_ind].bump_h)
		{
			cl_float3 dp1 = vec_sub(f.verts[1], f.verts[0]);
			cl_float3 dp2 = vec_sub(f.verts[2], f.verts[0]);
			cl_float3 duv1 = vec_sub(f.tex[1], f.tex[0]);
			cl_float3 duv2 = vec_sub(f.tex[2], f.tex[0]);
			float r = duv1.x * duv2.y - duv1.y * duv2.x == 0.0f ? 1.0f : 1.0f / (duv1.x * duv2.y - duv1.y * duv2.x);

			TN[i] = unit_vec(vec_scale(vec_sub(vec_scale(dp1, duv2.y), vec_scale(dp2, duv1.y)), r));
			TN[i] = unit_vec(vec_scale(vec_sub(vec_scale(dp1, duv2.y), vec_scale(dp2, duv1.y)), r));
			BTN[i] = unit_vec(cross(TN[i], cross(vec_sub(f.verts[1], f.verts[0]), vec_sub(f.verts[2], f.verts[0]))));
		}
		if (nonzero(simple_mats[f.mat_ind].Ke))
			light_count++;
	}

	cl_int3 *lights = calloc(light_count, sizeof(cl_int3));
	light_count = 0;
	for (int i = 0; i < s->face_count; i++)
	{
		Face f = s->faces[i];
		if (nonzero(simple_mats[f.mat_ind].Ke))
			lights[light_count++] = (cl_int3){i * 3, i * 3 + 1, i * 3 + 2};
	}


	//BINS
	gpu_bin *flat_bvh = flatten_bvh(s);

	//COMBINE
	gpu_scene *gs = calloc(1, sizeof(gpu_scene));
	*gs = (gpu_scene){V, T, N, M, TN, BTN, s->face_count * 3, flat_bvh, s->bin_count, h_tex, tex_size, simple_mats, s->mat_count, h_seeds, xdim * ydim * 2 * CL->numDevices * CL->numPlatforms, lights, light_count};
	return gs;
}

gpu_context *prep_gpu(void)
{
	// printf("prepping for GPU launch\n");
	gpu_context *gpu = calloc(1, sizeof(gpu_context));
    int err;

    // Find number of platforms
    err = clGetPlatformIDs(0, NULL, &gpu->numPlatforms);

    // Get all platforms
    gpu->platform = calloc(gpu->numPlatforms, sizeof(cl_platform_id));
    err = clGetPlatformIDs(gpu->numPlatforms, gpu->platform, NULL);

    //count devices
    gpu->numDevices = 0;
    for (int i = 0; i < gpu->numPlatforms; i++)
    {
    	cl_uint d;
        err = clGetDeviceIDs(gpu->platform[i], CL_DEVICE_TYPE_GPU, 0, NULL, &d);
        if (err == CL_SUCCESS)
            gpu->numDevices += d;
    }

    printf("%u devices, %u platforms\n", gpu->numDevices, gpu->numPlatforms);

    //get ids for devices and create (platforms) compute contexts, (devices) command queues
	cl_device_id *device_ids = calloc(gpu->numDevices, sizeof(cl_device_id));
	gpu->device_ids = device_ids;
    cl_uint offset = 0;
    gpu->contexts = calloc(gpu->numPlatforms, sizeof(cl_context));
    gpu->commands = calloc(gpu->numDevices, sizeof(cl_command_queue));
    for (int i = 0; i < gpu->numPlatforms; i++)
    {
    	cl_uint d;
    	clGetDeviceIDs(gpu->platform[i], CL_DEVICE_TYPE_GPU, gpu->numDevices, &device_ids[offset], &d);
    	gpu->contexts[i] = clCreateContext(0, d, &device_ids[offset], NULL, NULL, &err);
    	for (int j = 0; j < d; j++)
    	{
    		// #ifndef __APPLE__
    		// 	gpu->commands[offset + j] = clCreateCommandQueueWithProperties(gpu->contexts[i], device_ids[offset + j], NULL, &err);
    		// #endif
    		// #ifdef __APPLE__
    			gpu->commands[offset + j] = clCreateCommandQueue(gpu->contexts[i], device_ids[offset + j], CL_QUEUE_PROFILING_ENABLE, &err);
    		// #endif
    	}
    	offset += d;
    }
    // printf("made contexts and CQs\n");

    char *source = load_cl_file("multi.cl");
    printf("loaded kernel source\n");

    //create (platforms) programs and build them
    gpu->programs = calloc(gpu->numPlatforms, sizeof(cl_program));
    for (int i = 0; i < gpu->numPlatforms; i++)
    {
    	gpu->programs[i] = clCreateProgramWithSource(gpu->contexts[i], 1, (const char **) &source, NULL, &err);
    	err = clBuildProgram(gpu->programs[i], 0, NULL, NULL, NULL, NULL);
    	if (err != CL_SUCCESS)
    	{
    	 	printf("bad compile\n");
    	 	char *build_log;
			size_t ret_val_size = 0;
			clGetProgramBuildInfo(gpu->programs[i], device_ids[i], CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);
			printf("ret val %lu\n", ret_val_size);
			build_log = malloc(ret_val_size + 1);
			clGetProgramBuildInfo(gpu->programs[i], device_ids[i], CL_PROGRAM_BUILD_LOG, ret_val_size, build_log, NULL);
			printf("%s\n", build_log);
			free(build_log);
    	 	exit(0);
    	}
	    free(source);
    }
    printf("good compile\n");
    //getchar();
    return gpu;
}

void recompile(gpu_context *gpu)
{
	cl_int err;
	//release old program(s)
	for (int i = 0; i < gpu->numPlatforms; i++)
		clReleaseProgram(gpu->programs[i]);

	char *source = load_cl_file("multi.cl");
	//printf("loaded kernel source\n");

    //create (platforms) programs and build them
    gpu->programs = calloc(gpu->numPlatforms, sizeof(cl_program));
	for (int i = 0; i < gpu->numPlatforms; i++)
	{
		gpu->programs[i] = clCreateProgramWithSource(gpu->contexts[i], 1, (const char **) &source, NULL, &err);
		err = clBuildProgram(gpu->programs[i], 0, NULL, NULL, NULL, NULL);
		if (err != CL_SUCCESS)
		{
			printf("bad compile\n");
			char *build_log;
			size_t ret_val_size = 0;
			clGetProgramBuildInfo(gpu->programs[i], gpu->device_ids[i], CL_PROGRAM_BUILD_LOG, 0, NULL, &ret_val_size);
			printf("ret val %lu\n", ret_val_size);
			build_log = malloc(ret_val_size + 1);
			clGetProgramBuildInfo(gpu->programs[i], gpu->device_ids[i], CL_PROGRAM_BUILD_LOG, ret_val_size, build_log, NULL);
			printf("%s\n", build_log);
			free(build_log);
			exit(0);
		}
		free(source);
	}
	printf("good compile\n");
}

void	alt_composite(t_mlx_data *data, int resolution, unsigned int samples)
{
	double Lw = 0.0;
	for (int i = 0; i < resolution; i++)
	{
		double scale = 1.0 / (double)samples;
		data->pixels[i].x = data->total_clr[i].x * scale;
		data->pixels[i].y = data->total_clr[i].y * scale;
		data->pixels[i].z = data->total_clr[i].z * scale;

		double this_lw = log(0.1 + 0.2126 * data->pixels[i].x + 0.7152 * data->pixels[i].y + 0.0722 * data->pixels[i].z);
		if (this_lw == this_lw)
			Lw += this_lw;
		else
			printf("NaN alert\n");
	}

	Lw /= (double)resolution;
	Lw = exp(Lw);

	for (int i = 0; i < resolution; i++)
	{
		data->pixels[i].x = data->pixels[i].x * 0.36 / Lw;
		data->pixels[i].y = data->pixels[i].y * 0.36 / Lw;
		data->pixels[i].z = data->pixels[i].z * 0.36 / Lw;

		data->pixels[i].x = data->pixels[i].x / (data->pixels[i].x + 1.0);
		data->pixels[i].y = data->pixels[i].y / (data->pixels[i].y + 1.0);
		data->pixels[i].z = data->pixels[i].z / (data->pixels[i].z + 1.0);
	}
}

typedef struct s_gpu_ray {
	cl_float3 origin;
	cl_float3 direction;
	cl_float3 inv_dir;

	cl_float3 diff;
	cl_float3 spec;
	cl_float3 trans;
	cl_float3 bump;
	cl_float Ni;
	cl_float roughness;
	cl_float transparency;
	cl_float3 N;
	cl_float t;
	cl_float u;
	cl_float v;

	cl_float3 color;
	cl_float3 mask;

	cl_int hit_ind;
	cl_int pixel_id;
	cl_int bounce_count;
	cl_int status;
	cl_uint seed0;
	cl_uint seed1;
}				gpu_ray;

typedef struct s_gpu_camera {
	cl_float3 origin;
	cl_float3 focus;
	cl_float3 d_x;
	cl_float3 d_y;
	cl_int width;
}				gpu_camera;

cl_float3 *gpu_render(Scene *S, t_camera cam, int xdim, int ydim, unsigned int samples, int first)
{
	//jank!
	samples *= 12;

	static gpu_context *CL;
	if (!CL)
		CL = prep_gpu();
	else if (first)
		recompile(CL);
	static gpu_scene *scene;
	if (!scene)
		scene = prep_scene(S, CL, xdim, ydim);
	else
		reseed(scene);

	gpu_camera gcam;
	gcam.focus = cam.focus;
	gcam.origin = cam.origin;
	gcam.d_x = cam.d_x;
	gcam.d_y = cam.d_y;
	gcam.width = xdim;

	//for simplicity assuming one platform for now. can easily be extended, see old gpu_launch.c

	size_t worksize = xdim * ydim;
	size_t localsize = 32;
	size_t sample_max = samples;
	size_t width = xdim;

	//per-platform pointers
	cl_mem d_vertexes;
	cl_mem d_tex_coords;
	cl_mem d_normal;
	cl_mem d_tangent;
	cl_mem d_bitangent;
	cl_mem d_boxes;
	cl_mem d_materials;
	cl_mem d_tex;
	cl_mem d_material_indices;
	cl_mem d_lights;
	
	//per-platform
	d_vertexes = 		clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_float3) * scene->tri_count, scene->V, NULL);
	d_tex_coords = 		clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_float3) * scene->tri_count, scene->T, NULL);
	d_normal = 			clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_float3) * scene->tri_count, scene->N, NULL);
	d_tangent = 		clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_float3) * scene->tri_count / 3, scene->TN, NULL);
	d_bitangent = 		clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_float3) * scene->tri_count / 3, scene->BTN, NULL);
	d_boxes = 			clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(gpu_bin) * scene->bin_count, scene->bins, NULL);
	d_materials = 		clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(gpu_mat) * scene->mat_count, scene->mats, NULL);
	d_tex = 			clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_uchar) * scene->tex_size, scene->tex, NULL);
	d_material_indices= clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_int) * scene->tri_count / 3, scene->M, NULL);
	d_lights = 			clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_int3) * scene->light_count, scene->lights, NULL);

	//per-device
	cl_mem *d_outputs = calloc(CL->numDevices, sizeof(cl_mem));
	cl_mem *d_seeds = 	calloc(CL->numDevices, sizeof(cl_mem));
	cl_mem *d_rays = 	calloc(CL->numDevices, sizeof(cl_mem));
	cl_mem *d_counts = 	calloc(CL->numDevices, sizeof(cl_mem));
	
	cl_float3 *blank_output = calloc(worksize, sizeof(cl_float3));
	gpu_ray *empty_rays = calloc(worksize, sizeof(gpu_ray));
	cl_int *zero_counts = calloc(worksize, sizeof(cl_int));


	cl_uint d;
	clGetDeviceIDs(CL->platform[0], CL_DEVICE_TYPE_GPU, 0, NULL, &d);
	for (int i = 0; i < d; i++)
	{
		d_seeds[i] = clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_uint) * 2 * worksize, &scene->seeds[2 * worksize * i], NULL);
		d_outputs[i] = clCreateBuffer(CL->contexts[0], CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_float3) * worksize, blank_output, NULL);
		d_rays[i] = clCreateBuffer(CL->contexts[0], CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(gpu_ray) * worksize, empty_rays, NULL);
		d_counts[i] = clCreateBuffer(CL->contexts[0], CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(cl_int) * worksize, zero_counts, NULL);
	}

	free(blank_output);
	free(empty_rays);
	free(zero_counts);

	for (int i = 0; i < d; i++)
		clFinish(CL->commands[i]);

	//printf("everything should be alloced and copied??\n");

	cl_int err;

	cl_kernel *collect = calloc(d, sizeof(cl_kernel));
	cl_kernel *traverse = calloc(d, sizeof(cl_kernel));
	cl_kernel *fetch = calloc(d, sizeof(cl_kernel));
	cl_kernel *bounce = calloc(d, sizeof(cl_kernel));
	cl_kernel *nee = calloc(d, sizeof(cl_kernel));

	for (int i = 0; i < d; i++)
	{
		collect[i] = clCreateKernel(CL->programs[0], "collect", &err);
		traverse[i] = clCreateKernel(CL->programs[0], "traverse", &err);
		fetch[i] = clCreateKernel(CL->programs[0], "fetch", &err);
		bounce[i] = clCreateKernel(CL->programs[0], "bounce", &err);
		nee[i] = clCreateKernel(CL->programs[0], "nee", &err);
	}

	//printf("made kernels!\n");


	//connect arguments
	for (int i = 0; i < d; i++)
	{
		/*
		__kernel void collect(	__global Ray *rays,
								__global uint *seeds,
								const Camera cam,
								__global float3 *output,
								__global int *sample_counts,
								const int sample_max)
		*/
		clSetKernelArg(collect[i], 0, sizeof(cl_mem), &d_rays[i]);
		clSetKernelArg(collect[i], 1, sizeof(cl_mem), &d_seeds[i]);
		clSetKernelArg(collect[i], 2, sizeof(gpu_camera), &gcam);
		clSetKernelArg(collect[i], 3, sizeof(cl_mem), &d_outputs[i]);
		clSetKernelArg(collect[i], 4, sizeof(cl_mem), &d_counts[i]);
		clSetKernelArg(collect[i], 5, sizeof(cl_int), &sample_max);
		/*
		__kernel void traverse(	__global Ray *rays,
								__global Box *boxes,
								__global float3 *V)
		*/
		clSetKernelArg(traverse[i], 0, sizeof(cl_mem), &d_rays[i]);
		clSetKernelArg(traverse[i], 1, sizeof(cl_mem), &d_boxes);
		clSetKernelArg(traverse[i], 2, sizeof(cl_mem), &d_vertexes);

		/*
		__kernel void fetch(	__global Ray *rays,
								__global float3 *V,
								__global float3 *T,
								__global float3 *N,
								__global float3 *TN,
								__global float3 *BTN,
								__global Material *mats,
								__global int *M,
								__global uchar *tex)
		*/
		clSetKernelArg(fetch[i], 0, sizeof(cl_mem), &d_rays[i]);
		clSetKernelArg(fetch[i], 1, sizeof(cl_mem), &d_vertexes);
		clSetKernelArg(fetch[i], 2, sizeof(cl_mem), &d_tex_coords);
		clSetKernelArg(fetch[i], 3, sizeof(cl_mem), &d_normal);
		clSetKernelArg(fetch[i], 4, sizeof(cl_mem), &d_tangent);
		clSetKernelArg(fetch[i], 5, sizeof(cl_mem), &d_bitangent);
		clSetKernelArg(fetch[i], 6, sizeof(cl_mem), &d_materials);
		clSetKernelArg(fetch[i], 7, sizeof(cl_mem), &d_material_indices);
		clSetKernelArg(fetch[i], 8, sizeof(cl_mem), &d_tex);

		/*
		__kernel void bounce( 	__global Ray *rays)
		*/
		clSetKernelArg(bounce[i], 0, sizeof(cl_mem), &d_rays[i]);

		/*
		__kernel void nee(		__global Ray *rays,
								__global Box *boxes,
								__global float3 *V,
								__global int3 *lights,
								const uint light_count)
		*/

		clSetKernelArg(nee[i], 0, sizeof(cl_mem), &d_rays[i]);
		clSetKernelArg(nee[i], 1, sizeof(cl_mem), &d_boxes);
		clSetKernelArg(nee[i], 2, sizeof(cl_mem), &d_vertexes);
		clSetKernelArg(nee[i], 3, sizeof(cl_mem), &d_lights);
		clSetKernelArg(nee[i], 4, sizeof(cl_uint), &scene->light_count);
		clSetKernelArg(nee[i], 5, sizeof(cl_mem), &d_normal);
	}


	//printf("any key to launch\n");
	//getchar();
	//ACTUAL LAUNCH TIME
	cl_event begin, finish;
	cl_ulong start, end;
	// cl_event collectE, traverseE, fetchE, bounceE;
	for (int i = 0; i < d; i++)
		for (int j = 0; j < samples; j++)
		{
			clEnqueueNDRangeKernel(CL->commands[i], collect[i], 1, 0, &worksize, &localsize, 0, NULL, j == 0 ? &begin: NULL);
			clEnqueueNDRangeKernel(CL->commands[i], traverse[i], 1, 0, &worksize, &localsize, 0, NULL, NULL);
			clEnqueueNDRangeKernel(CL->commands[i], fetch[i], 1, 0, &worksize, &localsize, 0, NULL, NULL);
			clEnqueueNDRangeKernel(CL->commands[i], nee[i], 1, 0, &worksize, &localsize, 0, NULL, NULL);
			clEnqueueNDRangeKernel(CL->commands[i], bounce[i], 1, 0, &worksize, &localsize, 0, NULL, j == samples - 1 ? &finish : NULL);
			
		}

	for (int i = 0; i < d; i++)
		clFlush(CL->commands[i]);

	for (int i = 0; i < d; i++)
		clFinish(CL->commands[i]);

	clGetEventProfilingInfo(begin, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
	clGetEventProfilingInfo(finish, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
	printf("took %.3f seconds\n", (float)(end - start) / 1000000000.0f);
	clReleaseEvent(begin);
	clReleaseEvent(finish);

	cl_float3 **outputs = calloc(CL->numDevices, sizeof(cl_float3 *));
	for (int i = 0; i < CL->numDevices; i++)
		outputs[i] = calloc(worksize, sizeof(cl_float3));

	cl_int **counts = calloc(CL->numDevices, sizeof(cl_int *));
	for (int i = 0; i < CL->numDevices; i++)
		counts[i] = calloc(worksize, sizeof(cl_int));

	for (int i = 0; i < d; i++)
	{
		clEnqueueReadBuffer(CL->commands[i], d_outputs[i], CL_TRUE, 0, worksize * sizeof(cl_float3), outputs[i], 0, NULL, NULL);
		clEnqueueReadBuffer(CL->commands[i], d_counts[i], CL_TRUE, 0, worksize * sizeof(cl_int), counts[i], 0, NULL, NULL);
	}
	//printf("read complete\n");

	clReleaseMemObject(d_vertexes);
	clReleaseMemObject(d_tex_coords);
	clReleaseMemObject(d_normal);
	clReleaseMemObject(d_tangent);
	clReleaseMemObject(d_bitangent);
	clReleaseMemObject(d_boxes);
	clReleaseMemObject(d_materials);
	clReleaseMemObject(d_tex);
	clReleaseMemObject(d_material_indices);
	for (int i = 0; i < d; i++)
	{
		//kernels
		clReleaseKernel(collect[i]);
		clReleaseKernel(traverse[i]);
		clReleaseKernel(fetch[i]);
		clReleaseKernel(bounce[i]);

		//per-device buffers
		clReleaseMemObject(d_outputs[i]);
		clReleaseMemObject(d_seeds[i]);
		clReleaseMemObject(d_rays[i]);
		clReleaseMemObject(d_counts[i]);
	}

	free(collect);
	free(traverse);
	free(fetch);
	free(bounce);
	free(d_outputs);
	free(d_seeds);
	free(d_rays);
	free(d_counts);
	cl_float3 *output = calloc(worksize, sizeof(cl_float3));
	cl_int *count = calloc(worksize, sizeof(cl_int));
	for (int i = 0; i < d; i++)
		for (int j = 0; j < worksize; j++)
		{
			output[j] = vec_add(output[j], outputs[i][j]);
			count[j] += counts[i][j];
			if (i == d - 1 && count[j] != 0)
				output[j] = vec_scale(output[j], 1.0f / (float)count[j]);
		}
	for (int i = 0; i < CL->numDevices; i++)
	{
		free(counts[i]);
		free(outputs[i]);
	}
	free(count);
	free(counts);
	free(outputs);

	return output;
}
