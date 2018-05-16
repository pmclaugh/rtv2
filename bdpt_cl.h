#define PI 3.14159265359f
#define NORMAL_SHIFT 0.0003f
#define COLLISION_ERROR 0.0001f

#define BLACK (float3)(0.0f, 0.0f, 0.0f)
#define WHITE (float3)(1.0f, 1.0f, 1.0f)
#define RED (float3)(0.8f, 0.2f, 0.2f)
#define GREEN (float3)(0.2f, 0.8f, 0.2f)
#define BLUE (float3)(0.2f, 0.2f, 0.8f)
#define GREY (float3)(0.5f, 0.5f, 0.5f)

#define BRIGHTNESS 10000.0f

#define UNIT_X (float3)(1.0f, 0.0f, 0.0f)
#define UNIT_Y (float3)(0.0f, 1.0f, 0.0f)
#define UNIT_Z (float3)(0.0f, 0.0f, 1.0f)

#define RR_PROB 0.1f

typedef struct s_ray {
	float3 origin;
	float3 direction;
	float3 inv_dir;

	float3 color;
	float3 mask;

	int hit_ind;
	int bounce_count;
	int status;
	int type;
}				Ray;

typedef struct s_path {
	float3 origin;
	float3 direction;
	float3 normal;
	float3 mask;
}				Path;

typedef struct s_material
{
	float3 Kd;
	float3 Ks;
	float3 Ke;

	float Ni;
	float Tr;
	float roughness;

	//here's where texture and stuff goes
	int d_index;
	int d_height;
	int d_width;

	int s_index;
	int s_height;
	int s_width;

	int e_index;
	int e_height;
	int e_width;

	int b_index;
	int b_height;
	int b_width;

	int t_index;
	int t_height;
	int t_width;
}				Material;

typedef struct s_camera
{
	float3 pos;
	float3 origin;
	float3 focus;
	float3 d_x;
	float3 d_y;
	int width;
	float focal_length;
	float aperture;
}				Camera;

typedef struct s_box
{
	float min_x;
	float min_y;
	float min_z;
	int l_ind;
	float max_x;
	float max_y;
	float max_z;
	int r_ind;
}				Box;