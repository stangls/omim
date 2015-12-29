varying vec4 v_length;

#ifdef SAMSUNG_GOOGLE_NEXUS
uniform sampler2D u_colorTex;
#endif

uniform vec4 u_color;
uniform vec4 u_color_light;

void main(void)
{
#ifdef SAMSUNG_GOOGLE_NEXUS
  // Because of a bug in OpenGL driver on Samsung Google Nexus this workaround is here.
  const float kFakeColorScalar = 0.0;
  lowp vec4 fakeColor = texture2D(u_colorTex, vec2(0.0, 0.0)) * kFakeColorScalar;
#endif

    // we can safely assume that v_length.w ( = position of non-crossing ) is greater than v_length.z ( = position of user )
    if (v_length.x > v_length.w){
#ifdef SAMSUNG_GOOGLE_NEXUS
      gl_FragColor = u_color_light + fakeColor;
#else
      gl_FragColor = u_color_light;
#endif
    }else{
      vec4 color = u_color;
      if (v_length.x < v_length.z)
        color.a = 0.0;
#ifdef SAMSUNG_GOOGLE_NEXUS
    gl_FragColor = color + fakeColor;
#else
    gl_FragColor = color;
  }
#endif
}
