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

gpu_scene *prep_scene(Scene *s, gpu_context *CL, int worksize)
{
	//SEEDS
	cl_uint *h_seeds = calloc(worksize * 2 * CL->numDevices * CL->numPlatforms, sizeof(cl_uint));
	for (int i = 0; i < worksize * 2 * CL->numDevices; i++)
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

	cl_int light_poly_count = 0;
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
		if (dot(simple_mats[f.mat_ind].Ke, simple_mats[f.mat_ind].Ke) > 0.0f)
			light_poly_count++;
	}

	printf("lights\n");
	cl_int3 *lights = calloc(light_poly_count, sizeof(cl_int3));
	light_poly_count = 0;
	for (int i = 0; i < s->face_count; i++)
	{
		Face f = s->faces[i];
		if (dot(simple_mats[f.mat_ind].Ke, simple_mats[f.mat_ind].Ke) > 0.0f)
			lights[light_poly_count++] = (cl_int3){i * 3, i * 3 + 1, i * 3 + 2};
	}

	printf("lights done\n");
	//BINS
	gpu_bin *flat_bvh = flatten_bvh(s);

	//COMBINE
	gpu_scene *gs = calloc(1, sizeof(gpu_scene));
	*gs = (gpu_scene){V, T, N, M, TN, BTN, s->face_count * 3, lights, light_poly_count, flat_bvh, s->bin_count, h_tex, tex_size, simple_mats, s->mat_count, h_seeds, worksize * 2 * CL->numDevices * CL->numPlatforms};
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

    char *source = load_cl_file("bdpt_full.cl");
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
    return gpu;
}

