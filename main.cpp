// math_funcs, skybox related operations from Anton: https://github.com/capnramses/antons_opengl_tutorials_book
// image_loading functions (std_image) from https://github.com/nothings/stb/blob/master/stb_image.h
// 3D model jupi.obj is a sphere generated in Blender

#include "stdafx.h"
#define _CRT_SECURE_NO_DEPRECATE
#include <stdio.h>
//Some Windows Headers (For Time, IO, etc.)
#include <windows.h>
#include <mmsystem.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <iostream>
#include "maths_funcs.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <stdlib.h>

// Macro for indexing vertex buffer
#define BUFFER_OFFSET(i) ((char *)NULL + (i))

using namespace std;

//window
int width = 1200;
int height = 900;

//VBO/VAO
unsigned int vao = 0;
int pointCount = 0;

//time factor
GLfloat bumpD = 0.0; 

//shader skybox
GLuint shaderSB;
GLuint cubeVAO;
GLuint cubeTex;
mat4 skyboxR = identity_mat4();

// shader jupi
GLuint shaderTx[8]; //8different shaders
int i = 0; //index for shaders, show no noise shader by default
GLuint tex;


// camera: look at function at z axis
float z = -5.0; 

// keyboard events
bool key_x_pressed = false;
bool key_z_pressed = false;
bool key_c_pressed = false;
bool key_v_pressed = false;
bool key_b_pressed = false;
bool key_n_pressed = false;
bool key_d_pressed = false;
bool key_f_pressed = false;
bool key_g_pressed = false;
bool key_h_pressed = false;
bool key_s_pressed = false;
bool key_a_pressed = false;
bool key_w_pressed = false;
bool key_q_pressed = false;




#pragma region TEXTURE_FUNCTIONS
bool load_texture(const char *file_name, GLuint *tex) {
	int x, y, n;
	int force_channels = 4;
	unsigned char *image_data = stbi_load(file_name, &x, &y, &n, force_channels);
	if (!image_data) {
		fprintf(stderr, "ERROR: could not load %s\n", file_name);
		return false;
	}
	// NPOT check
	if ((x & (x - 1)) != 0 || (y & (y - 1)) != 0) {
		fprintf(stderr, "WARNING: texture %s is not power-of-2 dimensions\n",
			file_name);
	}
	int width_in_bytes = x * 4;
	unsigned char *top = NULL;
	unsigned char *bottom = NULL;
	unsigned char temp = 0;
	int half_height = y / 2;

	for (int row = 0; row < half_height; row++) {
		top = image_data + row * width_in_bytes;
		bottom = image_data + (y - row - 1) * width_in_bytes;
		for (int col = 0; col < width_in_bytes; col++) {
			temp = *top;
			*top = *bottom;
			*bottom = temp;
			top++;
			bottom++;
		}
	}
	glGenTextures(1, tex);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, *tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE,
		image_data);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	GLfloat max_aniso = 0.0f;
	glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_aniso);
	// set the maximum!
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, max_aniso);
	return true;
}
#pragma endregion TEXTURE_FUNCTIONS


// Shader Functions- click on + to expand
#pragma region SHADER_FUNCTIONS

// Create a NULL-terminated string by reading the provided file
char* readShaderSource(const char* shaderFile) {
	FILE* fp = fopen(shaderFile, "rb"); //!->Why does binary flag "RB" work and not "R"... wierd msvc thing?

	if (fp == NULL) { return NULL; }

	fseek(fp, 0L, SEEK_END);
	long size = ftell(fp);

	fseek(fp, 0L, SEEK_SET);
	char* buf = new char[size + 1];
	fread(buf, 1, size, fp);
	buf[size] = '\0';

	fclose(fp);

	return buf;
}


static void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
	// create a shader object
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0) {
		fprintf(stderr, "Error creating shader type %d\n", ShaderType);
		exit(0);
	}
	const char* pShaderSource = readShaderSource(pShaderText);

	// Bind the source code to the shader, this happens before compilation
	glShaderSource(ShaderObj, 1, (const GLchar**)&pShaderSource, NULL);
	// compile the shader and check for errors
	glCompileShader(ShaderObj);
	GLint success;
	// check for shader related errors using glGetShaderiv
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar InfoLog[1024];
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		fprintf(stderr, "Error compiling shader type %d: '%s'\n", ShaderType, InfoLog);
		exit(1);
	}
	// Attach the compiled shader object to the program object
	glAttachShader(ShaderProgram, ShaderObj);
}

