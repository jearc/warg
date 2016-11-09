#version 430
uniform sampler2D current;
uniform sampler2D previous;

layout(location = 0) in vec2 uv;

void main()
{
  vec4 previous = texture2D(previous, uv);
  vec4 current = texture2D(current, uv);

  // vec4 result = vec4(0,1,0,1);
  // if(current!=previous)
  //{
  //	result = vec4(1,0,0,1);
  //}

  vec4 result = mix(previous, current, 0.5f);
  gl_FragData[0] = result;
}