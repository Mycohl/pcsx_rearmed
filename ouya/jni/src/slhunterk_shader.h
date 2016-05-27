static const char* _slhunterk_vsh_source = 
"attribute vec4 a_Position;					\n"
"attribute vec2 a_TexCoord;					\n"
"varying vec2 v_TexCoord;					\n"
"void main()								\n"
"{											\n"
"   v_TexCoord = a_TexCoord;				\n"
"   gl_Position = a_Position;				\n"
"}											\n"
;

static const char* _slhunterk_fsh_source = 
"precision mediump float;													\n"
"uniform sampler2D u_TexUnit;												\n"
"uniform float u_Brightness;												\n"
"varying vec2 v_TexCoord;													\n"
"void main(void) {															\n"
"	vec4 rgb = texture2D(u_TexUnit, v_TexCoord);							\n"
"	vec4 intens;															\n"
"	intens = smoothstep(0.2,0.8,rgb) + normalize(vec4(rgb.xyz, 1.0));		\n"
"	intens *= step(0.5, fract(gl_FragCoord.y * 0.25));						\n"
"	float level = (4.0 - u_Brightness) * 0.19;								\n"
"	gl_FragColor = intens * (0.5-level) + rgb * 1.1 ;						\n"
"}																			\n"
;

static struct _slhunterk_shader_handles {
	GLuint program;
	GLuint position;
	GLuint texcoord;
	GLuint texunit;
	GLuint brightness;
} _slhunterk_handles;

