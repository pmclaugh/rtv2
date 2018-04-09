#include "rt.h"

void		set_camera(t_camera *cam, float win_dim)
{
	cam->focus = vec_add(cam->pos, vec_scale(cam->dir, cam->dist));

	//horizontal reference
	if (fabs(cam->dir.x) < .00001)
		cam->hor_ref = (cam->dir.z > 0) ? UNIT_X : vec_scale(UNIT_X, -1);
	else
		cam->hor_ref = unit_vec(cross((cl_float3){cam->dir.x, 0, cam->dir.z}, (cl_float3){0, -1, 0}));
	//vertical reference
	if (fabs(cam->dir.y) < .00001)
		cam->vert_ref = UNIT_Y;
	else
		cam->vert_ref = unit_vec(cross(cam->dir, cam->hor_ref));
	// if (camera_y.y < 0) //when vertical plane reference points down
	// 	camera_y.y *= -1;
	
	//pixel deltas
	cam->d_x = vec_scale(cam->hor_ref, cam->width / win_dim);
	cam->d_y = vec_scale(cam->vert_ref, cam->height / win_dim);

	//start at bottom corner (the plane's "origin")
	cam->origin = vec_sub(cam->pos, vec_scale(cam->hor_ref, cam->width / 2.0));
	cam->origin = vec_sub(cam->origin, vec_scale(cam->vert_ref, cam->height / 2.0));

	if (cam->dir.x == 0 && cam->dir.z == 0)
		cam->angle_x = 3 * M_PI / 2;
	else
		cam->angle_x = atan2(cam->dir.z, cam->dir.x);
}

t_camera	init_camera(void)
{
	t_camera	cam;
	//cam.pos = (cl_float3){-400.0, 50.0, -220.0}; //reference vase view (1,0,0)
	//cam.pos = (cl_float3){-540.0, 150.0, 380.0}; //weird wall-hole (0,0,1)
	cam.pos = (cl_float3){800.0, 450.0, 0.0}; //standard high perspective on curtain
	//cam.pos = (cl_float3){-800.0, 600.0, 350.0}; upstairs left
	//cam.pos = (cl_float3){800.0, 100.0, 350.0}; //down left
	//cam.pos = (cl_float3){900.0, 150.0, -35.0}; //lion
	//cam.pos = (cl_float3){-250.0, 100.0, 0.0};
	cam.dir = unit_vec((cl_float3){-1.0, 0.0, 0.0});
	cam.width = 1.0;
	cam.height = 1.0;
	cam.angle_x = 0;
	cam.angle_y = 0;
	//determine a focus point in front of the view-plane
	//such that the edge-focus-edge vertex has angle H_FOV

	cam.dist = (cam.width / 2.0) / tan(H_FOV / 2.0);
	return cam;
}