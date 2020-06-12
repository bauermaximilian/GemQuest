/*
 * Copyright(c) 2020 Maximilian Bauer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software andassociated documentation files(the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, andto permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice andthis permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
*/

#include <GL/glew.h>
#include <GL/freeglut.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>

//If you want the quest item in the main room, uncomment the following line.
//#define BORING_MODE

#define LENGTHOF(x) (sizeof(x)/sizeof((x)[0]))
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define PI 3.1415926f
#define EPSILON 0.0001f

#define FLOATS_PER_VERTEX 6

#define CALCULATION_TRESHOLD 0.01f
#define UPDATE_TIMEOUT_MS 30
#define INFO_LOG_SIZE 512

//In units/second, without any friction.
#define PLAYER_MAX_SPEED 0.2f
#define PLAYER_JUMP_SPEED 1.8f
//Applied once/update (in units/second).
#define PLAYER_FRICTION 5.0f
#define PLAYER_GRAVITY 0.08f
//Applied once/update (in degrees/second).
#define ITEM_ROTATION_SPEED 45.0f

#define FADEOUT_SPEED 0.5f

//Applied once/bounce
#define FLOOR_BOUNCYNESS 0.25f

#define MOUSE_SPEED 1.75f
#define MOUSE_FRICTION 7.5f

#define DEFAULT_WINDOW_WIDTH 640
#define DEFAULT_WINDOW_HEIGHT 480

//=============================================================================
//  Commonly used utility and simple math functions used across the program.
//=============================================================================
void Common_terminate(const char *applicationStage, const char *errorMessage)
{
  printf("A critical application error has occurred in application stage \"");
  printf(applicationStage);
  printf("\":\n");
  printf(errorMessage);
  printf("\nThe application will be terminated.\n");
  exit(1);
}

float Common_radToDeg(float rad)
{
  return rad * (180.0f / PI);
}

float Common_degToRad(float deg)
{
  return deg * (PI / 180.0f);
}

//=============================================================================
// Matrix4x4: Matrix4x4 struct and basic calculations with matrices.
//=============================================================================

//Provides a 4-dimensional float matrix.
//Use "Matrix4x4_create" to initialize a new instance.
typedef struct
{
  float a00, a01, a02, a03;
  float a10, a11, a12, a13;
  float a20, a21, a22, a23;
  float a30, a31, a32, a33;
} Matrix4x4;

//Initializes a new Matrix4x4 instance.
//identity: true to initialize the matrix as identity matrix, false for all 0.
Matrix4x4 Matrix4x4_create(bool identity)
{
  Matrix4x4 m;

  float i = identity ? 1.0f : 0.0f;

  m.a00 = i; m.a01 = 0; m.a02 = 0; m.a03 = 0;
  m.a10 = 0; m.a11 = i; m.a12 = 0; m.a13 = 0;
  m.a20 = 0; m.a21 = 0; m.a22 = i; m.a23 = 0;
  m.a30 = 0; m.a31 = 0; m.a32 = 0; m.a33 = i;

  return m;
}

//Initializes a new Matrix4x4 instance as translation matrix.
//x: The X coordinate of the translation.
//y: The Y coordinate of the translation.
//z: The Z coordinate of the translation.
Matrix4x4 Matrix4x4_createTranslation(float x, float y, float z)
{
  Matrix4x4 m = Matrix4x4_create(true);
  m.a03 = x;
  m.a13 = y;
  m.a23 = z;
  return m;
}

//Initializes a new Matrix4x4 instance as rotation matrix around the X axis.
//rotationDeg: The rotation around the X axis in degrees.
Matrix4x4 Matrix4x4_createRotationX(float rotationDeg)
{
  Matrix4x4 m = Matrix4x4_create(true);
  float rotationRad = Common_degToRad(rotationDeg);
  m.a11 = (float)cos(rotationRad);
  m.a12 = (float)-sin(rotationRad);
  m.a21 = (float)sin(rotationRad);
  m.a22 = (float)cos(rotationRad);
  return m;
}

//Initializes a new Matrix4x4 instance as rotation matrix around the Y axis.
//rotationDeg: The rotation around the Y axis in degrees.
Matrix4x4 Matrix4x4_createRotationY(float rotationDeg)
{
  Matrix4x4 m = Matrix4x4_create(true);
  float rotationRad = Common_degToRad(rotationDeg);
  m.a00 = (float)cos(rotationRad);
  m.a02 = (float)sin(rotationRad);
  m.a20 = (float)-sin(rotationRad);
  m.a22 = (float)cos(rotationRad);
  return m;
}

//Initializes a new Matrix4x4 instance as multiplication of two matrices.
//a: A pointer to the first matrix.
//b: A pointer to the second matrix.
Matrix4x4 Matrix4x4_multiply(const Matrix4x4 *a, const Matrix4x4 *b)
{
  Matrix4x4 m;

  m.a00 = (a->a00 * b->a00) + (a->a01 * b->a10)
    + (a->a02 * b->a20) + (a->a03 * b->a30);
  m.a01 = (a->a00 * b->a01) + (a->a01 * b->a11)
    + (a->a02 * b->a21) + (a->a03 * b->a31);
  m.a02 = (a->a00 * b->a02) + (a->a01 * b->a12)
    + (a->a02 * b->a22) + (a->a03 * b->a32);
  m.a03 = (a->a00 * b->a03) + (a->a01 * b->a13)
    + (a->a02 * b->a23) + (a->a03 * b->a33);

  m.a10 = (a->a10 * b->a00) + (a->a11 * b->a10)
    + (a->a12 * b->a20) + (a->a13 * b->a30);
  m.a11 = (a->a10 * b->a01) + (a->a11 * b->a11)
    + (a->a12 * b->a21) + (a->a13 * b->a31);
  m.a12 = (a->a10 * b->a02) + (a->a11 * b->a12)
    + (a->a12 * b->a22) + (a->a13 * b->a32);
  m.a13 = (a->a10 * b->a03) + (a->a11 * b->a13)
    + (a->a12 * b->a23) + (a->a13 * b->a33);

  m.a20 = (a->a20 * b->a00) + (a->a21 * b->a10)
    + (a->a22 * b->a20) + (a->a23 * b->a30);
  m.a21 = (a->a20 * b->a01) + (a->a21 * b->a11)
    + (a->a22 * b->a21) + (a->a23 * b->a31);
  m.a22 = (a->a20 * b->a02) + (a->a21 * b->a12)
    + (a->a22 * b->a22) + (a->a23 * b->a32);
  m.a23 = (a->a20 * b->a03) + (a->a21 * b->a13)
    + (a->a22 * b->a23) + (a->a23 * b->a33);

  m.a30 = (a->a30 * b->a00) + (a->a31 * b->a10)
    + (a->a32 * b->a20) + (a->a33 * b->a30);
  m.a31 = (a->a30 * b->a01) + (a->a31 * b->a11)
    + (a->a32 * b->a21) + (a->a33 * b->a31);
  m.a32 = (a->a30 * b->a02) + (a->a31 * b->a12)
    + (a->a32 * b->a22) + (a->a33 * b->a32);
  m.a33 = (a->a30 * b->a03) + (a->a31 * b->a13)
    + (a->a32 * b->a23) + (a->a33 * b->a33);

  return m;
}

//Initializes a new Matrix4x4 instance as camera transformation matrix.
//x: The X position of the camera.
//y: The Y position of the camera.
//z: The Z position of the camera.
//rotationYDeg: The camera rotation around the Y axis (in degrees).
//rotationXDeg: The camera rotation around the X axis (in degrees).
Matrix4x4 Matrix4x4_createCamera(float x, float y, float z,
  float rotationYDeg, float rotationXDeg)
{
  Matrix4x4 translation = Matrix4x4_createTranslation(-x, -y, -z);
  Matrix4x4 rotationX = Matrix4x4_createRotationX(rotationXDeg);
  Matrix4x4 rotationY = Matrix4x4_createRotationY(rotationYDeg);
  Matrix4x4 rotation = Matrix4x4_multiply(&rotationX, &rotationY);
  return Matrix4x4_multiply(&rotation, &translation);
}

//Initializes a new Matrix4x4 instance as perspective projection matrix.
//aspect: The current aspect ratio (width/height).
//zNear: The near clipping plane.
//zFar: The far clipping plane.
//fovDeg: The field of view (in degrees).
//Based on http://ogldev.atspace.co.uk/www/tutorial12/tutorial12.html
Matrix4x4 Matrix4x4_createPerspective(float aspect, float zNear, float zFar,
  float fovDeg)
{
  Matrix4x4 m = Matrix4x4_create(true);

  float tanHalfFov = tanf(Common_degToRad(fovDeg) / 2.0f);
  float zRange = zNear - zFar;

  m.a00 = 1.0f / (tanHalfFov * aspect);
  m.a11 = 1.0f / (tanHalfFov);
  m.a22 = (-zNear - zFar) / zRange;
  m.a23 = (2.0f * zFar * zNear) / zRange;
  m.a32 = 1.0f;
  m.a33 = 0.0f;

  return m;
}

//=============================================================================
//    Shader functionality: ShaderProgram struct and associated functions.
//=============================================================================
typedef struct
{
  GLuint handle;

  GLint attribLocation_position;
  GLint attribLocation_color;

  GLint uniformLocation_model;
  GLint uniformLocation_view;
  GLint uniformLocation_projection;
  GLint uniformLocation_screenHeight;
  GLint uniformLocation_opacity;
  GLint uniformLocation_currentTimeMs;
  GLint uniformLocation_brightness;
} ShaderProgram;

const char *ShaderProgram_DefaultVertexShaderSourceCode =
"#version 120\n"
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"\n"
"attribute vec3 position;\n"
"attribute vec3 color;\n"
"varying vec3 vertexColor;\n"
"varying vec3 fragmentPosition;\n"
"\n"
"void main()\n"
"{\n"
"   gl_Position = projection * view * model * vec4(position, 1.0f);\n"
"   fragmentPosition = position;\n"
"   vertexColor = color;\n"
"}\n";

const char *ShaderProgram_DefaultFragmentShaderSourceCode =
"#version 120\n"
"\n"
"const float INTENSITY = 0.15;\n"
"const float LINE_THICCNESS = 5;\n"//this code gonna be thicc even thiccer soon
"\n"
"uniform float screenHeight;\n"
"uniform float currentTimeMs;\n"
"uniform float opacity = 1;\n"
"uniform float brightness = 1;\n"
"varying vec3 vertexColor;\n"
"\n"
"void main()\n"
"{\n"
"   float screenY = (gl_FragCoord.y + currentTimeMs) / screenHeight;\n"
"   float scanLine = 1.0 - INTENSITY * \n"
"     mod(screenY * screenHeight/LINE_THICCNESS, 1.0);\n"
"   gl_FragColor = vec4(vertexColor.rgb * scanLine * brightness, opacity);\n"
"}\n";

//Initializes (generates, compiles and links) a new ShaderProgram instance.
//vertexShaderSourceCode: The vertex shader code as '\0'-terminated char*.
//fragmentShaderSourceCode: The fragment shader code as '\0'-terminated char*.
//makeCurrent: true to use the new program as current program, false not to.
//Returns a new ShaderProgram instance.
//Terminates the program if compiling the shader or linking the program fails.
ShaderProgram ShaderProgram_create(const char *vertexShaderSourceCode,
  const char *fragmentShaderSourceCode, bool makeCurrent)
{
  int shaderStatusCode;
  char log[INFO_LOG_SIZE];

  //Create a new ShaderProgram instance and put a new program handle into it.
  ShaderProgram newShaderProgram;
  GLuint prog = glCreateProgram();
  newShaderProgram.handle = prog;

  //Create and compile the vertex shader.
  GLuint vertexShaderHandle = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertexShaderHandle, 1, &vertexShaderSourceCode, NULL);
  glCompileShader(vertexShaderHandle);
  glGetShaderiv(vertexShaderHandle, GL_COMPILE_STATUS, &shaderStatusCode);

  if (!shaderStatusCode)
  {
    glGetShaderInfoLog(vertexShaderHandle, INFO_LOG_SIZE, NULL, log);
    Common_terminate("SHADER_VERTEXSHADER_COMPILATION", log);
  }

  //Create and compile the fragment shader.
  GLuint fragmentShaderHandle = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragmentShaderHandle, 1, &fragmentShaderSourceCode, NULL);
  glCompileShader(fragmentShaderHandle);
  glGetShaderiv(fragmentShaderHandle, GL_COMPILE_STATUS, &shaderStatusCode);

  if (!shaderStatusCode)
  {
    glGetShaderInfoLog(fragmentShaderHandle, INFO_LOG_SIZE, NULL, log);
    Common_terminate("SHADER_FRAGMENTSHADER_COMPILATION", log);
  }

  //Attach the created shaders to the shader program and link them.
  glAttachShader(newShaderProgram.handle, vertexShaderHandle);
  glAttachShader(newShaderProgram.handle, fragmentShaderHandle);

  glLinkProgram(newShaderProgram.handle);

  glGetShaderiv(newShaderProgram.handle, GL_LINK_STATUS, &shaderStatusCode);

  if (!shaderStatusCode)
  {
    glGetShaderInfoLog(newShaderProgram.handle, INFO_LOG_SIZE, NULL, log);
    Common_terminate("SHADER_PROGRAM_LINKING", log);
  }

  //Delete the (no longer required) shader objects.
  glDeleteShader(vertexShaderHandle);
  glDeleteShader(fragmentShaderHandle);

  //Make the new shader program the current one (if requested).
  if (makeCurrent) glUseProgram(newShaderProgram.handle);

  //Extract the locations of the attributes and uniforms and put them into
  //the ShaderProgram instance.
  newShaderProgram.attribLocation_position = glGetAttribLocation(
    newShaderProgram.handle, "position");
  newShaderProgram.attribLocation_color = glGetAttribLocation(
    newShaderProgram.handle, "color");

  newShaderProgram.uniformLocation_model = glGetUniformLocation(
    newShaderProgram.handle, "model");
  newShaderProgram.uniformLocation_view = glGetUniformLocation(
    newShaderProgram.handle, "view");
  newShaderProgram.uniformLocation_projection = glGetUniformLocation(
    newShaderProgram.handle, "projection");
  newShaderProgram.uniformLocation_screenHeight = glGetUniformLocation(
    newShaderProgram.handle, "screenHeight");
  newShaderProgram.uniformLocation_opacity = glGetUniformLocation(
    newShaderProgram.handle, "opacity");
  newShaderProgram.uniformLocation_currentTimeMs = glGetUniformLocation(
    newShaderProgram.handle, "currentTimeMs");
  newShaderProgram.uniformLocation_brightness = glGetUniformLocation(
    newShaderProgram.handle, "brightness");

  return newShaderProgram;
}

//Destroys a shader program and sets the program handle in the specified 
//program to 0 afterwards.
//self: A pointer to the shader program.
//Does nothing if NULL is provided as "self".
void ShaderProgram_destroy(ShaderProgram *self)
{
  if (self == NULL) return;

  glDeleteProgram(self->handle);
  self->handle = 0;
}

