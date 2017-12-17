#include "rt.h"
#include <fcntl.h>

#define SAMPLES_PER_DEVICE 100

char *load_cl_file(char *file)
{
	int fd = open(file, O_RDONLY);
	char *source = calloc(sizeof(char),30000); // real kernel is about 8k characters, whatever
	read(fd, source, 30000);
	close(fd);
	return source;
}

gpu_scene *prep_scene(Scene *s, gpu_context *CL, int xdim, int ydim)
{
	//FACES
	cl_float3 *V, *T, *N;
	V = calloc(s->face_count * 3, sizeof(cl_float3));
	T = calloc(s->face_count * 3, sizeof(cl_float3));
	N = calloc(s->face_count * 3, sizeof(cl_float3));

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
	}

	//SEEDS
	cl_uint *h_seeds = calloc(xdim * ydim * 2 * CL->numDevices, sizeof(cl_uint));
	for (int i = 0; i < xdim * ydim * 2 * CL->numDevices; i++)
		h_seeds[i] = rand();


	//TEXTURES
	cl_int tex_size = 0;
	for (int i = 0; i < s->mat_count; i++)
		if (s->materials[i].map_Kd)
			tex_size += s->materials[i].map_Kd->height * s->materials[i].map_Kd->width * 3;

	cl_uchar *h_tex = calloc(sizeof(cl_uchar), tex_size);
	tex_size = 0;

	gpu_mat *simple_mats = calloc(s->mat_count, sizeof(gpu_mat));
	for (int i = 0; i < s->mat_count; i++)
	{
		if (s->materials[i].map_Kd)
		{
			memcpy(&h_tex[tex_size], s->materials[i].map_Kd->pixels, s->materials[i].map_Kd->height * s->materials[i].map_Kd->width * 3);
			simple_mats[i] = (gpu_mat){
				s->materials[i].Ka,
				s->materials[i].Kd,
				s->materials[i].Ks,
				s->materials[i].Ke,
				tex_size,
				s->materials[i].map_Kd->width,
				s->materials[i].map_Kd->height
			};
			tex_size += s->materials[i].map_Kd->height * s->materials[i].map_Kd->width * 3;
		}
		else
		{
			printf("tex for material %d is null!\n", i);
			simple_mats[i] = (gpu_mat){
				s->materials[i].Ka,
				s->materials[i].Kd,
				s->materials[i].Ks,
				s->materials[i].Ke,
				0,
				0,
				0,
			};
		}
	}

	//BINS
	gpu_bin *flat_bvh = flatten_bvh(s->bins, s->bin_count);

	//COMBINE
	gpu_scene *gs = calloc(1, sizeof(gpu_scene));
	*gs = (gpu_scene){V, T, N, s->face_count * 3, flat_bvh, s->bin_count, h_seeds, xdim * ydim * 2 * CL->numDevices};
	return gs;
}

gpu_context *prep_gpu(void)
{
	printf("prepping for GPU launch\n");
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
        err = clGetDeviceIDs(gpu->platform[i], CL_DEVICE_TYPE_ALL, 0, NULL, &d);
        if (err == CL_SUCCESS)
            gpu->numDevices += d;
    }

    printf("%u devices, %u platforms\n", gpu->numDevices, gpu->numPlatforms);

    //get ids for devices and create (platforms) compute contexts, (devices) command queues
    cl_device_id device_ids[gpu->numDevices];
    cl_uint offset = 0;
    gpu->contexts = calloc(gpu->numPlatforms, sizeof(cl_context));
    gpu->commands = calloc(gpu->numDevices, sizeof(cl_command_queue));
    for (int i = 0; i < gpu->numPlatforms; i++)
    {
    	cl_uint d;
    	clGetDeviceIDs(gpu->platform[i], CL_DEVICE_TYPE_GPU, gpu->numDevices, &device_ids[offset], &d);
    	gpu->contexts[i] = clCreateContext(0, d, &device_ids[offset], NULL, NULL, &err);
    	for (int j = 0; j < d; j++)
    		gpu->commands[offset + j] = clCreateCommandQueue(gpu->contexts[i], device_ids[offset + j], CL_QUEUE_PROFILING_ENABLE, &err);
    	offset += d;
    }
    printf("made contexts and CQs\n");

    char *source = load_cl_file("new_kernel.cl");


    //create (platforms) programs and build them
    gpu->programs = calloc(gpu->numPlatforms, sizeof(cl_program));
    for (int i = 0; i < gpu->numPlatforms; i++)
    {
    	gpu->programs[i] = clCreateProgramWithSource(gpu->contexts[i], 1, (const char **) &source, NULL, &err);
    	err = clBuildProgram(gpu->programs[i], 0, NULL, NULL, NULL, NULL);
    	if (err != CL_SUCCESS)
    	{
    	 	printf("bad compile\n");
    	 	exit(0);
    	}
	    free(source);
    }
    return gpu;
}