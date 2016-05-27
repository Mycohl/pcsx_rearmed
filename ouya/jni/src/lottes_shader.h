static const char* _lottes_vsh_source = 
	//~ "#version 110                   \n"
	"attribute vec4 a_Position;     \n"		// Per-vertex position information we will pass in.
	"attribute vec2 a_TexCoord;     \n"		// Per-vertex texture coordinates
	"varying vec2 v_TexCoord;       \n"		// This will be passed into the fragment shader.
	"void main()                    \n"		// The entry point for our vertex shader.
	"{                              \n"
	"   v_TexCoord = a_TexCoord;    \n" 	// Pass texture coordinate through to the fragment shader
	"   gl_Position = a_Position;   \n"		// gl_Position is a special variable used to store the final position.
	"}                              \n"	
;

static const char* _lottes_fsh_source = 
//
// PUBLIC DOMAIN CRT STYLED SCAN-LINE SHADER
//
//   by Timothy Lottes
//
// This is more along the style of a really good CGA arcade monitor.
// With RGB inputs instead of NTSC.
// The shadow mask example has the mask rotated 90 degrees for less chromatic aberration.
//
// Left it unoptimized to show the theory behind the algorithm.
//
// It is an example what I personally would want as a display option for pixel art games.
// Please take and use, change, or whatever.
//

//Comment these out to disable the corresponding effect.
//"#define CURVATURE						\n"	//Screen curvature effect.
//"#define YUV								\n"	//Tint and Saturation adjustments.
//"#define GAMMA_CONTRAST_BOOST				\n"//Expands contrast and makes image brighter but causes clipping.
"#define ORIGINAL_SCANLINES					\n"	//Enable to use the original scanlines.
"#define ORIGINAL_HARDPIX					\n"	//Enable to use the original hardPix calculation.  
												//But systems rendered in lower res textures will be much blurrier 
												//than systems in higher resolution textures (compare NES and TG16...)

"precision mediump float;\n"					// Set the default precision to medium.
"varying vec2 v_TexCoord;\n"					// This is the texture coordinate from the vertex shader

//Normal MAME GLSL Uniforms
"uniform sampler2D mpass_texture;\n"
"uniform vec2      color_texture_sz;\n"			// size of color_texture
"uniform vec2      color_texture_pow2_sz;\n"	// size of color texture rounded up to power of 2
//~ "uniform vec2      screen_texture_sz;\n"		// size of output resolution
//~ "uniform vec2      screen_texture_pow2_sz;\n"   // size of output resolution rounded up to power of 2

//CRT Filter Variables
"const float hardScan=-12.0;\n"					//-8,-12,-16, etc to make scalines more prominent.
"const vec2 warp=vec2(1.0/64.0,1.0/48.0);\n"	//adjusts the warp filter (curvature). Use "vec2 warp=vec2(1.0/32.0,1.0/24.0);" for a stronger effect.
"const float maskDark=0.4;\n"					//Sets how dark a "dark subpixel" is in the aperture pattern.
"const float maskLight=1.0;\n"					//Sets how dark a "bright subpixel" is in the aperture pattern.
"const float hardPix=-2.3;\n"					//-1,-2,-4, etc to make the upscaling sharper.


//Here are the Tint/Saturation/GammaContrastBoost Variables.  
//Comment out "#define YUV" and "#define GAMMA_CONTRAST_BOOST" to disable these altogether.
"const float PI = 3.1415926535;\n"
"const float blackClip = 0.08;\n"								//Drops the final color value by this amount if GAMMA_CONTRAST_BOOST is defined
"const float brightMult = 1.25;\n"								//Multiplies the color values by this amount if GAMMA_CONTRAST_BOOST is defined
"const vec3 gammaBoost = vec3(1.0/1.2, 1.0/1.2, 1.0/1.2);\n"	//An extra per channel gamma adjustment applied at the end.
"const float saturation = 1.25;\n"								// 1.0 is normal saturation. Increase as needed.
"const float tint = 0.1;\n"										//0.0 is 0.0 degrees of Tint. Adjust as needed.
"const float U = cos(tint*PI/180.0);\n"
"const float W = sin(tint*PI/180.0);\n"
"const vec3  YUVr=vec3(0.701*saturation*U+0.16774*saturation*W+0.299,0.587-0.32931*saturation*W-0.587*saturation*U,-0.497*saturation*W-0.114*saturation*U+0.114);\n"
"const vec3  YUVg=vec3(-0.3281*saturation*W-0.299*saturation*U+0.299,0.413*saturation*U+0.03547*saturation*W+0.587,0.114+0.29265*saturation*W-0.114*saturation*U);\n"
"const vec3  YUVb=vec3(0.299+1.24955*saturation*W-0.299*saturation*U,-1.04634*saturation*W-0.587*saturation*U+0.587,0.886*saturation*U-0.20321*saturation*W+0.114);\n"