GLuint CompileShaders(GLuint shaderProgramID, const char *vs, const char *fs)
{
	//Start the process of setting up our shaders by creating a program ID
	//Note: we will link all the shaders together into this ID
	shaderProgramID = glCreateProgram();
	if (shaderProgramID == 0) {
		fprintf(stderr, "Error creating shader program\n");
		exit(1);
	}

	// Create two shader objects, one for the vertex, and one for the fragment shader
	AddShader(shaderProgramID, vs, GL_VERTEX_SHADER);
	AddShader(shaderProgramID, fs, GL_FRAGMENT_SHADER);

	GLint Success = 0;
	GLchar ErrorLog[1024] = { 0 };
	// After compiling all shader objects and attaching them to the program, we can finally link it
	glLinkProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &Success);
	if (Success == 0) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		fprintf(stderr, "Error linking shader program: '%s'\n", ErrorLog);
		exit(1);
	}

	// program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
	glValidateProgram(shaderProgramID);
	// check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_VALIDATE_STATUS, &Success);
	if (!Success) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		fprintf(stderr, "Invalid shader program: '%s'\n", ErrorLog);
		exit(1);
	}
	// Finally, use the linked shader program
	// Note: this program will stay in effect for all draw calls until you replace it with another or explicitly disable its use
	glUseProgram(shaderProgramID);
	return shaderProgramID;
}
#pragma endregion SHADER_FUNCTIONS

#pragma region SKYBOX
/* big cube. returns Vertex Array Object */
GLuint make_big_cube() {
	float points[] = {
		-10.0f, 10.0f,	-10.0f, -10.0f, -10.0f, -10.0f, 10.0f,	-10.0f, -10.0f,
		10.0f,	-10.0f, -10.0f, 10.0f,	10.0f,	-10.0f, -10.0f, 10.0f,	-10.0f,

		-10.0f, -10.0f, 10.0f,	-10.0f, -10.0f, -10.0f, -10.0f, 10.0f,	-10.0f,
		-10.0f, 10.0f,	-10.0f, -10.0f, 10.0f,	10.0f,	-10.0f, -10.0f, 10.0f,

		10.0f,	-10.0f, -10.0f, 10.0f,	-10.0f, 10.0f,	10.0f,	10.0f,	10.0f,
		10.0f,	10.0f,	10.0f,	10.0f,	10.0f,	-10.0f, 10.0f,	-10.0f, -10.0f,

		-10.0f, -10.0f, 10.0f,	-10.0f, 10.0f,	10.0f,	10.0f,	10.0f,	10.0f,
		10.0f,	10.0f,	10.0f,	10.0f,	-10.0f, 10.0f,	-10.0f, -10.0f, 10.0f,

		-10.0f, 10.0f,	-10.0f, 10.0f,	10.0f,	-10.0f, 10.0f,	10.0f,	10.0f,
		10.0f,	10.0f,	10.0f,	-10.0f, 10.0f,	10.0f,	-10.0f, 10.0f,	-10.0f,

		-10.0f, -10.0f, -10.0f, -10.0f, -10.0f, 10.0f,	10.0f,	-10.0f, -10.0f,
		10.0f,	-10.0f, -10.0f, -10.0f, -10.0f, 10.0f,	10.0f,	-10.0f, 10.0f
	};
	GLuint vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, 3 * 36 * sizeof(GLfloat), &points,
		GL_STATIC_DRAW);

	GLuint vao;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

	printf("cube loaded\n");

	return vao;
}

