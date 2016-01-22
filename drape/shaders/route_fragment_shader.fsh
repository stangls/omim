varying vec4 v_length;

#ifdef SAMSUNG_GOOGLE_NEXUS
uniform sampler2D u_colorTex;
#endif

uniform vec4 u_color;
uniform vec4 u_color_tour;
uniform float u_tourParams;

const float kAntialiasingThreshold = 0.92;

void main(void)
{
#ifdef SAMSUNG_GOOGLE_NEXUS
  // Because of a bug in OpenGL driver on Samsung Google Nexus this workaround is here.
  const float kFakeColorScalar = 0.0;
  lowp vec4 fakeColor = texture2D(u_colorTex, vec2(0.0, 0.0)) * kFakeColorScalar;
#endif

  // default color
  vec4 color = u_color;

  // MX-addition: this allows a different color for the tour
  if (v_length.x >= u_tourParams){
      color = u_color_tour;
  }

  // MX-addition: this allows blending out the route/tour in case it crosses itself
  if (v_length.x > v_length.w){
    color.a*=0.5;
  }

  // before user-position: no rendering of route/tour
  if (v_length.x < v_length.z){
    color.a = 0.0;
  }else{
    // anti-aliasing of route/tour
    color.a *= (1.0 - smoothstep(kAntialiasingThreshold, 1.0, abs(v_length.y)));
  }

  // see above
#ifdef SAMSUNG_GOOGLE_NEXUS
  gl_FragColor = color + fakeColor;
#else
  gl_FragColor = color;
#endif
}
