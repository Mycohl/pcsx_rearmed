static const char* _slthemaister_vsh_source = 
"uniform vec2 u_TexSize;					\n"
"uniform vec2 u_InSize;						\n"
"uniform vec2 u_OutSize;					\n"
"attribute vec4 a_Position;					\n"
"attribute vec2 a_TexCoord;					\n"
"varying vec2 v_TexCoord;					\n"
"varying vec2 omega;						\n"
"void main()								\n"
"{											\n"
"   v_TexCoord = a_TexCoord;				\n"
"   gl_Position = a_Position;				\n"
"	omega = vec2(3.1415 * u_OutSize.x * u_TexSize.x / u_InSize.x, 2.0 * 3.1415 * u_TexSize.y);		\n"
"}											\n"
;

static const char* _slthemaister_fsh_source = 
"precision mediump float;													\n"
"uniform sampler2D u_TexUnit;												\n"
"varying vec2 v_TexCoord;													\n"
"varying vec2 omega;														\n"
"const float base_brightness = 0.60;										\n"
"const vec2 sine_comp = vec2(0.0005, 0.750);								\n"
"void main ()																\n"
"{																			\n"
"	vec4 c11 = texture2D(u_TexUnit, v_TexCoord);							\n"
"	vec4 scanline = c11 * (base_brightness + dot(sine_comp * sin(v_TexCoord * omega), vec2(1.0)));	\n"
"	gl_FragColor = clamp(scanline, 0.0, 1.0);								\n"
"}																			\n"
;

static struct _slthemaister_shader_handles {
	GLuint program;
	GLuint position;
	GLuint texcoord;
	GLuint texunit;
	GLuint texsize;
	GLuint insize;
	GLuint outsize;
} _slthemaister_handles;