/* use stb_image to load an image file into memory, and then into one side of
a cube-map texture. */
bool load_cube_map_side(GLuint texture, GLenum side_target,
	const char *file_name) {
	glBindTexture(GL_TEXTURE_CUBE_MAP, texture);

	int x, y, n;
	int force_channels = 4;
	unsigned char *image_data = stbi_load(file_name, &x, &y, &n, force_channels);
	if (!image_data) {
		fprintf(stderr, "ERROR: could not load %s\n", file_name);
		return false;
	}
	// non-power-of-2 dimensions check
	if ((x & (x - 1)) != 0 || (y & (y - 1)) != 0) {
		fprintf(stderr, "WARNING: image %s is not power-of-2 dimensions\n",
			file_name);
	}

	// copy image data into 'target' side of cube map
	glTexImage2D(side_target, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE,
		image_data);
	free(image_data);
	return true;
}

/* load all 6 sides of the cube-map from images, then apply formatting to the
final texture */
void create_cube_map(const char *front,GLuint *tex_cube) { //half-cooked skybox with only one same tecture
	// generate a cube-map texture to hold all the sides
	glActiveTexture(GL_TEXTURE0);
	glGenTextures(1, tex_cube);

	// load each image and copy into a side of the cube-map texture
	(load_cube_map_side(*tex_cube, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z, front));
	(load_cube_map_side(*tex_cube, GL_TEXTURE_CUBE_MAP_POSITIVE_Z, front));
	(load_cube_map_side(*tex_cube, GL_TEXTURE_CUBE_MAP_POSITIVE_Y, front));
	(load_cube_map_side(*tex_cube, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y, front));
	(load_cube_map_side(*tex_cube, GL_TEXTURE_CUBE_MAP_NEGATIVE_X, front));
	(load_cube_map_side(*tex_cube, GL_TEXTURE_CUBE_MAP_POSITIVE_X, front));
	// format cube map texture
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	printf("cube map loaded\n");
}

#pragma endregion SKYBOX

#pragma region VBO_FUNCTIONS
// VBO Functions - following Anton's code
// https://github.com/capnramses/antons_opengl_tutorials_book/tree/master/20_normal_mapping


