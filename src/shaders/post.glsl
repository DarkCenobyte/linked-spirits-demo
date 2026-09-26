in vec2 q;
out vec4 o;
layout(binding=0) uniform sampler2D A;
layout(binding=1) uniform sampler2D B;
layout(binding=2) uniform sampler2D X;
void main(){
#ifdef DOWN
o=texture(A,q);
#endif
#ifdef UP
o=texture(A,q)*.5;
#endif
#ifdef FINAL
vec3 c=texture(A,q).rgb+texture(B,q).rgb*.1;
o=vec4(c/(1.+c),1);
#endif
}
