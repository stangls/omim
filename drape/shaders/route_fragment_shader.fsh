varying vec4 v_length;

uniform vec4 u_color;
uniform vec4 u_color_light;

void main(void)
{
    if (v_length.x > v_length.w){
      gl_FragColor = u_color_light;
    }else{
      vec4 color = u_color;
      if (v_length.x < v_length.z)
        color.a = 0.0;
      gl_FragColor = color;
    }
}