//Initializes (generates, compiles and links) a new ShaderProgram instance.
//makeCurrent: true to use the new program as current program, false not to.
//Returns a new ShaderProgram instance.
//Terminates the program if compiling the shader or linking the program fails.
ShaderProgram ShaderProgram_createDefault(bool makeCurrent)
{
  return ShaderProgram_create(ShaderProgram_DefaultVertexShaderSourceCode,
    ShaderProgram_DefaultFragmentShaderSourceCode, makeCurrent);
}

//Sets a 4-dimensional matrix value on the shader program.
//uniformLocation: The location of the uniform (of type "mat4").
//matrix: A pointer to the matrix value which should be uploaded to the shader.
void ShaderProgram_setUniformValue_Matrix4x4(GLint uniformLocation,
  const Matrix4x4 *matrix)
{
  glUniformMatrix4fv(uniformLocation, 1, true, (GLfloat *)matrix);
}

//Sets a float value on the shader program.
//uniformLocation: The location of the uniform (of type "float").
//value: The value which should be uploaded to the shader.
void ShaderProgram_setUniformValue_float(GLint uniformLocation,
  const float value)
{
  glUniform1f(uniformLocation, value);
}

//=============================================================================
// BufferedMesh: BufferedMesh and associated functions.
//=============================================================================
//Provides a mesh buffered on the GPU, which can be drawn to screen.
//Use the "BufferedMesh_create" function to initialize new instances.
typedef struct
{
  GLuint bufferHandle;
  GLuint vaoHandle;
  unsigned int vertexCount;
} BufferedMesh;