int generateObjectBuffer(const char *file_name) {

	int point_count = 0;
	float *points = NULL;
	float *normals = NULL;
	float *texcoords = NULL;
	float *vtans = NULL;

	Assimp::Importer importer;

	const aiScene* scene = importer.ReadFile(file_name,
		aiProcess_Triangulate | aiProcess_CalcTangentSpace);

	if (!scene) {
		fprintf(stderr, "ERROR: reading mesh %s\n", file_name);
		return 0;
	}
	fprintf(stderr, "reading mesh");
	printf("  %i animations\n", scene->mNumAnimations);
	printf("  %i cameras\n", scene->mNumCameras);
	printf("  %i lights\n", scene->mNumLights);
	printf("  %i materials\n", scene->mNumMaterials);
	printf("  %i meshes\n", scene->mNumMeshes);
	printf("  %i textures\n", scene->mNumTextures);

	/* get first mesh in file only */
	const aiMesh *mesh = scene->mMeshes[0];
	printf("    %i vertices in mesh[0]\n", mesh->mNumVertices);

	/* pass back number of vertex points in mesh */
	point_count = mesh->mNumVertices;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);


	if (mesh->HasPositions()) {

		points = (float *)malloc(point_count * 3 * sizeof(float));
		for (int i = 0; i < point_count; i++) {
			const aiVector3D *vp = &(mesh->mVertices[i]);
			points[i * 3] = (float)vp->x;
			points[i * 3 + 1] = (float)vp->y;
			points[i * 3 + 2] = (float)vp->z;
		}

		GLuint vp_vbo = 0;
		glGenBuffers(1, &vp_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vp_vbo);
		glBufferData(GL_ARRAY_BUFFER, 3 * point_count * sizeof(float), points, GL_STATIC_DRAW);

		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		free(points);

	}
	if (mesh->HasNormals()) {
		normals = (float *)malloc(point_count * 3 * sizeof(float));
		for (int i = 0; i < point_count; i++) {
			const aiVector3D *vn = &(mesh->mNormals[i]);
			normals[i * 3] = (float)vn->x;
			normals[i * 3 + 1] = (float)vn->y;
			normals[i * 3 + 2] = (float)vn->z;
		}

		GLuint vn_vbo = 0;
		glGenBuffers(1, &vn_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vn_vbo);
		glBufferData(GL_ARRAY_BUFFER, 3 * point_count * sizeof(float), normals, GL_STATIC_DRAW);

		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, NULL);

		free(normals);
	}
	if (mesh->HasTextureCoords(0)) {
		printf("loading uv \n");
		texcoords = (float *)malloc(point_count * 2 * sizeof(float));
		for (int i = 0; i < point_count; i++) {
			const aiVector3D *vt = &(mesh->mTextureCoords[0][i]);
			texcoords[i * 2] = (float)vt->x;
			texcoords[i * 2 + 1] = (float)vt->y;
		}

		GLuint vt_vbo = 0;
		glGenBuffers(1, &vt_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, vt_vbo);
		glBufferData(GL_ARRAY_BUFFER, 2 * point_count * sizeof(float), texcoords, GL_STATIC_DRAW);

		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, NULL);

		free(texcoords);
	}

	if (mesh->HasTangentsAndBitangents()) {
		printf("loading tg \n");
		vtans = (float *)malloc(point_count * 4 * sizeof(float));
		for (int i = 0; i < point_count; i++) {
			const aiVector3D *tangent = &(mesh->mTangents[i]);
			const aiVector3D *bitangent = &(mesh->mBitangents[i]);
			const aiVector3D *normal = &(mesh->mNormals[i]);

			// put the three vectors into my vec3 struct format for doing maths
			vec3 t(tangent->x, tangent->y, tangent->z);
			vec3 n(normal->x, normal->y, normal->z);
			vec3 b(bitangent->x, bitangent->y, bitangent->z);
			// orthogonalise and normalise the tangent so we can use it in something
			// approximating a T,N,B inverse matrix
			vec3 t_i = normalise(t - n * dot(n, t));

			// get determinant of T,B,N 3x3 matrix by dot*cross method
			float det = (dot(cross(n, t), b));
			if (det < 0.0f) {
				det = -1.0f;
			}
			else {
				det = 1.0f;
			}

			// push back 4d vector for inverse tangent with determinant
			vtans[i * 4] = t_i.v[0];
			vtans[i * 4 + 1] = t_i.v[1];
			vtans[i * 4 + 2] = t_i.v[2];
			vtans[i * 4 + 3] = det;

		}

		GLuint tangents_vbo = 0;
		glGenBuffers(1, &tangents_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, tangents_vbo);
		glBufferData(GL_ARRAY_BUFFER, 4 * point_count * sizeof(GLfloat), vtans,
			GL_STATIC_DRAW);
		glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 0, NULL);
		glEnableVertexAttribArray(3);


		free(vtans);
	}

	return point_count;

}


#pragma endregion VBO_FUNCTIONS