//CRT Filter Functions

// sRGB to Linear.
// Assuming using sRGB typed textures this should not be needed.
"float ToLinear1(float c){return(c<=0.04045)?c/12.92:pow((c+0.055)/1.055,2.4);}\n"
"vec3 ToLinear(vec3 c){return vec3(ToLinear1(c.r),ToLinear1(c.g),ToLinear1(c.b));}\n"

// Linear to sRGB.
// Assuming using sRGB typed textures this should not be needed.
"float ToSrgb1(float c){return(c<0.0031308?c*12.92:1.055*pow(c,0.41666)-0.055);}\n"
"vec3 ToSrgb(vec3 c){return vec3(ToSrgb1(c.r),ToSrgb1(c.g),ToSrgb1(c.b));}\n"

// Nearest emulated sample given floating point position and texel offset.
// Also zero's off screen.
"vec3 Fetch(vec2 pos,vec2 off){\n"
"  pos=(floor(pos*color_texture_pow2_sz+off)+0.5)/color_texture_pow2_sz;\n"
"  //if(max(abs(pos.x-0.5),abs(pos.y-0.5))>0.5)return vec3(0.0,0.0,0.0);\n"
"  return ToLinear(texture2D(mpass_texture,pos.xy).rgb);}\n"

// Distance in emulated pixels to nearest texel.
"vec2 Dist(vec2 pos){pos=pos*color_texture_pow2_sz;return -((pos-floor(pos))-vec2(0.5));}\n"
    
// 1D Gaussian.
"float Gaus(float pos,float scale){return exp2(scale*pos*pos);}\n"

// 3-tap Gaussian filter along horz line.
"vec3 Horz3(vec2 pos,float off){\n"
"  vec3 b=Fetch(pos,vec2(-1.0,off));\n"
"  vec3 c=Fetch(pos,vec2( 0.0,off));\n"
"  vec3 d=Fetch(pos,vec2( 1.0,off));\n"
"  float dst=Dist(pos).x;\n"
  // Convert distance to weight.
"#ifdef ORIGINAL_HARDPIX\n"
"  float scale=hardPix;\n"
"#else\n"
"  float scale=hardPix * max(0.0, 2.0-color_texture_sz.x/512.0);\n"		//Modified to keep sharpness somewhat comparable across drivers.
"#endif\n"
"  float wb=Gaus(dst-1.0,scale);\n"
"  float wc=Gaus(dst+0.0,scale);\n"
"  float wd=Gaus(dst+1.0,scale);\n"
  // Return filtered sample.
"  return (b*wb+c*wc+d*wd)/(wb+wc+wd);}\n"

// 5-tap Gaussian filter along horz line.
"vec3 Horz5(vec2 pos,float off){\n"
"  vec3 a=Fetch(pos,vec2(-2.0,off));\n"
"  vec3 b=Fetch(pos,vec2(-1.0,off));\n"
"  vec3 c=Fetch(pos,vec2( 0.0,off));\n"
"  vec3 d=Fetch(pos,vec2( 1.0,off));\n"
"  vec3 e=Fetch(pos,vec2( 2.0,off));\n"
"  float dst=Dist(pos).x;\n"
  // Convert distance to weight.