//Initializes a new BufferedMesh instance.
//vertexData: A pointer to vertex data with vertices in the format XYZRGB.
//arrayLength: The amount of float elements in vertexData.
//targetShader: The target shader program (required for the vertex attributes).
//Terminates the program when the arrayLength is not divisible by 6.
BufferedMesh BufferedMesh_create(const float *vertexData,
  const int arrayLength, ShaderProgram targetShader)
{
  BufferedMesh bufferedMesh;

  bufferedMesh.vertexCount = arrayLength / FLOATS_PER_VERTEX;
  if (arrayLength % FLOATS_PER_VERTEX != 0)
    Common_terminate("BUFFEREDMESH_CREATION", "Invalid vertex data length - "
      "must be divisable by the amount of floats per vertex.");

  glGenVertexArrays(1, &bufferedMesh.vaoHandle);
  glGenBuffers(1, &bufferedMesh.bufferHandle);

  glBindVertexArray(bufferedMesh.vaoHandle);
  glBindBuffer(GL_ARRAY_BUFFER, bufferedMesh.bufferHandle);

  glBufferData(GL_ARRAY_BUFFER, sizeof(float) * arrayLength, vertexData,
    GL_STATIC_DRAW);

  //Each vertex is defined by two 3-dimensional vectors (which contain 3 floats
  //each) - therefore, the size of one vertex is 6 floats - this is the stride.
  glVertexAttribPointer(targetShader.attribLocation_position, 3, GL_FLOAT,
    GL_FALSE, 6 * sizeof(float), NULL);
  glEnableVertexAttribArray(targetShader.attribLocation_position);

  //The offset for the color attribute is 3 floats - the size of the 
  //position vector, which comes before.
  glVertexAttribPointer(targetShader.attribLocation_color, 3, GL_FLOAT,
    GL_FALSE, 6 * sizeof(float), (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(targetShader.attribLocation_color);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  return bufferedMesh;
}

//Deletes the buffer and vertex array allocated by a mesh and sets the handles
//to the buffers and the vertex count inside the BufferedMesh instance to 0 
//afterwards.
//mesh: The pointer to the BufferedMesh instance which should be destroyed.
//Does nothing if NULL is provided.
void BufferedMesh_destroy(BufferedMesh *self)
{
  if (self == NULL) return;

  glDeleteVertexArrays(1, &(self->vaoHandle));
  glDeleteBuffers(1, &(self->bufferHandle));

  self->vaoHandle = 0;
  self->bufferHandle = 0;
  self->vertexCount = 0;
}

//Draws a BufferedMesh to the screen.
//self: A pointer to the buffered mesh.
//Does nothing if NULL is provided.
//If the properties of BufferedMesh are invalid, the application will crash.
void BufferedMesh_draw(const BufferedMesh *self)
{
  if (self == NULL) return;

  glBindVertexArray(self->vaoHandle);
  glDrawArrays(GL_TRIANGLES, 0, self->vertexCount);
  glBindVertexArray(0);
}

//The following variable definitions contain the vertex definitions as raw
//float arrays - this data was generated using Blender 2.82 and the python
//script attached to the bottom of this file. 
//The actual program code continues at line 1686.
const float floorMeshData[] =
{
   0.5f, 0.0f, 0.0f, 0.353f, 0.894f, 0.498f,
   0.5f, 0.0f, 0.5f, 0.153f, 0.384f, 0.216f,
   0.0f, 0.0f, 0.5f, 0.353f, 0.894f, 0.498f,

   0.5f, 0.0f, 0.0f, 0.353f, 0.894f, 0.498f,
   0.0f, 0.0f, 0.5f, 0.353f, 0.894f, 0.498f,
   0.0f, 0.0f, 0.0f, 0.396f, 1.0f, 0.557f,

   0.0f, 0.0f, -0.5f, 0.353f, 0.894f, 0.498f,
   0.5f, 0.0f, 0.0f, 0.353f, 0.894f, 0.498f,
   0.0f, 0.0f, 0.0f, 0.396f, 1.0f, 0.557f,

   0.0f, 0.0f, -0.5f, 0.353f, 0.894f, 0.498f,
   0.5f, 0.0f, -0.5f, 0.153f, 0.384f, 0.216f,
   0.5f, 0.0f, 0.0f, 0.353f, 0.894f, 0.498f,

   -0.5f, 0.0f, 0.0f, 0.353f, 0.894f, 0.498f,
   -0.5f, 0.0f, -0.5f, 0.153f, 0.384f, 0.216f,
   0.0f, 0.0f, -0.5f, 0.353f, 0.894f, 0.498f,

   0.0f, 0.0f, -0.5f, 0.353f, 0.894f, 0.498f,
   0.0f, 0.0f, 0.0f, 0.396f, 1.0f, 0.557f,
   -0.5f, 0.0f, 0.0f, 0.353f, 0.894f, 0.498f,

   0.0f, 0.0f, 0.5f, 0.353f, 0.894f, 0.498f,
   -0.5f, 0.0f, 0.5f, 0.153f, 0.384f, 0.216f,
   -0.5f, 0.0f, 0.0f, 0.353f, 0.894f, 0.498f,

   0.0f, 0.0f, 0.0f, 0.396f, 1.0f, 0.557f,
   0.0f, 0.0f, 0.5f, 0.353f, 0.894f, 0.498f,
   -0.5f, 0.0f, 0.0f, 0.353f, 0.894f, 0.498f,
};

const float wallMeshData[] =
{
   0.5f, 1.0f, 0.5f, 0.008f, 0.173f, 0.051f,
   -0.5f, 1.0f, -0.5f, 0.008f, 0.173f, 0.051f,
   0.5f, 1.0f, -0.5f, 0.008f, 0.173f, 0.051f,

   0.5f, 0.0f, -0.5f, 0.012f, 0.357f, 0.106f,
   0.5f, 1.0f, -0.5f, 0.008f, 0.173f, 0.051f,
   0.0f, 0.0f, -0.5f, 0.031f, 0.835f, 0.247f,

   -0.5f, 0.0f, -0.5f, 0.012f, 0.357f, 0.106f,
   -0.5f, 1.0f, -0.5f, 0.008f, 0.173f, 0.051f,
   -0.5f, 0.0f, 0.0f, 0.031f, 0.835f, 0.247f,

   0.0f, 0.0f, 0.5f, 0.031f, 0.835f, 0.247f,
   0.5f, 0.0f, 0.0f, 0.031f, 0.835f, 0.247f,
   0.0f, 0.0f, -0.5f, 0.031f, 0.835f, 0.247f,

   0.5f, 0.0f, 0.5f, 0.012f, 0.357f, 0.106f,
   0.5f, 1.0f, 0.5f, 0.008f, 0.173f, 0.051f,
   0.5f, 0.0f, 0.0f, 0.031f, 0.835f, 0.247f,

   -0.5f, 0.0f, 0.5f, 0.012f, 0.357f, 0.106f,
   -0.5f, 1.0f, 0.5f, 0.008f, 0.173f, 0.051f,
   0.0f, 0.0f, 0.5f, 0.031f, 0.835f, 0.247f,

   0.0f, 0.0f, -0.5f, 0.031f, 0.835f, 0.247f,
   0.5f, 1.0f, -0.5f, 0.008f, 0.173f, 0.051f,
   -0.5f, 1.0f, -0.5f, 0.008f, 0.173f, 0.051f,

   -0.5f, 1.0f, -0.5f, 0.008f, 0.173f, 0.051f,
   -0.5f, 0.0f, -0.5f, 0.012f, 0.357f, 0.106f,
   0.0f, 0.0f, -0.5f, 0.031f, 0.835f, 0.247f,

   -0.5f, 0.0f, 0.0f, 0.031f, 0.835f, 0.247f,
   -0.5f, 1.0f, -0.5f, 0.008f, 0.173f, 0.051f,
   -0.5f, 1.0f, 0.5f, 0.008f, 0.173f, 0.051f,

   -0.5f, 1.0f, 0.5f, 0.008f, 0.173f, 0.051f,
   -0.5f, 0.0f, 0.5f, 0.012f, 0.357f, 0.106f,
   -0.5f, 0.0f, 0.0f, 0.031f, 0.835f, 0.247f,

   0.0f, 0.0f, 0.5f, 0.031f, 0.835f, 0.247f,
   -0.5f, 1.0f, 0.5f, 0.008f, 0.173f, 0.051f,
   0.5f, 1.0f, 0.5f, 0.008f, 0.173f, 0.051f,

   0.5f, 1.0f, 0.5f, 0.008f, 0.173f, 0.051f,
   0.5f, 0.0f, 0.5f, 0.012f, 0.357f, 0.106f,
   0.0f, 0.0f, 0.5f, 0.031f, 0.835f, 0.247f,

   0.5f, 0.0f, 0.0f, 0.031f, 0.835f, 0.247f,
   0.5f, 1.0f, 0.5f, 0.008f, 0.173f, 0.051f,
   0.5f, 1.0f, -0.5f, 0.008f, 0.173f, 0.051f,

   0.5f, 1.0f, -0.5f, 0.008f, 0.173f, 0.051f,
   0.5f, 0.0f, -0.5f, 0.012f, 0.357f, 0.106f,
   0.5f, 0.0f, 0.0f, 0.031f, 0.835f, 0.247f,

   0.5f, 1.0f, 0.5f, 0.008f, 0.173f, 0.051f,
   -0.5f, 1.0f, 0.5f, 0.008f, 0.173f, 0.051f,
   -0.5f, 1.0f, -0.5f, 0.008f, 0.173f, 0.051f,

   -0.5f, 0.0f, 0.0f, 0.031f, 0.835f, 0.247f,
   -0.5f, 0.0f, 0.5f, 0.012f, 0.357f, 0.106f,
   0.0f, 0.0f, 0.5f, 0.031f, 0.835f, 0.247f,

   0.0f, 0.0f, 0.5f, 0.031f, 0.835f, 0.247f,
   0.5f, 0.0f, 0.5f, 0.012f, 0.357f, 0.106f,
   0.5f, 0.0f, 0.0f, 0.031f, 0.835f, 0.247f,

   0.5f, 0.0f, 0.0f, 0.031f, 0.835f, 0.247f,
   0.5f, 0.0f, -0.5f, 0.012f, 0.357f, 0.106f,
   0.0f, 0.0f, -0.5f, 0.031f, 0.835f, 0.247f,

   0.0f, 0.0f, -0.5f, 0.031f, 0.835f, 0.247f,
   -0.5f, 0.0f, -0.5f, 0.012f, 0.357f, 0.106f,
   -0.5f, 0.0f, 0.0f, 0.031f, 0.835f, 0.247f,

   -0.5f, 0.0f, 0.0f, 0.031f, 0.835f, 0.247f,
   0.0f, 0.0f, 0.5f, 0.031f, 0.835f, 0.247f,
   0.0f, 0.0f, -0.5f, 0.031f, 0.835f, 0.247f,
};

const float archMeshData[] =
{
   0.4f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,
   0.5f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,
   0.5f, 0.9f, 0.05f, 0.016f, 0.459f, 0.333f,

   0.4f, 0.0f, 0.05f, 0.031f, 0.831f, 0.604f,
   0.5f, 0.9f, 0.05f, 0.016f, 0.459f, 0.333f,
   0.5f, 0.0f, 0.05f, 0.125f, 0.945f, 0.71f,

   0.5f, 0.0f, -0.05f, 0.125f, 0.945f, 0.71f,
   0.4f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,
   0.4f, 0.0f, -0.05f, 0.051f, 0.831f, 0.604f,

   0.4f, 0.0f, -0.05f, 0.051f, 0.831f, 0.604f,
   0.4f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,
   0.4f, 0.0f, 0.05f, 0.031f, 0.831f, 0.604f,

   0.5f, 0.0f, 0.05f, 0.125f, 0.945f, 0.71f,
   0.5f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,
   0.5f, 0.0f, -0.05f, 0.125f, 0.945f, 0.71f,

   0.5f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,
   0.4f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,
   0.5f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,

   0.5f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,
   0.4f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,
   0.4f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,

   0.4f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,
   -0.4f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,
   0.4f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,

   0.5f, 0.9f, 0.05f, 0.016f, 0.459f, 0.333f,
   0.5f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,
   0.5f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,

   -0.4f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,
   -0.5f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,
   -0.4f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,

   0.4f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,
   -0.4f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,
   0.4f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,

   0.4f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,
   -0.4f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,
   0.4f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,

   0.4f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,
   -0.4f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,
   0.4f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,

   -0.5f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,
   -0.5f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,
   -0.5f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,

   -0.5f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,
   -0.5f, 0.0f, -0.05f, 0.125f, 0.945f, 0.71f,
   -0.5f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,

   -0.4f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,
   -0.5f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,
   -0.4f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,

   -0.4f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,
   -0.5f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,
   -0.4f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,

   -0.4f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,
   -0.4f, 0.0f, 0.05f, 0.031f, 0.831f, 0.604f,
   -0.4f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,

   -0.5f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,
   -0.4f, 0.0f, -0.05f, 0.031f, 0.831f, 0.604f,
   -0.4f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,

   -0.4f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,
   -0.5f, 0.0f, 0.05f, 0.125f, 0.945f, 0.71f,
   -0.5f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,

   0.4f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,
   0.4f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,
   0.5f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,

   0.4f, 0.0f, 0.05f, 0.031f, 0.831f, 0.604f,
   0.4f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,
   0.5f, 0.9f, 0.05f, 0.016f, 0.459f, 0.333f,

   0.5f, 0.0f, -0.05f, 0.125f, 0.945f, 0.71f,
   0.5f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,
   0.4f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,

   0.4f, 0.0f, -0.05f, 0.051f, 0.831f, 0.604f,
   0.4f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,
   0.4f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,

   0.5f, 0.0f, 0.05f, 0.125f, 0.945f, 0.71f,
   0.5f, 0.9f, 0.05f, 0.016f, 0.459f, 0.333f,
   0.5f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,

   0.5f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,
   0.4f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,
   0.4f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,

   0.5f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,
   0.5f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,
   0.4f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,

   0.4f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,
   -0.4f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,
   -0.4f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,

   0.5f, 0.9f, 0.05f, 0.016f, 0.459f, 0.333f,
   0.5f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,
   0.5f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,

   -0.4f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,
   -0.5f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,
   -0.5f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,

   0.4f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,
   -0.4f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,
   -0.4f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,

   0.4f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,
   -0.4f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,
   -0.4f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,

   0.4f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,
   -0.4f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,
   -0.4f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,

   -0.5f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,
   -0.5f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,
   -0.5f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,

   -0.5f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,
   -0.5f, 0.0f, 0.05f, 0.125f, 0.945f, 0.71f,
   -0.5f, 0.0f, -0.05f, 0.125f, 0.945f, 0.71f,

   -0.4f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,
   -0.5f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,
   -0.5f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,

   -0.4f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,
   -0.5f, 1.0f, 0.05f, 0.008f, 0.231f, 0.169f,
   -0.5f, 1.0f, -0.05f, 0.008f, 0.231f, 0.169f,

   -0.4f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,
   -0.4f, 0.0f, -0.05f, 0.031f, 0.831f, 0.604f,
   -0.4f, 0.0f, 0.05f, 0.031f, 0.831f, 0.604f,

   -0.5f, 0.9f, -0.05f, 0.02f, 0.51f, 0.373f,
   -0.5f, 0.0f, -0.05f, 0.125f, 0.945f, 0.71f,
   -0.4f, 0.0f, -0.05f, 0.031f, 0.831f, 0.604f,

   -0.4f, 0.9f, 0.05f, 0.02f, 0.51f, 0.373f,
   -0.4f, 0.0f, 0.05f, 0.031f, 0.831f, 0.604f,
   -0.5f, 0.0f, 0.05f, 0.125f, 0.945f, 0.71f,
};

const float crystalMeshData[] =
{
   0.25f, 0.5f, -0.0f, 0.447f, 0.0f, 0.514f,
   0.0f, 0.5f, 0.25f, 0.678f, 0.0f, 0.78f,
   0.0f, 0.875f, 0.0f, 0.18f, 0.0f, 0.208f,

   -0.25f, 0.5f, 0.0f, 0.447f, 0.0f, 0.514f,
   -0.0f, 0.5f, -0.25f, 0.678f, 0.0f, 0.78f,
   0.0f, 0.875f, 0.0f, 0.18f, 0.0f, 0.208f,

   0.0f, 0.5f, 0.25f, 0.678f, 0.0f, 0.78f,
   -0.25f, 0.5f, 0.0f, 0.447f, 0.0f, 0.514f,
   0.0f, 0.875f, 0.0f, 0.18f, 0.0f, 0.208f,

   -0.0f, 0.5f, -0.25f, 0.678f, 0.0f, 0.78f,
   0.25f, 0.5f, -0.0f, 0.447f, 0.0f, 0.514f,
   0.0f, 0.875f, 0.0f, 0.18f, 0.0f, 0.208f,

   0.0f, 0.5f, 0.25f, 0.678f, 0.0f, 0.78f,
   0.25f, 0.5f, -0.0f, 0.447f, 0.0f, 0.514f,
   0.0f, 0.125f, 0.0f, 0.18f, 0.0f, 0.208f,

   -0.0f, 0.5f, -0.25f, 0.678f, 0.0f, 0.78f,
   -0.25f, 0.5f, 0.0f, 0.447f, 0.0f, 0.514f,
   0.0f, 0.125f, 0.0f, 0.18f, 0.0f, 0.208f,

   -0.25f, 0.5f, 0.0f, 0.447f, 0.0f, 0.514f,
   0.0f, 0.5f, 0.25f, 0.678f, 0.0f, 0.78f,
   0.0f, 0.125f, 0.0f, 0.18f, 0.0f, 0.208f,

   0.25f, 0.5f, -0.0f, 0.447f, 0.0f, 0.514f,
   -0.0f, 0.5f, -0.25f, 0.678f, 0.0f, 0.78f,
   0.0f, 0.125f, 0.0f, 0.18f, 0.0f, 0.208f,

   -0.004f, 0.525f, -0.356f, 1.0f, 0.973f, 0.753f,
   0.394f, 0.586f, -0.0f, 1.0f, 0.973f, 0.753f,
   -0.004f, 0.525f, -0.402f, 1.0f, 0.973f, 0.753f,

   -0.004f, 0.525f, 0.356f, 1.0f, 0.973f, 0.749f,
   0.394f, 0.586f, -0.0f, 1.0f, 0.973f, 0.753f,
   0.348f, 0.579f, -0.0f, 1.0f, 0.973f, 0.753f,

   -0.004f, 0.525f, -0.356f, 1.0f, 0.973f, 0.753f,
   -0.401f, 0.463f, 0.0f, 0.984f, 0.933f, 0.69f,
   -0.356f, 0.47f, 0.0f, 1.0f, 0.98f, 0.561f,

   -0.004f, 0.525f, 0.356f, 1.0f, 0.973f, 0.749f,
   -0.401f, 0.463f, 0.0f, 0.984f, 0.933f, 0.69f,
   -0.004f, 0.525f, 0.402f, 1.0f, 0.973f, 0.753f,

   -0.004f, 0.525f, -0.356f, 1.0f, 0.973f, 0.753f,
   0.356f, 0.53f, -0.0f, 0.929f, 0.773f, 0.984f,
   0.348f, 0.579f, -0.0f, 1.0f, 0.973f, 0.753f,

   0.394f, 0.586f, -0.0f, 1.0f, 0.973f, 0.753f,
   0.004f, 0.475f, 0.402f, 0.925f, 0.753f, 1.0f,
   0.401f, 0.537f, -0.0f, 0.925f, 0.757f, 0.996f,

   -0.004f, 0.525f, -0.402f, 1.0f, 0.973f, 0.753f,
   -0.394f, 0.414f, 0.0f, 0.925f, 0.753f, 1.0f,
   -0.401f, 0.463f, 0.0f, 0.984f, 0.933f, 0.69f,

   -0.004f, 0.525f, 0.356f, 1.0f, 0.973f, 0.749f,
   0.356f, 0.53f, -0.0f, 0.929f, 0.773f, 0.984f,
   0.004f, 0.475f, 0.356f, 0.925f, 0.753f, 1.0f,

   -0.356f, 0.47f, 0.0f, 1.0f, 0.98f, 0.561f,
   0.004f, 0.475f, -0.356f, 0.925f, 0.757f, 1.0f,
   -0.004f, 0.525f, -0.356f, 1.0f, 0.973f, 0.753f,

   -0.401f, 0.463f, 0.0f, 0.984f, 0.933f, 0.69f,
   0.004f, 0.475f, 0.402f, 0.925f, 0.753f, 1.0f,
   -0.004f, 0.525f, 0.402f, 1.0f, 0.973f, 0.753f,

   -0.004f, 0.525f, -0.402f, 1.0f, 0.973f, 0.753f,
   0.401f, 0.537f, -0.0f, 0.925f, 0.757f, 0.996f,
   0.004f, 0.475f, -0.402f, 0.984f, 0.945f, 0.698f,

   -0.004f, 0.525f, 0.356f, 1.0f, 0.973f, 0.749f,
   -0.348f, 0.421f, 0.0f, 0.941f, 0.816f, 0.941f,
   -0.356f, 0.47f, 0.0f, 1.0f, 0.98f, 0.561f,

   0.004f, 0.475f, 0.402f, 0.925f, 0.753f, 1.0f,
   -0.348f, 0.421f, 0.0f, 0.941f, 0.816f, 0.941f,
   0.004f, 0.475f, 0.356f, 0.925f, 0.753f, 1.0f,

   0.401f, 0.537f, -0.0f, 0.925f, 0.757f, 0.996f,
   0.004f, 0.475f, 0.356f, 0.925f, 0.753f, 1.0f,
   0.356f, 0.53f, -0.0f, 0.929f, 0.773f, 0.984f,

   0.004f, 0.475f, -0.402f, 0.984f, 0.945f, 0.698f,
   0.356f, 0.53f, -0.0f, 0.929f, 0.773f, 0.984f,
   0.004f, 0.475f, -0.356f, 0.925f, 0.757f, 1.0f,

   -0.394f, 0.414f, 0.0f, 0.925f, 0.753f, 1.0f,
   0.004f, 0.475f, -0.356f, 0.925f, 0.757f, 1.0f,
   -0.348f, 0.421f, 0.0f, 0.941f, 0.816f, 0.941f,

   0.355f, 0.462f, -0.0f, 1.0f, 0.973f, 0.753f,
   0.004f, 0.525f, -0.323f, 1.0f, 0.973f, 0.753f,
   0.004f, 0.525f, -0.277f, 1.0f, 0.973f, 0.753f,

   0.355f, 0.462f, -0.0f, 1.0f, 0.973f, 0.753f,
   0.004f, 0.525f, 0.323f, 1.0f, 0.973f, 0.753f,
   0.4f, 0.454f, -0.0f, 1.0f, 0.973f, 0.753f,

   -0.346f, 0.587f, -0.0f, 1.0f, 0.98f, 0.561f,
   0.004f, 0.525f, -0.323f, 1.0f, 0.973f, 0.753f,
   -0.391f, 0.595f, -0.0f, 0.984f, 0.933f, 0.69f,

   -0.346f, 0.587f, -0.0f, 1.0f, 0.98f, 0.561f,
   0.004f, 0.525f, 0.323f, 1.0f, 0.973f, 0.753f,
   0.004f, 0.525f, 0.277f, 1.0f, 0.973f, 0.749f,

   0.004f, 0.525f, -0.277f, 1.0f, 0.973f, 0.753f,
   0.346f, 0.413f, -0.0f, 0.929f, 0.773f, 0.984f,
   0.355f, 0.462f, -0.0f, 1.0f, 0.973f, 0.753f,

   0.004f, 0.525f, 0.323f, 1.0f, 0.973f, 0.753f,
   0.391f, 0.405f, -0.0f, 0.925f, 0.757f, 0.996f,
   0.4f, 0.454f, -0.0f, 1.0f, 0.973f, 0.753f,

   -0.391f, 0.595f, -0.0f, 0.984f, 0.933f, 0.69f,
   -0.004f, 0.475f, -0.323f, 0.984f, 0.945f, 0.698f,
   -0.4f, 0.546f, -0.0f, 0.925f, 0.753f, 1.0f,

   0.355f, 0.462f, -0.0f, 1.0f, 0.973f, 0.753f,
   -0.004f, 0.475f, 0.277f, 0.925f, 0.753f, 1.0f,
   0.004f, 0.525f, 0.277f, 1.0f, 0.973f, 0.749f,

   -0.346f, 0.587f, -0.0f, 1.0f, 0.98f, 0.561f,
   -0.004f, 0.475f, -0.277f, 0.925f, 0.757f, 1.0f,
   0.004f, 0.525f, -0.277f, 1.0f, 0.973f, 0.753f,

   0.004f, 0.525f, 0.323f, 1.0f, 0.973f, 0.753f,
   -0.4f, 0.546f, -0.0f, 0.925f, 0.753f, 1.0f,
   -0.004f, 0.475f, 0.323f, 0.925f, 0.753f, 1.0f,

   0.4f, 0.454f, -0.0f, 1.0f, 0.973f, 0.753f,
   -0.004f, 0.475f, -0.323f, 0.984f, 0.945f, 0.698f,
   0.004f, 0.525f, -0.323f, 1.0f, 0.973f, 0.753f,

   0.004f, 0.525f, 0.277f, 1.0f, 0.973f, 0.749f,
   -0.355f, 0.538f, -0.0f, 0.941f, 0.816f, 0.941f,
   -0.346f, 0.587f, -0.0f, 1.0f, 0.98f, 0.561f,

   -0.004f, 0.475f, 0.323f, 0.925f, 0.753f, 1.0f,
   -0.355f, 0.538f, -0.0f, 0.941f, 0.816f, 0.941f,
   -0.004f, 0.475f, 0.277f, 0.925f, 0.753f, 1.0f,

   0.346f, 0.413f, -0.0f, 0.929f, 0.773f, 0.984f,
   -0.004f, 0.475f, 0.323f, 0.925f, 0.753f, 1.0f,
   -0.004f, 0.475f, 0.277f, 0.925f, 0.753f, 1.0f,

   -0.004f, 0.475f, -0.323f, 0.984f, 0.945f, 0.698f,
   0.346f, 0.413f, -0.0f, 0.929f, 0.773f, 0.984f,
   -0.004f, 0.475f, -0.277f, 0.925f, 0.757f, 1.0f,

   -0.355f, 0.538f, -0.0f, 0.941f, 0.816f, 0.941f,
   -0.004f, 0.475f, -0.323f, 0.984f, 0.945f, 0.698f,
   -0.004f, 0.475f, -0.277f, 0.925f, 0.757f, 1.0f,

   -0.004f, 0.525f, -0.356f, 1.0f, 0.973f, 0.753f,
   0.348f, 0.579f, -0.0f, 1.0f, 0.973f, 0.753f,
   0.394f, 0.586f, -0.0f, 1.0f, 0.973f, 0.753f,

   -0.004f, 0.525f, 0.356f, 1.0f, 0.973f, 0.749f,
   -0.004f, 0.525f, 0.402f, 1.0f, 0.973f, 0.753f,
   0.394f, 0.586f, -0.0f, 1.0f, 0.973f, 0.753f,

   -0.004f, 0.525f, -0.356f, 1.0f, 0.973f, 0.753f,
   -0.004f, 0.525f, -0.402f, 1.0f, 0.973f, 0.753f,
   -0.401f, 0.463f, 0.0f, 0.984f, 0.933f, 0.69f,

   -0.004f, 0.525f, 0.356f, 1.0f, 0.973f, 0.749f,
   -0.356f, 0.47f, 0.0f, 1.0f, 0.98f, 0.561f,
   -0.401f, 0.463f, 0.0f, 0.984f, 0.933f, 0.69f,

   -0.004f, 0.525f, -0.356f, 1.0f, 0.973f, 0.753f,
   0.004f, 0.475f, -0.356f, 0.925f, 0.757f, 1.0f,
   0.356f, 0.53f, -0.0f, 0.929f, 0.773f, 0.984f,

   0.394f, 0.586f, -0.0f, 1.0f, 0.973f, 0.753f,
   -0.004f, 0.525f, 0.402f, 1.0f, 0.973f, 0.753f,
   0.004f, 0.475f, 0.402f, 0.925f, 0.753f, 1.0f,

   -0.004f, 0.525f, -0.402f, 1.0f, 0.973f, 0.753f,
   0.004f, 0.475f, -0.402f, 0.984f, 0.945f, 0.698f,
   -0.394f, 0.414f, 0.0f, 0.925f, 0.753f, 1.0f,

   -0.004f, 0.525f, 0.356f, 1.0f, 0.973f, 0.749f,
   0.348f, 0.579f, -0.0f, 1.0f, 0.973f, 0.753f,
   0.356f, 0.53f, -0.0f, 0.929f, 0.773f, 0.984f,

   -0.356f, 0.47f, 0.0f, 1.0f, 0.98f, 0.561f,
   -0.348f, 0.421f, 0.0f, 0.941f, 0.816f, 0.941f,
   0.004f, 0.475f, -0.356f, 0.925f, 0.757f, 1.0f,

   -0.401f, 0.463f, 0.0f, 0.984f, 0.933f, 0.69f,
   -0.394f, 0.414f, 0.0f, 0.925f, 0.753f, 1.0f,
   0.004f, 0.475f, 0.402f, 0.925f, 0.753f, 1.0f,

   -0.004f, 0.525f, -0.402f, 1.0f, 0.973f, 0.753f,
   0.394f, 0.586f, -0.0f, 1.0f, 0.973f, 0.753f,
   0.401f, 0.537f, -0.0f, 0.925f, 0.757f, 0.996f,

   -0.004f, 0.525f, 0.356f, 1.0f, 0.973f, 0.749f,
   0.004f, 0.475f, 0.356f, 0.925f, 0.753f, 1.0f,
   -0.348f, 0.421f, 0.0f, 0.941f, 0.816f, 0.941f,

   0.004f, 0.475f, 0.402f, 0.925f, 0.753f, 1.0f,
   -0.394f, 0.414f, 0.0f, 0.925f, 0.753f, 1.0f,
   -0.348f, 0.421f, 0.0f, 0.941f, 0.816f, 0.941f,

   0.401f, 0.537f, -0.0f, 0.925f, 0.757f, 0.996f,
   0.004f, 0.475f, 0.402f, 0.925f, 0.753f, 1.0f,
   0.004f, 0.475f, 0.356f, 0.925f, 0.753f, 1.0f,

   0.004f, 0.475f, -0.402f, 0.984f, 0.945f, 0.698f,
   0.401f, 0.537f, -0.0f, 0.925f, 0.757f, 0.996f,
   0.356f, 0.53f, -0.0f, 0.929f, 0.773f, 0.984f,

   -0.394f, 0.414f, 0.0f, 0.925f, 0.753f, 1.0f,
   0.004f, 0.475f, -0.402f, 0.984f, 0.945f, 0.698f,
   0.004f, 0.475f, -0.356f, 0.925f, 0.757f, 1.0f,

   0.355f, 0.462f, -0.0f, 1.0f, 0.973f, 0.753f,
   0.4f, 0.454f, -0.0f, 1.0f, 0.973f, 0.753f,
   0.004f, 0.525f, -0.323f, 1.0f, 0.973f, 0.753f,

   0.355f, 0.462f, -0.0f, 1.0f, 0.973f, 0.753f,
   0.004f, 0.525f, 0.277f, 1.0f, 0.973f, 0.749f,
   0.004f, 0.525f, 0.323f, 1.0f, 0.973f, 0.753f,

   -0.346f, 0.587f, -0.0f, 1.0f, 0.98f, 0.561f,
   0.004f, 0.525f, -0.277f, 1.0f, 0.973f, 0.753f,
   0.004f, 0.525f, -0.323f, 1.0f, 0.973f, 0.753f,

   -0.346f, 0.587f, -0.0f, 1.0f, 0.98f, 0.561f,
   -0.391f, 0.595f, -0.0f, 0.984f, 0.933f, 0.69f,
   0.004f, 0.525f, 0.323f, 1.0f, 0.973f, 0.753f,

   0.004f, 0.525f, -0.277f, 1.0f, 0.973f, 0.753f,
   -0.004f, 0.475f, -0.277f, 0.925f, 0.757f, 1.0f,
   0.346f, 0.413f, -0.0f, 0.929f, 0.773f, 0.984f,

   0.004f, 0.525f, 0.323f, 1.0f, 0.973f, 0.753f,
   -0.004f, 0.475f, 0.323f, 0.925f, 0.753f, 1.0f,
   0.391f, 0.405f, -0.0f, 0.925f, 0.757f, 0.996f,

   -0.391f, 0.595f, -0.0f, 0.984f, 0.933f, 0.69f,
   0.004f, 0.525f, -0.323f, 1.0f, 0.973f, 0.753f,
   -0.004f, 0.475f, -0.323f, 0.984f, 0.945f, 0.698f,

   0.355f, 0.462f, -0.0f, 1.0f, 0.973f, 0.753f,
   0.346f, 0.413f, -0.0f, 0.929f, 0.773f, 0.984f,
   -0.004f, 0.475f, 0.277f, 0.925f, 0.753f, 1.0f,

   -0.346f, 0.587f, -0.0f, 1.0f, 0.98f, 0.561f,
   -0.355f, 0.538f, -0.0f, 0.941f, 0.816f, 0.941f,
   -0.004f, 0.475f, -0.277f, 0.925f, 0.757f, 1.0f,

   0.004f, 0.525f, 0.323f, 1.0f, 0.973f, 0.753f,
   -0.391f, 0.595f, -0.0f, 0.984f, 0.933f, 0.69f,
   -0.4f, 0.546f, -0.0f, 0.925f, 0.753f, 1.0f,

   0.4f, 0.454f, -0.0f, 1.0f, 0.973f, 0.753f,
   0.391f, 0.405f, -0.0f, 0.925f, 0.757f, 0.996f,
   -0.004f, 0.475f, -0.323f, 0.984f, 0.945f, 0.698f,

   0.004f, 0.525f, 0.277f, 1.0f, 0.973f, 0.749f,
   -0.004f, 0.475f, 0.277f, 0.925f, 0.753f, 1.0f,
   -0.355f, 0.538f, -0.0f, 0.941f, 0.816f, 0.941f,

   -0.004f, 0.475f, 0.323f, 0.925f, 0.753f, 1.0f,
   -0.4f, 0.546f, -0.0f, 0.925f, 0.753f, 1.0f,
   -0.355f, 0.538f, -0.0f, 0.941f, 0.816f, 0.941f,

   0.346f, 0.413f, -0.0f, 0.929f, 0.773f, 0.984f,
   0.391f, 0.405f, -0.0f, 0.925f, 0.757f, 0.996f,
   -0.004f, 0.475f, 0.323f, 0.925f, 0.753f, 1.0f,

   -0.004f, 0.475f, -0.323f, 0.984f, 0.945f, 0.698f,
   0.391f, 0.405f, -0.0f, 0.925f, 0.757f, 0.996f,
   0.346f, 0.413f, -0.0f, 0.929f, 0.773f, 0.984f,

   -0.355f, 0.538f, -0.0f, 0.941f, 0.816f, 0.941f,
   -0.4f, 0.546f, -0.0f, 0.925f, 0.753f, 1.0f,
   -0.004f, 0.475f, -0.323f, 0.984f, 0.945f, 0.698f,
};

const float tubeMeshData[] =
{
   0.0f, 0.1f, -0.281f, 0.482f, 0.004f, 0.773f,
   0.199f, 0.2f, -0.199f, 0.525f, 0.016f, 0.831f,
   0.0f, 0.2f, -0.281f, 0.525f, 0.016f, 0.831f,

   -0.281f, 0.1f, -0.0f, 0.482f, 0.0f, 0.773f,
   -0.199f, 0.2f, -0.199f, 0.525f, 0.016f, 0.831f,
   -0.281f, 0.2f, -0.0f, 0.525f, 0.016f, 0.831f,

   0.0f, 0.1f, 0.281f, 0.482f, 0.004f, 0.773f,
   -0.199f, 0.2f, 0.199f, 0.525f, 0.016f, 0.831f,
   0.0f, 0.2f, 0.281f, 0.525f, 0.016f, 0.831f,

   0.199f, 0.1f, 0.199f, 0.482f, 0.0f, 0.773f,
   0.0f, 0.2f, 0.281f, 0.525f, 0.016f, 0.831f,
   0.199f, 0.2f, 0.199f, 0.525f, 0.016f, 0.831f,

   0.199f, 0.1f, -0.199f, 0.482f, 0.0f, 0.773f,
   0.281f, 0.2f, 0.0f, 0.525f, 0.016f, 0.831f,
   0.199f, 0.2f, -0.199f, 0.525f, 0.016f, 0.831f,

   -0.199f, 0.1f, -0.199f, 0.482f, 0.0f, 0.761f,
   0.0f, 0.2f, -0.281f, 0.525f, 0.016f, 0.831f,
   -0.199f, 0.2f, -0.199f, 0.525f, 0.016f, 0.831f,

   -0.199f, 0.1f, 0.199f, 0.482f, 0.0f, 0.773f,
   -0.281f, 0.2f, -0.0f, 0.525f, 0.016f, 0.831f,
   -0.199f, 0.2f, 0.199f, 0.525f, 0.016f, 0.831f,

   0.281f, 0.1f, 0.0f, 0.486f, 0.027f, 0.773f,
   0.199f, 0.2f, 0.199f, 0.525f, 0.016f, 0.831f,
   0.281f, 0.2f, 0.0f, 0.525f, 0.016f, 0.831f,

   0.199f, 0.2f, -0.199f, 0.525f, 0.016f, 0.831f,
   0.0f, 0.2f, -0.211f, 0.412f, 0.008f, 0.655f,
   0.0f, 0.2f, -0.281f, 0.525f, 0.016f, 0.831f,

   -0.199f, 0.2f, -0.199f, 0.525f, 0.016f, 0.831f,
   -0.211f, 0.2f, -0.0f, 0.412f, 0.008f, 0.655f,
   -0.281f, 0.2f, -0.0f, 0.525f, 0.016f, 0.831f,

   -0.149f, 0.2f, 0.149f, 0.412f, 0.008f, 0.655f,
   0.0f, 0.2f, 0.281f, 0.525f, 0.016f, 0.831f,
   -0.199f, 0.2f, 0.199f, 0.525f, 0.016f, 0.831f,

   0.0f, 0.2f, 0.211f, 0.412f, 0.008f, 0.655f,
   0.199f, 0.2f, 0.199f, 0.525f, 0.016f, 0.831f,
   0.0f, 0.2f, 0.281f, 0.525f, 0.016f, 0.831f,

   0.211f, 0.2f, 0.0f, 0.412f, 0.008f, 0.655f,
   0.199f, 0.2f, -0.199f, 0.525f, 0.016f, 0.831f,
   0.281f, 0.2f, 0.0f, 0.525f, 0.016f, 0.831f,

   0.0f, 0.2f, -0.211f, 0.412f, 0.008f, 0.655f,
   -0.199f, 0.2f, -0.199f, 0.525f, 0.016f, 0.831f,
   0.0f, 0.2f, -0.281f, 0.525f, 0.016f, 0.831f,

   -0.211f, 0.2f, -0.0f, 0.412f, 0.008f, 0.655f,
   -0.199f, 0.2f, 0.199f, 0.525f, 0.016f, 0.831f,
   -0.281f, 0.2f, -0.0f, 0.525f, 0.016f, 0.831f,

   0.149f, 0.2f, 0.149f, 0.412f, 0.008f, 0.655f,
   0.281f, 0.2f, 0.0f, 0.525f, 0.016f, 0.831f,
   0.199f, 0.2f, 0.199f, 0.525f, 0.016f, 0.831f,

   -0.149f, 0.02f, -0.149f, 0.325f, 0.0f, 0.522f,
   -0.211f, 0.2f, -0.0f, 0.412f, 0.008f, 0.655f,
   -0.149f, 0.2f, -0.149f, 0.412f, 0.008f, 0.655f,

   -0.149f, 0.02f, 0.149f, 0.325f, 0.0f, 0.522f,
   0.0f, 0.2f, 0.211f, 0.412f, 0.008f, 0.655f,
   -0.149f, 0.2f, 0.149f, 0.412f, 0.008f, 0.655f,

   0.0f, 0.02f, 0.211f, 0.325f, 0.0f, 0.522f,
   0.149f, 0.2f, 0.149f, 0.412f, 0.008f, 0.655f,
   0.0f, 0.2f, 0.211f, 0.412f, 0.008f, 0.655f,

   0.211f, 0.02f, 0.0f, 0.325f, 0.0f, 0.522f,
   0.149f, 0.2f, -0.149f, 0.412f, 0.008f, 0.655f,
   0.211f, 0.2f, 0.0f, 0.412f, 0.008f, 0.655f,

   0.0f, 0.02f, -0.211f, 0.325f, 0.0f, 0.522f,
   -0.149f, 0.2f, -0.149f, 0.412f, 0.008f, 0.655f,
   0.0f, 0.2f, -0.211f, 0.412f, 0.008f, 0.655f,

   -0.211f, 0.02f, -0.0f, 0.325f, 0.0f, 0.522f,
   -0.149f, 0.2f, 0.149f, 0.412f, 0.008f, 0.655f,
   -0.211f, 0.2f, -0.0f, 0.412f, 0.008f, 0.655f,

   0.149f, 0.02f, 0.149f, 0.325f, 0.0f, 0.522f,
   0.211f, 0.2f, 0.0f, 0.412f, 0.008f, 0.655f,
   0.149f, 0.2f, 0.149f, 0.412f, 0.008f, 0.655f,

   0.149f, 0.02f, -0.149f, 0.325f, 0.0f, 0.522f,
   0.0f, 0.2f, -0.211f, 0.412f, 0.008f, 0.655f,
   0.149f, 0.2f, -0.149f, 0.412f, 0.008f, 0.655f,

   0.199f, 0.1f, 0.199f, 0.482f, 0.0f, 0.773f,
   0.0f, 0.1f, 0.246f, 0.255f, 0.0f, 0.408f,
   0.0f, 0.1f, 0.281f, 0.482f, 0.004f, 0.773f,

   0.174f, 0.1f, -0.174f, 0.255f, 0.0f, 0.408f,
   0.281f, 0.1f, 0.0f, 0.486f, 0.027f, 0.773f,
   0.199f, 0.1f, -0.199f, 0.482f, 0.0f, 0.773f,

   -0.174f, 0.1f, -0.174f, 0.255f, 0.0f, 0.408f,
   0.0f, 0.1f, -0.281f, 0.482f, 0.004f, 0.773f,
   -0.199f, 0.1f, -0.199f, 0.482f, 0.0f, 0.761f,

   -0.174f, 0.1f, 0.174f, 0.255f, 0.0f, 0.408f,
   -0.281f, 0.1f, -0.0f, 0.482f, 0.0f, 0.773f,
   -0.199f, 0.1f, 0.199f, 0.482f, 0.0f, 0.773f,

   0.246f, 0.1f, 0.0f, 0.255f, 0.0f, 0.408f,
   0.199f, 0.1f, 0.199f, 0.482f, 0.0f, 0.773f,
   0.281f, 0.1f, 0.0f, 0.486f, 0.027f, 0.773f,

   0.0f, 0.1f, -0.246f, 0.255f, 0.0f, 0.408f,
   0.199f, 0.1f, -0.199f, 0.482f, 0.0f, 0.773f,
   0.0f, 0.1f, -0.281f, 0.482f, 0.004f, 0.773f,

   -0.281f, 0.1f, -0.0f, 0.482f, 0.0f, 0.773f,
   -0.174f, 0.1f, -0.174f, 0.255f, 0.0f, 0.408f,
   -0.199f, 0.1f, -0.199f, 0.482f, 0.0f, 0.761f,

   0.0f, 0.1f, 0.246f, 0.255f, 0.0f, 0.408f,
   -0.199f, 0.1f, 0.199f, 0.482f, 0.0f, 0.773f,
   0.0f, 0.1f, 0.281f, 0.482f, 0.004f, 0.773f,

   -0.174f, -0.0f, -0.174f, 0.38f, 0.0f, 0.608f,
   0.0f, 0.1f, -0.246f, 0.255f, 0.0f, 0.408f,
   -0.174f, 0.1f, -0.174f, 0.255f, 0.0f, 0.408f,

   -0.174f, -0.0f, 0.174f, 0.38f, 0.0f, 0.608f,
   -0.246f, 0.1f, -0.0f, 0.255f, 0.0f, 0.408f,
   -0.174f, 0.1f, 0.174f, 0.255f, 0.0f, 0.408f,

   0.246f, -0.0f, 0.0f, 0.38f, 0.0f, 0.608f,
   0.174f, 0.1f, 0.174f, 0.255f, 0.0f, 0.408f,
   0.246f, 0.1f, 0.0f, 0.255f, 0.0f, 0.408f,

   0.0f, -0.0f, -0.246f, 0.38f, 0.0f, 0.608f,
   0.174f, 0.1f, -0.174f, 0.255f, 0.0f, 0.408f,
   0.0f, 0.1f, -0.246f, 0.255f, 0.0f, 0.408f,

   -0.246f, -0.0f, -0.0f, 0.38f, 0.0f, 0.608f,
   -0.174f, 0.1f, -0.174f, 0.255f, 0.0f, 0.408f,
   -0.246f, 0.1f, -0.0f, 0.255f, 0.0f, 0.408f,

   0.0f, -0.0f, 0.246f, 0.38f, 0.0f, 0.608f,
   -0.174f, 0.1f, 0.174f, 0.255f, 0.0f, 0.408f,
   0.0f, 0.1f, 0.246f, 0.255f, 0.0f, 0.408f,

   0.174f, -0.0f, 0.174f, 0.38f, 0.0f, 0.608f,
   0.0f, 0.1f, 0.246f, 0.255f, 0.0f, 0.408f,
   0.174f, 0.1f, 0.174f, 0.255f, 0.0f, 0.408f,

   0.174f, -0.0f, -0.174f, 0.38f, 0.0f, 0.608f,
   0.246f, 0.1f, 0.0f, 0.255f, 0.0f, 0.408f,
   0.174f, 0.1f, -0.174f, 0.255f, 0.0f, 0.408f,

   0.149f, 0.02f, 0.149f, 0.325f, 0.0f, 0.522f,
   0.0f, 0.02f, 0.0f, 0.325f, 0.0f, 0.522f,
   0.211f, 0.02f, 0.0f, 0.325f, 0.0f, 0.522f,

   0.149f, 0.02f, -0.149f, 0.325f, 0.0f, 0.522f,
   0.0f, 0.02f, 0.0f, 0.325f, 0.0f, 0.522f,
   0.0f, 0.02f, -0.211f, 0.325f, 0.0f, 0.522f,

   -0.149f, 0.02f, -0.149f, 0.325f, 0.0f, 0.522f,
   0.0f, 0.02f, 0.0f, 0.325f, 0.0f, 0.522f,
   -0.211f, 0.02f, -0.0f, 0.325f, 0.0f, 0.522f,

   -0.149f, 0.02f, 0.149f, 0.325f, 0.0f, 0.522f,
   0.0f, 0.02f, 0.0f, 0.325f, 0.0f, 0.522f,
   0.0f, 0.02f, 0.211f, 0.325f, 0.0f, 0.522f,

   0.0f, 0.02f, 0.211f, 0.325f, 0.0f, 0.522f,
   0.0f, 0.02f, 0.0f, 0.325f, 0.0f, 0.522f,
   0.149f, 0.02f, 0.149f, 0.325f, 0.0f, 0.522f,

   0.211f, 0.02f, 0.0f, 0.325f, 0.0f, 0.522f,
   0.0f, 0.02f, 0.0f, 0.325f, 0.0f, 0.522f,
   0.149f, 0.02f, -0.149f, 0.325f, 0.0f, 0.522f,

   0.0f, 0.02f, -0.211f, 0.325f, 0.0f, 0.522f,
   0.0f, 0.02f, 0.0f, 0.325f, 0.0f, 0.522f,
   -0.149f, 0.02f, -0.149f, 0.325f, 0.0f, 0.522f,

   -0.211f, 0.02f, -0.0f, 0.325f, 0.0f, 0.522f,
   0.0f, 0.02f, 0.0f, 0.325f, 0.0f, 0.522f,
   -0.149f, 0.02f, 0.149f, 0.325f, 0.0f, 0.522f,

   0.0f, 0.1f, -0.281f, 0.482f, 0.004f, 0.773f,
   0.199f, 0.1f, -0.199f, 0.482f, 0.0f, 0.773f,
   0.199f, 0.2f, -0.199f, 0.525f, 0.016f, 0.831f,

   -0.281f, 0.1f, -0.0f, 0.482f, 0.0f, 0.773f,
   -0.199f, 0.1f, -0.199f, 0.482f, 0.0f, 0.761f,
   -0.199f, 0.2f, -0.199f, 0.525f, 0.016f, 0.831f,

   0.0f, 0.1f, 0.281f, 0.482f, 0.004f, 0.773f,
   -0.199f, 0.1f, 0.199f, 0.482f, 0.0f, 0.773f,
   -0.199f, 0.2f, 0.199f, 0.525f, 0.016f, 0.831f,

   0.199f, 0.1f, 0.199f, 0.482f, 0.0f, 0.773f,
   0.0f, 0.1f, 0.281f, 0.482f, 0.004f, 0.773f,
   0.0f, 0.2f, 0.281f, 0.525f, 0.016f, 0.831f,

   0.199f, 0.1f, -0.199f, 0.482f, 0.0f, 0.773f,
   0.281f, 0.1f, 0.0f, 0.486f, 0.027f, 0.773f,
   0.281f, 0.2f, 0.0f, 0.525f, 0.016f, 0.831f,

   -0.199f, 0.1f, -0.199f, 0.482f, 0.0f, 0.761f,
   0.0f, 0.1f, -0.281f, 0.482f, 0.004f, 0.773f,
   0.0f, 0.2f, -0.281f, 0.525f, 0.016f, 0.831f,

   -0.199f, 0.1f, 0.199f, 0.482f, 0.0f, 0.773f,
   -0.281f, 0.1f, -0.0f, 0.482f, 0.0f, 0.773f,
   -0.281f, 0.2f, -0.0f, 0.525f, 0.016f, 0.831f,

   0.281f, 0.1f, 0.0f, 0.486f, 0.027f, 0.773f,
   0.199f, 0.1f, 0.199f, 0.482f, 0.0f, 0.773f,
   0.199f, 0.2f, 0.199f, 0.525f, 0.016f, 0.831f,

   0.199f, 0.2f, -0.199f, 0.525f, 0.016f, 0.831f,
   0.149f, 0.2f, -0.149f, 0.412f, 0.008f, 0.655f,
   0.0f, 0.2f, -0.211f, 0.412f, 0.008f, 0.655f,

   -0.199f, 0.2f, -0.199f, 0.525f, 0.016f, 0.831f,
   -0.149f, 0.2f, -0.149f, 0.412f, 0.008f, 0.655f,
   -0.211f, 0.2f, -0.0f, 0.412f, 0.008f, 0.655f,

   -0.149f, 0.2f, 0.149f, 0.412f, 0.008f, 0.655f,
   0.0f, 0.2f, 0.211f, 0.412f, 0.008f, 0.655f,
   0.0f, 0.2f, 0.281f, 0.525f, 0.016f, 0.831f,

   0.0f, 0.2f, 0.211f, 0.412f, 0.008f, 0.655f,
   0.149f, 0.2f, 0.149f, 0.412f, 0.008f, 0.655f,
   0.199f, 0.2f, 0.199f, 0.525f, 0.016f, 0.831f,

   0.211f, 0.2f, 0.0f, 0.412f, 0.008f, 0.655f,
   0.149f, 0.2f, -0.149f, 0.412f, 0.008f, 0.655f,
   0.199f, 0.2f, -0.199f, 0.525f, 0.016f, 0.831f,

   0.0f, 0.2f, -0.211f, 0.412f, 0.008f, 0.655f,
   -0.149f, 0.2f, -0.149f, 0.412f, 0.008f, 0.655f,
   -0.199f, 0.2f, -0.199f, 0.525f, 0.016f, 0.831f,

   -0.211f, 0.2f, -0.0f, 0.412f, 0.008f, 0.655f,
   -0.149f, 0.2f, 0.149f, 0.412f, 0.008f, 0.655f,
   -0.199f, 0.2f, 0.199f, 0.525f, 0.016f, 0.831f,

   0.149f, 0.2f, 0.149f, 0.412f, 0.008f, 0.655f,
   0.211f, 0.2f, 0.0f, 0.412f, 0.008f, 0.655f,
   0.281f, 0.2f, 0.0f, 0.525f, 0.016f, 0.831f,

   -0.149f, 0.02f, -0.149f, 0.325f, 0.0f, 0.522f,
   -0.211f, 0.02f, -0.0f, 0.325f, 0.0f, 0.522f,
   -0.211f, 0.2f, -0.0f, 0.412f, 0.008f, 0.655f,

   -0.149f, 0.02f, 0.149f, 0.325f, 0.0f, 0.522f,
   0.0f, 0.02f, 0.211f, 0.325f, 0.0f, 0.522f,
   0.0f, 0.2f, 0.211f, 0.412f, 0.008f, 0.655f,

   0.0f, 0.02f, 0.211f, 0.325f, 0.0f, 0.522f,
   0.149f, 0.02f, 0.149f, 0.325f, 0.0f, 0.522f,
   0.149f, 0.2f, 0.149f, 0.412f, 0.008f, 0.655f,

   0.211f, 0.02f, 0.0f, 0.325f, 0.0f, 0.522f,
   0.149f, 0.02f, -0.149f, 0.325f, 0.0f, 0.522f,
   0.149f, 0.2f, -0.149f, 0.412f, 0.008f, 0.655f,

   0.0f, 0.02f, -0.211f, 0.325f, 0.0f, 0.522f,
   -0.149f, 0.02f, -0.149f, 0.325f, 0.0f, 0.522f,
   -0.149f, 0.2f, -0.149f, 0.412f, 0.008f, 0.655f,

   -0.211f, 0.02f, -0.0f, 0.325f, 0.0f, 0.522f,
   -0.149f, 0.02f, 0.149f, 0.325f, 0.0f, 0.522f,
   -0.149f, 0.2f, 0.149f, 0.412f, 0.008f, 0.655f,

   0.149f, 0.02f, 0.149f, 0.325f, 0.0f, 0.522f,
   0.211f, 0.02f, 0.0f, 0.325f, 0.0f, 0.522f,
   0.211f, 0.2f, 0.0f, 0.412f, 0.008f, 0.655f,

   0.149f, 0.02f, -0.149f, 0.325f, 0.0f, 0.522f,
   0.0f, 0.02f, -0.211f, 0.325f, 0.0f, 0.522f,
   0.0f, 0.2f, -0.211f, 0.412f, 0.008f, 0.655f,

   0.199f, 0.1f, 0.199f, 0.482f, 0.0f, 0.773f,
   0.174f, 0.1f, 0.174f, 0.255f, 0.0f, 0.408f,
   0.0f, 0.1f, 0.246f, 0.255f, 0.0f, 0.408f,

   0.174f, 0.1f, -0.174f, 0.255f, 0.0f, 0.408f,
   0.246f, 0.1f, 0.0f, 0.255f, 0.0f, 0.408f,
   0.281f, 0.1f, 0.0f, 0.486f, 0.027f, 0.773f,

   -0.174f, 0.1f, -0.174f, 0.255f, 0.0f, 0.408f,
   0.0f, 0.1f, -0.246f, 0.255f, 0.0f, 0.408f,
   0.0f, 0.1f, -0.281f, 0.482f, 0.004f, 0.773f,

   -0.174f, 0.1f, 0.174f, 0.255f, 0.0f, 0.408f,
   -0.246f, 0.1f, -0.0f, 0.255f, 0.0f, 0.408f,
   -0.281f, 0.1f, -0.0f, 0.482f, 0.0f, 0.773f,

   0.246f, 0.1f, 0.0f, 0.255f, 0.0f, 0.408f,
   0.174f, 0.1f, 0.174f, 0.255f, 0.0f, 0.408f,
   0.199f, 0.1f, 0.199f, 0.482f, 0.0f, 0.773f,

   0.0f, 0.1f, -0.246f, 0.255f, 0.0f, 0.408f,
   0.174f, 0.1f, -0.174f, 0.255f, 0.0f, 0.408f,
   0.199f, 0.1f, -0.199f, 0.482f, 0.0f, 0.773f,

   -0.281f, 0.1f, -0.0f, 0.482f, 0.0f, 0.773f,
   -0.246f, 0.1f, -0.0f, 0.255f, 0.0f, 0.408f,
   -0.174f, 0.1f, -0.174f, 0.255f, 0.0f, 0.408f,

   0.0f, 0.1f, 0.246f, 0.255f, 0.0f, 0.408f,
   -0.174f, 0.1f, 0.174f, 0.255f, 0.0f, 0.408f,
   -0.199f, 0.1f, 0.199f, 0.482f, 0.0f, 0.773f,

   -0.174f, -0.0f, -0.174f, 0.38f, 0.0f, 0.608f,
   0.0f, -0.0f, -0.246f, 0.38f, 0.0f, 0.608f,
   0.0f, 0.1f, -0.246f, 0.255f, 0.0f, 0.408f,

   -0.174f, -0.0f, 0.174f, 0.38f, 0.0f, 0.608f,
   -0.246f, -0.0f, -0.0f, 0.38f, 0.0f, 0.608f,
   -0.246f, 0.1f, -0.0f, 0.255f, 0.0f, 0.408f,

   0.246f, -0.0f, 0.0f, 0.38f, 0.0f, 0.608f,
   0.174f, -0.0f, 0.174f, 0.38f, 0.0f, 0.608f,
   0.174f, 0.1f, 0.174f, 0.255f, 0.0f, 0.408f,

   0.0f, -0.0f, -0.246f, 0.38f, 0.0f, 0.608f,
   0.174f, -0.0f, -0.174f, 0.38f, 0.0f, 0.608f,
   0.174f, 0.1f, -0.174f, 0.255f, 0.0f, 0.408f,

   -0.246f, -0.0f, -0.0f, 0.38f, 0.0f, 0.608f,
   -0.174f, -0.0f, -0.174f, 0.38f, 0.0f, 0.608f,
   -0.174f, 0.1f, -0.174f, 0.255f, 0.0f, 0.408f,

   0.0f, -0.0f, 0.246f, 0.38f, 0.0f, 0.608f,
   -0.174f, -0.0f, 0.174f, 0.38f, 0.0f, 0.608f,
   -0.174f, 0.1f, 0.174f, 0.255f, 0.0f, 0.408f,

   0.174f, -0.0f, 0.174f, 0.38f, 0.0f, 0.608f,
   0.0f, -0.0f, 0.246f, 0.38f, 0.0f, 0.608f,
   0.0f, 0.1f, 0.246f, 0.255f, 0.0f, 0.408f,

   0.174f, -0.0f, -0.174f, 0.38f, 0.0f, 0.608f,
   0.246f, -0.0f, 0.0f, 0.38f, 0.0f, 0.608f,
   0.246f, 0.1f, 0.0f, 0.255f, 0.0f, 0.408f,
}; //Keep going, almost there...

const float skyboxMeshData[] =
{
   0.0f, -30.0f, 0.0f, 0.255f, 0.122f, 0.565f,
   12.99f, -25.981f, 7.5f, 0.255f, 0.122f, 0.565f,
   0.0f, -25.981f, 15.0f, 0.255f, 0.122f, 0.565f,

   22.5f, -15.0f, 12.99f, 0.184f, 0.086f, 0.412f,
   0.0f, -0.0f, 30.0f, 0.086f, 0.09f, 0.259f,
   0.0f, -15.0f, 25.981f, 0.239f, 0.11f, 0.525f,

   22.5f, 15.0f, 12.99f, 0.024f, 0.024f, 0.067f,
   0.0f, 25.981f, 15.0f, 0.024f, 0.024f, 0.063f,
   0.0f, 15.0f, 25.981f, 0.024f, 0.024f, 0.067f,

   0.0f, -25.981f, 15.0f, 0.255f, 0.122f, 0.565f,
   22.5f, -15.0f, 12.99f, 0.184f, 0.086f, 0.412f,
   0.0f, -15.0f, 25.981f, 0.239f, 0.11f, 0.525f,

   25.981f, -0.0f, 15.0f, 0.086f, 0.09f, 0.259f,
   0.0f, 15.0f, 25.981f, 0.024f, 0.024f, 0.067f,
   0.0f, -0.0f, 30.0f, 0.086f, 0.09f, 0.259f,

   0.0f, 25.981f, 15.0f, 0.024f, 0.024f, 0.063f,
   12.99f, 25.981f, 7.5f, 0.024f, 0.024f, 0.067f,
   -0.0f, 30.0f, 0.0f, 0.024f, 0.024f, 0.067f,

   12.99f, -25.981f, -7.5f, 0.255f, 0.122f, 0.565f,
   22.5f, -15.0f, 12.99f, 0.184f, 0.086f, 0.412f,
   12.99f, -25.981f, 7.5f, 0.255f, 0.122f, 0.565f,

   25.981f, -0.0f, -15.0f, 0.086f, 0.09f, 0.259f,
   22.5f, 15.0f, 12.99f, 0.024f, 0.024f, 0.067f,
   25.981f, -0.0f, 15.0f, 0.086f, 0.09f, 0.259f,

   12.99f, 25.981f, 7.5f, 0.024f, 0.024f, 0.067f,
   12.99f, 25.981f, -7.5f, 0.024f, 0.024f, 0.067f,
   -0.0f, 30.0f, 0.0f, 0.024f, 0.024f, 0.067f,

   0.0f, -30.0f, 0.0f, 0.255f, 0.122f, 0.565f,
   12.99f, -25.981f, -7.5f, 0.255f, 0.122f, 0.565f,
   12.99f, -25.981f, 7.5f, 0.255f, 0.122f, 0.565f,

   22.5f, -15.0f, -12.99f, 0.235f, 0.106f, 0.525f,
   25.981f, -0.0f, 15.0f, 0.086f, 0.09f, 0.259f,
   22.5f, -15.0f, 12.99f, 0.184f, 0.086f, 0.412f,

   22.5f, 15.0f, -12.99f, 0.024f, 0.024f, 0.067f,
   12.99f, 25.981f, 7.5f, 0.024f, 0.024f, 0.067f,
   22.5f, 15.0f, 12.99f, 0.024f, 0.024f, 0.067f,

   -0.0f, -25.981f, -15.0f, 0.255f, 0.122f, 0.565f,
   22.5f, -15.0f, -12.99f, 0.235f, 0.106f, 0.525f,
   12.99f, -25.981f, -7.5f, 0.255f, 0.122f, 0.565f,

   -0.0f, -0.0f, -30.0f, 0.086f, 0.09f, 0.259f,
   22.5f, 15.0f, -12.99f, 0.024f, 0.024f, 0.067f,
   25.981f, -0.0f, -15.0f, 0.086f, 0.09f, 0.259f,

   12.99f, 25.981f, -7.5f, 0.024f, 0.024f, 0.067f,
   -0.0f, 25.981f, -15.0f, 0.024f, 0.024f, 0.067f,
   -0.0f, 30.0f, 0.0f, 0.024f, 0.024f, 0.067f,

   0.0f, -30.0f, 0.0f, 0.255f, 0.122f, 0.565f,
   -0.0f, -25.981f, -15.0f, 0.255f, 0.122f, 0.565f,
   12.99f, -25.981f, -7.5f, 0.255f, 0.122f, 0.565f,

   -0.0f, -15.0f, -25.981f, 0.235f, 0.11f, 0.525f,
   25.981f, -0.0f, -15.0f, 0.086f, 0.09f, 0.259f,
   22.5f, -15.0f, -12.99f, 0.235f, 0.106f, 0.525f,

   -0.0f, 15.0f, -25.981f, 0.024f, 0.024f, 0.067f,
   12.99f, 25.981f, -7.5f, 0.024f, 0.024f, 0.067f,
   22.5f, 15.0f, -12.99f, 0.024f, 0.024f, 0.067f,

   -12.99f, -25.981f, -7.5f, 0.255f, 0.122f, 0.565f,
   -0.0f, -15.0f, -25.981f, 0.235f, 0.11f, 0.525f,
   -0.0f, -25.981f, -15.0f, 0.255f, 0.122f, 0.565f,

   -25.981f, -0.0f, -15.0f, 0.086f, 0.09f, 0.259f,
   -0.0f, 15.0f, -25.981f, 0.024f, 0.024f, 0.067f,
   -0.0f, -0.0f, -30.0f, 0.086f, 0.09f, 0.259f,

   -0.0f, 25.981f, -15.0f, 0.024f, 0.024f, 0.067f,
   -12.99f, 25.981f, -7.5f, 0.024f, 0.024f, 0.067f,
   -0.0f, 30.0f, 0.0f, 0.024f, 0.024f, 0.067f,

   0.0f, -30.0f, 0.0f, 0.255f, 0.122f, 0.565f,
   -12.99f, -25.981f, -7.5f, 0.255f, 0.122f, 0.565f,
   -0.0f, -25.981f, -15.0f, 0.255f, 0.122f, 0.565f,

   -22.5f, -15.0f, -12.99f, 0.255f, 0.122f, 0.565f,
   -0.0f, -0.0f, -30.0f, 0.086f, 0.09f, 0.259f,
   -0.0f, -15.0f, -25.981f, 0.235f, 0.11f, 0.525f,

   -0.0f, 15.0f, -25.981f, 0.024f, 0.024f, 0.067f,
   -12.99f, 25.981f, -7.5f, 0.024f, 0.024f, 0.067f,
   -0.0f, 25.981f, -15.0f, 0.024f, 0.024f, 0.067f,

   -12.99f, -25.981f, 7.5f, 0.255f, 0.122f, 0.565f,
   -22.5f, -15.0f, -12.99f, 0.255f, 0.122f, 0.565f,
   -12.99f, -25.981f, -7.5f, 0.255f, 0.122f, 0.565f,

   -25.981f, -0.0f, 15.0f, 0.086f, 0.09f, 0.259f,
   -22.5f, 15.0f, -12.99f, 0.024f, 0.024f, 0.067f,
   -25.981f, -0.0f, -15.0f, 0.086f, 0.09f, 0.259f,

   -12.99f, 25.981f, -7.5f, 0.024f, 0.024f, 0.067f,
   -12.99f, 25.981f, 7.5f, 0.024f, 0.024f, 0.067f,
   -0.0f, 30.0f, 0.0f, 0.024f, 0.024f, 0.067f,

   0.0f, -30.0f, 0.0f, 0.255f, 0.122f, 0.565f,
   -12.99f, -25.981f, 7.5f, 0.255f, 0.122f, 0.565f,
   -12.99f, -25.981f, -7.5f, 0.255f, 0.122f, 0.565f,

   -22.5f, -15.0f, 12.99f, 0.243f, 0.118f, 0.545f,
   -25.981f, -0.0f, -15.0f, 0.086f, 0.09f, 0.259f,
   -22.5f, -15.0f, -12.99f, 0.255f, 0.122f, 0.565f,

   -22.5f, 15.0f, 12.99f, 0.024f, 0.024f, 0.067f,
   -12.99f, 25.981f, -7.5f, 0.024f, 0.024f, 0.067f,
   -22.5f, 15.0f, -12.99f, 0.024f, 0.024f, 0.067f,

   0.0f, -0.0f, 30.0f, 0.086f, 0.09f, 0.259f,
   -22.5f, 15.0f, 12.99f, 0.024f, 0.024f, 0.067f,
   -25.981f, -0.0f, 15.0f, 0.086f, 0.09f, 0.259f,

   -12.99f, 25.981f, 7.5f, 0.024f, 0.024f, 0.067f,
   0.0f, 25.981f, 15.0f, 0.024f, 0.024f, 0.063f,
   -0.0f, 30.0f, 0.0f, 0.024f, 0.024f, 0.067f,

   0.0f, -30.0f, 0.0f, 0.255f, 0.122f, 0.565f,
   0.0f, -25.981f, 15.0f, 0.255f, 0.122f, 0.565f,
   -12.99f, -25.981f, 7.5f, 0.255f, 0.122f, 0.565f,

   0.0f, -15.0f, 25.981f, 0.239f, 0.11f, 0.525f,
   -25.981f, -0.0f, 15.0f, 0.086f, 0.09f, 0.259f,
   -22.5f, -15.0f, 12.99f, 0.243f, 0.118f, 0.545f,

   0.0f, 15.0f, 25.981f, 0.024f, 0.024f, 0.067f,
   -12.99f, 25.981f, 7.5f, 0.024f, 0.024f, 0.067f,
   -22.5f, 15.0f, 12.99f, 0.024f, 0.024f, 0.067f,

   -12.99f, -25.981f, 7.5f, 0.255f, 0.122f, 0.565f,
   0.0f, -15.0f, 25.981f, 0.239f, 0.11f, 0.525f,
   -22.5f, -15.0f, 12.99f, 0.243f, 0.118f, 0.545f,

   22.5f, -15.0f, 12.99f, 0.184f, 0.086f, 0.412f,
   25.981f, -0.0f, 15.0f, 0.086f, 0.09f, 0.259f,
   0.0f, -0.0f, 30.0f, 0.086f, 0.09f, 0.259f,

   22.5f, 15.0f, 12.99f, 0.024f, 0.024f, 0.067f,
   12.99f, 25.981f, 7.5f, 0.024f, 0.024f, 0.067f,
   0.0f, 25.981f, 15.0f, 0.024f, 0.024f, 0.063f,

   0.0f, -25.981f, 15.0f, 0.255f, 0.122f, 0.565f,
   12.99f, -25.981f, 7.5f, 0.255f, 0.122f, 0.565f,
   22.5f, -15.0f, 12.99f, 0.184f, 0.086f, 0.412f,

   25.981f, -0.0f, 15.0f, 0.086f, 0.09f, 0.259f,
   22.5f, 15.0f, 12.99f, 0.024f, 0.024f, 0.067f,
   0.0f, 15.0f, 25.981f, 0.024f, 0.024f, 0.067f,

   12.99f, -25.981f, -7.5f, 0.255f, 0.122f, 0.565f,
   22.5f, -15.0f, -12.99f, 0.235f, 0.106f, 0.525f,
   22.5f, -15.0f, 12.99f, 0.184f, 0.086f, 0.412f,

   25.981f, -0.0f, -15.0f, 0.086f, 0.09f, 0.259f,
   22.5f, 15.0f, -12.99f, 0.024f, 0.024f, 0.067f,
   22.5f, 15.0f, 12.99f, 0.024f, 0.024f, 0.067f,

   22.5f, -15.0f, -12.99f, 0.235f, 0.106f, 0.525f,
   25.981f, -0.0f, -15.0f, 0.086f, 0.09f, 0.259f,
   25.981f, -0.0f, 15.0f, 0.086f, 0.09f, 0.259f,

   22.5f, 15.0f, -12.99f, 0.024f, 0.024f, 0.067f,
   12.99f, 25.981f, -7.5f, 0.024f, 0.024f, 0.067f,
   12.99f, 25.981f, 7.5f, 0.024f, 0.024f, 0.067f,

   -0.0f, -25.981f, -15.0f, 0.255f, 0.122f, 0.565f,
   -0.0f, -15.0f, -25.981f, 0.235f, 0.11f, 0.525f,
   22.5f, -15.0f, -12.99f, 0.235f, 0.106f, 0.525f,

   -0.0f, -0.0f, -30.0f, 0.086f, 0.09f, 0.259f,
   -0.0f, 15.0f, -25.981f, 0.024f, 0.024f, 0.067f,
   22.5f, 15.0f, -12.99f, 0.024f, 0.024f, 0.067f,

   -0.0f, -15.0f, -25.981f, 0.235f, 0.11f, 0.525f,
   -0.0f, -0.0f, -30.0f, 0.086f, 0.09f, 0.259f,
   25.981f, -0.0f, -15.0f, 0.086f, 0.09f, 0.259f,

   -0.0f, 15.0f, -25.981f, 0.024f, 0.024f, 0.067f,
   -0.0f, 25.981f, -15.0f, 0.024f, 0.024f, 0.067f,
   12.99f, 25.981f, -7.5f, 0.024f, 0.024f, 0.067f,

   -12.99f, -25.981f, -7.5f, 0.255f, 0.122f, 0.565f,
   -22.5f, -15.0f, -12.99f, 0.255f, 0.122f, 0.565f,
   -0.0f, -15.0f, -25.981f, 0.235f, 0.11f, 0.525f,

   -25.981f, -0.0f, -15.0f, 0.086f, 0.09f, 0.259f,
   -22.5f, 15.0f, -12.99f, 0.024f, 0.024f, 0.067f,
   -0.0f, 15.0f, -25.981f, 0.024f, 0.024f, 0.067f,

   -22.5f, -15.0f, -12.99f, 0.255f, 0.122f, 0.565f,
   -25.981f, -0.0f, -15.0f, 0.086f, 0.09f, 0.259f,
   -0.0f, -0.0f, -30.0f, 0.086f, 0.09f, 0.259f,

   -0.0f, 15.0f, -25.981f, 0.024f, 0.024f, 0.067f,
   -22.5f, 15.0f, -12.99f, 0.024f, 0.024f, 0.067f,
   -12.99f, 25.981f, -7.5f, 0.024f, 0.024f, 0.067f,

   -12.99f, -25.981f, 7.5f, 0.255f, 0.122f, 0.565f,
   -22.5f, -15.0f, 12.99f, 0.243f, 0.118f, 0.545f,
   -22.5f, -15.0f, -12.99f, 0.255f, 0.122f, 0.565f,

   -25.981f, -0.0f, 15.0f, 0.086f, 0.09f, 0.259f,
   -22.5f, 15.0f, 12.99f, 0.024f, 0.024f, 0.067f,
   -22.5f, 15.0f, -12.99f, 0.024f, 0.024f, 0.067f,

   -22.5f, -15.0f, 12.99f, 0.243f, 0.118f, 0.545f,
   -25.981f, -0.0f, 15.0f, 0.086f, 0.09f, 0.259f,
   -25.981f, -0.0f, -15.0f, 0.086f, 0.09f, 0.259f,

   -22.5f, 15.0f, 12.99f, 0.024f, 0.024f, 0.067f,
   -12.99f, 25.981f, 7.5f, 0.024f, 0.024f, 0.067f,
   -12.99f, 25.981f, -7.5f, 0.024f, 0.024f, 0.067f,

   0.0f, -0.0f, 30.0f, 0.086f, 0.09f, 0.259f,
   0.0f, 15.0f, 25.981f, 0.024f, 0.024f, 0.067f,
   -22.5f, 15.0f, 12.99f, 0.024f, 0.024f, 0.067f,

   0.0f, -15.0f, 25.981f, 0.239f, 0.11f, 0.525f,
   0.0f, -0.0f, 30.0f, 0.086f, 0.09f, 0.259f,
   -25.981f, -0.0f, 15.0f, 0.086f, 0.09f, 0.259f,

   0.0f, 15.0f, 25.981f, 0.024f, 0.024f, 0.067f,
   0.0f, 25.981f, 15.0f, 0.024f, 0.024f, 0.063f,
   -12.99f, 25.981f, 7.5f, 0.024f, 0.024f, 0.067f,

   -12.99f, -25.981f, 7.5f, 0.255f, 0.122f, 0.565f,
   0.0f, -25.981f, 15.0f, 0.255f, 0.122f, 0.565f,
   0.0f, -15.0f, 25.981f, 0.239f, 0.11f, 0.525f,
};

//=============================================================================
// Game logic.
//=============================================================================

//Defines an enum of valid states the game item can have.
typedef enum
{
  //The item is at its initial position.
  Initial,
  //The item is currently held by the player.
  Held,
  //The item was dropped at the goal and the game will close.
  Dropped
} ItemState;

//Defines an enum of valid field types.
typedef enum
{
  //The starting point of the user.
  Init = -2,
  //The gate between the main room and the labyrinth.
  Arch = -1,
  //A normal floor tile.
  Tile = 0,
  //An impenetrable wall block.
  Wall = 1,
  //The main quest item in the game.
  Item = 2,
  //The goal of the game, into which the quest item needs to be placed.
  Goal = 3
} Field;

//The following three variables define the game field and need to be consistent
//to allow the game to start.
const int mapWidth = 15;
const int mapDepth = 11;
const Field map[] =
{
  Wall, Wall, Wall, Wall, Wall, Wall, Wall, Wall, Wall, Wall, Wall,
  Wall, Tile, Tile, Tile, Tile, Wall, Tile, Tile, Tile, Tile, Wall,
  Wall, Tile, Wall, Wall, Wall, Wall, Wall, Wall, Wall, Tile, Wall,
  Wall, Tile, Tile, Tile, Tile, Tile, Tile, Tile, Wall, Tile, Wall,
  Wall, Wall, Wall, Wall, Wall, Tile, Wall, Wall, Wall, Tile, Wall,
  Wall, Tile, Goal, Tile, Wall, Tile, Wall, Tile, Wall, Tile, Wall,
  Wall, Tile, Tile, Tile, Wall, Tile, Wall, Tile, Tile, Tile, Wall,
  Wall, Init, Tile, Tile, Arch, Tile, Wall, Tile, Wall, Tile, Wall,
  Wall, Tile, Tile, Tile, Wall, Tile, Tile, Tile, Wall, Tile, Wall,
#if defined(BORING_MODE)
  Wall, Tile, Tile, Item, Wall, Wall, Wall, Wall, Wall, Tile, Wall,
#else
  Wall, Tile, Tile, Tile, Wall, Wall, Wall, Wall, Wall, Tile, Wall,
#endif
  Wall, Wall, Wall, Wall, Wall, Tile, Tile, Tile, Tile, Tile, Wall,
  Wall, Tile, Tile, Tile, Tile, Tile, Wall, Wall, Wall, Wall, Wall,
  Wall, Tile, Wall, Wall, Wall, Wall, Wall, Tile, Tile, Item, Wall,
  Wall, Tile, Tile, Tile, Tile, Tile, Tile, Tile, Wall, Wall, Wall,
  Wall, Wall, Wall, Wall, Wall, Wall, Wall, Wall, Wall, Wall, Wall
};

bool isLoaded = false;
ShaderProgram shaderProgram;
BufferedMesh skyboxMesh, wallMesh, floorMesh, archMesh, crystalMesh, tubeMesh;

//Contains the current states of the input actions, which get updated by the
//user input event handlers and should not be modified anywhere else.
bool inputForward = false, inputRight = false, inputBackwards = false,
inputLeft = false, inputJump = false, inputAction = false;

//Contains the mouse data from the last update (updated by onUpdate event).
float previousMouseX = 0, previousMouseY = 0;
//Always contains the current mouse data (updated by onMouse event).
float currentMouseX = 0, currentMouseY = 0;

//The following values are modified in the update method and should not be 
//changed anywhere else. Unless stated otherwise, the values are all in world
//units, rotation angles are in degrees.

//The current exact player position in the world.
float playerX = 0, playerY = 0, playerZ = 0;
//The current player accerlation.
float playerAccerlationX = 0, playerAccerlationY = 0, playerAccerlationZ = 0;
//The current player rotation.
float playerRotationY = 0, playerRotationX = 0;
//The current player rotation accerlation.
float playerRotationAccerlationY = 0, playerRotationAccerlationX = 0;
//The current rotation of the quest item (animated value).
float itemRotationY = 0;

//The current state of the quest item (and therefore of the game).
ItemState itemState = Initial;
//A current value that - if decremented - will fade out the game to black.
float gameBrightness = 0.0f;

//The time (with the application start as "point zero") when the game was 
//updated the last time (in milliseconds).
int lastUpdateTime;
//The milliseconds part of the current time (= application runtime % 1000).
float currentTimeMs = 0;

//The current dimensions of the game window.
int currentWindowWidth, currentWindowHeight;

//Translates a world position into field indicies (without checking bounds).
//positionX: The X position in world coordinates.
//positionY: The Y position in world coordinates.
//indexX: The pointer to store the resulting field index (X axis) into.
//indexY: The pointer to store the resulting field index (Y axis) into.
void Game_getMapFieldIndiciesByPosition(float positionX, float positionZ,
  int *indexX, int *indexZ)
{
  (*indexX) = (int)roundf(positionX);
  (*indexZ) = (int)roundf(positionZ);
}

//Translates field indicies into world coordinates (without checking bounds).
//indexX: The field index on the X-axis.
//indexY: The field index on the Y-axis.
//positionX: The pointer to store the resulting X position world coordinate.
//positionY: The pointer to store the resulting Y position world coordinate.
void Game_getMapFieldPositionByIndicies(int indexX, int indexZ,
  float *positionX, float *positionZ)
{
  (*positionX) = (float)indexX;
  (*positionZ) = (float)indexZ;
}

//Gets the field type at specific field indicies.
//To retrieve the field type at a specific world position, use the
//"Game_getMapFieldByPosition" function instead.
//x: The x index of the field.
//z: The z index of the field.
//Terminates the application if x or z are out of bounds.
Field Game_getMapFieldByIndicies(int x, int z)
{
  if (x < 0 || x > mapWidth || z < 0 || z > mapDepth)
    Common_terminate("INGAME", "An invalid field position was requested.");
  return map[x * mapDepth + z];
}

//Gets the field type at a specific world position.
//To retrieve the field type at specific field indicies, use the
//"Game_getMapFieldByIndicies" function instead.
//x: The X position world coordinate.
//z: The X position world coordinate.
//Terminates the application if x or z are out of bounds of the field.
Field Game_getMapFieldByPosition(float x, float z)
{
  int indexX = 0, indexZ = 0;
  Game_getMapFieldIndiciesByPosition(x, z, &indexX, &indexZ);
  return map[indexX * mapDepth + indexZ];
}

//Ocurrs when the game is loaded, after the window was opened the first time.
//Terminates the application when the function is called more than once or when
//the map definition is invalid.
void Game_onLoad(void)
{
  printf("\n");

  if (isLoaded) Common_terminate("LOADING",
    "The load function was called more than once.");

  lastUpdateTime = glutGet(GLUT_ELAPSED_TIME);

  printf("Initializing OpenGL context and shaders...\n");
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glEnable(GL_CULL_FACE);
  shaderProgram = ShaderProgram_createDefault(true);

  printf("Loading game assets...\n");

  if (LENGTHOF(map) != (mapWidth * mapDepth))
    Common_terminate("LOADING",
      "The map size doesn't match with the actual data size.");

  bool spawnPointFound = false;
  int spawnX = 0, spawnZ = 0;
  for (spawnX = 0; spawnX < mapWidth && !spawnPointFound; spawnX++)
    for (spawnZ = 0; spawnZ < mapDepth && !spawnPointFound; spawnZ++)
      if (Game_getMapFieldByIndicies(spawnX, spawnZ) == Init)
      {
        spawnPointFound = true;
        //As the for loop will increment spawnX/spawnZ once more even though 
        //the spawn point was already found, the values need to be decremented 
        //here once to be correct.
        spawnX--;
        spawnZ--;
      }

  Game_getMapFieldPositionByIndicies(spawnX, spawnZ, &playerX, &playerZ);

  if (!spawnPointFound) Common_terminate("LOADING",
    "The map doesn't contain a player spawn point.");

  skyboxMesh = BufferedMesh_create(
    skyboxMeshData, LENGTHOF(skyboxMeshData), shaderProgram);
  wallMesh = BufferedMesh_create(
    wallMeshData, LENGTHOF(wallMeshData), shaderProgram);
  floorMesh = BufferedMesh_create(
    floorMeshData, LENGTHOF(floorMeshData), shaderProgram);
  archMesh = BufferedMesh_create(
    archMeshData, LENGTHOF(archMeshData), shaderProgram);
  crystalMesh = BufferedMesh_create(
    crystalMeshData, LENGTHOF(crystalMeshData), shaderProgram);
  tubeMesh = BufferedMesh_create(
    tubeMeshData, LENGTHOF(tubeMeshData), shaderProgram);

  isLoaded = true;

  printf("Application initialized successfully!\n");
}

//Ocurrs when the game is destroyed.
void Game_onDestroy()
{
  if (isLoaded)
  {
    printf("Unloading game resources and closing application...\n");
    BufferedMesh_destroy(&skyboxMesh);
    BufferedMesh_destroy(&wallMesh);
    BufferedMesh_destroy(&floorMesh);
    BufferedMesh_destroy(&archMesh);
    BufferedMesh_destroy(&crystalMesh);
    BufferedMesh_destroy(&tubeMesh);

    ShaderProgram_destroy(&shaderProgram);

    glutLeaveMainLoop();
    printf("Application terminated successfully!\n\n");
  }
}

//Ocurrs when the game window is resized.
void Game_onResize(int newWidth, int newHeight)
{
  const Matrix4x4 projection =
    Matrix4x4_createPerspective((float)newWidth / newHeight, 0.001f, 200, 70);

  glViewport(0, 0, newWidth, newHeight);
  ShaderProgram_setUniformValue_Matrix4x4(
    shaderProgram.uniformLocation_projection, &projection);
  ShaderProgram_setUniformValue_float(
    shaderProgram.uniformLocation_screenHeight, (float)newHeight);
  currentWindowWidth = newWidth;
  currentWindowHeight = newHeight;
}

//Ocurrs after the player has pressed a key on the keyboard.
void Game_onKeyboardDown(unsigned char key, int mouseX, int mouseY)
{
  mouseX; mouseY;

  switch (key)
  {
    case 'w': inputForward = true; break;
    case 's': inputBackwards = true; break;
    case 'a': inputLeft = true; break;
    case 'd': inputRight = true; break;
    case ' ': inputJump = true; break;
    case 'e': inputAction = true; break;
    case 27: Game_onDestroy(); break;
  }
}

//Ocurrs after the player has released a key on the keyboard.
void Game_onKeyboardUp(unsigned char key, int mouseX, int mouseY)
{
  mouseX; mouseY;

  switch (key)
  {
    case 'w': inputForward = false; break;
    case 's': inputBackwards = false; break;
    case 'a': inputLeft = false; break;
    case 'd': inputRight = false; break;
    case ' ': inputJump = false; break;
    case 'e': inputAction = false; break;
  }
}

//Ocurrs after the player has moved the mouse.
void Game_onMouseMove(int mouseX, int mouseY)
{
  currentMouseX = (float)mouseX;
  currentMouseY = (float)mouseY;
}

//Ocurrs after a "Game_onUpdate" or when GLUT thinks that a redraw is required.
void Game_onRedraw(void)
{
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  //Initialize the shader uniforms for this drawing call.
  ShaderProgram_setUniformValue_float(
    shaderProgram.uniformLocation_currentTimeMs, currentTimeMs);
  ShaderProgram_setUniformValue_float(
    shaderProgram.uniformLocation_brightness, gameBrightness);

  const Matrix4x4 viewTransformation =
    Matrix4x4_createCamera(playerX, playerY + 0.5f, playerZ,
      playerRotationY, playerRotationX);
  const Matrix4x4 originTranslationTransformation =
    Matrix4x4_createTranslation(0, 0, 0);

  ShaderProgram_setUniformValue_Matrix4x4(shaderProgram.uniformLocation_view,
    &viewTransformation);
  ShaderProgram_setUniformValue_float(
    shaderProgram.uniformLocation_opacity, 1);
  ShaderProgram_setUniformValue_Matrix4x4(
    shaderProgram.uniformLocation_model, &originTranslationTransformation);

  //First, draw the skybox (the gradient around the game field).
  BufferedMesh_draw(&skyboxMesh);

  //Calculate the rotation transformation of the quest item, which is used 
  //in different parts of the drawing function.
  const Matrix4x4 meshRotationTransformation =
    Matrix4x4_createRotationY(itemRotationY);

  //If the quest item is currently "held" (it was picked up by the player),
  //it will be drawn right at the player position - with backface culling and 
  //a little translation downwards, only the rotation rings are visible to the
  //player, giving us a "blessed by the gem" kind of look.
  if (itemState == Held)
  {
    const Matrix4x4 meshHoverTranslationTransformation =
      Matrix4x4_createTranslation(playerX, playerY - 0.2f, playerZ);
    const Matrix4x4 meshTransformation = Matrix4x4_multiply(
      &meshHoverTranslationTransformation, &meshRotationTransformation);
    ShaderProgram_setUniformValue_Matrix4x4(
      shaderProgram.uniformLocation_model, &meshTransformation);
    BufferedMesh_draw(&crystalMesh);
  }

  for (int x = 0; x < mapWidth; x++)
  {
    for (int z = 0; z < mapDepth; z++)
    {
      float fieldX, fieldZ;
      Game_getMapFieldPositionByIndicies(x, z, &fieldX, &fieldZ);

      //Fields which are too far away to the player will be faded out. This
      //both looks nice and makes things a bit more efficient. Even though that
      //probably wouldn't be a bottleneck in an application like this.
      float objectPlayerDistance =
        (float)sqrt(pow(fieldX - (double)playerX, 2) +
          pow(fieldZ - (double)playerZ, 2));
      float distanceOpacity =
        1 - MIN(MAX((objectPlayerDistance - 4.0f), 0), 1);

      if (distanceOpacity < EPSILON) continue;

      ShaderProgram_setUniformValue_float(
        shaderProgram.uniformLocation_opacity, distanceOpacity);

      //Set the current field position as transformation matrix for subsequent
      //drawing calls.
      Field currentField = map[x * mapDepth + z];

      const Matrix4x4 meshTranslationTransformation =
        Matrix4x4_createTranslation(fieldX, 0, (float)z);

      ShaderProgram_setUniformValue_Matrix4x4(
        shaderProgram.uniformLocation_model, &meshTranslationTransformation);

      //Drawing the floor under a wall cube isn't required - with the other
      //field types, it is.
      if (currentField != Wall) BufferedMesh_draw(&floorMesh);

      if (currentField == Arch)
        BufferedMesh_draw(&archMesh);
      else if (currentField == Wall)
        BufferedMesh_draw(&wallMesh);
      else if (currentField == Item && itemState == Initial)
      {
        //As the item rotates, the transformation matrix needs to be updated
        //once more here - as a combination of the translation based on the
        //field position and the rotation calculated above already.
        const Matrix4x4 meshTransformation = Matrix4x4_multiply(
          &meshTranslationTransformation, &meshRotationTransformation);
        ShaderProgram_setUniformValue_Matrix4x4(
          shaderProgram.uniformLocation_model, &meshTransformation);

        BufferedMesh_draw(&crystalMesh);
      }
      else if (currentField == Goal)
      {
        BufferedMesh_draw(&tubeMesh);

        //If the player dropped the quest item at the target, the quest item
        //will be drawn right above it... levitating and rotating in its glory.
        if (itemState == Dropped)
        {
          const Matrix4x4 meshTransformation = Matrix4x4_multiply(
            &meshTranslationTransformation, &meshRotationTransformation);
          ShaderProgram_setUniformValue_Matrix4x4(
            shaderProgram.uniformLocation_model, &meshTransformation);
          BufferedMesh_draw(&crystalMesh);
        }
      }
    }
  }

  glutSwapBuffers();
}

//Ocurrs after the configured update timer event fires.
//Also re-registers the update event timer after invocation.
void Game_onUpdate(int uselessValue)
{
  uselessValue;

  int currentUpdateTime = glutGet(GLUT_ELAPSED_TIME);
  float deltaSeconds = (float)(currentUpdateTime - lastUpdateTime) / 1000;
  currentTimeMs = (float)(currentUpdateTime % 1000);

  itemRotationY += deltaSeconds * ITEM_ROTATION_SPEED;

  if (itemState == Initial && gameBrightness < 1.0f)
    gameBrightness = MIN(1, gameBrightness + FADEOUT_SPEED * deltaSeconds);
  else if (itemState == Dropped)
  {
    if (gameBrightness > 0.0f) gameBrightness -= FADEOUT_SPEED * deltaSeconds;
    else
    {
      printf("You finished the game in %.2f seconds. Well done!\n",
        (currentUpdateTime / 1000.0f));
      Game_onDestroy();
    }
  }

  //Calculate the current mouse position by checking how much the mouse was
  //moved from the center of the window since the last update, storing the 
  //distance as (absolute) mouse speed vector, and reposition the mouse to the
  //center of the screen.
  float mouseSpeedX = 0, mouseSpeedY = 0;

  float capturedMouseX = currentWindowWidth / 2.0f;
  float capturedMouseY = currentWindowHeight / 2.0f;

  //Prevent the camera rotation to change too much until the game is actually 
  //visible. Also prevents the camera to rotate wildly due to the initial 
  //cursor warp to the screen center (which would otherwise be interpreted as
  //very rapid mouse movement).
  mouseSpeedX = (capturedMouseX - currentMouseX) * gameBrightness;
  mouseSpeedY = (capturedMouseY - currentMouseY) * gameBrightness;

  glutSetCursor(GLUT_CURSOR_NONE);
  glutWarpPointer((int)capturedMouseX, (int)capturedMouseY);

  //This mouse movement is then added to the player rotation accerlation 
  //(which will result into a smoother mouse movement afterwards).
  //The accerlation is dampened a bit and then applied to the player rotation, 
  //where the vertical mouse movement will eventually rotate the camera around 
  //the players' X axis (which points to the "right") and the horizontal mouse 
  //movement will rotate the camera around the players' Y axis (which points 
  //to the "top").
  playerRotationAccerlationX += mouseSpeedY * MOUSE_SPEED * deltaSeconds;
  playerRotationAccerlationY += mouseSpeedX * MOUSE_SPEED * deltaSeconds;
  playerRotationAccerlationX -=
    playerRotationAccerlationX * MOUSE_FRICTION * deltaSeconds;
  playerRotationAccerlationY -=
    playerRotationAccerlationY * MOUSE_FRICTION * deltaSeconds;

  playerRotationX += playerRotationAccerlationX;
  playerRotationY += playerRotationAccerlationY;

  //The raw (independent of the current player view) accerlation is calculated
  //using the current keyboard input values.
  float newAxisAccerlationX =
    (inputRight ? 1.0f : 0) - (inputLeft ? 1 : 0);
  float newAxisAccerlationZ =
    (inputForward ? 1.0f : 0) - (inputBackwards ? 1.0f : 0);

  //That new accerlation must be normalized so that the player doesn't move 
  //faster when going into two different directions simultaneously 
  //(e.g. forward and left).
  float newAxisAccerlation =
    (float)sqrt(pow(newAxisAccerlationX, 2) + pow(newAxisAccerlationZ, 2));
  if (fabsf(newAxisAccerlation) > 1.0f)
  {
    newAxisAccerlationX /= newAxisAccerlation;
    newAxisAccerlationZ /= newAxisAccerlation;
  }

  //Rotate the new "raw" accerlation depending the current player rotation 
  //with some sin/cos magic.
  float playerRotationYSin = (float)sin(Common_degToRad(playerRotationY));
  float playerRotationYCos = (float)cos(Common_degToRad(playerRotationY));
  float newAccerlationX = newAxisAccerlationX * playerRotationYCos
    - newAxisAccerlationZ * playerRotationYSin;
  float newAccerlationZ = newAxisAccerlationZ * playerRotationYCos
    + newAxisAccerlationX * playerRotationYSin;

  //Add the rotated accerlation to the overall rotation and apply friction.
  playerAccerlationX = (playerAccerlationX + newAccerlationX *
    (deltaSeconds * PLAYER_MAX_SPEED));
  playerAccerlationZ = (playerAccerlationZ + newAccerlationZ *
    (deltaSeconds * PLAYER_MAX_SPEED));

  //If the player hits jump (and is currently on the floor), the vertical 
  //accerlation is set to the PLAYER_JUMP_SPEED - so that the player bolts into
  //the air without inertia. While airborne, the accerlation will slowly 
  //decrease through continuosuly applied gravity. When the player falls back
  //down and finally hits the floor, the player will slightly bounce off the
  //floor a few times, depending on FLOOR_BOUNCYNESS, and then be static again.
  //The FLOOR_BOUNCYNESS prevents another jump while bouncing though - if that
  //is not wanted, FLOOR_BOUNCYNESS needs to be set to 0.
  if (playerY > CALCULATION_TRESHOLD)
    playerAccerlationY -= (PLAYER_GRAVITY * deltaSeconds);
  else if (inputJump)
    playerAccerlationY = PLAYER_JUMP_SPEED * deltaSeconds;
  else if (fabsf(playerAccerlationY) > CALCULATION_TRESHOLD)
    playerAccerlationY = -playerAccerlationY * FLOOR_BOUNCYNESS;
  else playerAccerlationY = 0;

  //Apply friction to the current player accerlation (on the X and Z axis).
  playerAccerlationX -= playerAccerlationX * (deltaSeconds * PLAYER_FRICTION);
  playerAccerlationZ -= playerAccerlationZ * (deltaSeconds * PLAYER_FRICTION);

  //Calculate the new player position and make that the new position if it 
  //doesn't collide with any fields that are not just "floor". If the player
  //collides with a wall or another object, invert the accerlation to give us
  //a small bounce effect.
  float newPlayerX = playerX + playerAccerlationX;
  float newPlayerY = MAX(0, playerY + playerAccerlationY);
  float newPlayerZ = playerZ + playerAccerlationZ;

  Field newPlayerField = Game_getMapFieldByPosition(newPlayerX, newPlayerZ);
  if (newPlayerField <= 0)
  {
    playerX = newPlayerX;
    playerZ = newPlayerZ;
  }
  else
  {
    playerAccerlationX = -playerAccerlationX;
    playerAccerlationZ = -playerAccerlationZ;
    playerX += playerAccerlationX;
    playerZ += playerAccerlationZ;
  }

  playerY = newPlayerY;

  //If the player hits the interaction key and is close to the quest item, the
  //item will be picked up. If he's currently carrying the item and is close
  //to the goal, the item will be dropped into the goal and the game is done.
  if (inputAction)
  {
    int currentPlayerFieldX, currentPlayerFieldZ;
    Game_getMapFieldIndiciesByPosition(playerX, playerZ,
      &currentPlayerFieldX, &currentPlayerFieldZ);

    for (int probeFieldX = currentPlayerFieldX - 1;
      probeFieldX <= currentPlayerFieldX + 1; probeFieldX++)
    {
      for (int probeFieldZ = currentPlayerFieldZ - 1;
        probeFieldZ <= currentPlayerFieldZ + 1; probeFieldZ++)
      {
        Field probedField =
          Game_getMapFieldByIndicies(probeFieldX, probeFieldZ);

        if (probedField == Item && itemState == Initial)
          itemState = Held;
        else if (probedField == Goal && itemState == Held)
          itemState = Dropped;
      }
    }
  }

  glutPostRedisplay();

  lastUpdateTime = currentUpdateTime;

  //I wonder what that mandatory value should really be used for...
  glutTimerFunc(UPDATE_TIMEOUT_MS, Game_onUpdate, 42);
}

//=============================================================================
// Main function.
//=============================================================================

int main(int argc, char **argv)
{
  printf("** GemQuest **\n");
  printf("Find the magic gem and yeet it into the GemContainer(TM)!\n");
  printf("Move: WASD, Jump: Space, Interact: E, Look: Mouse, Exit: ESC.\n");
  printf("Hint: If you can't move, click once with your left mouse button.\n");
  printf("Run game in fullscreen ('f') or window ('w'): ");
  int c = getchar();

  glutInit(&argc, argv);

  glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
  glutInitContextVersion(2, 0);
  glutInitWindowSize(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
  glutCreateWindow("OpenGL window");
  if (c == 'f') glutFullScreen();
  glewInit();

  Game_onLoad();

  glutDisplayFunc(Game_onRedraw);
  glutReshapeFunc(Game_onResize);
  glutKeyboardFunc(Game_onKeyboardDown);
  glutKeyboardUpFunc(Game_onKeyboardUp);
  glutPassiveMotionFunc(Game_onMouseMove);
  glutTimerFunc(UPDATE_TIMEOUT_MS, Game_onUpdate, 0);

  glutMainLoop();

  return 0;
}

//=============================================================================
// Python script for creating vertex data in the required format in Blender.
//=============================================================================
/*
import types
import bpy
import bmesh

def show_message(message = "", title = "Message Box", icon = 'INFO'):
    def draw(self, context):
        self.layout.label(text=message)
    bpy.context.window_manager.popup_menu(draw, title = title, icon = icon)

if len(bpy.context.selected_objects) == 0:
    show_message("Please select an object which should be exported.",
    "Empty selection", "ERROR")
elif len(bpy.context.selected_objects) > 1:
    show_message("Please only select one object which should be exported.",
    "Too many items selected", "ERROR")
elif not isinstance(bpy.context.selected_objects[0].data, bpy.types.Mesh):
    show_message("Please select an object with a mesh - \
    only these are supported for export.", "Invalid selection", "ERROR")
else:
    exportMesh = bmesh.new()
    exportMesh.from_mesh(bpy.context.selected_objects[0].data)
    bmesh.ops.triangulate(exportMesh, faces=exportMesh.faces)
    exportMesh.verts.ensure_lookup_table()

    try :
        colorLayer = exportMesh.loops.layers.color.active
    except:
        colorLayer = None

    vd = []

    for face in exportMesh.faces:
        for loop in face.loops:
            vd.append(round(loop.vert.co.x,3))
            vd.append(round(loop.vert.co.z,3))
            vd.append(round(loop.vert.co.y,3))

            if colorLayer is None:
                vd.append(1)
                vd.append(1)
                vd.append(1)
            else:
                color = loop[colorLayer]
                vd.append(round(color.x,3))
                vd.append(round(color.y,3))
                vd.append(round(color.z,3))

    meshDefinition = "{\n"

    i = 0
    while (i < len(vd)):
        meshDefinition += "   %sf, %sf, %sf, %sf, %sf, %sf,\n" % (
        vd[i], vd[i+1], vd[i+2], vd[i+3], vd[i+4], vd[i+5])
        i += 6
        if i % 18 == 0 and i < len(vd):
            meshDefinition += "\n"

    meshDefinition += "};"

    bpy.context.window_manager.clipboard = meshDefinition
    show_message("The mesh definition was copied to the clipboard!",
    "Operation successful")
*/