void display() {

	// tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable(GL_DEPTH_TEST); // enable depth-testing
	glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"
	glClearColor(0.0, 0.0, 0.0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	//set up time
	static double previous_seconds = glutGet(GLUT_ELAPSED_TIME);
	double current_seconds = glutGet(GLUT_ELAPSED_TIME);
	double deltaT = (current_seconds - previous_seconds) / 0.01;
	previous_seconds = current_seconds;

	//set project mat for the scene
	mat4 persp_proj = perspective(45.0, (float)width / (float)height, 0.1, 100.0);
	//transform
	mat4 view = identity_mat4();


	//camera answers keyboard event zooming in/out
	if (key_z_pressed) {

		z += 0.5;
	}

	if (key_x_pressed) {

		z -= 0.5;
	}

	view = look_at(vec3(0.0, 0.0, z), vec3(0.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0));

	
	//----------------
	// render a skybox using the cube - map texture

	glDepthMask(GL_FALSE);
	glUseProgram(shaderSB);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_CUBE_MAP, cubeTex);
	glBindVertexArray(cubeVAO);

	int cube_V_location = glGetUniformLocation(shaderSB, "V");
	int cube_P_location = glGetUniformLocation(shaderSB, "P");
	int cubeTex_location = glGetUniformLocation(shaderSB, "cubeTex");
	glUniform1i(cubeTex_location, 0);


	// here is third person perspective, so skybox don't move,
	// cube_V_location link to indentity mat
	// first person view, cube_V_location link to  inverse(local1R).m
	
	glUniformMatrix4fv(cube_V_location, 1, GL_FALSE, inverse(skyboxR).m);
	glUniformMatrix4fv(cube_P_location, 1, GL_FALSE, persp_proj.m);


	glDrawArrays(GL_TRIANGLES, 0, 36);
	glDepthMask(GL_TRUE);

	
	//----------------
	//choose shader according to inputs

	if (key_n_pressed) {
		i++;
		if ((i < 0) || (i>7)) { i = 0; }
	}
	if (key_b_pressed) {
		i--;
		if ((i < 0)||(i>7) ){ i = 0; }
	}



	load_texture("../Textures/jupi5.png", &tex);
	pointCount = generateObjectBuffer("../Meshs/jupi.obj");
	glUseProgram(shaderTx[i]);

	//Declare your uniform variables that will be used in your shader
	int matrix_location = glGetUniformLocation(shaderTx[i], "model");
	int view_mat_location = glGetUniformLocation(shaderTx[i], "view");
	int proj_mat_location = glGetUniformLocation(shaderTx[i], "proj");
	int tex_location = glGetUniformLocation(shaderTx[i], "normal_map");
	int bumpDegree = glGetUniformLocation(shaderTx[i], "bumpDegree");
	glUniform1i(tex_location, 0);





	mat4 local = identity_mat4();
	local = rotate_z_deg(local, -30.0);
	local = rotate_x_deg(local,15.0); //  rotate ball itself

	// gloabal is the model matrix
	// for the root, we orient it in global space
	mat4 global = local;


	//uniform var, control time_factor
	bumpD += 1.0;

	// update uniforms & draw
	glViewport(0, 0, width, height);
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, persp_proj.m);
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, view.m);
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, global.m);
	glUniform1f(bumpDegree, (GLfloat)bumpD);
	glDrawArrays(GL_TRIANGLES, 0, pointCount);


	//swap buffer for all 
	glutSwapBuffers();
}



void updateScene() {

	// Placeholder code, if you want to work with framerate
	// Wait until at least 16ms passed since start of last frame (Effectively caps framerate at ~60fps)
	static DWORD  last_time = 0;
	DWORD  curr_time = timeGetTime();
	float  delta = (curr_time - last_time) * 0.001f;
	if (delta > 0.03f)
		delta = 0.03f;
	last_time = curr_time;

	// Draw the next frame
	glutPostRedisplay();
}


void init()
{
	// Set up the shaders
	shaderSB = CompileShaders(shaderSB, "../Shaders/skyboxJupiVS.glsl", "../Shaders/skyboxJupiFS.glsl");
	shaderTx[0]= CompileShaders(shaderTx[0], "../Shaders/JupiVS0.glsl", "../Shaders/JupiFS0.glsl");
	
	shaderTx[1] = CompileShaders(shaderTx[1], "../Shaders/JupiVS0.glsl", "../Shaders/JupiFS1.glsl");
	shaderTx[2] = CompileShaders(shaderTx[2], "../Shaders/JupiVS0.glsl", "../Shaders/JupiFS2.glsl");
	shaderTx[3] = CompileShaders(shaderTx[3], "../Shaders/JupiVS0.glsl", "../Shaders/JupiFS3.glsl");
	shaderTx[4] = CompileShaders(shaderTx[4], "../Shaders/JupiVS0.glsl", "../Shaders/JupiFS4.glsl");
	shaderTx[5] = CompileShaders(shaderTx[5], "../Shaders/JupiVS0.glsl", "../Shaders/JupiFS5.glsl");
	shaderTx[6] = CompileShaders(shaderTx[6], "../Shaders/JupiVS0.glsl", "../Shaders/JupiFS6.glsl");
	shaderTx[7] = CompileShaders(shaderTx[7], "../Shaders/JupiVS0.glsl", "../Shaders/JupiFS7.glsl");
	
	
	//create cube map
	cubeVAO = make_big_cube();
	//create_cube_map("../Textures/space_up.jpg", &cubeTex);
	create_cube_map("../Textures/Space_skybox.jpg", &cubeTex);


	//add in other textures
	//load_texture("../Textures/texture.png", &tex);
	glEnable(GL_CULL_FACE); // cull face
	glCullFace(GL_BACK);		// cull back face
	glFrontFace(GL_CCW);		// GL_CCW for counter clock-wise


}


