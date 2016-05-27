static const char* _slcgwg_vsh_source = 
"attribute vec4 a_Position;							\n"
"attribute vec2 a_TexCoord;							\n"
"uniform vec2 u_TexSize;							\n"
"uniform vec2 u_InSize;								\n"
"uniform vec2 u_OutSize;							\n"
"varying vec2 v_TexCoord;					\n"
"void main()								\n"
"{											\n"
"   v_TexCoord = a_TexCoord;				\n"
"   gl_Position = a_Position;				\n"
"}											\n"
;

static const char* _slcgwg_fsh_source = 
"precision mediump float;								\n"		// Set the default precision to medium.
"uniform sampler2D u_TexUnit;							\n"
"uniform vec2 u_TexSize;							\n"
"uniform vec2 u_InSize;								\n"
"uniform vec2 u_OutSize;							\n"
"varying vec2 v_TexCoord;					\n"

//"#extension GL_OES_standard_derivatives : enable		\n"

"#define TEX2D(c) texture2D(u_TexUnit,(c))			\n"
"#define PI 3.141592653589			\n"
"#define phase 0.0			\n"
"#define gamma 2.5			\n"

"void main()																					\n"
"{																								\n"
"	vec2 xy = v_TexCoord.xy;																	\n"

"	vec2 one          = 1.0/u_TexSize;															\n"
"	xy = xy + vec2(0.0 , -0.5 * (phase + (1.0-phase) * u_InSize.y/u_OutSize.y) * one.y);			\n"

"	vec2 uv_ratio     = fract(xy*u_TexSize);													\n"

"	vec4 coeffs = vec4(1.0 + uv_ratio.x, uv_ratio.x, 1.0 - uv_ratio.x, 2.0 - uv_ratio.x);		\n"
"	coeffs = (sin(PI * coeffs) * sin(PI * coeffs / 2.0)) / (coeffs * coeffs);					\n"
"	coeffs = coeffs / (coeffs.x+coeffs.y+coeffs.z+coeffs.w);									\n"

"	vec4 col;																					\n"
"	vec4 col2;   																				\n"

"	col  = clamp(coeffs.x * TEX2D(xy + vec2(-one.x,0.0))  										\n"
"					+ coeffs.y * TEX2D(xy)														\n"
"					+ coeffs.z * TEX2D(xy + vec2(one.x, 0.0))									\n" 
"					+ coeffs.w * TEX2D(xy + vec2(2.0 * one.x, 0.0)),0.0,1.0);					\n"
"	col2 = clamp(coeffs.x * TEX2D(xy + vec2(-one.x,one.y))										\n"
"					+ coeffs.y * TEX2D(xy + vec2(0.0, one.y)) 									\n"
"					+ coeffs.z * TEX2D(xy + one) 												\n"
"					+ coeffs.w * TEX2D(xy + vec2(2.0 * one.x, one.y)),0.0,1.0);					\n"
"	col =  pow(col,  vec4(gamma));																\n"
"	col2 = pow(col2, vec4(gamma));																\n"
"	vec4 wid;																					\n"
"	vec4 weights;																				\n"
"	vec4 weights2;																				\n"
"	wid = 0.2 + 0.4 * pow(col, vec4(2.0));														\n"
"	weights = uv_ratio.y / wid;																	\n"
"	weights = 0.51 * exp(-weights * weights) / wid;												\n"
"	wid = 0.2 + 0.4 * pow(col2, vec4(2.0));														\n"
"	weights2 = (1.0 - uv_ratio.y) / wid;														\n"
"	weights2 = 0.51 * exp(-weights2 * weights2) / wid;											\n"

"	gl_FragColor = pow(col * weights + col2 * weights2, vec4(1.0/2.2));							\n"
"}																								\n"
;

static struct _slcgwg_shader_handles {
	GLuint program;
	GLuint position;
	GLuint texcoord;
	GLuint texunit;
	GLuint texsize;
	GLuint insize;
	GLuint outsize;
} _slcgwg_handles;
