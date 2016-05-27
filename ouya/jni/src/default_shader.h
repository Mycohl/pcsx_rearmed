static const char* _default_vsh_source = 
"attribute vec4 a_Position;					\n"		// Per-vertex position information we will pass in.
"attribute vec2 a_TexCoord;					\n"		// Per-vertex texture coordinates
"varying vec2 v_TexCoord;					\n"		// This will be passed into the fragment shader.
"void main()								\n"		// The entry point for our vertex shader.
"{											\n"
"   v_TexCoord = a_TexCoord;				\n" 	// Pass texture coordinate through to the fragment shader
"   gl_Position = a_Position;				\n"		// gl_Position is a special variable used to store the final position.
"}											\n"	
;

static const char* _default_fsh_source = 
"precision mediump float;								\n"		// Set the default precision to medium.
"uniform sampler2D u_TexUnit;							\n"
"varying vec2 v_TexCoord;								\n"		// This is the texture coordinate from the vertex shader
"void main()											\n"		// The entry point for our fragment shader.
"{														\n"
"    gl_FragColor = texture2D(u_TexUnit, v_TexCoord);	\n"
"}														\n"
;

static struct _default_shader_handles {
	GLuint program;
	GLuint position;
	GLuint texcoord;
	GLuint texunit;
} _default_handles;
