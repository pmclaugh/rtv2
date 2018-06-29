#include "rt.h"

void		set_camera(t_camera *cam, float win_dim)
{
	cam->focus = vec_add(cam->pos, vec_scale(cam->dir, cam->dist));

	cl_float3 camera_x, camera_y;
	//horizontal reference
	if (fabs(cam->dir.x) < .00001)
		camera_x = (cam->dir.z > 0) ? UNIT_X : vec_scale(UNIT_X, -1);
	else
		camera_x = unit_vec(cross((cl_float3){cam->dir.x, 0, cam->dir.z}, (cl_float3){0, -1, 0}));
	//vertical reference
	if (fabs(cam->dir.y) < .00001)
		camera_y = UNIT_Y;
	else
		camera_y = unit_vec(cross(cam->dir, camera_x));
	// if (camera_y.y < 0) //when vertical plane reference points down
	// 	camera_y.y *= -1;
	
	//pixel deltas
	cam->d_x = vec_scale(camera_x, cam->width / win_dim);
	cam->d_y = vec_scale(camera_y, cam->height / win_dim);

	//start at bottom corner (the plane's "origin")
	cam->origin = vec_sub(cam->pos, vec_scale(camera_x, cam->width / 2.0));
	cam->origin = vec_sub(cam->origin, vec_scale(camera_y, cam->height / 2.0));

	cam->angle_x = atan2(cam->dir.z, cam->dir.x);
}

t_camera	init_camera(void)
{
	t_camera	cam;

	cam.pos = (cl_float3){0.0, 0.0, 0.0};
	cam.dir = unit_vec((cl_float3){-1.0, 0.0, 0.0});
	cam.width = 1.0;
	cam.height = 1.0;
	cam.angle_x = 0;
	cam.angle_y = 0;
	cam.dist = (cam.width / 2.0) / tan(H_FOV / 2.0);
	
	return cam;
}