void recompile(gpu_context *gpu)
{
	cl_int err;
	//release old program(s)
	for (int i = 0; i < gpu->numPlatforms; i++)
		clReleaseProgram(gpu->programs[i]);

	char *source = load_cl_file("bdpt_full.cl");
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

typedef struct s_gpu_path{
	cl_float3 origin;
	cl_float3 direction;
	cl_float3 normal;
	cl_float3 mask;
}				gpu_path;

typedef struct s_gpu_camera {
	cl_float3 pos;
	cl_float3 origin;
	cl_float3 direction;
	cl_float3 focus;
	cl_float3 d_x;
	cl_float3 d_y;
	cl_int width;
	cl_float focal_length;
	cl_float aperture;
}				gpu_camera;

cl_float3 *gpu_render(Scene *S, t_camera cam, int xdim, int ydim, int samples, int min_bounces, int first, cl_int **count_out)
{
	size_t half_worksize = xdim * ydim;
	size_t worksize =  half_worksize * 2;
	
	size_t localsize = 64;
	size_t sample_max = samples;
	size_t width = xdim;

	static gpu_context *CL;
	if (!CL)
		CL = prep_gpu();
	else if (first)
		recompile(CL);
	static gpu_scene *scene;
	if (!scene)
		scene = prep_scene(S, CL, worksize);
	else
		reseed(scene);

	printf("prep done\n");

	gpu_camera gcam;
	gcam.pos = cam.pos;
	gcam.focus = cam.focus;
	gcam.origin = cam.origin;
	gcam.direction = cam.dir;
	gcam.d_x = cam.d_x;
	gcam.d_y = cam.d_y;
	gcam.width = xdim;
	gcam.focal_length = cam.focal_length;
	gcam.aperture = cam.aperture;

	//for simplicity assuming one platform for now. can easily be extended, see old gpu_launch.c
	
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
	d_lights = 			clCreateBuffer(CL->contexts[0], CL_MEM_READ_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_int3) * scene->light_poly_count, scene->lights, NULL);

	//per-device
	cl_mem *d_outputs = calloc(CL->numDevices, sizeof(cl_mem));
	cl_mem *d_seeds = 	calloc(CL->numDevices, sizeof(cl_mem));
	cl_mem *d_paths = 	calloc(CL->numDevices, sizeof(cl_mem));
	cl_mem *d_counts = 	calloc(CL->numDevices, sizeof(cl_mem));
	
	cl_float3 *blank_output = calloc(worksize, sizeof(cl_float3));
	gpu_path *empty_rays = calloc(worksize * 10, sizeof(gpu_path));
	cl_int *zero_counts = calloc(worksize, sizeof(cl_int));


	cl_uint d;
	clGetDeviceIDs(CL->platform[0], CL_DEVICE_TYPE_GPU, 0, NULL, &d);
	for (int i = 0; i < d; i++)
	{
		d_seeds[i] = clCreateBuffer(CL->contexts[0], CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(cl_uint) * 2 * worksize, &scene->seeds[2 * worksize * i], NULL);
		d_outputs[i] = clCreateBuffer(CL->contexts[0], CL_MEM_WRITE_ONLY | CL_MEM_COPY_HOST_PTR, sizeof(cl_float3) * worksize, blank_output, NULL);
		d_paths[i] = clCreateBuffer(CL->contexts[0], CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(gpu_path) * worksize * 10, empty_rays, NULL);
		d_counts[i] = clCreateBuffer(CL->contexts[0], CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, sizeof(cl_int) * worksize, zero_counts, NULL);
	}

	free(blank_output);
	free(empty_rays);
	free(zero_counts);

	for (int i = 0; i < d; i++)
		clFinish(CL->commands[i]);

	printf("everything should be alloced and copied\n");

	cl_int err;

	// cl_kernel *collect = calloc(d, sizeof(cl_kernel));
	// cl_kernel *traverse = calloc(d, sizeof(cl_kernel));
	// cl_kernel *fetch = calloc(d, sizeof(cl_kernel));
	// cl_kernel *bounce = calloc(d, sizeof(cl_kernel));

	cl_kernel *init_paths = calloc(d, sizeof(cl_kernel));
	cl_kernel *trace_paths = calloc(d, sizeof(cl_kernel));
	cl_kernel *connect_paths = calloc(d, sizeof(cl_kernel));

	for (int i = 0; i < d; i++)
	{
		init_paths[i] = clCreateKernel(CL->programs[0], "init_paths", &err);
		trace_paths[i] = clCreateKernel(CL->programs[0], "trace_paths", &err);
		connect_paths[i] = clCreateKernel(CL->programs[0], "connect_paths", &err);
	}

	//printf("made kernels!\n");


	//connect arguments
	for (int i = 0; i < d; i++)
	{
		/*
		__kernel void init_paths(const Camera cam,
						__global uint *seeds,
						__global Path *paths,
						__global int *path_lengths,
						__global int3 *lights,
						const uint light_poly_count,
						__global float3 *V,
						__global float3 *N,
						__global float3 *output
						)
		*/
		clSetKernelArg(init_paths[i], 0, sizeof(gpu_camera), &gcam);
		clSetKernelArg(init_paths[i], 1, sizeof(cl_mem), &d_seeds[i]);
		clSetKernelArg(init_paths[i], 2, sizeof(cl_mem), &d_paths[i]);
		clSetKernelArg(init_paths[i], 3, sizeof(cl_mem), &d_counts[i]);
		clSetKernelArg(init_paths[i], 4, sizeof(cl_mem), &d_lights);
		clSetKernelArg(init_paths[i], 5, sizeof(cl_uint), &scene->light_poly_count);
		clSetKernelArg(init_paths[i], 6, sizeof(cl_mem), &d_vertexes);
		clSetKernelArg(init_paths[i], 7, sizeof(cl_mem), &d_normal);
		clSetKernelArg(init_paths[i], 8, sizeof(cl_mem), &d_outputs[i]);
		/*
		__kernel void trace_paths(__global Path *paths,
						__global float3 *V,
						__global float3 *T,
						__global float3 *N,
						__global float3 *TN,
						__global float3 *BTN,
						__global int *M,
						__global Material *mats,
						__global uchar *tex,
						__global int *path_lengths,
						__global Box *boxes,
						__global uint *seeds.
						__global float3 *output)
		*/
		clSetKernelArg(trace_paths[i], 0, sizeof(cl_mem), &d_paths[i]);
		clSetKernelArg(trace_paths[i], 1, sizeof(cl_mem), &d_vertexes);
		clSetKernelArg(trace_paths[i], 2, sizeof(cl_mem), &d_tex_coords);
		clSetKernelArg(trace_paths[i], 3, sizeof(cl_mem), &d_normal);
		clSetKernelArg(trace_paths[i], 4, sizeof(cl_mem), &d_tangent);
		clSetKernelArg(trace_paths[i], 5, sizeof(cl_mem), &d_bitangent);
		clSetKernelArg(trace_paths[i], 6, sizeof(cl_mem), &d_material_indices);
		clSetKernelArg(trace_paths[i], 7, sizeof(cl_mem), &d_materials);
		clSetKernelArg(trace_paths[i], 8, sizeof(cl_mem), &d_tex);
		clSetKernelArg(trace_paths[i], 9, sizeof(cl_mem), &d_counts[i]);
		clSetKernelArg(trace_paths[i], 10, sizeof(cl_mem), &d_boxes);
		clSetKernelArg(trace_paths[i], 11, sizeof(cl_mem), &d_seeds[i]);
		clSetKernelArg(trace_paths[i], 12, sizeof(cl_mem), &d_outputs[i]);


		/*
		__kernel void connect_paths(__global Path *paths,
							__global int *path_lengths,
							__global Box *boxes,
							__global float3 *V,
							__global float3 *output)
		*/
		clSetKernelArg(connect_paths[i], 0, sizeof(cl_mem), &d_paths[i]);
		clSetKernelArg(connect_paths[i], 1, sizeof(cl_mem), &d_counts[i]);
		clSetKernelArg(connect_paths[i], 2, sizeof(cl_mem), &d_boxes);
		clSetKernelArg(connect_paths[i], 3, sizeof(cl_mem), &d_vertexes);
		clSetKernelArg(connect_paths[i], 4, sizeof(cl_mem), &d_outputs[i]);
	}


	printf("any key to launch\n");
	// getchar();
	//ACTUAL LAUNCH TIME
	cl_event begin, finish;
	cl_ulong start, end;
	// cl_event collectE, traverseE, fetchE, bounceE;
	for (int i = 0; i < d; i++)
		for (int j = 0; j < samples; j++)
		{
			clEnqueueNDRangeKernel(CL->commands[i], init_paths[i], 1, 0, &worksize, &localsize, 0, NULL, j == 0 ? &begin : NULL);
			clEnqueueNDRangeKernel(CL->commands[i], trace_paths[i], 1, 0, &worksize, &localsize, 0, NULL, NULL);
			// clEnqueueNDRangeKernel(CL->commands[i], connect_paths[i], 1, 0, &half_worksize, &localsize, 0, NULL, j == samples - 1 ? &finish : NULL);
		}

	for (int i = 0; i < d; i++)
		clFlush(CL->commands[i]);

	for (int i = 0; i < d; i++)
		clFinish(CL->commands[i]);

	printf("made it out of kernel\n");

	// clGetEventProfilingInfo(begin, CL_PROFILING_COMMAND_START, sizeof(cl_ulong), &start, NULL);
	// clGetEventProfilingInfo(finish, CL_PROFILING_COMMAND_END, sizeof(cl_ulong), &end, NULL);
	// printf("took %.3f seconds\n", (float)(end - start) / 1000000000.0f);
	// clReleaseEvent(begin);
	// clReleaseEvent(finish);

	printf("timer\n");
	cl_float3 **outputs = calloc(CL->numDevices, sizeof(cl_float3 *));
	for (int i = 0; i < CL->numDevices; i++)
		outputs[i] = calloc(half_worksize, sizeof(cl_float3));

	cl_int **counts = calloc(CL->numDevices, sizeof(cl_int *));
	for (int i = 0; i < CL->numDevices; i++)
		counts[i] = calloc(worksize, sizeof(cl_int));

	for (int i = 0; i < d; i++)
	{
		clEnqueueReadBuffer(CL->commands[i], d_outputs[i], CL_TRUE, 0, half_worksize * sizeof(cl_float3), outputs[i], 0, NULL, NULL);
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
		clReleaseKernel(init_paths[i]);
		clReleaseKernel(trace_paths[i]);
		clReleaseKernel(connect_paths[i]);

		//per-device buffers
		clReleaseMemObject(d_outputs[i]);
		clReleaseMemObject(d_seeds[i]);
		clReleaseMemObject(d_paths[i]);
		clReleaseMemObject(d_counts[i]);
	}

	free(init_paths);
	free(trace_paths);
	free(connect_paths);
	free(d_outputs);
	free(d_seeds);
	free(d_paths);
	free(d_counts);
	cl_float3 *output = calloc(half_worksize, sizeof(cl_float3));
	cl_int *count = calloc(half_worksize, sizeof(cl_int));
	for (int i = 0; i < d; i++)
		for (int j = 0; j < half_worksize; j++)
		{
			output[j] = vec_add(output[j], outputs[i][j]);
			count[j] += counts[i][2 * j];
		}
	for (int i = 0; i < CL->numDevices; i++)
	{
		free(counts[i]);
		free(outputs[i]);
	}
	//free(count);
	free(counts);
	free(outputs);

	*count_out = count;
	return output;
}