"#ifdef ORIGINAL_HARDPIX		\n"
"  float scale=hardPix;\n"
"#else							\n"
"  float scale=hardPix * max(0.0, 2.0-color_texture_sz.x/512.0);\n"		//Modified to keep sharpness somewhat comparable across drivers.
"#endif							\n"
"  float wa=Gaus(dst-2.0,scale);\n"
"  float wb=Gaus(dst-1.0,scale);\n"
"  float wc=Gaus(dst+0.0,scale);\n"
"  float wd=Gaus(dst+1.0,scale);\n"
"  float we=Gaus(dst+2.0,scale);\n"
  // Return filtered sample.
"  return (a*wa+b*wb+c*wc+d*wd+e*we)/(wa+wb+wc+wd+we);}\n"

// Return scanline weight.
"float Scan(vec2 pos,float off){\n"
"  float dst=Dist(pos).y;\n"
"  vec3 col=Fetch(pos,vec2(0.0));\n"
"#ifdef ORIGINAL_SCANLINES			\n"
"  return Gaus(dst+off,hardScan);}\n"
"#else								\n"
"  return Gaus(dst+off,hardScan/(dot(col,col)*0.1667+1.0));}\n" //Modified to make scanline respond to pixel brightness
"#endif								\n"

// Allow nearest three lines to effect pixel.
"vec3 Tri(vec2 pos){\n"
"  vec3 a=Horz3(pos,-1.0);\n"
"  vec3 b=Horz5(pos, 0.0);\n"
"  vec3 c=Horz3(pos, 1.0);\n"
"  float wa=Scan(pos,-1.0);\n"
"  float wb=Scan(pos, 0.0);\n"
"  float wc=Scan(pos, 1.0);\n"
"  return a*wa+b*wb+c*wc;}\n"

// Distortion of scanlines, and end of screen alpha.
"vec2 Warp(vec2 pos){\n"
"  pos=pos*2.0-1.0;\n"
"  pos*=vec2(1.0+(pos.y*pos.y)*warp.x,1.0+(pos.x*pos.x)*warp.y);\n"
"  return pos*0.5+0.5;}\n"
	
// Shadow mask.
"vec3 Mask(vec2 pos){\n"
"  pos.x+=pos.y*3.0;\n"
"  vec3 mask=vec3(maskDark,maskDark,maskDark);\n"
"  pos.x=fract(pos.x/6.0);\n"
"  if(pos.x<0.333)mask.r=maskLight;\n"
"  else if(pos.x<0.666)mask.g=maskLight;\n"
"  else mask.b=maskLight;\n"
"  return mask;}\n"

// Entry
"void main(void){\n"

"#ifdef CURVATURE							\n"
"  vec2 pos=Warp(v_TexCoord);			\n"
"#else										\n"
"  vec2 pos=v_TexCoord;					\n"
"#endif										\n"

"  gl_FragColor.rgb=Tri(pos)*Mask(gl_FragCoord.xy);\n"
	 
//The YUV Saturation/Tint is not part of Timothy Lottes' original shader but I like it...
"#ifdef YUV									\n"
"  gl_FragColor.rgb = vec3(dot(YUVr,gl_FragColor.rgb),dot(YUVg,gl_FragColor.rgb),dot(YUVb,gl_FragColor.rgb));\n"
"  gl_FragColor.rgb=clamp(gl_FragColor.rgb,0.0,1.0);\n"
"#endif 									\n"
  
"  gl_FragColor.rgb=clamp(ToSrgb(gl_FragColor.rgb),0.0,1.0);\n"
	
"#ifdef GAMMA_CONTRAST_BOOST				\n"
"  gl_FragColor.rgb=brightMult*pow(gl_FragColor.rgb,gammaBoost )-vec3(blackClip);\n"
"#endif										\n"
"};\n"
;

static struct _lottes_shader_handles {
	GLuint program;
	GLuint position;
	GLuint texcoord;
	GLuint texunit;
	GLuint color_texture_sz;			// size of color_texture
	GLuint color_texture_pow2_sz;	// size of color texture rounded up to power of 2
} _lottes_handles;