#pragma region KEYBOARD
// Placeholder code for the keypress
void keypress(unsigned char key, int x, int y) {

	if (key == 'x') {
		//Translate the base, etc.
		key_x_pressed = true;
		display();
	}

	if (key == 'z') {
		//Translate the base, etc.
		key_z_pressed = true;
		display();
	}

	if (key == 'c') {
		//Rotate the base, etc.
		key_c_pressed = true;
		display();
	}

	if (key == 'v') {
		//Rotate the base, etc.
		key_v_pressed = true;
		display();
	}

	if (key == 'b') {
		//Scale the base, etc.
		key_b_pressed = true;
		display();
	}

	if (key == 'n') {
		//Scale the base, etc.
		key_n_pressed = true;
		display();
	}



	if (key == 's') {
		//Rotate the base, etc.
		key_s_pressed = true;
		display();
	}

	if (key == 'a') {
		//Rotate the base, etc.
		key_a_pressed = true;
		display();
	}

	if (key == 'h') {
		//Rotate the base, etc.
		key_h_pressed = true;
		display();
	}

	if (key == 'd') {
		//Rotate the base, etc.
		key_d_pressed = true;
		display();
	}

	if (key == 'f') {
		//Scale the base, etc.
		key_f_pressed = true;
		display();
	}

	if (key == 'g') {
		//Scale the base, etc.
		key_g_pressed = true;
		display();
	}



	if (key == 'w') {
		//Scale the base, etc.
		key_w_pressed = true;
		display();
	}

	if (key == 'q') {
		//Scale the base, etc.
		key_q_pressed = true;
		display();
	}

}

void keyUp(unsigned char key, int x, int y) {
	if (key == 'x') {
		key_x_pressed = false;
		display();
	}

	if (key == 'z') {
		//Translate the base, etc.
		key_z_pressed = false;
		display();
	}

	if (key == 'c') {
		key_c_pressed = false;
		display();
	}

	if (key == 'v') {
		//Translate the base, etc.
		key_v_pressed = false;
		display();
	}

	if (key == 'b') {
		//Rotate the base, etc.
		key_b_pressed = false;
		display();
	}

	if (key == 'n') {
		//Rotate the base, etc.
		key_n_pressed = false;
		display();
	}




	if (key == 's') {
		//Rotate the base, etc.
		key_s_pressed = false;
		display();
	}

	if (key == 'a') {
		//Rotate the base, etc.
		key_a_pressed = false;
		display();
	}

	if (key == 'd') {
		//Rotate the base, etc.
		key_d_pressed = false;
		display();
	}

	if (key == 'f') {
		//Rotate the base, etc.
		key_f_pressed = false;
		display();
	}


	if (key == 'g') {
		//Rotate the base, etc.
		key_g_pressed = false;
		display();
	}

	if (key == 'h') {
		//Rotate the base, etc.
		key_h_pressed = false;
		display();
	}




	if (key == 'w') {
		//Scale the base, etc.
		key_w_pressed = false;
		display();
	}

	if (key == 'q') {
		//Scale the base, etc.
		key_q_pressed = false;
		display();
	}
}
#pragma endregion KEYBOARD

int main(int argc, char** argv) {

	// Set up the window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize(width, height);
	glutCreateWindow("Transform");

	// Tell glut where the display function is
	glutDisplayFunc(display);
	glutIdleFunc(updateScene);
	// glut keyboard callbacks
	glutKeyboardFunc(keypress);
	glutKeyboardUpFunc(keyUp);

	// A call to glewInit() must be done after glut is initialized!
	GLenum res = glewInit();
	// Check for any errors
	if (res != GLEW_OK) {
		fprintf(stderr, "Error: '%s'\n", glewGetErrorString(res));
		return 1;
	}
	// Set up your objects and shaders
	init();

	// Begin infinite event loop
	glutMainLoop();
	return 0;
}











